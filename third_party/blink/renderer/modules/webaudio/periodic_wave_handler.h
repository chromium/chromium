// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_PERIODIC_WAVE_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_PERIODIC_WAVE_HANDLER_H_

#include <array>
#include <memory>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/audio/audio_array.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// PeriodicWaveHandler generates and stores band-limited time-domain wave tables
// from Fourier coefficients or basic waveform shapes, and provides wave table
// lookup for OscillatorHandler at a given fundamental frequency.
//
// PeriodicWaveHandler is not scriptable and thus can never have back references
// to an AudioNode. This allows it to be ref-counted and held directly from the
// audio thread without GC interference.
class MODULES_EXPORT PeriodicWaveHandler final
    : public ThreadSafeRefCounted<PeriodicWaveHandler> {
 public:
  static scoped_refptr<PeriodicWaveHandler> Create(float sample_rate,
                                                   base::span<const float> real,
                                                   base::span<const float> imag,
                                                   bool disable_normalization);

  static scoped_refptr<PeriodicWaveHandler> CreateBasicWaveform(
      float sample_rate,
      int shape);

  // Returns pointers to the lower and higher wave data for the pitch range
  // containing the given fundamental frequency. These two tables are in
  // adjacent "pitch" ranges where the higher table will have the maximum number
  // of partials which won't alias when played back at this fundamental
  // frequency. The lower wave is the next range containing fewer partials than
  // the higher wave.  Interpolation between these two tables can be made
  // according to tableInterpolationFactor.
  // Where values from 0 -> 1 interpolate between lower -> higher.
  void WaveDataForFundamentalFrequency(
      float fundamental_frequency,
      base::span<const float>& lower_wave_data,
      base::span<const float>& higher_wave_data,
      float& table_interpolation_factor) const;

  // Like the above, except we accept 4 frequencies at a time and return 4
  // lower/higher wave data tables and the 4 corresponding table interpolation
  // factors.  Intended for use with the OscillatorNode for faster a-rate
  // processing.
  void WaveDataForFundamentalFrequency(
      const std::array<float, 4>& fundamental_frequency,
      std::array<base::span<const float>, 4>& lower_wave_data,
      std::array<base::span<const float>, 4>& higher_wave_data,
      std::array<float, 4>& table_interpolation_factor) const;

  // Returns the scalar multiplier to the oscillator frequency to calculate wave
  // buffer phase increment.
  float RateScale() const { return rate_scale_; }

  // The size of the FFT to use based on the sampling rate.
  unsigned PeriodicWaveSize() const { return periodic_wave_size_; }

  // Returns the total byte size of allocated band-limited wave tables.
  size_t AllocatedMemorySize() const {
    return band_limited_tables_.size() * periodic_wave_size_ * sizeof(float);
  }

 private:
  friend class ThreadSafeRefCounted<PeriodicWaveHandler>;
  template <typename T, typename... Args>
  friend scoped_refptr<T> base::MakeRefCounted(Args&&... args);

  explicit PeriodicWaveHandler(float sample_rate);
  ~PeriodicWaveHandler() = default;

  // Generates basic waveforms (sine, square, etc.) and creates band-limited
  // tables. Returns false if allocation fails.
  bool GenerateBasicWaveform(int shape);

  // Converts Fourier coefficients into time-domain wave buffers. One table is
  // created for each pitch range to prevent aliasing during playback at
  // different rates. Higher ranges have more high-frequency partials culled
  // out. Returns false if allocation fails.
  bool CreateBandLimitedTables(base::span<const float> real,
                               base::span<const float> imag,
                               bool disable_normalization);

  const unsigned periodic_wave_size_;

  // The number of ranges needed for the given sampling rate and FFT size.
  const unsigned number_of_ranges_;

  // The lowest frequency (in Hertz) where playback will include all of the
  // partials.  Playing back lower than this frequency will gradually lose more
  // high-frequency information.  This frequency is quite low (~10Hz @ 44.1KHz)
  const float lowest_fundamental_frequency_;

  // Scale factor to convert frequency in Hertz to table index increment per
  // sample. Calculated as periodic_wave_size_ / sample_rate.
  const float rate_scale_;

  Vector<std::unique_ptr<AudioFloatArray>> band_limited_tables_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_PERIODIC_WAVE_HANDLER_H_
