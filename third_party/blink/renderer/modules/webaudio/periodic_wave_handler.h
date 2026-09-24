// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_PERIODIC_WAVE_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_PERIODIC_WAVE_HANDLER_H_

#include <array>
#include <memory>

#include "base/containers/span.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/audio/audio_array.h"
#include "third_party/blink/renderer/platform/bindings/v8_external_memory_accounter.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// PeriodicWaveHandler is not scriptable and thus can never have back references
// to an AudioNode. This allows it to be held strongly from the audio thread
// which avoids converting weak to strong references which is prone to
// GC interference.
class MODULES_EXPORT PeriodicWaveHandler final
    : public GarbageCollected<PeriodicWaveHandler> {
 public:
  explicit PeriodicWaveHandler(float sample_rate);
  ~PeriodicWaveHandler();

  void Trace(Visitor*) const {}

  // Returns pointers to the lower and higher wave data for the pitch range
  // containing the given fundamental frequency. These two tables are in
  // adjacent "pitch" ranges where the higher table will have the maximum number
  // of partials which won't alias when played back at this fundamental
  // frequency. The lower wave is the next range containing fewer partials than
  // the higher wave.  Interpolation between these two tables can be made
  // according to tableInterpolationFactor.
  // Where values from 0 -> 1 interpolate between lower -> higher.
  void WaveDataForFundamentalFrequency(
      float,
      base::span<const float>& lower_wave_data,
      base::span<const float>& higher_wave_data,
      float& table_interpolation_factor);

  // Like the above, except we compute accept 4 frequencies at a time and return
  // 4 lower/higher wave data tables and the 4 corresponding table interpolation
  // factors.  Intended for use with the OscillatorNode for faster a-rate
  // processing.
  void WaveDataForFundamentalFrequency(
      const std::array<float, 4> fundamental_frequency,
      std::array<base::span<const float>, 4>& lower_wave_data,
      std::array<base::span<const float>, 4>& higher_wave_data,
      std::array<float, 4>& table_interpolation_factor);

  // Returns the scalar multiplier to the oscillator frequency to calculate wave
  // buffer phase increment.
  float RateScale() const { return rate_scale_; }

  // The size of the FFT to use based on the sampling rate.
  unsigned PeriodicWaveSize() const;

  // The number of ranges needed for the given sampling rate and FFT size.
  unsigned NumberOfRanges() const { return number_of_ranges_; }

 private:
  // Generates basic waveforms (sine, square, etc.) and creates band-limited
  // tables. Returns false if allocation fails.
  bool GenerateBasicWaveform(int);

  float sample_rate_;
  unsigned number_of_ranges_;
  float cents_per_range_;

  // The lowest frequency (in Hertz) where playback will include all of the
  // partials.  Playing back lower than this frequency will gradually lose more
  // high-frequency information.  This frequency is quite low (~10Hz @ 44.1KHz)
  float lowest_fundamental_frequency_;

  float rate_scale_;

  // Maximum possible number of partials (before culling).
  unsigned MaxNumberOfPartials() const;

  unsigned NumberOfPartialsForRange(unsigned range_index) const;

  // Converts Fourier coefficients into time-domain wave buffers. One table is
  // created for each pitch range to prevent aliasing during playback at
  // different rates. Higher ranges have more high-frequency partials culled
  // out. Returns false if allocation fails.
  bool CreateBandLimitedTables(base::span<const float> real,
                               base::span<const float> imag,
                               bool disable_normalization);

  Vector<std::unique_ptr<AudioFloatArray>> band_limited_tables_;

  friend class PeriodicWave;

  V8ExternalMemoryAccounter external_memory_accounter_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_PERIODIC_WAVE_HANDLER_H_
