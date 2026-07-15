// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//third_party/blink/renderer/platform:rustfft_ffi";
}

use rust_gtest_interop::prelude::*;
use rustfft::num_complex::Complex;
use rustfft_ffi::{prepare_inverse_fft_even, reconstruct_real_fft_even, rustfft_new};

#[gtest(RustFftTest, HelperFunctions)]
fn test_helpers() {
    // We can test with N=4 equivalent.
    // half_size = 2, limit = 0, middle_index = Some(1)
    let z = vec![Complex::new(1.0, 0.0), Complex::new(2.0, 3.0)];
    let twiddles = vec![Complex::new(0.5, 0.0), Complex::new(0.0, -0.5)];
    let mut real_data = vec![0.0; 2];
    let mut imag_data = vec![0.0; 2];

    reconstruct_real_fft_even(&z, &twiddles, 2, 0, Some(1), &mut real_data, &mut imag_data);

    // Expected:
    // real_data[0] = z[0].re + z[0].im = 1.0 + 0.0 = 1.0
    // imag_data[0] = z[0].re - z[0].im = 1.0 - 0.0 = 1.0
    // middle_index = Some(1) ->
    // real_data[1] = z[1].re = 2.0
    // imag_data[1] = -z[1].im = -3.0
    expect_eq!(real_data[0], 1.0);
    expect_eq!(imag_data[0], 1.0);
    expect_eq!(real_data[1], 2.0);
    expect_eq!(imag_data[1], -3.0);

    // Now test prepare_inverse_fft with these outputs to see if we get back z.
    let mut z_out = vec![Complex::new(0.0, 0.0); 2];
    prepare_inverse_fft_even(&real_data, &imag_data, &twiddles, 2, 0, Some(1), &mut z_out);

    expect_eq!(z_out[0], z[0]);
    expect_eq!(z_out[1], z[1]);
}

#[gtest(RustFftTest, ForwardInverseEven)]
fn test_forward_inverse_even() {
    let mut fft = rustfft_new(4);
    let input = vec![1.0, 2.0, 3.0, 4.0];
    let mut real = vec![0.0; 2];
    let mut imag = vec![0.0; 2];
    fft.do_fft(&input, &mut real, &mut imag);

    let mut output = vec![0.0; 4];
    fft.do_inverse_fft(&real, &imag, &mut output);

    const EPSILON: f32 = 1e-6;
    for i in 0..4 {
        expect_true!((output[i] - input[i]).abs() < EPSILON);
    }
}

#[gtest(RustFftTest, ForwardInverseOdd)]
fn test_forward_inverse_odd() {
    let mut fft = rustfft_new(5);
    let input = vec![1.0, 2.0, 3.0, 4.0, 5.0];
    let mut real = vec![0.0; 3];
    let mut imag = vec![0.0; 3];
    fft.do_fft(&input, &mut real, &mut imag);

    let mut output = vec![0.0; 5];
    fft.do_inverse_fft(&real, &imag, &mut output);

    const EPSILON: f32 = 1e-6;
    for i in 0..5 {
        expect_true!((output[i] - input[i]).abs() < EPSILON);
    }
}
