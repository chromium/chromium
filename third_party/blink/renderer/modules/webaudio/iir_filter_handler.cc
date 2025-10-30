// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/iir_filter_handler.h"

#include <memory>

#include "base/compiler_specific.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/platform/audio/iir_filter.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"

namespace blink {

namespace {

constexpr unsigned kDefaultNumberOfOutputChannels = 1;

}  // namespace

scoped_refptr<IIRFilterHandler> IIRFilterHandler::Create(
    AudioNode& node,
    float sample_rate,
    const Vector<double>& feedforward_coef,
    const Vector<double>& feedback_coef,
    bool is_filter_stable) {
  return base::AdoptRef(new IIRFilterHandler(
      node, sample_rate, feedforward_coef, feedback_coef, is_filter_stable));
}

IIRFilterHandler::~IIRFilterHandler() {
  // Safe to call the uninitialize() because it's final.
  Uninitialize();
}

// Get the magnitude and phase response of the filter at the given set of
// frequencies (in Hz). The phase response is in radians.
void IIRFilterHandler::GetFrequencyResponse(
    base::span<const float> frequency_hz,
    base::span<float> mag_response,
    base::span<float> phase_response) const {
  DCHECK(!frequency_hz.empty());
  DCHECK(!mag_response.empty());
  DCHECK(!phase_response.empty());

  Vector<float> frequency(frequency_hz.size());

  // Convert from frequency in Hz to normalized frequency (0 -> 1),
  // with 1 equal to the Nyquist frequency.
  for (size_t k = 0; k < frequency_hz.size(); ++k) {
    UNSAFE_TODO(frequency[k] = frequency_hz[k] / nyquist_frequency_);
  }

  response_kernel_->GetFrequencyResponse(frequency, mag_response,
                                         phase_response);
}

IIRFilterHandler::IIRFilterHandler(AudioNode& node,
                                   float sample_rate,
                                   const Vector<double>& feedforward_coef,
                                   const Vector<double>& feedback_coef,
                                   bool is_filter_stable)
    : AudioHandler(NodeType::kNodeTypeIIRFilter, node, sample_rate),
      nyquist_frequency_(0.5 * sample_rate) {
  CHECK(Context());
  CHECK(Context()->GetExecutionContext());

  AddInput();
  AddOutput(kDefaultNumberOfOutputChannels);
  task_runner_ = Context()->GetExecutionContext()->GetTaskRunner(
      TaskType::kMediaElementEvent);

  const unsigned feedback_length = feedback_coef.size();
  const unsigned feedforward_length = feedforward_coef.size();
  CHECK_GT(feedback_length, 0u);
  CHECK_GT(feedforward_length, 0u);

  feedforward_.Allocate(feedforward_length);
  feedback_.Allocate(feedback_length);
  feedforward_.CopyToRange(feedforward_coef.data(), 0, feedforward_length);
  feedback_.CopyToRange(feedback_coef.data(), 0, feedback_length);

  // Need to scale the feedback and feedforward coefficients appropriately.
  // (It's up to the caller to ensure feedbackCoef[0] is not 0.)
  CHECK_NE(feedback_coef[0], 0);

  if (feedback_coef[0] != 1) {
    // The provided filter is:
    //
    //   a[0]*y(n) + a[1]*y(n-1) + ... = b[0]*x(n) + b[1]*x(n-1) + ...
    //
    // We want the leading coefficient of y(n) to be 1:
    //
    //   y(n) + a[1]/a[0]*y(n-1) + ... = b[0]/a[0]*x(n) + b[1]/a[0]*x(n-1) +
    //   ...
    //
    // Thus, the feedback and feedforward coefficients need to be scaled by
    // 1/a[0].
    const float scale = feedback_coef[0];
    for (unsigned k = 1; k < feedback_length; ++k) {
      feedback_[k] /= scale;
    }

    for (unsigned k = 0; k < feedforward_length; ++k) {
      feedforward_[k] /= scale;
    }

    // The IIRFilter checks to make sure this coefficient is 1, so make it so.
    feedback_[0] = 1;
  }

  response_kernel_ = std::make_unique<IIRFilter>(&feedforward_, &feedback_);
  tail_time_ = response_kernel_->TailTime(sample_rate, is_filter_stable,
                                          node.context()->renderQuantumSize());
}

void IIRFilterHandler::Process(uint32_t frames_to_process) {
  AudioBus* destination_bus = Output(0).Bus();

  if (!IsInitialized()) {
    destination_bus->Zero();
  } else {
    scoped_refptr<AudioBus> source_bus = Input(0).Bus();

    // TODO(crbug.com/396149720): if we take "tail time" into account, then we
    // can avoid calling process once the tail dies down.
    if (!Input(0).IsConnected()) {
      source_bus->Zero();
    }

    base::AutoTryLock try_locker(process_lock_);
    if (try_locker.is_acquired()) {
      DCHECK_EQ(source_bus->NumberOfChannels(),
                destination_bus->NumberOfChannels());
      DCHECK_EQ(source_bus->NumberOfChannels(), kernels_.size());

      for (unsigned i = 0; i < kernels_.size(); ++i) {
        kernels_[i]->Process(source_bus->Channel(i)->Data(),
                             destination_bus->Channel(i)->MutableData(),
                             frames_to_process);
      }
    } else {
      // Unfortunately, the kernel is being processed by another thread.
      // See also ConvolverNode::process().
      destination_bus->Zero();
    }
  }

  if (!did_warn_bad_filter_state_) {
    // Inform the user once if the output has a non-finite value.  This is a
    // proxy for the filter state containing non-finite values since the output
    // is also saved as part of the state of the filter.
    if (HasNonFiniteOutput()) {
      did_warn_bad_filter_state_ = true;

      PostCrossThreadTask(*task_runner_, FROM_HERE,
                          CrossThreadBindOnce(&IIRFilterHandler::NotifyBadState,
                                              weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void IIRFilterHandler::Initialize() {
  if (IsInitialized()) {
    return;
  }

  {
    base::AutoLock locker(process_lock_);
    DCHECK(!kernels_.size());

    // Create processing kernels, one per channel.
    for (unsigned i = 0; i < Output(0).NumberOfChannels(); ++i) {
      kernels_.push_back(
          std::make_unique<IIRFilter>(&feedforward_, &feedback_));
    }
  }

  AudioHandler::Initialize();
}

void IIRFilterHandler::Uninitialize() {
  if (!IsInitialized()) {
    return;
  }

  {
    base::AutoLock locker(process_lock_);
    kernels_.clear();
  }

  AudioHandler::Uninitialize();
}

void IIRFilterHandler::CheckNumberOfChannelsForInput(AudioNodeInput* input) {
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

    // Re-initialize the processor with the new channel count.
    Initialize();
  }

  AudioHandler::CheckNumberOfChannelsForInput(input);
}

bool IIRFilterHandler::RequiresTailProcessing() const {
  return true;
}

double IIRFilterHandler::TailTime() const {
  return tail_time_;
}

double IIRFilterHandler::LatencyTime() const {
  return 0;
}

void IIRFilterHandler::PullInputs(uint32_t frames_to_process) {
  // Render directly into output bus for in-place processing
  Input(0).Pull(Output(0).Bus(), frames_to_process);
}

bool IIRFilterHandler::HasNonFiniteOutput() const {
  AudioBus* output_bus = Output(0).Bus();

  for (wtf_size_t k = 0; k < output_bus->NumberOfChannels(); ++k) {
    AudioChannel* channel = output_bus->Channel(k);
    if (channel->length() > 0 && !std::isfinite(channel->Data()[0])) {
      return true;
    }
  }

  return false;
}

void IIRFilterHandler::NotifyBadState() const {
  DCHECK(IsMainThread());
  if (!Context() || !Context()->GetExecutionContext()) {
    return;
  }

  Context()->GetExecutionContext()->AddConsoleMessage(
      MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kJavaScript,
          mojom::blink::ConsoleMessageLevel::kWarning,
          StrCat({NodeTypeName(),
                  ": state is bad, probably due to unstable filter."})));
}

}  // namespace blink
