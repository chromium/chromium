// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! FFI interface for RustFFT.
//!
//! This module provides helper functions to plan and execute FFTs using the
//! `rustfft` crate, designed to be called from C++ WebAudio code.

use rustfft::num_complex::Complex;
use rustfft::{Fft, FftPlanner};
use std::collections::HashMap;
use std::sync::{Arc, OnceLock, RwLock};

/// A pair of FFT plans: (Forward, Inverse).
type FftPair = (Arc<dyn Fft<f32>>, Arc<dyn Fft<f32>>);

/// Global cache of FFT plans, mapping FFT size to its forward/inverse plan
/// pair.
static CACHE: OnceLock<RwLock<HashMap<usize, FftPair>>> = OnceLock::new();

/// Helper to retrieve the global cache, initializing it on first access.
fn get_cache() -> &'static RwLock<HashMap<usize, FftPair>> {
    CACHE.get_or_init(|| RwLock::new(HashMap::new()))
}

/// Retrieves a cached FFT plan pair for the given size, or creates and caches
/// it if not present.
///
/// This uses double-checked locking to minimize lock contention. Planning is
/// done outside the write lock, allowing parallel planning for different sizes.
fn get_fft(size: usize) -> FftPair {
    let cache = get_cache();

    if let Some(pair) = cache.read().unwrap().get(&size) {
        return pair.clone();
    }

    let mut planner = FftPlanner::new();
    let forward = planner.plan_fft_forward(size);
    let inverse = planner.plan_fft_inverse(size);
    let pair = (forward, inverse);

    let mut cache_write = cache.write().unwrap();
    cache_write.entry(size).or_insert(pair).clone()
}

/// Strategy for computing the FFT based on the input size.
enum FftStrategy {
    /// For even sizes, we use a half-size complex FFT to compute the real FFT.
    /// This is faster but requires packing/unpacking math.
    Even {
        fft_forward: Arc<dyn Fft<f32>>,
        fft_inverse: Arc<dyn Fft<f32>>,
        /// Precomputed twiddle factors for the reconstruction phase.
        twiddles: Vec<Complex<f32>>,
        half_size: usize,
        limit: usize,
        middle_index: Option<usize>,
    },
    /// For odd sizes, we use a full-size complex FFT.
    Odd { fft_forward: Arc<dyn Fft<f32>>, fft_inverse: Arc<dyn Fft<f32>> },
}

/// Wrapper around RustFFT that matches the `FFTFrame` interface in Blink.
pub struct RustFft {
    strategy: FftStrategy,
    /// Intermediate buffer for complex FFT data.
    complex_data: Vec<Complex<f32>>,
    /// Scratch space required by RustFFT.
    scratch: Vec<Complex<f32>>,
    /// Scaling factor applied during inverse FFT.
    scale: f32,
    /// Size of the FFT.
    size: usize,
}

/// Creates a new `RustFft` instance.
///
/// For even sizes, it initializes the `Even` strategy which uses a half-size
/// FFT.
pub fn rustfft_new(size: usize) -> Box<RustFft> {
    if size.is_multiple_of(2) {
        let half_size = size / 2;
        let (fft_forward, fft_inverse) = get_fft(half_size);
        let complex_data = vec![Complex::new(0.0, 0.0); half_size];
        let scratch_len = std::cmp::max(
            fft_forward.get_inplace_scratch_len(),
            fft_inverse.get_inplace_scratch_len(),
        );
        let scratch = vec![Complex::new(0.0, 0.0); scratch_len];
        let scale = 1.0 / (half_size as f32);

        let twiddles: Vec<Complex<f32>> = (0..half_size)
            .map(|k| {
                let angle = -2.0 * std::f64::consts::PI * (k as f64) / (size as f64);
                let (sin, cos) = angle.sin_cos();
                Complex::new((0.5 * cos) as f32, (0.5 * sin) as f32)
            })
            .collect();

        let limit = (half_size - 1) / 2;
        let middle_index = if half_size.is_multiple_of(2) { Some(half_size / 2) } else { None };

        Box::new(RustFft {
            strategy: FftStrategy::Even {
                fft_forward,
                fft_inverse,
                twiddles,
                half_size,
                limit,
                middle_index,
            },
            complex_data,
            scratch,
            scale,
            size,
        })
    } else {
        let (fft_forward, fft_inverse) = get_fft(size);
        let complex_data = vec![Complex::new(0.0, 0.0); size];
        let scratch_len = std::cmp::max(
            fft_forward.get_inplace_scratch_len(),
            fft_inverse.get_inplace_scratch_len(),
        );
        let scratch = vec![Complex::new(0.0, 0.0); scratch_len];
        let scale = 1.0 / (size as f32);
        Box::new(RustFft {
            strategy: FftStrategy::Odd { fft_forward, fft_inverse },
            complex_data,
            scratch,
            scale,
            size,
        })
    }
}

/// Reconstructs the real FFT output from the half-size complex FFT output `z`.
///
/// This is used for even-sized FFTs.
pub fn reconstruct_real_fft_even(
    z: &[Complex<f32>],
    twiddles: &[Complex<f32>],
    half_size: usize,
    limit: usize,
    middle_index: Option<usize>,
    real_data: &mut [f32],
    imag_data: &mut [f32],
) {
    // Reconstruct the real FFT output from the half-size complex FFT.
    // The output is packed: real_data[0] stores the DC component,
    // and imag_data[0] stores the Nyquist component (which is real-valued).
    real_data[0] = z[0].re + z[0].im;
    imag_data[0] = z[0].re - z[0].im;

    // Reconstruct the remaining frequency bins using symmetries.
    // We split the complex FFT output Z[k] into even and odd parts of the
    // real FFT, and then combine them using the precomputed twiddle factors.
    for k in 1..=limit {
        let z_k = z[k];
        let z_mk = z[half_size - k];

        // F_even[k] = 0.5 * (Z[k] + Z*[N-k])
        let f_even = Complex::new(0.5 * (z_k.re + z_mk.re), 0.5 * (z_k.im - z_mk.im));
        // F_odd[k] = 0.5/i * (Z[k] - Z*[N-k])
        let f_odd_unscaled = Complex::new(z_k.im + z_mk.im, -(z_k.re - z_mk.re));
        let w_f_odd = twiddles[k] * f_odd_unscaled;

        // X[k] = F_even[k] + W^k * F_odd[k]
        let x_k = f_even + w_f_odd;
        // X[N-k] = (F_even[k] - W^k * F_odd[k])* (using Hermitian symmetry)
        let x_mk_conj = f_even - w_f_odd;

        real_data[k] = x_k.re;
        imag_data[k] = x_k.im;

        real_data[half_size - k] = x_mk_conj.re;
        imag_data[half_size - k] = -x_mk_conj.im;
    }

    if let Some(k) = middle_index {
        let z_k = z[k];
        real_data[k] = z_k.re;
        imag_data[k] = -z_k.im;
    }
}

/// Prepares the complex input `z` for the half-size inverse FFT from the packed
/// real/imaginary data.
///
/// This is used for even-sized FFTs.
pub fn prepare_inverse_fft_even(
    real_data: &[f32],
    imag_data: &[f32],
    twiddles: &[Complex<f32>],
    half_size: usize,
    limit: usize,
    middle_index: Option<usize>,
    z: &mut [Complex<f32>],
) {
    // Pack the DC and Nyquist components back into the complex array.
    // This is the inverse of the packing done in do_fft.
    z[0] = Complex::new(0.5 * (real_data[0] + imag_data[0]), 0.5 * (real_data[0] - imag_data[0]));

    // Reconstruct the complex FFT input from the real FFT output.
    // We extract the even and odd parts, apply the conjugate of the
    // twiddle factors, and combine them back into Z[k].
    for k in 1..=limit {
        let r_k = real_data[k];
        let i_k = imag_data[k];
        let r_mk = real_data[half_size - k];
        let i_mk = imag_data[half_size - k];

        let x_k = Complex::new(r_k, i_k);
        let x_mk_conj = Complex::new(r_mk, -i_mk);

        let f_even = (x_k + x_mk_conj) * 0.5;
        let x_diff = x_k - x_mk_conj;
        // Multiply by conjugate of twiddle factors for inverse
        let f_odd = twiddles[k].conj() * x_diff;
        let i_f_odd = Complex::new(-f_odd.im, f_odd.re);

        z[k] = f_even + i_f_odd;
        z[half_size - k] = (f_even - i_f_odd).conj();
    }

    if let Some(k) = middle_index {
        z[k] = Complex::new(real_data[k], -imag_data[k]);
    }
}

impl RustFft {
    /// Computes the forward FFT for even-sized inputs using a half-size complex
    /// FFT.
    ///
    /// This method copies the real input data into the complex buffer, performs
    /// a half-size complex FFT, and then reconstructs the full real FFT output
    /// using symmetry properties.
    fn do_fft_even(&mut self, data: &[f32], real_data: &mut [f32], imag_data: &mut [f32]) {
        let FftStrategy::Even { fft_forward, twiddles, half_size, limit, middle_index, .. } =
            &self.strategy
        else {
            unreachable!();
        };

        assert_eq!(self.complex_data.len(), *half_size);
        assert!(real_data.len() >= *half_size);
        assert!(imag_data.len() >= *half_size);

        // SAFETY: `Complex<f32>` is layout-compatible with `[f32; 2]` and has the same
        // alignment as `f32`. `self.complex_data` has length `half_size`, so casting it
        // to `f32` slice yields a slice of length `2 * half_size`, which is equal to
        // `self.size` (since `self.size` is even). The memory is valid and aligned to
        // `f32` boundary, and no other references to it exist while `flat_complex` is
        // alive.
        let flat_complex: &mut [f32] = unsafe {
            std::slice::from_raw_parts_mut(self.complex_data.as_mut_ptr() as *mut f32, self.size)
        };
        flat_complex.copy_from_slice(data);

        fft_forward.process_with_scratch(&mut self.complex_data, &mut self.scratch);

        reconstruct_real_fft_even(
            &self.complex_data,
            twiddles,
            *half_size,
            *limit,
            *middle_index,
            real_data,
            imag_data,
        );
    }

    /// Computes the forward FFT for odd-sized inputs using a full-size complex
    /// FFT.
    ///
    /// This method copies the real input data into the complex buffer (with
    /// zero imaginary part), performs a full-size complex FFT, and then
    /// packs the results into the output buffers.
    fn do_fft_odd(&mut self, data: &[f32], real_data: &mut [f32], imag_data: &mut [f32]) {
        let FftStrategy::Odd { fft_forward, .. } = &self.strategy else {
            unreachable!();
        };

        assert_eq!(self.complex_data.len(), self.size);
        let packed_size = self.size.div_ceil(2);
        assert!(real_data.len() >= packed_size);
        assert!(imag_data.len() >= packed_size);

        for (c, &r) in self.complex_data.iter_mut().zip(data.iter()) {
            *c = Complex::new(r, 0.0);
        }

        fft_forward.process_with_scratch(&mut self.complex_data, &mut self.scratch);

        real_data[0] = self.complex_data[0].re;
        imag_data[0] = 0.0;

        let complex_part = &self.complex_data[1..packed_size];
        let real_part = &mut real_data[1..packed_size];
        let imag_part = &mut imag_data[1..packed_size];

        for ((c, r), im) in complex_part.iter().zip(real_part.iter_mut()).zip(imag_part.iter_mut())
        {
            *r = c.re;
            *im = c.im;
        }
    }

    /// Computes the forward FFT.
    ///
    /// The input `data` must have the same size as the FFT size.
    /// The results are written to `real_data` and `imag_data` in a packed
    /// format.
    pub fn do_fft(&mut self, data: &[f32], real_data: &mut [f32], imag_data: &mut [f32]) {
        assert_eq!(data.len(), self.size);

        match &self.strategy {
            FftStrategy::Even { .. } => self.do_fft_even(data, real_data, imag_data),
            FftStrategy::Odd { .. } => self.do_fft_odd(data, real_data, imag_data),
        }
    }

    /// Computes the inverse FFT for even-sized inputs using a half-size complex
    /// FFT.
    ///
    /// This method prepares the complex buffer from the packed real/imaginary
    /// input data using symmetry properties, performs a half-size complex
    /// inverse FFT, and then copies the real part of the result to the
    /// output buffer with appropriate scaling.
    fn do_inverse_fft_even(&mut self, real_data: &[f32], imag_data: &[f32], data: &mut [f32]) {
        let FftStrategy::Even { fft_inverse, twiddles, half_size, limit, middle_index, .. } =
            &self.strategy
        else {
            unreachable!();
        };

        assert_eq!(self.complex_data.len(), *half_size);
        assert!(real_data.len() >= *half_size);
        assert!(imag_data.len() >= *half_size);

        prepare_inverse_fft_even(
            real_data,
            imag_data,
            twiddles,
            *half_size,
            *limit,
            *middle_index,
            &mut self.complex_data,
        );

        fft_inverse.process_with_scratch(&mut self.complex_data, &mut self.scratch);

        // SAFETY: `Complex<f32>` is layout-compatible with `[f32; 2]` and has the same
        // alignment as `f32`. `z` (which is `self.complex_data`) has length
        // `half_size`, so casting it to `f32` slice yields a slice of
        // length `2 * half_size`, which is equal to `self.size` (since
        // `self.size` is even). The memory is valid and aligned to
        // `f32` boundary, and is not mutated while `flat_z` is alive.
        let flat_z: &[f32] = unsafe {
            std::slice::from_raw_parts(self.complex_data.as_ptr() as *const f32, self.size)
        };
        for (d, &z_val) in data.iter_mut().zip(flat_z.iter()) {
            *d = z_val * self.scale;
        }
    }

    /// Computes the inverse FFT for odd-sized inputs using a full-size complex
    /// FFT.
    ///
    /// This method reconstructs the full complex spectrum from the packed
    /// real/imaginary input data using Hermitian symmetry, performs a full-size
    /// complex inverse FFT, and then copies the real part of the result to the
    /// output buffer with appropriate scaling.
    fn do_inverse_fft_odd(&mut self, real_data: &[f32], imag_data: &[f32], data: &mut [f32]) {
        let FftStrategy::Odd { fft_inverse, .. } = &self.strategy else {
            unreachable!();
        };

        let packed_size = self.size.div_ceil(2);
        assert!(real_data.len() >= packed_size);
        assert!(imag_data.len() >= packed_size);
        assert_eq!(self.complex_data.len(), self.size);

        self.complex_data[0] = Complex::new(real_data[0], 0.0);

        let (first_half, second_half) = self.complex_data.split_at_mut(packed_size);
        let first_part = &mut first_half[1..packed_size];
        let second_part = second_half;

        let real_part = &real_data[1..packed_size];
        let imag_part = &imag_data[1..packed_size];

        for (((c_first, c_second), &r), &im) in first_part
            .iter_mut()
            .zip(second_part.iter_mut().rev())
            .zip(real_part.iter())
            .zip(imag_part.iter())
        {
            *c_first = Complex::new(r, im);
            *c_second = Complex::new(r, -im);
        }

        fft_inverse.process_with_scratch(&mut self.complex_data, &mut self.scratch);

        for (d, c) in data.iter_mut().zip(self.complex_data.iter()) {
            *d = c.re * self.scale;
        }
    }

    /// Computes the inverse FFT.
    ///
    /// The input `real_data` and `imag_data` must be in the packed format.
    /// The reconstructed real signal is written to `data`.
    pub fn do_inverse_fft(&mut self, real_data: &[f32], imag_data: &[f32], data: &mut [f32]) {
        assert_eq!(data.len(), self.size);

        match &self.strategy {
            FftStrategy::Even { .. } => self.do_inverse_fft_even(real_data, imag_data, data),
            FftStrategy::Odd { .. } => self.do_inverse_fft_odd(real_data, imag_data, data),
        }
    }
}

/// FFI bridge definition for CXX.
///
/// This defines the interface exposed to C++ WebAudio code.
#[cxx::bridge(namespace = "blink::rust_fft")]
pub mod ffi {
    extern "Rust" {
        type RustFft;
        fn rustfft_new(size: usize) -> Box<RustFft>;
        fn do_fft(self: &mut RustFft, data: &[f32], real_data: &mut [f32], imag_data: &mut [f32]);
        fn do_inverse_fft(
            self: &mut RustFft,
            real_data: &[f32],
            imag_data: &[f32],
            data: &mut [f32],
        );
    }
}
