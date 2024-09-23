/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webaudio/realtime_analyser.h"

#include <limits.h>

#include <algorithm>
#include <bit>
#include <complex>

#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

namespace {

void ApplyWindow(float* p, size_t n) {
  DCHECK(IsMainThread());

  // Blackman window
  double alpha = 0.16;
  double a0 = 0.5 * (1 - alpha);
  double a1 = 0.5;
  double a2 = 0.5 * alpha;

  for (unsigned i = 0; i < n; ++i) {
    double x = static_cast<double>(i) / static_cast<double>(n);
    double window =
        a0 - a1 * cos(kTwoPiDouble * x) + a2 * cos(kTwoPiDouble * 2.0 * x);
    p[i] *= static_cast<float>(window);
  }
}

// Returns x if x is finite (not NaN or infinite), otherwise returns
// default_value
float EnsureFinite(float x, float default_value) {
  return std::isfinite(x) ? x : default_value;
}

}  // namespace

RealtimeAnalyser::RealtimeAnalyser(unsigned render_quantum_frames)
    : input_buffer_(kInputBufferSize),
      down_mix_bus_(AudioBus::Create(1, render_quantum_frames)),
      fft_size_(kDefaultFFTSize),
      magnitude_buffer_(kDefaultFFTSize / 2),
      smoothing_time_constant_(kDefaultSmoothingTimeConstant),
      min_decibels_(kDefaultMinDecibels),
      max_decibels_(kDefaultMaxDecibels) {
  analysis_frame_ = std::make_unique<FFTFrame>(kDefaultFFTSize);
}

bool RealtimeAnalyser::SetFftSize(uint32_t size) {
  DCHECK(IsMainThread());

  // Only allow powers of two within the allowed range.
  if (size > kMaxFFTSize || size < kMinFFTSize || !std::has_single_bit(size)) {
    return false;
  }

  if (fft_size_ != size) {
    analysis_frame_ = std::make_unique<FFTFrame>(size);
    // m_magnitudeBuffer has size = fftSize / 2 because it contains floats
    // reduced from complex values in m_analysisFrame.
    magnitude_buffer_.Allocate(size / 2);
    fft_size_ = size;
  }

  return true;
}

void RealtimeAnalyser::GetFloatFrequencyData(DOMFloat32Array* destination_array,
                                             double current_time) {
  DCHECK(IsMainThread());
  DCHECK(destination_array);

  if (current_time <= last_analysis_time_) {
    ConvertFloatToDb(destination_array);
    return;
  }

  // Time has advanced since the last call; update the FFT data.
  last_analysis_time_ = current_time;
  DoFFTAnalysis();

  ConvertFloatToDb(destination_array);
}

void RealtimeAnalyser::GetByteFrequencyData(DOMUint8Array* destination_array,
                                            double current_time) {
  DCHECK(IsMainThread());
  DCHECK(destination_array);

  if (current_time <= last_analysis_time_) {
    // FIXME: Is it worth caching the data so we don't have to do the conversion
    // every time?  Perhaps not, since we expect many calls in the same
    // rendering quantum.
    ConvertToByteData(destination_array);
    return;
  }

  // Time has advanced since the last call; update the FFT data.
  last_analysis_time_ = current_time;
  DoFFTAnalysis();

  ConvertToByteData(destination_array);
}

void RealtimeAnalyser::GetFloatTimeDomainData(
    DOMFloat32Array* destination_array) {
  DCHECK(IsMainThread());
  DCHECK(destination_array);

  unsigned fft_size = FftSize();
  size_t len =
      std::min(static_cast<size_t>(fft_size), destination_array->length());
  if (len > 0) {
    DCHECK_EQ(input_buffer_.size(), kInputBufferSize);
    DCHECK_GT(input_buffer_.size(), fft_size);

    float* input_buffer = input_buffer_.Data();
    float* destination = destination_array->Data();

    unsigned write_index = GetWriteIndex();

    for (unsigned i = 0; i < len; ++i) {
      // Buffer access is protected due to modulo operation.
      float value =
          input_buffer[(i + write_index - fft_size + kInputBufferSize) %
                       kInputBufferSize];

      destination[i] = value;
    }
  }
}

void RealtimeAnalyser::GetByteTimeDomainData(DOMUint8Array* destination_array) {
  DCHECK(IsMainThread());
  DCHECK(destination_array);

  unsigned fft_size = FftSize();
  size_t len =
      std::min(static_cast<size_t>(fft_size), destination_array->length());
  if (len > 0) {
    DCHECK_EQ(input_buffer_.size(), kInputBufferSize);
    DCHECK_GT(input_buffer_.size(), fft_size);

    float* input_buffer = input_buffer_.Data();
    unsigned char* destination = destination_array->Data();

    unsigned write_index = GetWriteIndex();

    for (unsigned i = 0; i < len; ++i) {
      // Buffer access is protected due to modulo operation.
      float value =
          input_buffer[(i + write_index - fft_size + kInputBufferSize) %
                       kInputBufferSize];

      // Scale from nominal -1 -> +1 to unsigned byte.
      double scaled_value = 128 * (value + 1);

      // Clip to valid range.
      destination[i] =
          static_cast<unsigned char>(ClampTo(scaled_value, 0, UCHAR_MAX));
    }
  }
}

void RealtimeAnalyser::WriteInput(AudioBus* bus, uint32_t frames_to_process) {
  DCHECK(bus);
  DCHECK_GT(bus->NumberOfChannels(), 0u);
  DCHECK_GE(bus->Channel(0)->length(), frames_to_process);

  unsigned write_index = GetWriteIndex();
  // FIXME : allow to work with non-FFTSize divisible chunking
  DCHECK_LT(write_index, input_buffer_.size());
  DCHECK_LE(write_index + frames_to_process, input_buffer_.size());

  // Perform real-time analysis
  float* dest = input_buffer_.Data() + write_index;

  // Clear the bus and downmix the input according to the down mixing rules.
  // Then save the result in the m_inputBuffer at the appropriate place.
  down_mix_bus_->Zero();
  down_mix_bus_->SumFrom(*bus);
  memcpy(dest, down_mix_bus_->Channel(0)->Data(),
         frames_to_process * sizeof(*dest));

  write_index += frames_to_process;
  if (write_index >= kInputBufferSize) {
    write_index = 0;
  }
  SetWriteIndex(write_index);
}

void RealtimeAnalyser::DoFFTAnalysis() {
  DCHECK(IsMainThread());

  // Unroll the input buffer into a temporary buffer, where we'll apply an
  // analysis window followed by an FFT.
  uint32_t fft_size = FftSize();

  AudioFloatArray temporary_buffer(fft_size);
  float* input_buffer = input_buffer_.Data();
  float* temp_p = temporary_buffer.Data();

  // Take the previous fftSize values from the input buffer and copy into the
  // temporary buffer.
  unsigned write_index = GetWriteIndex();
  if (write_index < fft_size) {
    memcpy(temp_p, input_buffer + write_index - fft_size + kInputBufferSize,
           sizeof(*temp_p) * (fft_size - write_index));
    memcpy(temp_p + fft_size - write_index, input_buffer,
           sizeof(*temp_p) * write_index);
  } else {
    memcpy(temp_p, input_buffer + write_index - fft_size,
           sizeof(*temp_p) * fft_size);
  }

  // Window the input samples.
  ApplyWindow(temp_p, fft_size);

  // Do the analysis.
  analysis_frame_->DoFFT(temp_p);

  const AudioFloatArray& real = analysis_frame_->RealData();
  AudioFloatArray& imag = analysis_frame_->ImagData();

  // Blow away the packed nyquist component.
  imag[0] = 0;

  // Normalize so than an input sine wave at 0dBfs registers as 0dBfs (undo FFT
  // scaling factor).
  const double magnitude_scale = 1.0 / fft_size;

  // A value of 0 does no averaging with the previous result.  Larger values
  // produce slower, but smoother changes.
  const double k = ClampTo(smoothing_time_constant_, 0.0, 1.0);

  // Convert the analysis data from complex to magnitude and average with the
  // previous result.
  float* destination = MagnitudeBuffer().Data();
  size_t n = MagnitudeBuffer().size();
  DCHECK_GE(real.size(), n);
  const float* real_p_data = real.Data();
  DCHECK_GE(imag.size(), n);
  const float* imag_p_data = imag.Data();
  for (size_t i = 0; i < n; ++i) {
    std::complex<double> c(real_p_data[i], imag_p_data[i]);
    double scalar_magnitude = abs(c) * magnitude_scale;
    destination[i] = EnsureFinite(
        static_cast<float>(k * destination[i] + (1 - k) * scalar_magnitude), 0);
  }
}

void RealtimeAnalyser::ConvertToByteData(DOMUint8Array* destination_array) {
  // Convert from linear magnitude to unsigned-byte decibels.
  size_t source_length = MagnitudeBuffer().size();
  size_t len = std::min(source_length, destination_array->length());
  if (len > 0) {
    const double range_scale_factor = max_decibels_ == min_decibels_
                                          ? 1
                                          : 1 / (max_decibels_ - min_decibels_);
    const double min_decibels = min_decibels_;

    const float* source = MagnitudeBuffer().Data();
    unsigned char* destination = destination_array->Data();

    for (unsigned i = 0; i < len; ++i) {
      float linear_value = source[i];
      double db_mag = audio_utilities::LinearToDecibels(linear_value);

      // The range m_minDecibels to m_maxDecibels will be scaled to byte values
      // from 0 to UCHAR_MAX.
      double scaled_value =
          UCHAR_MAX * (db_mag - min_decibels) * range_scale_factor;

      // Clip to valid range.
      destination[i] =
          static_cast<unsigned char>(ClampTo(scaled_value, 0, UCHAR_MAX));
    }
  }
}

void RealtimeAnalyser::ConvertFloatToDb(DOMFloat32Array* destination_array) {
  // Convert from linear magnitude to floating-point decibels.
  size_t source_length = MagnitudeBuffer().size();
  size_t len = std::min(source_length, destination_array->length());
  if (len > 0) {
    const float* source = MagnitudeBuffer().Data();
    float* destination = destination_array->Data();

    for (unsigned i = 0; i < len; ++i) {
      float linear_value = source[i];
      double db_mag = audio_utilities::LinearToDecibels(linear_value);
      destination[i] = static_cast<float>(db_mag);
    }
  }
}

}  // namespace blink
