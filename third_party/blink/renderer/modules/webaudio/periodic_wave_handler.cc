// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/periodic_wave_handler.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <numbers>

#include "base/bit_cast.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/modules/webaudio/oscillator_handler.h"
#include "third_party/blink/renderer/platform/audio/fft_frame.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "v8/include/v8-isolate.h"

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

constexpr float kCentsPerOctave = 1200;
constexpr float kCentsPerRange = kCentsPerOctave / kNumberOfOctaveBands;

// Choose an appropriate wave size for the given sample rate. This allows us
// to use shorter FFTs when possible to limit the complexity. The breakpoints
// here are somewhat arbitrary, but we want sample rates around 44.1 kHz or so
// to have a size of 4096 to preserve backward compatibility.
constexpr unsigned GetPeriodicWaveSize(float sample_rate) {
  if (sample_rate <= 24000) {
    return 2048;
  }
  if (sample_rate <= 88200) {
    return 4096;
  }
  return kMaxPeriodicWaveSize;
}

}  // namespace

PeriodicWaveHandler* PeriodicWaveHandler::Create(float sample_rate,
                                                 base::span<const float> real,
                                                 base::span<const float> imag,
                                                 bool disable_normalization) {
  auto* handler = MakeGarbageCollected<PeriodicWaveHandler>(sample_rate);
  if (!handler->CreateBandLimitedTables(real, imag, disable_normalization)) {
    return nullptr;
  }
  return handler;
}

PeriodicWaveHandler* PeriodicWaveHandler::CreateBasicWaveform(float sample_rate,
                                                              int shape) {
  auto* handler = MakeGarbageCollected<PeriodicWaveHandler>(sample_rate);
  if (!handler->GenerateBasicWaveform(shape)) {
    return nullptr;
  }
  return handler;
}

PeriodicWaveHandler::PeriodicWaveHandler(float sample_rate)
    : periodic_wave_size_(GetPeriodicWaveSize(sample_rate)),
      // Compute the number of ranges needed to cover the entire frequency
      // range, assuming kNumberOfOctaveBands per octave.
      number_of_ranges_(static_cast<unsigned>(
          std::lround(kNumberOfOctaveBands * log2f(periodic_wave_size_)))),
      lowest_fundamental_frequency_(sample_rate / periodic_wave_size_),
      rate_scale_(periodic_wave_size_ / sample_rate) {}

PeriodicWaveHandler::~PeriodicWaveHandler() {
  external_memory_accounter_.Clear(v8::Isolate::GetCurrent());
}

void PeriodicWaveHandler::WaveDataForFundamentalFrequency(
    float fundamental_frequency,
    base::span<const float>& lower_wave_data,
    base::span<const float>& higher_wave_data,
    float& table_interpolation_factor) const {
  // Negative frequencies are allowed, in which case we alias to the positive
  // frequency.
  fundamental_frequency = fabsf(fundamental_frequency);

  // Calculate the pitch range.
  const float ratio =
      fundamental_frequency > 0
          ? fundamental_frequency / lowest_fundamental_frequency_
          : 0.5f;
  const float cents_above_lowest_frequency = log2f(ratio) * kCentsPerOctave;

  // Add one to round-up to the next range just in time to truncate partials
  // before aliasing occurs.
  const float pitch_range =
      std::clamp(1 + cents_above_lowest_frequency / kCentsPerRange, 0.0f,
                 static_cast<float>(number_of_ranges_ - 1));

  // The words "lower" and "higher" refer to the table data having the lower and
  // higher numbers of partials.  It's a little confusing since the range index
  // gets larger the more partials we cull out.  So the lower table data will
  // have a larger range index.
  const unsigned range_index1 = static_cast<unsigned>(pitch_range);
  const unsigned range_index2 =
      range_index1 < number_of_ranges_ - 1 ? range_index1 + 1 : range_index1;

  lower_wave_data = band_limited_tables_[range_index2]->as_span();
  higher_wave_data = band_limited_tables_[range_index1]->as_span();

  // Ranges from 0 -> 1 to interpolate between lower -> higher.
  table_interpolation_factor = pitch_range - range_index1;
}

#if defined(ARCH_CPU_X86_FAMILY)
void PeriodicWaveHandler::WaveDataForFundamentalFrequency(
    const std::array<float, 4>& fundamental_frequency,
    std::array<base::span<const float>, 4>& lower_wave_data,
    std::array<base::span<const float>, 4>& higher_wave_data,
    std::array<float, 4>& table_interpolation_factor) const {
  // Negative frequencies are allowed, in which case we alias to the positive
  // frequency.  SSE2 doesn't have an fabs instruction, so just remove the sign
  // bit of the float numbers, effectively taking the absolute value.
  const __m128 frequency =
      _mm_and_ps(_mm_loadu_ps(fundamental_frequency.data()),
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

  const std::array<float, 4> ratio =
      base::bit_cast<std::array<float, 4>>(v_ratio);

  alignas(16) std::array<float, 4> cents_above_lowest_frequency;

  for (int k = 0; k < 4; ++k) {
    cents_above_lowest_frequency[k] = log2f(ratio[k]) * kCentsPerOctave;
  }

  __m128 v_pitch_range =
      _mm_add_ps(_mm_set1_ps(1.0),
                 _mm_div_ps(_mm_load_ps(cents_above_lowest_frequency.data()),
                            _mm_set1_ps(kCentsPerRange)));
  v_pitch_range = _mm_max_ps(v_pitch_range, _mm_set1_ps(0.0));
  v_pitch_range = _mm_min_ps(v_pitch_range, _mm_set1_ps(number_of_ranges_ - 1));

  const __m128i v_index1 = _mm_cvttps_epi32(v_pitch_range);
  __m128i v_index2 = _mm_add_epi32(v_index1, _mm_set1_epi32(1));

  // SSE2 deosn't have _mm_min_epi32 (but SSE4.2 does).
  //
  // The following ought to work because the small integers for the index and
  // number of ranges should look like tiny denormals that should compare in the
  // same order as integers.  This doesn't work because we have flush-to-zero
  // enabled.
  //
  //   __m128i v_range = _mm_set1_epi32(number_of_ranges_ - 1);
  //  v_index2 = _mm_min_ps(v_index2, v_range);
  //
  // Instead we convert to float, take the min and convert back. No round off
  // because the integers are small.
  v_index2 = _mm_cvttps_epi32(_mm_min_ps(_mm_cvtepi32_ps(v_index2),
                                         _mm_set1_ps(number_of_ranges_ - 1)));

  const __m128 table_factor =
      _mm_sub_ps(v_pitch_range, _mm_cvtepi32_ps(v_index1));
  table_interpolation_factor =
      base::bit_cast<std::array<float, 4>>(table_factor);

  const std::array<unsigned, 4> range_index1 =
      base::bit_cast<std::array<unsigned, 4>>(v_index1);
  const std::array<unsigned, 4> range_index2 =
      base::bit_cast<std::array<unsigned, 4>>(v_index2);

  for (unsigned k = 0; k < 4; ++k) {
    lower_wave_data[k] = band_limited_tables_[range_index2[k]]->as_span();
    higher_wave_data[k] = band_limited_tables_[range_index1[k]]->as_span();
  }
}
#elif defined(CPU_ARM_NEON)
void PeriodicWaveHandler::WaveDataForFundamentalFrequency(
    const std::array<float, 4>& fundamental_frequency,
    std::array<base::span<const float>, 4>& lower_wave_data,
    std::array<base::span<const float>, 4>& higher_wave_data,
    std::array<float, 4>& table_interpolation_factor) const {
  // Negative frequencies are allowed, in which case we alias to the positive
  // frequency.
  const float32x4_t frequency =
      vabsq_f32(vld1q_f32(fundamental_frequency.data()));

  // pos = 0xffffffff if frequency > 0; otherwise 0.
  const uint32x4_t pos = vcgtq_f32(frequency, vdupq_n_f32(0));

  // v_ratio = frequency / lowest_fundamental_frequency_.  But NEON
  // doesn't have a division instruction, so multiply by reciprocal.
  // (Aarch64 does, though).
  float32x4_t v_ratio =
      vmulq_f32(frequency, vdupq_n_f32(1 / lowest_fundamental_frequency_));

  // Select v_ratio or 0.5 depending on whether pos is all ones or all
  // zeroes.
  v_ratio = vbslq_f32(pos, v_ratio, vdupq_n_f32(0.5));

  const std::array<float, 4> ratio =
      base::bit_cast<std::array<float, 4>>(v_ratio);

  alignas(16) std::array<float, 4> cents_above_lowest_frequency;

  for (int k = 0; k < 4; ++k) {
    cents_above_lowest_frequency[k] = log2f(ratio[k]) * kCentsPerOctave;
  }

  float32x4_t v_pitch_range =
      vaddq_f32(vdupq_n_f32(1.0),
                vmulq_f32(vld1q_f32(cents_above_lowest_frequency.data()),
                          vdupq_n_f32(1 / kCentsPerRange)));

  v_pitch_range = vmaxq_f32(v_pitch_range, vdupq_n_f32(0));
  v_pitch_range = vminq_f32(v_pitch_range, vdupq_n_f32(number_of_ranges_ - 1));

  const uint32x4_t v_index1 = vcvtq_u32_f32(v_pitch_range);
  uint32x4_t v_index2 = vaddq_u32(v_index1, vdupq_n_u32(1));
  v_index2 = vminq_u32(v_index2, vdupq_n_u32(number_of_ranges_ - 1));

  const std::array<uint32_t, 4> range_index1 =
      base::bit_cast<std::array<uint32_t, 4>>(v_index1);
  const std::array<uint32_t, 4> range_index2 =
      base::bit_cast<std::array<uint32_t, 4>>(v_index2);

  const float32x4_t table_factor =
      vsubq_f32(v_pitch_range, vcvtq_f32_u32(v_index1));
  table_interpolation_factor =
      base::bit_cast<std::array<float, 4>>(table_factor);

  for (int k = 0; k < 4; ++k) {
    lower_wave_data[k] = band_limited_tables_[range_index2[k]]->as_span();
    higher_wave_data[k] = band_limited_tables_[range_index1[k]]->as_span();
  }
}
#else
void PeriodicWaveHandler::WaveDataForFundamentalFrequency(
    const std::array<float, 4>& fundamental_frequency,
    std::array<base::span<const float>, 4>& lower_wave_data,
    std::array<base::span<const float>, 4>& higher_wave_data,
    std::array<float, 4>& table_interpolation_factor) const {
  for (int k = 0; k < 4; ++k) {
    WaveDataForFundamentalFrequency(fundamental_frequency[k],
                                    lower_wave_data[k], higher_wave_data[k],
                                    table_interpolation_factor[k]);
  }
}
#endif

// Convert into time-domain wave buffers.  One table is created for each range
// for non-aliasing playback at different playback rates.  Thus, higher ranges
// have more high-frequency partials culled out.
bool PeriodicWaveHandler::CreateBandLimitedTables(
    base::span<const float> real_data,
    base::span<const float> imag_data,
    bool disable_normalization) {
  // The default scale factor for when normalization is disabled.
  float normalization_scale = 0.5f;

  const unsigned fft_size = periodic_wave_size_;
  const unsigned half_size = fft_size / 2;
  // Maximum possible number of partials (before culling).
  const unsigned max_number_of_partials = half_size;

  CHECK_EQ(real_data.size(), imag_data.size());
  const unsigned number_of_components =
      std::min(static_cast<unsigned>(real_data.size()), half_size);

  band_limited_tables_.reserve(number_of_ranges_);

  FFTFrame frame(fft_size);
  for (unsigned range_index = 0; range_index < number_of_ranges_;
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
    const float scale = fft_size;
    vector_math::Vsmul(real_data, scale, real.as_span(), number_of_components);
    vector_math::Vsmul(imag_data, -scale, imag.as_span(), number_of_components);

    // Find the starting bin where we should start culling.  We need to clear
    // out the highest frequencies to band-limit the waveform.

    // Number of cents below nyquist where we cull partials.
    const float cents_to_cull = range_index * kCentsPerRange;

    // A value from 0 -> 1 representing what fraction of the partials to keep.
    const float culling_scale = pow(2, -cents_to_cull / kCentsPerOctave);

    // The very top range will have all the partials culled.
    const unsigned number_of_partials = culling_scale * max_number_of_partials;

    // If fewer components were provided than 1/2 FFT size, then clear the
    // remaining bins.  We also need to cull the aliasing partials for this
    // pitch range.
    for (unsigned i = std::min(number_of_components, number_of_partials + 1);
         i < half_size; ++i) {
      real[i] = 0;
      imag[i] = 0;
    }

    // Clear packed-nyquist and any DC-offset.
    real[0] = 0;
    imag[0] = 0;

    // Create the band-limited table.
    auto table = std::make_unique<AudioFloatArray>();
    if (!table->TryAllocate(periodic_wave_size_)) {
      return false;
    }
    external_memory_accounter_.Increase(v8::Isolate::GetCurrent(),
                                        periodic_wave_size_ * sizeof(float));
    band_limited_tables_.push_back(std::move(table));

    // Apply an inverse FFT to generate the time-domain table data.
    base::span<float> data_span = band_limited_tables_[range_index]->as_span();
    frame.DoInverseFFT(data_span);

    // For the first range (which has the highest power), calculate its peak
    // value then compute normalization scale.
    if (!disable_normalization && !range_index) {
      const float max_value = vector_math::Vmaxmgv(data_span);
      if (max_value) {
        normalization_scale = 1.0f / max_value;
      }
    }

    // Apply normalization scale.
    vector_math::Vsmul(data_span, normalization_scale, data_span, fft_size);
  }
  return true;
}

bool PeriodicWaveHandler::GenerateBasicWaveform(int shape) {
  const unsigned fft_size = periodic_wave_size_;
  const unsigned half_size = fft_size / 2;

  AudioFloatArray real(half_size);
  AudioFloatArray imag(half_size);

  // Clear DC and Nyquist.
  real[0] = 0;
  imag[0] = 0;

  for (unsigned n = 1; n < half_size; ++n) {
    const float pi_factor = 2 / (n * std::numbers::pi_v<float>);

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
        NOTREACHED();
    }

    real[n] = 0;
    imag[n] = b;
  }

  return CreateBandLimitedTables(real.as_span(), imag.as_span(), false);
}

}  // namespace blink
