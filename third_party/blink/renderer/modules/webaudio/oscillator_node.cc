/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
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

#include <algorithm>
#include <limits>

#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/oscillator_node.h"
#include "third_party/blink/renderer/modules/webaudio/periodic_wave.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

OscillatorHandler::OscillatorHandler(AudioNode& node,
                                     float sample_rate,
                                     const String& oscillator_type,
                                     PeriodicWave* wave_table,
                                     AudioParamHandler& frequency,
                                     AudioParamHandler& detune)
    : AudioScheduledSourceHandler(kNodeTypeOscillator, node, sample_rate),
      frequency_(&frequency),
      detune_(&detune),
      first_render_(true),
      virtual_read_index_(0),
      phase_increments_(audio_utilities::kRenderQuantumFrames),
      detune_values_(audio_utilities::kRenderQuantumFrames) {
  if (wave_table) {
    // A PeriodicWave overrides any value for the oscillator type,
    // forcing the type to be 'custom".
    SetPeriodicWave(wave_table);
  } else {
    if (oscillator_type == "sine")
      SetType(SINE);
    else if (oscillator_type == "square")
      SetType(SQUARE);
    else if (oscillator_type == "sawtooth")
      SetType(SAWTOOTH);
    else if (oscillator_type == "triangle")
      SetType(TRIANGLE);
    else
      NOTREACHED();
  }

  // An oscillator is always mono.
  AddOutput(1);

  Initialize();
}

scoped_refptr<OscillatorHandler> OscillatorHandler::Create(
    AudioNode& node,
    float sample_rate,
    const String& oscillator_type,
    PeriodicWave* wave_table,
    AudioParamHandler& frequency,
    AudioParamHandler& detune) {
  return base::AdoptRef(new OscillatorHandler(
      node, sample_rate, oscillator_type, wave_table, frequency, detune));
}

OscillatorHandler::~OscillatorHandler() {
  Uninitialize();
}

String OscillatorHandler::GetType() const {
  switch (type_) {
    case SINE:
      return "sine";
    case SQUARE:
      return "square";
    case SAWTOOTH:
      return "sawtooth";
    case TRIANGLE:
      return "triangle";
    case CUSTOM:
      return "custom";
    default:
      NOTREACHED();
      return "custom";
  }
}

void OscillatorHandler::SetType(const String& type,
                                ExceptionState& exception_state) {
  if (type == "sine") {
    SetType(SINE);
  } else if (type == "square") {
    SetType(SQUARE);
  } else if (type == "sawtooth") {
    SetType(SAWTOOTH);
  } else if (type == "triangle") {
    SetType(TRIANGLE);
  } else if (type == "custom") {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "'type' cannot be set directly to "
                                      "'custom'.  Use setPeriodicWave() to "
                                      "create a custom Oscillator type.");
  }
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
      NOTREACHED();
      return false;
  }

  SetPeriodicWave(periodic_wave);
  type_ = type;
  return true;
}

// Convert the detune value (in cents) to a frequency scale multiplier:
// 2^(d/1200)
static float DetuneToFrequencyMultiplier(float detune_value) {
  return std::exp2(detune_value / 1200);
}

// Clamp the frequency value to lie with Nyquist frequency. For NaN, arbitrarily
// clamp to +Nyquist.
static void ClampFrequency(float* frequency,
                           int frames_to_process,
                           float nyquist) {
  for (int k = 0; k < frames_to_process; ++k) {
    float f = frequency[k];

    if (std::isnan(f)) {
      frequency[k] = nyquist;
    } else {
      frequency[k] = clampTo(f, -nyquist, nyquist);
    }
  }
}

bool OscillatorHandler::CalculateSampleAccuratePhaseIncrements(
    uint32_t frames_to_process) {
  DCHECK_LE(frames_to_process, phase_increments_.size());
  DCHECK_LE(frames_to_process, detune_values_.size());

  if (first_render_) {
    first_render_ = false;
    frequency_->ResetSmoothedValue();
    detune_->ResetSmoothedValue();
  }

  bool has_sample_accurate_values = false;
  bool has_frequency_changes = false;
  float* phase_increments = phase_increments_.Data();

  float final_scale = periodic_wave_->RateScale();

  if (frequency_->HasSampleAccurateValues()) {
    has_sample_accurate_values = true;
    has_frequency_changes = true;

    // Get the sample-accurate frequency values and convert to phase increments.
    // They will be converted to phase increments below.
    frequency_->CalculateSampleAccurateValues(phase_increments,
                                              frames_to_process);
  } else {
    // Handle ordinary parameter changes if there are no scheduled changes.
    float frequency = frequency_->Value();
    final_scale *= frequency;
  }

  if (detune_->HasSampleAccurateValues()) {
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
    float detune = detune_->Value();
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

static float DoInterpolation(double virtual_read_index,
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
  // size of 4096.  The relationship between |incr| and the frequency
  // of the oscillator is |incr| = freq * 4096/44100. Or freq =
  // |incr|*44100/4096 = 10.8*|incr|.
  //
  // For the |incr| thresholds below, this means that we use linear
  // interpolation for all freq >= 3.2 Hz, 3-point Lagrange
  // for freq >= 1.7 Hz and 5-point Lagrange for every thing else.
  //
  // We use Lagrange interpolation because it's relatively simple to
  // implement and fairly inexpensive, and the interpolator always
  // passes through known points.
  if (incr >= 0.3) {
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

  } else if (incr >= .16) {
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

void OscillatorHandler::Process(uint32_t frames_to_process) {
  AudioBus* output_bus = Output(0).Bus();

  if (!IsInitialized() || !output_bus->NumberOfChannels()) {
    output_bus->Zero();
    return;
  }

  DCHECK_LE(frames_to_process, phase_increments_.size());

  // The audio thread can't block on this lock, so we call tryLock() instead.
  MutexTryLocker try_locker(process_lock_);
  if (!try_locker.Locked()) {
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
  double inv_periodic_wave_size = 1.0 / periodic_wave_size;

  float* dest_p = output_bus->Channel(0)->MutableData();

  DCHECK_LE(quantum_frame_offset, frames_to_process);

  // We keep virtualReadIndex double-precision since we're accumulating values.
  double virtual_read_index = virtual_read_index_;

  float rate_scale = periodic_wave_->RateScale();
  float inv_rate_scale = 1 / rate_scale;
  bool has_sample_accurate_values =
      CalculateSampleAccuratePhaseIncrements(frames_to_process);

  float frequency = 0;
  float* higher_wave_data = nullptr;
  float* lower_wave_data = nullptr;
  float table_interpolation_factor = 0;

  if (!has_sample_accurate_values) {
    frequency = frequency_->Value();
    float detune = detune_->Value();
    float detune_scale = DetuneToFrequencyMultiplier(detune);
    frequency *= detune_scale;
    ClampFrequency(&frequency, 1, Context()->sampleRate() / 2);
    periodic_wave_->WaveDataForFundamentalFrequency(frequency, lower_wave_data,
                                                    higher_wave_data,
                                                    table_interpolation_factor);
  }

  float incr = frequency * rate_scale;
  float* phase_increments = phase_increments_.Data();

  unsigned read_index_mask = periodic_wave_size - 1;

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

  while (n--) {
    if (has_sample_accurate_values) {
      incr = *phase_increments++;

      frequency = inv_rate_scale * incr;
      periodic_wave_->WaveDataForFundamentalFrequency(
          frequency, lower_wave_data, higher_wave_data,
          table_interpolation_factor);
    }

    float sample = DoInterpolation(virtual_read_index, fabs(incr),
                                   read_index_mask, table_interpolation_factor,
                                   lower_wave_data, higher_wave_data);

    *dest_p++ = sample;

    // Increment virtual read index and wrap virtualReadIndex into the range
    // 0 -> periodicWaveSize.
    virtual_read_index += incr;
    virtual_read_index -=
        floor(virtual_read_index * inv_periodic_wave_size) * periodic_wave_size;
  }

  virtual_read_index_ = virtual_read_index;

  output_bus->ClearSilentFlag();
}

void OscillatorHandler::SetPeriodicWave(PeriodicWave* periodic_wave) {
  DCHECK(IsMainThread());
  DCHECK(periodic_wave);

  // This synchronizes with process().
  MutexLocker process_locker(process_lock_);
  periodic_wave_ = periodic_wave;
  type_ = CUSTOM;
}

bool OscillatorHandler::PropagatesSilence() const {
  return !IsPlayingOrScheduled() || HasFinished() || !periodic_wave_.Get();
}

void OscillatorHandler::HandleStoppableSourceNode() {
  double now = Context()->currentTime();

  // If we know the end time, and the source was started and the current time is
  // definitely past the end time, we can stop this node.  (This handles the
  // case where the this source is not connected to the destination and we want
  // to stop it.)
  if (end_time_ != kUnknownTime && IsPlayingOrScheduled() &&
      now >= end_time_ + kExtraStopFrames / Context()->sampleRate()) {
    Finish();
  }
}

// ----------------------------------------------------------------

OscillatorNode::OscillatorNode(BaseAudioContext& context,
                               const String& oscillator_type,
                               PeriodicWave* wave_table)
    : AudioScheduledSourceNode(context),
      // Use musical pitch standard A440 as a default.
      frequency_(
          AudioParam::Create(context,
                             Uuid(),
                             AudioParamHandler::kParamTypeOscillatorFrequency,
                             440,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable,
                             -context.sampleRate() / 2,
                             context.sampleRate() / 2)),
      // Default to no detuning.
      detune_(
          AudioParam::Create(context,
                             Uuid(),
                             AudioParamHandler::kParamTypeOscillatorDetune,
                             0,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable,
                             -1200 * log2f(std::numeric_limits<float>::max()),
                             1200 * log2f(std::numeric_limits<float>::max()))),
      periodic_wave_(wave_table) {
  SetHandler(OscillatorHandler::Create(
      *this, context.sampleRate(), oscillator_type, wave_table,
      frequency_->Handler(), detune_->Handler()));
}

OscillatorNode* OscillatorNode::Create(BaseAudioContext& context,
                                       const String& oscillator_type,
                                       PeriodicWave* wave_table,
                                       ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return MakeGarbageCollected<OscillatorNode>(context, oscillator_type,
                                              wave_table);
}

OscillatorNode* OscillatorNode::Create(BaseAudioContext* context,
                                       const OscillatorOptions* options,
                                       ExceptionState& exception_state) {
  if (options->type() == "custom" && !options->hasPeriodicWave()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "A PeriodicWave must be specified if the type is set to \"custom\"");
    return nullptr;
  }

  OscillatorNode* node = Create(*context, options->type(),
                                options->periodicWave(), exception_state);

  if (!node)
    return nullptr;

  node->HandleChannelOptions(options, exception_state);

  node->detune()->setValue(options->detune());
  node->frequency()->setValue(options->frequency());

  return node;
}

void OscillatorNode::Trace(blink::Visitor* visitor) {
  visitor->Trace(frequency_);
  visitor->Trace(detune_);
  visitor->Trace(periodic_wave_);
  AudioScheduledSourceNode::Trace(visitor);
}

OscillatorHandler& OscillatorNode::GetOscillatorHandler() const {
  return static_cast<OscillatorHandler&>(Handler());
}

String OscillatorNode::type() const {
  return GetOscillatorHandler().GetType();
}

void OscillatorNode::setType(const String& type,
                             ExceptionState& exception_state) {
  GetOscillatorHandler().SetType(type, exception_state);
}

AudioParam* OscillatorNode::frequency() {
  return frequency_;
}

AudioParam* OscillatorNode::detune() {
  return detune_;
}

void OscillatorNode::setPeriodicWave(PeriodicWave* wave) {
  periodic_wave_ = wave;
  GetOscillatorHandler().SetPeriodicWave(wave);
}

void OscillatorNode::ReportDidCreate() {
  GraphTracer().DidCreateAudioNode(this);
  GraphTracer().DidCreateAudioParam(detune_);
  GraphTracer().DidCreateAudioParam(frequency_);
}

void OscillatorNode::ReportWillBeDestroyed() {
  GraphTracer().WillDestroyAudioParam(detune_);
  GraphTracer().WillDestroyAudioParam(frequency_);
  GraphTracer().WillDestroyAudioNode(this);
}

}  // namespace blink
