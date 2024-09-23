/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webaudio/periodic_wave.h"

#include <algorithm>
#include <memory>

#include "build/build_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_periodic_wave_options.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/oscillator_node.h"
#include "third_party/blink/renderer/platform/audio/fft_frame.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

#if defined(ARCH_CPU_X86_FAMILY)
#include <xmmintrin.h>
#elif defined(CPU_ARM_NEON)
#include <arm_neon.h>
#endif

namespace blink {

namespace {

// The number of bands per octave.  Each octave will have this many entries in
// the wave tables.
constexpr unsigned kNumberOfOctaveBands = 3;

// The max length of a periodic wave. This must be a power of two greater than
// or equal to 2048 and must be supported by the FFT routines.
constexpr unsigned kMaxPeriodicWaveSize = 16384;

constexpr float kCentsPerRange = 1200 / kNumberOfOctaveBands;

}  // namespace

PeriodicWave* PeriodicWave::Create(BaseAudioContext& context,
                                   const Vector<float>& real,
                                   const Vector<float>& imag,
                                   bool disable_normalization,
                                   ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (real.size() != imag.size()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "length of real array (" + String::Number(real.size()) +
            ") and length of imaginary array (" + String::Number(imag.size()) +
            ") must match.");
    return nullptr;
  }

  if (real.size() < 2) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexExceedsMinimumBound("length of the real array",
                                                    real.size(), 2u));
    return nullptr;
  }

  if (imag.size() < 2) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexExceedsMinimumBound("length of the imag array",
                                                    imag.size(), 2u));
    return nullptr;
  }

  PeriodicWave* periodic_wave =
      MakeGarbageCollected<PeriodicWave>(context.sampleRate());
  periodic_wave->impl()->CreateBandLimitedTables(
      real.data(), imag.data(), real.size(), disable_normalization);
  return periodic_wave;
}

PeriodicWave* PeriodicWave::Create(BaseAudioContext* context,
                                   const PeriodicWaveOptions* options,
                                   ExceptionState& exception_state) {
  bool normalize = options->disableNormalization();

  Vector<float> real_coef;
  Vector<float> imag_coef;

  if (options->hasReal()) {
    real_coef = options->real();
    if (options->hasImag()) {
      imag_coef = options->imag();
    } else {
      imag_coef.resize(real_coef.size());
    }
  } else if (options->hasImag()) {
    // `real()` not given, but we have `imag()`.
    imag_coef = options->imag();
    real_coef.resize(imag_coef.size());
  } else {
    // Neither `real()` nor `imag()` given.  Return an object that would
    // generate a sine wave, which means real = [0,0], and imag = [0, 1]
    real_coef.resize(2);
    imag_coef.resize(2);
    imag_coef[1] = 1;
  }

  return Create(*context, real_coef, imag_coef, normalize, exception_state);
}

PeriodicWave* PeriodicWave::CreateSine(float sample_rate) {
  PeriodicWave* periodic_wave = MakeGarbageCollected<PeriodicWave>(sample_rate);
  periodic_wave->impl()->GenerateBasicWaveform(OscillatorHandler::SINE);
  return periodic_wave;
}

PeriodicWave* PeriodicWave::CreateSquare(float sample_rate) {
  PeriodicWave* periodic_wave = MakeGarbageCollected<PeriodicWave>(sample_rate);
  periodic_wave->impl()->GenerateBasicWaveform(OscillatorHandler::SQUARE);
  return periodic_wave;
}

PeriodicWave* PeriodicWave::CreateSawtooth(float sample_rate) {
  PeriodicWave* periodic_wave = MakeGarbageCollected<PeriodicWave>(sample_rate);
  periodic_wave->impl()->GenerateBasicWaveform(OscillatorHandler::SAWTOOTH);
  return periodic_wave;
}

PeriodicWave* PeriodicWave::CreateTriangle(float sample_rate) {
  PeriodicWave* periodic_wave = MakeGarbageCollected<PeriodicWave>(sample_rate);
  periodic_wave->impl()->GenerateBasicWaveform(OscillatorHandler::TRIANGLE);
  return periodic_wave;
}

PeriodicWave::PeriodicWave(float sample_rate)
    : periodic_wave_impl_(MakeGarbageCollected<PeriodicWaveImpl>(sample_rate)) {
}

void PeriodicWave::Trace(Visitor* visitor) const {
  visitor->Trace(periodic_wave_impl_);
  ScriptWrappable::Trace(visitor);
}

PeriodicWaveImpl::PeriodicWaveImpl(float sample_rate)
    : sample_rate_(sample_rate), cents_per_range_(kCentsPerRange) {
  float nyquist = 0.5 * sample_rate_;
  lowest_fundamental_frequency_ = nyquist / MaxNumberOfPartials();
  rate_scale_ = PeriodicWaveSize() / sample_rate_;
  // Compute the number of ranges needed to cover the entire frequency range,
  // assuming kNumberOfOctaveBands per octave.
  number_of_ranges_ = 0.5 + kNumberOfOctaveBands * log2f(PeriodicWaveSize());
}

PeriodicWaveImpl::~PeriodicWaveImpl() {
  external_memory_accounter_.Clear(v8::Isolate::GetCurrent());
}

unsigned PeriodicWaveImpl::PeriodicWaveSize() const {
  // Choose an appropriate wave size for the given sample rate.  This allows us
  // to use shorter FFTs when possible to limit the complexity.  The breakpoints
  // here are somewhat arbitrary, but we want sample rates around 44.1 kHz or so
  // to have a size of 4096 to preserve backward compatibility.
  if (sample_rate_ <= 24000) {
    return 2048;
  }

  if (sample_rate_ <= 88200) {
    return 4096;
  }

  return kMaxPeriodicWaveSize;
}

unsigned PeriodicWaveImpl::MaxNumberOfPartials() const {
  return PeriodicWaveSize() / 2;
}

void PeriodicWaveImpl::WaveDataForFundamentalFrequency(
    float fundamental_frequency,
    float*& lower_wave_data,
    float*& higher_wave_data,
    float& table_interpolation_factor) {
  // Negative frequencies are allowed, in which case we alias to the positive
  // frequency.
  fundamental_frequency = fabsf(fundamental_frequency);

  // Calculate the pitch range.
  float ratio = fundamental_frequency > 0
                    ? fundamental_frequency / lowest_fundamental_frequency_
                    : 0.5;
  float cents_above_lowest_frequency = log2f(ratio) * 1200;

  // Add one to round-up to the next range just in time to truncate partials
  // before aliasing occurs.
  float pitch_range = 1 + cents_above_lowest_frequency / cents_per_range_;

  pitch_range = std::max(pitch_range, 0.0f);
  pitch_range = std::min(pitch_range, static_cast<float>(NumberOfRanges() - 1));

  // The words "lower" and "higher" refer to the table data having the lower and
  // higher numbers of partials.  It's a little confusing since the range index
  // gets larger the more partials we cull out.  So the lower table data will
  // have a larger range index.
  unsigned range_index1 = static_cast<unsigned>(pitch_range);
  unsigned range_index2 =
      range_index1 < NumberOfRanges() - 1 ? range_index1 + 1 : range_index1;

  lower_wave_data = band_limited_tables_[range_index2]->Data();
  higher_wave_data = band_limited_tables_[range_index1]->Data();

  // Ranges from 0 -> 1 to interpolate between lower -> higher.
  table_interpolation_factor = pitch_range - range_index1;
}

#if defined(ARCH_CPU_X86_FAMILY)
void PeriodicWaveImpl::WaveDataForFundamentalFrequency(
    const float fundamental_frequency[4],
    float* lower_wave_data[4],
    float* higher_wave_data[4],
    float table_interpolation_factor[4]) {
  // Negative frequencies are allowed, in which case we alias to the positive
  // frequency.  SSE2 doesn't have an fabs instruction, so just remove the sign
  // bit of the float numbers, effecitvely taking the absolute value.
  const __m128 frequency =
      _mm_and_ps(_mm_loadu_ps(fundamental_frequency),
                 reinterpret_cast<__m128>(_mm_set1_epi32(0x7fffffff)));

  // pos = 0xffffffff if freq > 0; otherwise 0
  const __m128 pos = _mm_cmpgt_ps(frequency, _mm_set1_ps(0));

  // Calculate the pitch range.
  __m128 v_ratio =
      _mm_div_ps(frequency, _mm_set1_ps(lowest_fundamental_frequency_));

  // Set v_ratio to 0 if freq <= 0; otherwise keep the ratio.
  v_ratio = _mm_and_ps(v_ratio, pos);

  // If pos = 0, set value to 0.5 and 0 otherwise.  Or this into v_ratio so that
  // v_ratio is 0.5 if freq <= 0.  Otherwise preserve v_ratio.
  v_ratio = _mm_or_ps(v_ratio, _mm_andnot_ps(pos, _mm_set1_ps(0.5)));

  const float* ratio = reinterpret_cast<float*>(&v_ratio);

  float cents_above_lowest_frequency[4] __attribute__((aligned(16)));

  for (int k = 0; k < 4; ++k) {
    cents_above_lowest_frequency[k] = log2f(ratio[k]) * 1200;
  }

  __m128 v_pitch_range = _mm_add_ps(
      _mm_set1_ps(1.0), _mm_div_ps(_mm_load_ps(cents_above_lowest_frequency),
                                   _mm_set1_ps((cents_per_range_))));
  v_pitch_range = _mm_max_ps(v_pitch_range, _mm_set1_ps(0.0));
  v_pitch_range = _mm_min_ps(v_pitch_range, _mm_set1_ps(NumberOfRanges() - 1));

  const __m128i v_index1 = _mm_cvttps_epi32(v_pitch_range);
  __m128i v_index2 = _mm_add_epi32(v_index1, _mm_set1_epi32(1));

  // SSE2 deosn't have _mm_min_epi32 (but SSE4.2 does).
  //
  // The following ought to work because the small integers for the index and
  // number of ranges should look like tiny denormals that should compare in the
  // same order as integers.  This doesn't work because we have flush-to-zero
  // enabled.
  //
  //   __m128i v_range = _mm_set1_epi32(NumberOfRanges() - 1);
  //  v_index2 = _mm_min_ps(v_index2, v_range);
  //
  // Instead we convert to float, take the min and convert back. No round off
  // because the integers are small.
  v_index2 = _mm_cvttps_epi32(
      _mm_min_ps(_mm_cvtepi32_ps(v_index2), _mm_set1_ps(NumberOfRanges() - 1)));

  const __m128 table_factor =
      _mm_sub_ps(v_pitch_range, _mm_cvtepi32_ps(v_index1));
  _mm_storeu_ps(table_interpolation_factor, table_factor);

  const unsigned* range_index1 = reinterpret_cast<const unsigned*>(&v_index1);
  const unsigned* range_index2 = reinterpret_cast<const unsigned*>(&v_index2);

  for (int k = 0; k < 4; ++k) {
    lower_wave_data[k] = band_limited_tables_[range_index2[k]]->Data();
    higher_wave_data[k] = band_limited_tables_[range_index1[k]]->Data();
  }
}
#elif defined(CPU_ARM_NEON)
void PeriodicWaveImpl::WaveDataForFundamentalFrequency(
    const float fundamental_frequency[4],
    float* lower_wave_data[4],
    float* higher_wave_data[4],
    float table_interpolation_factor[4]) {
  // Negative frequencies are allowed, in which case we alias to the positive
  // frequency.
  float32x4_t frequency = vabsq_f32(vld1q_f32(fundamental_frequency));

  // pos = 0xffffffff if frequency > 0; otherwise 0.
  uint32x4_t pos = vcgtq_f32(frequency, vdupq_n_f32(0));

  // v_ratio = frequency / lowest_fundamental_frequency_.  But NEON
  // doesn't have a division instruction, so multiply by reciprocal.
  // (Aarch64 does, though).
  float32x4_t v_ratio =
      vmulq_f32(frequency, vdupq_n_f32(1 / lowest_fundamental_frequency_));

  // Select v_ratio or 0.5 depending on whether pos is all ones or all
  // zeroes.
  v_ratio = vbslq_f32(pos, v_ratio, vdupq_n_f32(0.5));

  float ratio[4] __attribute__((aligned(16)));
  vst1q_f32(ratio, v_ratio);

  float cents_above_lowest_frequency[4] __attribute__((aligned(16)));

  for (int k = 0; k < 4; ++k) {
    cents_above_lowest_frequency[k] = log2f(ratio[k]) * 1200;
  }

  float32x4_t v_pitch_range = vaddq_f32(
      vdupq_n_f32(1.0), vmulq_f32(vld1q_f32(cents_above_lowest_frequency),
                                  vdupq_n_f32(1 / cents_per_range_)));

  v_pitch_range = vmaxq_f32(v_pitch_range, vdupq_n_f32(0));
  v_pitch_range = vminq_f32(v_pitch_range, vdupq_n_f32(NumberOfRanges() - 1));

  const uint32x4_t v_index1 = vcvtq_u32_f32(v_pitch_range);
  uint32x4_t v_index2 = vaddq_u32(v_index1, vdupq_n_u32(1));
  v_index2 = vminq_u32(v_index2, vdupq_n_u32(NumberOfRanges() - 1));

  uint32_t range_index1[4] __attribute__((aligned(16)));
  uint32_t range_index2[4] __attribute__((aligned(16)));

  vst1q_u32(range_index1, v_index1);
  vst1q_u32(range_index2, v_index2);

  const float32x4_t table_factor =
      vsubq_f32(v_pitch_range, vcvtq_f32_u32(v_index1));
  vst1q_f32(table_interpolation_factor, table_factor);

  for (int k = 0; k < 4; ++k) {
    lower_wave_data[k] = band_limited_tables_[range_index2[k]]->Data();
    higher_wave_data[k] = band_limited_tables_[range_index1[k]]->Data();
  }
}
#else
void PeriodicWaveImpl::WaveDataForFundamentalFrequency(
    const float fundamental_frequency[4],
    float* lower_wave_data[4],
    float* higher_wave_data[4],
    float table_interpolation_factor[4]) {
  for (int k = 0; k < 4; ++k) {
    WaveDataForFundamentalFrequency(fundamental_frequency[k],
                                    lower_wave_data[k], higher_wave_data[k],
                                    table_interpolation_factor[k]);
  }
}
#endif

unsigned PeriodicWaveImpl::NumberOfPartialsForRange(
    unsigned range_index) const {
  // Number of cents below nyquist where we cull partials.
  float cents_to_cull = range_index * cents_per_range_;

  // A value from 0 -> 1 representing what fraction of the partials to keep.
  float culling_scale = pow(2, -cents_to_cull / 1200);

  // The very top range will have all the partials culled.
  unsigned number_of_partials = culling_scale * MaxNumberOfPartials();

  return number_of_partials;
}

// Convert into time-domain wave buffers.  One table is created for each range
// for non-aliasing playback at different playback rates.  Thus, higher ranges
// have more high-frequency partials culled out.
void PeriodicWaveImpl::CreateBandLimitedTables(const float* real_data,
                                               const float* imag_data,
                                               unsigned number_of_components,
                                               bool disable_normalization) {
  // The default scale factor for when normalization is disabled.
  float normalization_scale = 0.5;

  unsigned fft_size = PeriodicWaveSize();
  unsigned half_size = fft_size / 2;
  unsigned i;

  number_of_components = std::min(number_of_components, half_size);

  band_limited_tables_.reserve(NumberOfRanges());

  FFTFrame frame(fft_size);
  for (unsigned range_index = 0; range_index < NumberOfRanges();
       ++range_index) {
    // This FFTFrame is used to cull partials (represented by frequency bins).
    AudioFloatArray& real = frame.RealData();
    DCHECK_GE(real.size(), number_of_components);
    AudioFloatArray& imag = frame.ImagData();
    DCHECK_GE(imag.size(), number_of_components);

    // Copy from loaded frequency data and generate the complex conjugate
    // because of the way the inverse FFT is defined versus the values in the
    // arrays.  Need to scale the data by fftSize to remove the scaling that the
    // inverse IFFT would do.
    float scale = fft_size;
    vector_math::Vsmul(
        real_data, 1, &scale, real.Data(), 1, number_of_components);
    scale = -scale;
    vector_math::Vsmul(
        imag_data, 1, &scale, imag.Data(), 1, number_of_components);

    // Find the starting bin where we should start culling.  We need to clear
    // out the highest frequencies to band-limit the waveform.
    unsigned number_of_partials = NumberOfPartialsForRange(range_index);

    // If fewer components were provided than 1/2 FFT size, then clear the
    // remaining bins.  We also need to cull the aliasing partials for this
    // pitch range.
    for (i = std::min(number_of_components, number_of_partials + 1);
         i < half_size; ++i) {
      real[i] = 0;
      imag[i] = 0;
    }

    // Clear packed-nyquist and any DC-offset.
    real[0] = 0;
    imag[0] = 0;

    // Create the band-limited table.
    unsigned wave_size = PeriodicWaveSize();
    std::unique_ptr<AudioFloatArray> table =
        std::make_unique<AudioFloatArray>(wave_size);
    external_memory_accounter_.Increase(v8::Isolate::GetCurrent(),
                                        wave_size * sizeof(float));
    band_limited_tables_.push_back(std::move(table));

    // Apply an inverse FFT to generate the time-domain table data.
    float* data = band_limited_tables_[range_index]->Data();
    frame.DoInverseFFT(data);

    // For the first range (which has the highest power), calculate its peak
    // value then compute normalization scale.
    if (!disable_normalization) {
      if (!range_index) {
        float max_value;
        vector_math::Vmaxmgv(data, 1, &max_value, fft_size);

        if (max_value) {
          normalization_scale = 1.0f / max_value;
        }
      }
    }

    // Apply normalization scale.
    vector_math::Vsmul(data, 1, &normalization_scale, data, 1, fft_size);
  }
}

void PeriodicWaveImpl::GenerateBasicWaveform(int shape) {
  unsigned fft_size = PeriodicWaveSize();
  unsigned half_size = fft_size / 2;

  AudioFloatArray real(half_size);
  AudioFloatArray imag(half_size);
  float* real_p = real.Data();
  float* imag_p = imag.Data();

  // Clear DC and Nyquist.
  real_p[0] = 0;
  imag_p[0] = 0;

  for (unsigned n = 1; n < half_size; ++n) {
    float pi_factor = 2 / (n * kPiFloat);

    // All waveforms are odd functions with a positive slope at time 0. Hence
    // the coefficients for cos() are always 0.

    // Fourier coefficients according to standard definition:
    // b = 1/pi*integrate(f(x)*sin(n*x), x, -pi, pi)
    //   = 2/pi*integrate(f(x)*sin(n*x), x, 0, pi)
    // since f(x) is an odd function.

    float b;  // Coefficient for sin().

    // Calculate Fourier coefficients depending on the shape. Note that the
    // overall scaling (magnitude) of the waveforms is normalized in
    // createBandLimitedTables().
    switch (shape) {
      case OscillatorHandler::SINE:
        // Standard sine wave function.
        b = (n == 1) ? 1 : 0;
        break;
      case OscillatorHandler::SQUARE:
        // Square-shaped waveform with the first half its maximum value and the
        // second half its minimum value.
        //
        // See http://mathworld.wolfram.com/FourierSeriesSquareWave.html
        //
        // b[n] = 2/n/pi*(1-(-1)^n)
        //      = 4/n/pi for n odd and 0 otherwise.
        //      = 2*(2/(n*pi)) for n odd
        b = (n & 1) ? 2 * pi_factor : 0;
        break;
      case OscillatorHandler::SAWTOOTH:
        // Sawtooth-shaped waveform with the first half ramping from zero to
        // maximum and the second half from minimum to zero.
        //
        // b[n] = -2*(-1)^n/pi/n
        //      = (2/(n*pi))*(-1)^(n+1)
        b = pi_factor * ((n & 1) ? 1 : -1);
        break;
      case OscillatorHandler::TRIANGLE:
        // Triangle-shaped waveform going from 0 at time 0 to 1 at time pi/2 and
        // back to 0 at time pi.
        //
        // See http://mathworld.wolfram.com/FourierSeriesTriangleWave.html
        //
        // b[n] = 8*sin(pi*k/2)/(pi*k)^2
        //      = 8/pi^2/n^2*(-1)^((n-1)/2) for n odd and 0 otherwise
        //      = 2*(2/(n*pi))^2 * (-1)^((n-1)/2)
        if (n & 1) {
          b = 2 * (pi_factor * pi_factor) * ((((n - 1) >> 1) & 1) ? -1 : 1);
        } else {
          b = 0;
        }
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        b = 0;
        break;
    }

    real_p[n] = 0;
    imag_p[n] = b;
  }

  CreateBandLimitedTables(real_p, imag_p, half_size, false);
}

}  // namespace blink
