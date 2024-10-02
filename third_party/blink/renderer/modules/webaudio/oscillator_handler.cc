// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webaudio/oscillator_handler.h"

#include <algorithm>
#include <limits>

#include "base/synchronization/lock.h"
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/oscillator_node.h"
#include "third_party/blink/renderer/modules/webaudio/periodic_wave.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/bindings/enumeration_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace {

// An oscillator is always mono.
constexpr unsigned kNumberOfOutputChannels = 1;

// Convert the detune value (in cents) to a frequency scale multiplier:
// 2^(d/1200)
float DetuneToFrequencyMultiplier(float detune_value) {
  return std::exp2(detune_value / 1200);
}

// Clamp the frequency value to lie with Nyquist frequency. For NaN, arbitrarily
// clamp to +Nyquist.
void ClampFrequency(float* frequency, int frames_to_process, float nyquist) {
  for (int k = 0; k < frames_to_process; ++k) {
    float f = frequency[k];

    if (std::isnan(f)) {
      frequency[k] = nyquist;
    } else {
      frequency[k] = ClampTo(f, -nyquist, nyquist);
    }
  }
}

float DoInterpolation(double virtual_read_index,
                      float incr,
                      unsigned read_index_mask,
                      float table_interpolation_factor,
                      const float* lower_wave_data,
                      const float* higher_wave_data) {
  DCHECK_GE(incr, 0);
  DCHECK(std::isfinite(virtual_read_index));

  double sample_lower = 0;
  double sample_higher = 0;

  unsigned read_index_0 = static_cast<unsigned>(virtual_read_index);

  // Consider a typical sample rate of 44100 Hz and max periodic wave
  // size of 4096.  The relationship between `incr` and the frequency
  // of the oscillator is `incr` = freq * 4096/44100. Or freq =
  // `incr`*44100/4096 = 10.8*`incr`.
  //
  // For the `incr` thresholds below, this means that we use linear
  // interpolation for all freq >= 3.2 Hz, 3-point Lagrange
  // for freq >= 1.7 Hz and 5-point Lagrange for every thing else.
  //
  // We use Lagrange interpolation because it's relatively simple to
  // implement and fairly inexpensive, and the interpolator always
  // passes through known points.
  if (incr >= OscillatorHandler::kInterpolate2Point) {
    // Increment is fairly large, so we're doing no more than about 3
    // points between each wave table entry. Assume linear
    // interpolation between points is good enough.
    unsigned read_index2 = read_index_0 + 1;

    // Contain within valid range.
    read_index_0 = read_index_0 & read_index_mask;
    read_index2 = read_index2 & read_index_mask;

    float sample1_lower = lower_wave_data[read_index_0];
    float sample2_lower = lower_wave_data[read_index2];
    float sample1_higher = higher_wave_data[read_index_0];
    float sample2_higher = higher_wave_data[read_index2];

    // Linearly interpolate within each table (lower and higher).
    double interpolation_factor =
        static_cast<float>(virtual_read_index) - read_index_0;
    sample_higher = (1 - interpolation_factor) * sample1_higher +
                    interpolation_factor * sample2_higher;
    sample_lower = (1 - interpolation_factor) * sample1_lower +
                   interpolation_factor * sample2_lower;

  } else if (incr >= OscillatorHandler::kInterpolate3Point) {
    // We're doing about 6 interpolation values between each wave
    // table sample. Just use a 3-point Lagrange interpolator to get a
    // better estimate than just linear.
    //
    // See 3-point formula in http://dlmf.nist.gov/3.3#ii
    unsigned read_index[3];

    for (int k = -1; k <= 1; ++k) {
      read_index[k + 1] = (read_index_0 + k) & read_index_mask;
    }

    double a[3];
    double t = virtual_read_index - read_index_0;

    a[0] = 0.5 * t * (t - 1);
    a[1] = 1 - t * t;
    a[2] = 0.5 * t * (t + 1);

    for (int k = 0; k < 3; ++k) {
      sample_lower += a[k] * lower_wave_data[read_index[k]];
      sample_higher += a[k] * higher_wave_data[read_index[k]];
    }
  } else {
    // For everything else (more than 6 points per entry), we'll do a
    // 5-point Lagrange interpolator.  This is a trade-off between
    // quality and speed.
    //
    // See 5-point formula in http://dlmf.nist.gov/3.3#ii
    unsigned read_index[5];
    for (int k = -2; k <= 2; ++k) {
      read_index[k + 2] = (read_index_0 + k) & read_index_mask;
    }

    double a[5];
    double t = virtual_read_index - read_index_0;
    double t2 = t * t;

    a[0] = t * (t2 - 1) * (t - 2) / 24;
    a[1] = -t * (t - 1) * (t2 - 4) / 6;
    a[2] = (t2 - 1) * (t2 - 4) / 4;
    a[3] = -t * (t + 1) * (t2 - 4) / 6;
    a[4] = t * (t2 - 1) * (t + 2) / 24;

    for (int k = 0; k < 5; ++k) {
      sample_lower += a[k] * lower_wave_data[read_index[k]];
      sample_higher += a[k] * higher_wave_data[read_index[k]];
    }
  }

  // Then interpolate between the two tables.
  float sample = (1 - table_interpolation_factor) * sample_higher +
                 table_interpolation_factor * sample_lower;
  return sample;
}

}  // namespace

OscillatorHandler::OscillatorHandler(AudioNode& node,
                                     float sample_rate,
                                     const String& oscillator_type,
                                     PeriodicWaveImpl* wave_table,
                                     AudioParamHandler& frequency,
                                     AudioParamHandler& detune)
    : AudioScheduledSourceHandler(kNodeTypeOscillator, node, sample_rate),
      frequency_(&frequency),
      detune_(&detune),
      phase_increments_(GetDeferredTaskHandler().RenderQuantumFrames()),
      detune_values_(GetDeferredTaskHandler().RenderQuantumFrames()) {
  if (wave_table) {
    // A PeriodicWave overrides any value for the oscillator type,
    // forcing the type to be "custom".
    SetPeriodicWave(wave_table);
  } else {
    if (oscillator_type == "sine") {
      SetType(SINE);
    } else if (oscillator_type == "square") {
      SetType(SQUARE);
    } else if (oscillator_type == "sawtooth") {
      SetType(SAWTOOTH);
    } else if (oscillator_type == "triangle") {
      SetType(TRIANGLE);
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }

  AddOutput(kNumberOfOutputChannels);

  Initialize();
}

scoped_refptr<OscillatorHandler> OscillatorHandler::Create(
    AudioNode& node,
    float sample_rate,
    const String& oscillator_type,
    PeriodicWaveImpl* wave_table,
    AudioParamHandler& frequency,
    AudioParamHandler& detune) {
  return base::AdoptRef(new OscillatorHandler(
      node, sample_rate, oscillator_type, wave_table, frequency, detune));
}

OscillatorHandler::~OscillatorHandler() {
  Uninitialize();
}

V8OscillatorType::Enum OscillatorHandler::GetType() const {
  switch (type_) {
    case SINE:
      return V8OscillatorType::Enum::kSine;
    case SQUARE:
      return V8OscillatorType::Enum::kSquare;
    case SAWTOOTH:
      return V8OscillatorType::Enum::kSawtooth;
    case TRIANGLE:
      return V8OscillatorType::Enum::kTriangle;
    case CUSTOM:
      return V8OscillatorType::Enum::kCustom;
    default:
      NOTREACHED();
  }
}

void OscillatorHandler::SetType(V8OscillatorType::Enum type,
                                ExceptionState& exception_state) {
  switch (type) {
    case V8OscillatorType::Enum::kSine:
      SetType(SINE);
      return;
    case V8OscillatorType::Enum::kSquare:
      SetType(SQUARE);
      return;
    case V8OscillatorType::Enum::kSawtooth:
      SetType(SAWTOOTH);
      return;
    case V8OscillatorType::Enum::kTriangle:
      SetType(TRIANGLE);
      return;
    case V8OscillatorType::Enum::kCustom:
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "'type' cannot be set directly to "
                                        "'custom'.  Use setPeriodicWave() to "
                                        "create a custom Oscillator type.");
      return;
  }
  NOTREACHED();
}

bool OscillatorHandler::SetType(uint8_t type) {
  PeriodicWave* periodic_wave = nullptr;

  switch (type) {
    case SINE:
      periodic_wave = Context()->GetPeriodicWave(SINE);
      break;
    case SQUARE:
      periodic_wave = Context()->GetPeriodicWave(SQUARE);
      break;
    case SAWTOOTH:
      periodic_wave = Context()->GetPeriodicWave(SAWTOOTH);
      break;
    case TRIANGLE:
      periodic_wave = Context()->GetPeriodicWave(TRIANGLE);
      break;
    case CUSTOM:
    default:
      // Return false for invalid types, including CUSTOM since
      // setPeriodicWave() method must be called explicitly.
      NOTREACHED_IN_MIGRATION();
      return false;
  }

  SetPeriodicWave(periodic_wave->impl());
  type_ = type;
  return true;
}

bool OscillatorHandler::CalculateSampleAccuratePhaseIncrements(
    uint32_t frames_to_process) {
  DCHECK_LE(frames_to_process, phase_increments_.size());
  DCHECK_LE(frames_to_process, detune_values_.size());

  if (first_render_) {
    first_render_ = false;
  }

  bool has_sample_accurate_values = false;
  bool has_frequency_changes = false;
  float* phase_increments = phase_increments_.Data();

  float final_scale = periodic_wave_->RateScale();

  if (frequency_->HasSampleAccurateValues() && frequency_->IsAudioRate()) {
    has_sample_accurate_values = true;
    has_frequency_changes = true;

    // Get the sample-accurate frequency values and convert to phase increments.
    // They will be converted to phase increments below.
    frequency_->CalculateSampleAccurateValues(phase_increments,
                                              frames_to_process);
  } else {
    // Handle ordinary parameter changes if there are no scheduled changes.
    float frequency = frequency_->FinalValue();
    final_scale *= frequency;
  }

  if (detune_->HasSampleAccurateValues() && detune_->IsAudioRate()) {
    has_sample_accurate_values = true;

    // Get the sample-accurate detune values.
    float* detune_values =
        has_frequency_changes ? detune_values_.Data() : phase_increments;
    detune_->CalculateSampleAccurateValues(detune_values, frames_to_process);

    // Convert from cents to rate scalar.
    float k = 1.0 / 1200;
    vector_math::Vsmul(detune_values, 1, &k, detune_values, 1,
                       frames_to_process);
    for (unsigned i = 0; i < frames_to_process; ++i) {
      detune_values[i] = std::exp2(detune_values[i]);
    }

    if (has_frequency_changes) {
      // Multiply frequencies by detune scalings.
      vector_math::Vmul(detune_values, 1, phase_increments, 1, phase_increments,
                        1, frames_to_process);
    }
  } else {
    // Handle ordinary parameter changes if there are no scheduled
    // changes.
    float detune = detune_->FinalValue();
    float detune_scale = DetuneToFrequencyMultiplier(detune);
    final_scale *= detune_scale;
  }

  if (has_sample_accurate_values) {
    ClampFrequency(phase_increments, frames_to_process,
                   Context()->sampleRate() / 2);
    // Convert from frequency to wavetable increment.
    vector_math::Vsmul(phase_increments, 1, &final_scale, phase_increments, 1,
                       frames_to_process);
  }

  return has_sample_accurate_values;
}

#if !(defined(ARCH_CPU_X86_FAMILY) || defined(CPU_ARM_NEON))
// Vector operations not supported, so there's nothing to do except return 0 and
// virtual_read_index.  The scalar version will do the necessary processing.
std::tuple<int, double> OscillatorHandler::ProcessKRateVector(
    int n,
    float* dest_p,
    double virtual_read_index,
    float frequency,
    float rate_scale) const {
  DCHECK_GE(frequency * rate_scale, kInterpolate2Point);
  return std::make_tuple(0, virtual_read_index);
}
#endif

#if !(defined(ARCH_CPU_X86_FAMILY) || defined(CPU_ARM_NEON))
double OscillatorHandler::ProcessARateVectorKernel(
    float* dest_p,
    double virtual_read_index,
    const float* phase_increments,
    unsigned periodic_wave_size,
    const float* const lower_wave_data[4],
    const float* const higher_wave_data[4],
    const float table_interpolation_factor[4]) const {
  double inv_periodic_wave_size = 1.0 / periodic_wave_size;
  unsigned read_index_mask = periodic_wave_size - 1;

  for (int m = 0; m < 4; ++m) {
    unsigned read_index_0 = static_cast<unsigned>(virtual_read_index);

    // Increment is fairly large, so we're doing no more than about 3
    // points between each wave table entry. Assume linear
    // interpolation between points is good enough.
    unsigned read_index2 = read_index_0 + 1;

    // Contain within valid range.
    read_index_0 = read_index_0 & read_index_mask;
    read_index2 = read_index2 & read_index_mask;

    float sample1_lower = lower_wave_data[m][read_index_0];
    float sample2_lower = lower_wave_data[m][read_index2];
    float sample1_higher = higher_wave_data[m][read_index_0];
    float sample2_higher = higher_wave_data[m][read_index2];

    // Linearly interpolate within each table (lower and higher).
    double interpolation_factor =
        static_cast<float>(virtual_read_index) - read_index_0;
    // Doing linear interpolation via x0 + f*(x1-x0) gives slightly
    // different results from (1-f)*x0 + f*x1, but requires fewer
    // operations.  This causes a very slight decrease in SNR (< 0.05 dB) in
    // oscillator sweep tests.
    float sample_higher =
        sample1_higher +
        interpolation_factor * (sample2_higher - sample1_higher);
    float sample_lower =
        sample1_lower + interpolation_factor * (sample2_lower - sample1_lower);

    // Then interpolate between the two tables.
    float sample = sample_higher + table_interpolation_factor[m] *
                                       (sample_lower - sample_higher);

    dest_p[m] = sample;

    // Increment virtual read index and wrap virtualReadIndex into the range
    // 0 -> periodicWaveSize.
    virtual_read_index += phase_increments[m];
    virtual_read_index -=
        floor(virtual_read_index * inv_periodic_wave_size) * periodic_wave_size;
  }

  return virtual_read_index;
}
#endif

double OscillatorHandler::ProcessKRateScalar(int start,
                                             int n,
                                             float* dest_p,
                                             double virtual_read_index,
                                             float frequency,
                                             float rate_scale) const {
  const unsigned periodic_wave_size = periodic_wave_->PeriodicWaveSize();
  const double inv_periodic_wave_size = 1.0 / periodic_wave_size;
  const unsigned read_index_mask = periodic_wave_size - 1;

  float* higher_wave_data = nullptr;
  float* lower_wave_data = nullptr;
  float table_interpolation_factor = 0;

  periodic_wave_->WaveDataForFundamentalFrequency(
      frequency, lower_wave_data, higher_wave_data, table_interpolation_factor);

  const float incr = frequency * rate_scale;
  DCHECK_GE(incr, kInterpolate2Point);

  for (int k = start; k < n; ++k) {
    // Get indices for the current and next sample, and contain them within the
    // valid range
    const unsigned read_index_0 =
        static_cast<unsigned>(virtual_read_index) & read_index_mask;
    const unsigned read_index_1 = (read_index_0 + 1) & read_index_mask;

    const float sample1_lower = lower_wave_data[read_index_0];
    const float sample2_lower = lower_wave_data[read_index_1];
    const float sample1_higher = higher_wave_data[read_index_0];
    const float sample2_higher = higher_wave_data[read_index_1];

    // Linearly interpolate within each table (lower and higher).
    const float interpolation_factor =
        static_cast<float>(virtual_read_index) - read_index_0;
    const float sample_higher =
        sample1_higher +
        interpolation_factor * (sample2_higher - sample1_higher);
    const float sample_lower =
        sample1_lower + interpolation_factor * (sample2_lower - sample1_lower);

    // Then interpolate between the two tables.
    const float sample = sample_higher + table_interpolation_factor *
                                             (sample_lower - sample_higher);

    dest_p[k] = sample;

    // Increment virtual read index and wrap virtualReadIndex into the range
    // 0 -> periodicWaveSize.
    virtual_read_index += incr;
    virtual_read_index -=
        floor(virtual_read_index * inv_periodic_wave_size) * periodic_wave_size;
  }

  return virtual_read_index;
}

double OscillatorHandler::ProcessKRate(int n,
                                       float* dest_p,
                                       double virtual_read_index) const {
  const unsigned periodic_wave_size = periodic_wave_->PeriodicWaveSize();
  const double inv_periodic_wave_size = 1.0 / periodic_wave_size;
  const unsigned read_index_mask = periodic_wave_size - 1;

  float* higher_wave_data = nullptr;
  float* lower_wave_data = nullptr;
  float table_interpolation_factor = 0;

  float frequency = frequency_->FinalValue();
  const float detune_scale = DetuneToFrequencyMultiplier(detune_->FinalValue());
  frequency *= detune_scale;
  ClampFrequency(&frequency, 1, Context()->sampleRate() / 2);
  periodic_wave_->WaveDataForFundamentalFrequency(
      frequency, lower_wave_data, higher_wave_data, table_interpolation_factor);

  const float rate_scale = periodic_wave_->RateScale();
  const float incr = frequency * rate_scale;

  if (incr >= kInterpolate2Point) {
    int k;
    double v_index = virtual_read_index;

    std::tie(k, v_index) =
        ProcessKRateVector(n, dest_p, v_index, frequency, rate_scale);

    if (k < n) {
      // In typical cases, this won't be run because the number of frames is 128
      // so the vector version will process all the samples.
      v_index =
          ProcessKRateScalar(k, n, dest_p, v_index, frequency, rate_scale);
    }

    // Recompute to reduce round-off introduced when processing the samples
    // above.
    virtual_read_index += n * incr;
    virtual_read_index -=
        floor(virtual_read_index * inv_periodic_wave_size) * periodic_wave_size;
  } else {
    for (int k = 0; k < n; ++k) {
      float sample = DoInterpolation(
          virtual_read_index, fabs(incr), read_index_mask,
          table_interpolation_factor, lower_wave_data, higher_wave_data);

      *dest_p++ = sample;

      // Increment virtual read index and wrap virtualReadIndex into the range
      // 0 -> periodicWaveSize.
      virtual_read_index += incr;
      virtual_read_index -= floor(virtual_read_index * inv_periodic_wave_size) *
                            periodic_wave_size;
    }
  }

  return virtual_read_index;
}

std::tuple<int, double> OscillatorHandler::ProcessARateVector(
    int n,
    float* destination,
    double virtual_read_index,
    const float* phase_increments) const {
  float rate_scale = periodic_wave_->RateScale();
  float inv_rate_scale = 1 / rate_scale;
  unsigned periodic_wave_size = periodic_wave_->PeriodicWaveSize();
  double inv_periodic_wave_size = 1.0 / periodic_wave_size;
  unsigned read_index_mask = periodic_wave_size - 1;

  float* higher_wave_data[4];
  float* lower_wave_data[4];
  float table_interpolation_factor[4] __attribute__((aligned(16)));

  int k = 0;
  int n_loops = n / 4;

  for (int loop = 0; loop < n_loops; ++loop, k += 4) {
    bool is_big_increment = true;
    float frequency[4];

    for (int m = 0; m < 4; ++m) {
      float phase_incr = phase_increments[k + m];
      is_big_increment =
          is_big_increment && (fabs(phase_incr) >= kInterpolate2Point);
      frequency[m] = inv_rate_scale * phase_incr;
    }

    periodic_wave_->WaveDataForFundamentalFrequency(frequency, lower_wave_data,
                                                    higher_wave_data,
                                                    table_interpolation_factor);

    // If all the phase increments are large enough, we can use linear
    // interpolation with a possibly vectorized implementation.  If not, we need
    // to call DoInterpolation to handle it correctly.
    if (is_big_increment) {
      virtual_read_index = ProcessARateVectorKernel(
          destination + k, virtual_read_index, phase_increments + k,
          periodic_wave_size, lower_wave_data, higher_wave_data,
          table_interpolation_factor);
    } else {
      for (int m = 0; m < 4; ++m) {
        float sample =
            DoInterpolation(virtual_read_index, fabs(phase_increments[k + m]),
                            read_index_mask, table_interpolation_factor[m],
                            lower_wave_data[m], higher_wave_data[m]);

        destination[k + m] = sample;

        // Increment virtual read index and wrap virtualReadIndex into the range
        // 0 -> periodicWaveSize.
        virtual_read_index += phase_increments[k + m];
        virtual_read_index -=
            floor(virtual_read_index * inv_periodic_wave_size) *
            periodic_wave_size;
      }
    }
  }

  return std::make_tuple(k, virtual_read_index);
}

double OscillatorHandler::ProcessARateScalar(
    int k,
    int n,
    float* destination,
    double virtual_read_index,
    const float* phase_increments) const {
  float rate_scale = periodic_wave_->RateScale();
  float inv_rate_scale = 1 / rate_scale;
  unsigned periodic_wave_size = periodic_wave_->PeriodicWaveSize();
  double inv_periodic_wave_size = 1.0 / periodic_wave_size;
  unsigned read_index_mask = periodic_wave_size - 1;

  float* higher_wave_data = nullptr;
  float* lower_wave_data = nullptr;
  float table_interpolation_factor = 0;

  for (int m = k; m < n; ++m) {
    float incr = phase_increments[m];

    float frequency = inv_rate_scale * incr;
    periodic_wave_->WaveDataForFundamentalFrequency(frequency, lower_wave_data,
                                                    higher_wave_data,
                                                    table_interpolation_factor);

    float sample = DoInterpolation(virtual_read_index, fabs(incr),
                                   read_index_mask, table_interpolation_factor,
                                   lower_wave_data, higher_wave_data);

    destination[m] = sample;

    // Increment virtual read index and wrap virtualReadIndex into the range
    // 0 -> periodicWaveSize.
    virtual_read_index += incr;
    virtual_read_index -=
        floor(virtual_read_index * inv_periodic_wave_size) * periodic_wave_size;
  }

  return virtual_read_index;
}

double OscillatorHandler::ProcessARate(int n,
                                       float* destination,
                                       double virtual_read_index,
                                       float* phase_increments) const {
  int frames_processed = 0;

  std::tie(frames_processed, virtual_read_index) =
      ProcessARateVector(n, destination, virtual_read_index, phase_increments);

  virtual_read_index = ProcessARateScalar(frames_processed, n, destination,
                                          virtual_read_index, phase_increments);

  return virtual_read_index;
}

void OscillatorHandler::Process(uint32_t frames_to_process) {
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
              "OscillatorHandler::Process", "this",
              reinterpret_cast<void*>(this), "type", GetType());

  AudioBus* output_bus = Output(0).Bus();

  if (!IsInitialized() || !output_bus->NumberOfChannels()) {
    output_bus->Zero();
    return;
  }

  DCHECK_LE(frames_to_process, phase_increments_.size());

  // The audio thread can't block on this lock, so we call tryLock() instead.
  base::AutoTryLock try_locker(process_lock_);
  if (!try_locker.is_acquired()) {
    // Too bad - the tryLock() failed. We must be in the middle of changing
    // wave-tables.
    output_bus->Zero();
    return;
  }

  // We must access m_periodicWave only inside the lock.
  if (!periodic_wave_.Get()) {
    output_bus->Zero();
    return;
  }

  size_t quantum_frame_offset;
  uint32_t non_silent_frames_to_process;
  double start_frame_offset;

  std::tie(quantum_frame_offset, non_silent_frames_to_process,
           start_frame_offset) =
      UpdateSchedulingInfo(frames_to_process, output_bus);

  if (!non_silent_frames_to_process) {
    output_bus->Zero();
    return;
  }

  unsigned periodic_wave_size = periodic_wave_->PeriodicWaveSize();

  float* dest_p = output_bus->Channel(0)->MutableData();

  DCHECK_LE(quantum_frame_offset, frames_to_process);

  // We keep virtualReadIndex double-precision since we're accumulating values.
  double virtual_read_index = virtual_read_index_;

  float rate_scale = periodic_wave_->RateScale();
  bool has_sample_accurate_values =
      CalculateSampleAccuratePhaseIncrements(frames_to_process);

  float frequency = 0;
  float* higher_wave_data = nullptr;
  float* lower_wave_data = nullptr;
  float table_interpolation_factor = 0;

  if (!has_sample_accurate_values) {
    frequency = frequency_->FinalValue();
    float detune = detune_->FinalValue();
    float detune_scale = DetuneToFrequencyMultiplier(detune);
    frequency *= detune_scale;
    ClampFrequency(&frequency, 1, Context()->sampleRate() / 2);
    periodic_wave_->WaveDataForFundamentalFrequency(frequency, lower_wave_data,
                                                    higher_wave_data,
                                                    table_interpolation_factor);
  }

  float* phase_increments = phase_increments_.Data();

  // Start rendering at the correct offset.
  dest_p += quantum_frame_offset;
  int n = non_silent_frames_to_process;

  // If startFrameOffset is not 0, that means the oscillator doesn't actually
  // start at quantumFrameOffset, but just past that time.  Adjust destP and n
  // to reflect that, and adjust virtualReadIndex to start the value at
  // startFrameOffset.
  if (start_frame_offset > 0) {
    ++dest_p;
    --n;
    virtual_read_index += (1 - start_frame_offset) * frequency * rate_scale;
    DCHECK(virtual_read_index < periodic_wave_size);
  } else if (start_frame_offset < 0) {
    virtual_read_index = -start_frame_offset * frequency * rate_scale;
  }

  if (has_sample_accurate_values) {
    virtual_read_index =
        ProcessARate(n, dest_p, virtual_read_index, phase_increments);
  } else {
    virtual_read_index = ProcessKRate(n, dest_p, virtual_read_index);
  }

  virtual_read_index_ = virtual_read_index;

  output_bus->ClearSilentFlag();
}

void OscillatorHandler::SetPeriodicWave(PeriodicWaveImpl* periodic_wave) {
  DCHECK(IsMainThread());
  DCHECK(periodic_wave);

  // This synchronizes with process().
  base::AutoLock process_locker(process_lock_);
  periodic_wave_ = periodic_wave;
  type_ = CUSTOM;
}

bool OscillatorHandler::PropagatesSilence() const {
  return !IsPlayingOrScheduled() || HasFinished() || !periodic_wave_;
}

base::WeakPtr<AudioScheduledSourceHandler> OscillatorHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void OscillatorHandler::HandleStoppableSourceNode() {
  double now = Context()->currentTime();

  base::AutoTryLock try_locker(process_lock_);
  if (!try_locker.is_acquired()) {
    // Can't get the lock, so just return.  It's ok to handle these at a later
    // time; this was just a hint anyway so stopping them a bit later is ok.
    return;
  }

  // If we know the end time, and the source was started and the current time is
  // definitely past the end time, we can stop this node.  (This handles the
  // case where the this source is not connected to the destination and we want
  // to stop it.)
  if (end_time_ != kUnknownTime && IsPlayingOrScheduled() &&
      now >= end_time_ + kExtraStopFrames / Context()->sampleRate()) {
    Finish();
  }
}

}  // namespace blink
