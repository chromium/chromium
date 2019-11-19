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

#include <algorithm>
#include <memory>

#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/oscillator_node.h"
#include "third_party/blink/renderer/modules/webaudio/periodic_wave.h"
#include "third_party/blink/renderer/modules/webaudio/periodic_wave_options.h"
#include "third_party/blink/renderer/platform/audio/fft_frame.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

// The number of bands per octave.  Each octave will have this many entries in
// the wave tables.
const unsigned kNumberOfOctaveBands = 3;

// The max length of a periodic wave. This must be a power of two greater than
// or equal to 2048 and must be supported by the FFT routines.
const unsigned kMaxPeriodicWaveSize = 16384;

const float kCentsPerRange = 1200 / kNumberOfOctaveBands;

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

  PeriodicWave* periodic_wave =
      MakeGarbageCollected<PeriodicWave>(context.sampleRate());
  periodic_wave->CreateBandLimitedTables(real.data(), imag.data(), real.size(),
                                         disable_normalization);
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
    if (options->hasImag())
      imag_coef = options->imag();
    else
      imag_coef.resize(real_coef.size());
  } else if (options->hasImag()) {
    // |real| not given, but we have |imag|.
    imag_coef = options->imag();
    real_coef.resize(imag_coef.size());
  } else {
    // Neither |real| nor |imag| given.  Return an object that would
    // generate a sine wave, which means real = [0,0], and imag = [0, 1]
    real_coef.resize(2);
    imag_coef.resize(2);
    imag_coef[1] = 1;
  }

  return Create(*context, real_coef, imag_coef, normalize, exception_state);
}

PeriodicWave* PeriodicWave::CreateSine(float sample_rate) {
  PeriodicWave* periodic_wave = MakeGarbageCollected<PeriodicWave>(sample_rate);
  periodic_wave->GenerateBasicWaveform(OscillatorHandler::SINE);
  return periodic_wave;
}

PeriodicWave* PeriodicWave::CreateSquare(float sample_rate) {
  PeriodicWave* periodic_wave = MakeGarbageCollected<PeriodicWave>(sample_rate);
  periodic_wave->GenerateBasicWaveform(OscillatorHandler::SQUARE);
  return periodic_wave;
}

PeriodicWave* PeriodicWave::CreateSawtooth(float sample_rate) {
  PeriodicWave* periodic_wave = MakeGarbageCollected<PeriodicWave>(sample_rate);
  periodic_wave->GenerateBasicWaveform(OscillatorHandler::SAWTOOTH);
  return periodic_wave;
}

PeriodicWave* PeriodicWave::CreateTriangle(float sample_rate) {
  PeriodicWave* periodic_wave = MakeGarbageCollected<PeriodicWave>(sample_rate);
  periodic_wave->GenerateBasicWaveform(OscillatorHandler::TRIANGLE);
  return periodic_wave;
}

PeriodicWave::PeriodicWave(float sample_rate)
    : v8_external_memory_(0),
      sample_rate_(sample_rate),
      cents_per_range_(kCentsPerRange) {
  float nyquist = 0.5 * sample_rate_;
  lowest_fundamental_frequency_ = nyquist / MaxNumberOfPartials();
  rate_scale_ = PeriodicWaveSize() / sample_rate_;
  // Compute the number of ranges needed to cover the entire frequency range,
  // assuming kNumberOfOctaveBands per octave.
  number_of_ranges_ = 0.5 + kNumberOfOctaveBands * log2f(PeriodicWaveSize());
}

PeriodicWave::~PeriodicWave() {
  AdjustV8ExternalMemory(-static_cast<int64_t>(v8_external_memory_));
}

unsigned PeriodicWave::PeriodicWaveSize() const {
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

unsigned PeriodicWave::MaxNumberOfPartials() const {
  return PeriodicWaveSize() / 2;
}

void PeriodicWave::WaveDataForFundamentalFrequency(
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

unsigned PeriodicWave::NumberOfPartialsForRange(unsigned range_index) const {
  // Number of cents below nyquist where we cull partials.
  float cents_to_cull = range_index * cents_per_range_;

  // A value from 0 -> 1 representing what fraction of the partials to keep.
  float culling_scale = pow(2, -cents_to_cull / 1200);

  // The very top range will have all the partials culled.
  unsigned number_of_partials = culling_scale * MaxNumberOfPartials();

  return number_of_partials;
}

// Tell V8 about the memory we're using so it can properly schedule garbage
// collects.
void PeriodicWave::AdjustV8ExternalMemory(int64_t delta) {
  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(delta);
  v8_external_memory_ += delta;
}

// Convert into time-domain wave buffers.  One table is created for each range
// for non-aliasing playback at different playback rates.  Thus, higher ranges
// have more high-frequency partials culled out.
void PeriodicWave::CreateBandLimitedTables(const float* real_data,
                                           const float* imag_data,
                                           unsigned number_of_components,
                                           bool disable_normalization) {
  // TODO(rtoy): Figure out why this needs to be 0.5 when normalization is
  // disabled.
  float normalization_scale = 0.5;

  unsigned fft_size = PeriodicWaveSize();
  unsigned half_size = fft_size / 2;
  unsigned i;

  number_of_components = std::min(number_of_components, half_size);

  band_limited_tables_.ReserveCapacity(NumberOfRanges());

  FFTFrame frame(fft_size);
  for (unsigned range_index = 0; range_index < NumberOfRanges();
       ++range_index) {
    // This FFTFrame is used to cull partials (represented by frequency bins).
    float* real_p = frame.RealData();
    float* imag_p = frame.ImagData();

    // Copy from loaded frequency data and generate the complex conjugate
    // because of the way the inverse FFT is defined versus the values in the
    // arrays.  Need to scale the data by fftSize to remove the scaling that the
    // inverse IFFT would do.
    float scale = fft_size;
    vector_math::Vsmul(real_data, 1, &scale, real_p, 1, number_of_components);
    scale = -scale;
    vector_math::Vsmul(imag_data, 1, &scale, imag_p, 1, number_of_components);

    // Find the starting bin where we should start culling.  We need to clear
    // out the highest frequencies to band-limit the waveform.
    unsigned number_of_partials = NumberOfPartialsForRange(range_index);

    // If fewer components were provided than 1/2 FFT size, then clear the
    // remaining bins.  We also need to cull the aliasing partials for this
    // pitch range.
    for (i = std::min(number_of_components, number_of_partials + 1);
         i < half_size; ++i) {
      real_p[i] = 0;
      imag_p[i] = 0;
    }

    // Clear packed-nyquist and any DC-offset.
    real_p[0] = 0;
    imag_p[0] = 0;

    // Create the band-limited table.
    unsigned wave_size = PeriodicWaveSize();
    std::unique_ptr<AudioFloatArray> table =
        std::make_unique<AudioFloatArray>(wave_size);
    AdjustV8ExternalMemory(wave_size * sizeof(float));
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

        if (max_value)
          normalization_scale = 1.0f / max_value;
      }
    }

    // Apply normalization scale.
    vector_math::Vsmul(data, 1, &normalization_scale, data, 1, fft_size);
  }
}

void PeriodicWave::GenerateBasicWaveform(int shape) {
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
        NOTREACHED();
        b = 0;
        break;
    }

    real_p[n] = 0;
    imag_p[n] = b;
  }

  CreateBandLimitedTables(real_p, imag_p, half_size, false);
}

}  // namespace blink
