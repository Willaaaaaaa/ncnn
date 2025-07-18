// Copyright 2021 Tencent
// SPDX-License-Identifier: BSD-3-Clause

static void deconvolution_pack1ton_rvv(const Mat& bottom_blob, Mat& top_blob, const Mat& weight_data_pack1ton, const Mat& bias_data, int kernel_w, int kernel_h, int dilation_w, int dilation_h, int stride_w, int stride_h, int activation_type, const Mat& activation_params, const Option& opt)
{
    const int packn = csrr_vlenb() / 4;
    const size_t vl = __riscv_vsetvl_e32m1(packn);

    int w = bottom_blob.w;
    int h = bottom_blob.h;
    int channels = bottom_blob.c;

    int outw = top_blob.w;
    int outh = top_blob.h;
    int outch = top_blob.c;

    const int kernel_extent_w = dilation_w * (kernel_w - 1) + 1;
    const int kernel_extent_h = dilation_h * (kernel_h - 1) + 1;

    const int maxk = kernel_w * kernel_h;

    const float* bias_data_ptr = bias_data;

    // num_output
    #pragma omp parallel for num_threads(opt.num_threads)
    for (int p = 0; p < outch; p++)
    {
        float* outptr = top_blob.channel(p);

        for (int i = 0; i < outh; i++)
        {
            for (int j = 0; j < outw; j++)
            {
                vfloat32m1_t _sum = __riscv_vfmv_v_f_f32m1(0.f, vl);

                if (bias_data_ptr)
                {
                    _sum = __riscv_vle32_v_f32m1(bias_data_ptr + p * packn, vl);
                }

                const float* kptr = (const float*)weight_data_pack1ton + maxk * channels * p * packn;

                // channels
                for (int q = 0; q < channels; q++)
                {
                    const Mat m = bottom_blob.channel(q);

                    for (int y = 0; y < kernel_h; y++)
                    {
                        int sys = (i + y * dilation_h - (kernel_extent_h - 1));
                        if (sys < 0 || sys % stride_h != 0)
                            continue;

                        int sy = sys / stride_h;
                        if (sy >= h)
                            continue;

                        const float* sptr = m.row(sy);

                        for (int x = 0; x < kernel_w; x++)
                        {
                            int sxs = (j + x * dilation_w - (kernel_extent_w - 1));
                            if (sxs < 0 || sxs % stride_w != 0)
                                continue;

                            int sx = sxs / stride_w;
                            if (sx >= w)
                                continue;

                            float val = sptr[sx];

                            int k = y * kernel_w + x;

                            vfloat32m1_t _w = __riscv_vle32_v_f32m1(kptr + k * packn, vl);
                            _sum = __riscv_vfmacc_vf_f32m1(_sum, val, _w, vl);
                        }
                    }

                    kptr += maxk * packn;
                }

                _sum = activation_ps(_sum, activation_type, activation_params, vl);

                __riscv_vse32_v_f32m1(outptr + j * packn, _sum, vl);
            }

            outptr += outw * packn;
        }
    }
}
