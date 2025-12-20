// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/biquad_filter_handler.h"

#include <memory>

#include "base/containers/span.h"
#include "base/synchronization/lock.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/platform/audio/biquad.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"

#ifdef __SSE2__
#include <immintrin.h>
#elif defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

namespace blink {

namespace {

constexpr unsigned kRenderQuantumFramesExpected = 128;
constexpr unsigned kDefaultNumberOfOutputChannels = 1;

// TODO(crbug.com/40268882): A reasonable upper limit for the tail time. While
// it's easy to create biquad filters whose tail time can be much larger than
// this, limit the maximum to this value so that we don't keep such nodes alive
// "forever". Investigate if we can adjust this to a smaller value.
constexpr double kMaxTailTime = 30.0;

bool HasConstantValues(base::span<float> values) {
  if (values.size() <= 1) {
    return true;
  }

  // Load the initial value
  const float value = values[0];

  // Initialize to 1 to avoid redundantly comparing the first frame in the
  // non-SIMD path, although this will be re-initialized to 0 on platforms with
  // SIMD enabled for byte alignment purposes so it is only an optimization on
  // platforms without SIMD.
  int processed_frames = 1;
  // Due to `values_size - 3` below this value could be negative, so to save
  // having multiple static_cast<int>, we do it once and just use that to
  // silence warnings about comparing unsigned and signed.
  DCHECK_LE(values.size(),
            static_cast<size_t>(std::numeric_limits<int>::max()));
  const int values_size = static_cast<int>(values.size());

#if defined(__SSE2__)
  // Process 4 floats at a time using SIMD
  __m128 value_vec = _mm_set1_ps(value);
  // Start at 0 for byte alignment
  for (processed_frames = 0; processed_frames < values_size - 3;
       processed_frames += 4) {
    // Load 4 floats from memory
    __m128 input_vec = _mm_loadu_ps(&values[processed_frames]);
    // Compare the 4 floats with the value
    __m128 cmp_vec = _mm_cmpneq_ps(input_vec, value_vec);
    // Check if any of the floats are not equal to the value
    if (_mm_movemask_ps(cmp_vec) != 0) {
      return false;
    }
  }
#elif defined(__ARM_NEON__)
  // Process 4 floats at a time using SIMD
  float32x4_t value_vec = vdupq_n_f32(value);
  // Start at 0 for byte alignment
  for (processed_frames = 0; processed_frames < values_size - 3;
       processed_frames += 4) {
    // Load 4 floats from memory
    float32x4_t input_vec = vld1q_f32(&values[processed_frames]);
    // Compare the 4 floats with the value
    uint32x4_t cmp_vec = vceqq_f32(input_vec, value_vec);
    // Accumulate the elements of the cmp_vec vector using bitwise AND
    uint32x2_t cmp_reduced_32 =
        vand_u32(vget_low_u32(cmp_vec), vget_high_u32(cmp_vec));
    // Check if any of the floats are not equal to the value
    if (vget_lane_u32(vpmin_u32(cmp_reduced_32, cmp_reduced_32), 0) == 0) {
      return false;
    }
  }
#endif
  // Fallback implementation without SIMD optimization
  while (processed_frames < values_size) {
    if (values[processed_frames] != value) {
      return false;
    }
    processed_frames++;
  }
  return true;
}

// Convert from Hertz to normalized frequency 0 -> 1.
double NormalizeFrequency(float frequency, double nyquist, float detune) {
  const double normalized_frequency = frequency / nyquist;
  // Detune in Cents multiplies the frequency by 2^(detune / 1200).
  return detune ? normalized_frequency * exp2(detune / 1200)
                : normalized_frequency;
}

// Configure the biquad with the new filter parameters for the appropriate type
// of filter.
void SetBiquadParams(Biquad* biquad,
                     V8BiquadFilterType::Enum type,
                     int index,
                     double frequency,
                     double q,
                     double gain) {
  switch (type) {
    case V8BiquadFilterType::Enum::kLowpass:
      biquad->SetLowpassParams(index, frequency, q);
      return;

    case V8BiquadFilterType::Enum::kHighpass:
      biquad->SetHighpassParams(index, frequency, q);
      return;

    case V8BiquadFilterType::Enum::kBandpass:
      biquad->SetBandpassParams(index, frequency, q);
      return;

    case V8BiquadFilterType::Enum::kLowshelf:
      biquad->SetLowShelfParams(index, frequency, gain);
      return;

    case V8BiquadFilterType::Enum::kHighshelf:
      biquad->SetHighShelfParams(index, frequency, gain);
      return;

    case V8BiquadFilterType::Enum::kPeaking:
      biquad->SetPeakingParams(index, frequency, q, gain);
      return;

    case V8BiquadFilterType::Enum::kNotch:
      biquad->SetNotchParams(index, frequency, q);
      return;

    case V8BiquadFilterType::Enum::kAllpass:
      biquad->SetAllpassParams(index, frequency, q);
      return;
  }
  NOTREACHED();
}

}  // namespace

BiquadFilterHandler::BiquadFilterHandler(AudioNode& node,
                                         float sample_rate,
                                         AudioParamHandler& frequency,
                                         AudioParamHandler& q,
                                         AudioParamHandler& gain,
                                         AudioParamHandler& detune)
    : AudioHandler(NodeType::kNodeTypeBiquadFilter, node, sample_rate),
      parameter_cutoff_frequency_(&frequency),
      parameter_q_(&q),
      parameter_gain_(&gain),
      parameter_detune_(&detune),
      sample_rate_(sample_rate),
      nyquist_(0.5 * sample_rate),
      render_quantum_frames_(node.context()->renderQuantumSize()) {
  DCHECK(Context());
  DCHECK(Context()->GetExecutionContext());

  AddInput();
  AddOutput(kDefaultNumberOfOutputChannels);

  task_runner_ = Context()->GetExecutionContext()->GetTaskRunner(
      TaskType::kMediaElementEvent);

  // Initialize the handler so that AudioParams can be processed.
  Initialize();
}

BiquadFilterHandler::~BiquadFilterHandler() {
  Uninitialize();
}

scoped_refptr<BiquadFilterHandler> BiquadFilterHandler::Create(
    AudioNode& node,
    float sample_rate,
    AudioParamHandler& frequency,
    AudioParamHandler& q,
    AudioParamHandler& gain,
    AudioParamHandler& detune) {
  return base::AdoptRef(
      new BiquadFilterHandler(node, sample_rate, frequency, q, gain, detune));
}

void BiquadFilterHandler::Initialize() {
  if (IsInitialized()) {
    return;
  }

  AudioHandler::Initialize();

  {
    base::AutoLock locker(process_lock_);
    DCHECK(biquads_.empty());

    // Create processing kernels, one per channel.
    for (unsigned i = 0; i < Output(0).NumberOfChannels(); ++i) {
      biquads_.push_back(std::make_unique<Biquad>(render_quantum_frames_));
    }
    has_just_reset_ = true;
  }
}

void BiquadFilterHandler::Uninitialize() {
  if (!IsInitialized()) {
    return;
  }

  {
    base::AutoLock locker(process_lock_);
    biquads_.clear();
  }

  AudioHandler::Uninitialize();
}

void BiquadFilterHandler::Process(uint32_t frames_to_process) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
               "BiquadFilterHandler::Process");

  AudioBus* destination_bus = Output(0).Bus();

  if (!IsInitialized()) {
    destination_bus->Zero();
  } else {
    scoped_refptr<AudioBus> source_bus = Input(0).Bus();

    // FIXME: if we take "tail time" into account, then we can avoid calling
    // processor()->process() once the tail dies down.
    if (!Input(0).IsConnected()) {
      source_bus->Zero();
    }

    // Synchronize with possible dynamic changes to the impulse response.
    base::AutoTryLock try_locker(process_lock_);
    if (!try_locker.is_acquired()) {
      // Can't get the lock. We must be in the middle of changing something.
      destination_bus->Zero();
    } else {
      // The BiquadFilterHandler objects rely on this value to see if they
      // need to re-compute their internal filter coefficients. Start out
      // assuming filter parameters are not changing.
      bool are_filter_coefficients_dirty = false;
      bool has_sample_accurate_values = false;
      bool is_audio_rate = false;

      if (parameter_cutoff_frequency_->HasSampleAccurateValues() ||
          parameter_q_->HasSampleAccurateValues() ||
          parameter_gain_->HasSampleAccurateValues() ||
          parameter_detune_->HasSampleAccurateValues()) {
        // Coefficients are dirty if any of them has automations or if there
        // are connections to the AudioParam.
        are_filter_coefficients_dirty = true;
        has_sample_accurate_values = true;
        // If any parameter is a-rate, then the filter must do a-rate
        // processing for everything.
        is_audio_rate = parameter_cutoff_frequency_->IsAudioRate() ||
                        parameter_q_->IsAudioRate() ||
                        parameter_gain_->IsAudioRate() ||
                        parameter_detune_->IsAudioRate();
      } else {
        if (has_just_reset_) {
          // Snap to exact values first time after reset
          previous_parameter_cutoff_frequency_ =
              std::numeric_limits<float>::quiet_NaN();
          previous_parameter_q_ = std::numeric_limits<float>::quiet_NaN();
          previous_parameter_gain_ = std::numeric_limits<float>::quiet_NaN();
          previous_parameter_detune_ = std::numeric_limits<float>::quiet_NaN();
          are_filter_coefficients_dirty = true;
          has_just_reset_ = false;
        } else {
          // If filter parameters have changed then mark coefficients as
          // dirty.
          const float parameter_cutoff_frequency_final =
              parameter_cutoff_frequency_->FinalValue();
          const float parameter_q_final = parameter_q_->FinalValue();
          const float parameter_gain_final = parameter_gain_->FinalValue();
          const float parameter_detune_final = parameter_detune_->FinalValue();
          if ((previous_parameter_cutoff_frequency_ !=
               parameter_cutoff_frequency_final) ||
              (previous_parameter_q_ != parameter_q_final) ||
              (previous_parameter_gain_ != parameter_gain_final) ||
              (previous_parameter_detune_ != parameter_detune_final)) {
            are_filter_coefficients_dirty = true;
            previous_parameter_cutoff_frequency_ =
                parameter_cutoff_frequency_final;
            previous_parameter_q_ = parameter_q_final;
            previous_parameter_gain_ = parameter_gain_final;
            previous_parameter_detune_ = parameter_detune_final;
          }
        }
      }

      // Recompute filter coefficients if any of the parameters have changed.
      // FIXME: as an optimization, implement a way that a Biquad object can
      // simply copy its internal filter coefficients from another Biquad
      // object.  Then re-factor this code to only run for the first
      // BiquadDSPKernel of each BiquadProcessor.

      if (are_filter_coefficients_dirty) {
        // TODO(crbug.com/40637820): Eventually, the render quantum size
        // will no longer be hardcoded as 128. At that point, we'll need to
        // switch from stack allocation to heap allocation.
        CHECK_EQ(render_quantum_frames_, kRenderQuantumFramesExpected);
        float cutoff_frequency_data[kRenderQuantumFramesExpected];
        float q_data[kRenderQuantumFramesExpected];
        float gain_data[kRenderQuantumFramesExpected];
        float detune_data[kRenderQuantumFramesExpected];  // in Cents
        base::span<float> cutoff_frequency(cutoff_frequency_data);
        base::span<float> q(q_data);
        base::span<float> gain(gain_data);
        base::span<float> detune(detune_data);

        SECURITY_CHECK(static_cast<unsigned>(frames_to_process) <=
                       render_quantum_frames_);

        if (has_sample_accurate_values && is_audio_rate) {
          parameter_cutoff_frequency_->CalculateSampleAccurateValues(
              cutoff_frequency.first(static_cast<size_t>(frames_to_process)));
          parameter_q_->CalculateSampleAccurateValues(
              q.first(static_cast<size_t>(frames_to_process)));
          parameter_gain_->CalculateSampleAccurateValues(
              gain.first(static_cast<size_t>(frames_to_process)));
          parameter_detune_->CalculateSampleAccurateValues(
              detune.first(static_cast<size_t>(frames_to_process)));

          // If all the values are actually constant for this render (or the
          // automation rate is "k-rate" for all of the AudioParams), we
          // don't need to compute filter coefficients for each frame since
          // they would be the same as the first.
          bool is_constant = HasConstantValues(cutoff_frequency) &&
                             HasConstantValues(q) && HasConstantValues(gain) &&
                             HasConstantValues(detune);
          size_t needed_frames = is_constant ? 1 : std::size(cutoff_frequency);
          // Convert from Hertz to normalized frequency 0 -> 1.
          for (const auto& biquad : biquads_) {
            biquad->SetHasSampleAccurateValues(needed_frames > 1);

            for (size_t k = 0; k < needed_frames; ++k) {
              const double normalized_frequency =
                  NormalizeFrequency(cutoff_frequency[k], nyquist_, detune[k]);
              SetBiquadParams(biquad.get(), type_, k, normalized_frequency,
                              q[k], gain[k]);
            }
          }
          const int coef_index = needed_frames - 1;
          DCHECK(!biquads_.empty());
          const double tail =
              biquads_[0]->TailFrame(coef_index, kMaxTailTime * sample_rate_) /
              sample_rate_;
          tail_time_ = ClampTo(tail, 0.0, kMaxTailTime);
        } else {
          cutoff_frequency[0] = parameter_cutoff_frequency_->FinalValue();
          q[0] = parameter_q_->FinalValue();
          gain[0] = parameter_gain_->FinalValue();
          detune[0] = parameter_detune_->FinalValue();
          for (const auto& biquad : biquads_) {
            // Convert from Hertz to normalized frequency 0 -> 1.
            biquad->SetHasSampleAccurateValues(false);
            const double normalized_frequency =
                NormalizeFrequency(cutoff_frequency[0], nyquist_, detune[0]);
            SetBiquadParams(biquad.get(), type_, 0, normalized_frequency, q[0],
                            gain[0]);
          }
          DCHECK(!biquads_.empty());
          const double tail =
              biquads_[0]->TailFrame(0, kMaxTailTime * sample_rate_) /
              sample_rate_;
          tail_time_ = ClampTo(tail, 0.0, kMaxTailTime);
        }
      }

      // For each channel of our input, process using the corresponding
      // Biquad into the output channel.
      for (unsigned i = 0; i < biquads_.size(); ++i) {
        DCHECK(source_bus->Channel(i)->Data());
        DCHECK(destination_bus->Channel(i)->MutableData());
        biquads_[i]->Process(source_bus->Channel(i)->Data(),
                             destination_bus->Channel(i)->MutableData(),
                             frames_to_process);
      }
    }
  }

  if (!did_warn_bad_filter_state_) {
    // Inform the user once if the output has a non-finite value.  This is a
    // proxy for the filter state containing non-finite values since the output
    // is also saved as part of the state of the filter.
    AudioBus* output_bus = Output(0).Bus();
    for (wtf_size_t k = 0; k < output_bus->NumberOfChannels(); ++k) {
      AudioChannel* channel = output_bus->Channel(k);
      if (channel->length() > 0 && !std::isfinite(channel->Data()[0])) {
        did_warn_bad_filter_state_ = true;
        PostCrossThreadTask(
            *task_runner_, FROM_HERE,
            CrossThreadBindOnce(&BiquadFilterHandler::NotifyBadState,
                                weak_ptr_factory_.GetWeakPtr()));
        break;
      }
    }
  }
}

void BiquadFilterHandler::ProcessOnlyAudioParams(uint32_t frames_to_process) {
  if (!IsInitialized()) {
    return;
  }

  // TODO(crbug.com/40637820): Eventually, the render quantum size will no
  // longer be hardcoded as 128. At that point, we'll need to switch from
  // stack allocation to heap allocation.
  CHECK_EQ(render_quantum_frames_, kRenderQuantumFramesExpected);

  DCHECK_LE(frames_to_process, kRenderQuantumFramesExpected);

  float values[kRenderQuantumFramesExpected];

  parameter_cutoff_frequency_->CalculateSampleAccurateValues(
      base::span(values).first(frames_to_process));
  parameter_q_->CalculateSampleAccurateValues(
      base::span(values).first(frames_to_process));
  parameter_gain_->CalculateSampleAccurateValues(
      base::span(values).first(frames_to_process));
  parameter_detune_->CalculateSampleAccurateValues(
      base::span(values).first(frames_to_process));
}

// Nice optimization in the very common case allowing for "in-place" processing
void BiquadFilterHandler::PullInputs(uint32_t frames_to_process) {
  // Render input stream - suggest to the input to render directly into output
  // bus for in-place processing in process() if possible.
  Input(0).Pull(Output(0).Bus(), frames_to_process);
}

// As soon as we know the channel count of our input, we can lazily initialize.
// Sometimes this may be called more than once with different channel counts, in
// which case we must safely uninitialize and then re-initialize with the new
// channel count.
void BiquadFilterHandler::CheckNumberOfChannelsForInput(AudioNodeInput* input) {
  DCHECK(Context()->IsAudioThread());
  Context()->AssertGraphOwner();

  DCHECK_EQ(input, &Input(0));

  unsigned number_of_channels = input->NumberOfChannels();

  if (IsInitialized() && number_of_channels != Output(0).NumberOfChannels()) {
    // We're already initialized but the channel count has changed.
    Uninitialize();
  }

  if (!IsInitialized()) {
    // This will propagate the channel count to any nodes connected further down
    // the chain...
    Output(0).SetNumberOfChannels(number_of_channels);

    // Re-initialize with the new channel count.
    Initialize();
  }

  AudioHandler::CheckNumberOfChannelsForInput(input);
}

void BiquadFilterHandler::GetFrequencyResponse(
    base::span<const float> frequency_hz,
    base::span<float> mag_response,
    base::span<float> phase_response) {
  DCHECK(IsMainThread());

  // Compute the frequency response on a separate temporary kernel
  // to avoid interfering with the processing running in the audio
  // thread on the main kernels.
  std::unique_ptr<Biquad> response_kernel =
      std::make_unique<Biquad>(render_quantum_frames_);

  float cutoff_frequency;
  float q;
  float gain;
  float detune;  // in Cents

  {
    // Get a copy of the current biquad filter coefficients so we can update
    // `response_kernel` with these values.  We need to synchronize with
    // `Process()` to prevent process() from updating the filter coefficients
    // while we're trying to access them.  Since this is on the main thread, we
    // can wait.  The audio thread will update the coefficients the next time
    // around, if it was blocked.
    base::AutoLock process_locker(process_lock_);

    cutoff_frequency = parameter_cutoff_frequency_->Value();
    q = parameter_q_->Value();
    gain = parameter_gain_->Value();
    detune = parameter_detune_->Value();
  }

  const double normalized_frequency =
      NormalizeFrequency(cutoff_frequency, nyquist_, detune);
  SetBiquadParams(response_kernel.get(), type_, 0, normalized_frequency, q,
                  gain);

  DCHECK(!frequency_hz.empty());
  DCHECK(!mag_response.empty());
  DCHECK(!phase_response.empty());

  Vector<float> frequency(frequency_hz.size());

  // Convert from frequency in Hz to normalized frequency (0 -> 1),
  // with 1 equal to the Nyquist frequency.
  for (size_t k = 0; k < frequency_hz.size(); ++k) {
    frequency[k] = frequency_hz[k] / nyquist_;
  }

  response_kernel->GetFrequencyResponse(frequency, mag_response,
                                        phase_response);
}

V8BiquadFilterType::Enum BiquadFilterHandler::Type() const {
  return type_;
}

void BiquadFilterHandler::SetType(V8BiquadFilterType::Enum type) {
  DCHECK(IsMainThread());

  if (type == type_) {
    return;
  }

  type_ = type;

  if (!IsInitialized()) {
    return;
  }

  base::AutoLock locker(process_lock_);
  for (const auto& biquad : biquads_) {
    biquad->Reset();
  }

  has_just_reset_ = true;
}

bool BiquadFilterHandler::RequiresTailProcessing() const {
  // Always return true even if the tail time and latency might both
  // be zero. This is for simplicity and because TailTime() is 0
  // basically only when the filter response H(z) = 0 or H(z) = 1. And
  // it's ok to return true. It just means the node lives a little
  // longer than strictly necessary.
  return true;
}

double BiquadFilterHandler::TailTime() const {
  DCHECK(!IsMainThread());
  base::AutoTryLock try_locker(process_lock_);
  if (try_locker.is_acquired()) {
    // It is expected that all the kernels have the same tailTime.
    return tail_time_;
  }
  // Since we don't want to block the Audio Device thread, we return a large
  // value instead of trying to acquire the lock.
  return std::numeric_limits<double>::infinity();
}

double BiquadFilterHandler::LatencyTime() const {
  return 0;
}

void BiquadFilterHandler::NotifyBadState() const {
  DCHECK(IsMainThread());
  if (!Context() || !Context()->GetExecutionContext()) {
    return;
  }

  Context()->GetExecutionContext()->AddConsoleMessage(
      MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kJavaScript,
          mojom::blink::ConsoleMessageLevel::kWarning,
          StrCat({NodeTypeName(),
                  ": state is bad, probably due to unstable filter caused by "
                  "fast parameter automation."})));
}

bool BiquadFilterHandler::HasConstantValuesForTesting(
    base::span<float> values) {
  return HasConstantValues(values);
}

}  // namespace blink
