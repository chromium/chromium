/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/audio/fft_frame.h"

#include <complex>
#include <memory>
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

#ifndef NDEBUG
#include <stdio.h>
#endif

namespace blink {

void FFTFrame::DoPaddedFFT(const float* data, size_t data_size) {
  // Zero-pad the impulse response
  AudioFloatArray padded_response(FftSize());  // zero-initialized
  padded_response.CopyToRange(data, 0, data_size);

  // Get the frequency-domain version of padded response
  DoFFT(padded_response.Data());
}

std::unique_ptr<FFTFrame> FFTFrame::CreateInterpolatedFrame(
    const FFTFrame& frame1,
    const FFTFrame& frame2,
    double x) {
  std::unique_ptr<FFTFrame> new_frame =
      std::make_unique<FFTFrame>(frame1.FftSize());

  new_frame->InterpolateFrequencyComponents(frame1, frame2, x);

  // In the time-domain, the 2nd half of the response must be zero, to avoid
  // circular convolution aliasing...
  int fft_size = new_frame->FftSize();
  AudioFloatArray buffer(fft_size);
  new_frame->DoInverseFFT(buffer.Data());
  buffer.ZeroRange(fft_size / 2, fft_size);

  // Put back into frequency domain.
  new_frame->DoFFT(buffer.Data());

  return new_frame;
}

void FFTFrame::ScaleFFT(float factor) {
  vector_math::Vsmul(real_data_.Data(), 1, &factor, real_data_.Data(), 1,
                     real_data_.size());
  vector_math::Vsmul(imag_data_.Data(), 1, &factor, imag_data_.Data(), 1,
                     imag_data_.size());
}

void FFTFrame::InterpolateFrequencyComponents(const FFTFrame& frame1,
                                              const FFTFrame& frame2,
                                              double interp) {
  // FIXME : with some work, this method could be optimized

  float* real_p = RealData();
  float* imag_p = ImagData();

  const float* real_p1 = frame1.RealData();
  const float* imag_p1 = frame1.ImagData();
  const float* real_p2 = frame2.RealData();
  const float* imag_p2 = frame2.ImagData();

  fft_size_ = frame1.FftSize();
  log2fft_size_ = frame1.Log2FFTSize();

  double s1base = (1.0 - interp);
  double s2base = interp;

  double phase_accum = 0.0;
  double last_phase1 = 0.0;
  double last_phase2 = 0.0;

  real_p[0] = static_cast<float>(s1base * real_p1[0] + s2base * real_p2[0]);
  imag_p[0] = static_cast<float>(s1base * imag_p1[0] + s2base * imag_p2[0]);

  int n = fft_size_ / 2;

  for (int i = 1; i < n; ++i) {
    std::complex<double> c1(real_p1[i], imag_p1[i]);
    std::complex<double> c2(real_p2[i], imag_p2[i]);

    double mag1 = abs(c1);
    double mag2 = abs(c2);

    // Interpolate magnitudes in decibels
    double mag1db = 20.0 * log10(mag1);
    double mag2db = 20.0 * log10(mag2);

    double s1 = s1base;
    double s2 = s2base;

    double magdbdiff = mag1db - mag2db;

    // Empirical tweak to retain higher-frequency zeroes
    double threshold = (i > 16) ? 5.0 : 2.0;

    if (magdbdiff < -threshold && mag1db < 0.0) {
      s1 = pow(s1, 0.75);
      s2 = 1.0 - s1;
    } else if (magdbdiff > threshold && mag2db < 0.0) {
      s2 = pow(s2, 0.75);
      s1 = 1.0 - s2;
    }

    // Average magnitude by decibels instead of linearly
    double magdb = s1 * mag1db + s2 * mag2db;
    double mag = pow(10.0, 0.05 * magdb);

    // Now, deal with phase
    double phase1 = arg(c1);
    double phase2 = arg(c2);

    double delta_phase1 = phase1 - last_phase1;
    double delta_phase2 = phase2 - last_phase2;
    last_phase1 = phase1;
    last_phase2 = phase2;

    // Unwrap phase deltas
    if (delta_phase1 > kPiDouble)
      delta_phase1 -= kTwoPiDouble;
    if (delta_phase1 < -kPiDouble)
      delta_phase1 += kTwoPiDouble;
    if (delta_phase2 > kPiDouble)
      delta_phase2 -= kTwoPiDouble;
    if (delta_phase2 < -kPiDouble)
      delta_phase2 += kTwoPiDouble;

    // Blend group-delays
    double delta_phase_blend;

    if (delta_phase1 - delta_phase2 > kPiDouble) {
      delta_phase_blend =
          s1 * delta_phase1 + s2 * (kTwoPiDouble + delta_phase2);
    } else if (delta_phase2 - delta_phase1 > kPiDouble) {
      delta_phase_blend =
          s1 * (kTwoPiDouble + delta_phase1) + s2 * delta_phase2;
    } else {
      delta_phase_blend = s1 * delta_phase1 + s2 * delta_phase2;
    }

    phase_accum += delta_phase_blend;

    // Unwrap
    if (phase_accum > kPiDouble)
      phase_accum -= kTwoPiDouble;
    if (phase_accum < -kPiDouble)
      phase_accum += kTwoPiDouble;

    std::complex<double> c = std::polar(mag, phase_accum);

    real_p[i] = static_cast<float>(c.real());
    imag_p[i] = static_cast<float>(c.imag());
  }
}

double FFTFrame::ExtractAverageGroupDelay() {
  float* real_p = RealData();
  float* imag_p = ImagData();

  double ave_sum = 0.0;
  double weight_sum = 0.0;
  double last_phase = 0.0;

  int half_size = FftSize() / 2;

  const double sample_phase_delay =
      kTwoPiDouble / static_cast<double>(FftSize());

  // Calculate weighted average group delay
  for (int i = 0; i < half_size; i++) {
    std::complex<double> c(real_p[i], imag_p[i]);
    double mag = abs(c);
    double phase = arg(c);

    double delta_phase = phase - last_phase;
    last_phase = phase;

    // Unwrap
    if (delta_phase < -kPiDouble)
      delta_phase += kTwoPiDouble;
    if (delta_phase > kPiDouble)
      delta_phase -= kTwoPiDouble;

    ave_sum += mag * delta_phase;
    weight_sum += mag;
  }

  // Note how we invert the phase delta wrt frequency since this is how group
  // delay is defined
  double ave = ave_sum / weight_sum;
  double ave_sample_delay = -ave / sample_phase_delay;

  // Leave 20 sample headroom (for leading edge of impulse)
  if (ave_sample_delay > 20.0)
    ave_sample_delay -= 20.0;

  // Remove average group delay (minus 20 samples for headroom)
  AddConstantGroupDelay(-ave_sample_delay);

  // Remove DC offset
  real_p[0] = 0.0f;

  return ave_sample_delay;
}

void FFTFrame::AddConstantGroupDelay(double sample_frame_delay) {
  int half_size = FftSize() / 2;

  float* real_p = RealData();
  float* imag_p = ImagData();

  const double sample_phase_delay =
      kTwoPiDouble / static_cast<double>(FftSize());

  double phase_adj = -sample_frame_delay * sample_phase_delay;

  // Add constant group delay
  for (int i = 1; i < half_size; i++) {
    std::complex<double> c(real_p[i], imag_p[i]);
    double mag = abs(c);
    double phase = arg(c);

    phase += i * phase_adj;

    std::complex<double> c2 = std::polar(mag, phase);

    real_p[i] = static_cast<float>(c2.real());
    imag_p[i] = static_cast<float>(c2.imag());
  }
}

void FFTFrame::Multiply(const FFTFrame& frame) {
  FFTFrame& frame1 = *this;
  const FFTFrame& frame2 = frame;

  float* real_p1 = frame1.RealData();
  float* imag_p1 = frame1.ImagData();
  const float* real_p2 = frame2.RealData();
  const float* imag_p2 = frame2.ImagData();

  unsigned half_size = FftSize() / 2;
  float real0 = real_p1[0];
  float imag0 = imag_p1[0];

  vector_math::Zvmul(real_p1, imag_p1, real_p2, imag_p2, real_p1, imag_p1,
                     half_size);

  // Multiply the packed DC/nyquist component
  real_p1[0] = real0 * real_p2[0];
  imag_p1[0] = imag0 * imag_p2[0];
}

}  // namespace blink
