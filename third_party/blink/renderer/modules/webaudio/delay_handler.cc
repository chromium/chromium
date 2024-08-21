// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/delay_handler.h"

#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/delay.h"

namespace blink {

namespace {

constexpr unsigned kNumberOfOutputs = 1;
constexpr unsigned kDefaultNumberOfChannels = 1;

}  // namespace

scoped_refptr<DelayHandler> DelayHandler::Create(AudioNode& node,
                                                 float sample_rate,
                                                 AudioParamHandler& delay_time,
                                                 double max_delay_time) {
  return base::AdoptRef(
      new DelayHandler(node, sample_rate, delay_time, max_delay_time));
}

DelayHandler::~DelayHandler() {
  Uninitialize();
}

DelayHandler::DelayHandler(AudioNode& node,
                           float sample_rate,
                           AudioParamHandler& delay_time,
                           double max_delay_time)
    : AudioHandler(kNodeTypeDelay, node, sample_rate),
      number_of_channels_(kDefaultNumberOfChannels),
      sample_rate_(sample_rate),
      render_quantum_frames_(
          node.context()->GetDeferredTaskHandler().RenderQuantumFrames()),
      delay_time_(&delay_time),
      max_delay_time_(max_delay_time) {
  AddInput();
  AddOutput(kNumberOfOutputs);
  Initialize();
}

void DelayHandler::Process(uint32_t frames_to_process) {
  AudioBus* destination_bus = Output(0).Bus();

  if (!IsInitialized() || number_of_channels_ != Output(0).NumberOfChannels()) {
    destination_bus->Zero();
  } else {
    scoped_refptr<AudioBus> source_bus = Input(0).Bus();

    if (!Input(0).IsConnected()) {
      source_bus->Zero();
    }

    base::AutoTryLock process_try_locker(process_lock_);
    base::AutoTryLock rate_try_locker(delay_time_->RateLock());
    if (process_try_locker.is_acquired() && rate_try_locker.is_acquired()) {
      DCHECK_EQ(source_bus->NumberOfChannels(),
                destination_bus->NumberOfChannels());
      DCHECK_EQ(source_bus->NumberOfChannels(), kernels_.size());

      if (delay_time_->IsAudioRate()) {
        for (unsigned i = 0; i < kernels_.size(); ++i) {
          // Assumes that the automation rate cannot change in the middle of
          // the process function. (See crbug.com/357391257)
          CHECK(delay_time_->IsAudioRate());
          delay_time_->CalculateSampleAccurateValues(kernels_[i]->DelayTimes(),
                                                     frames_to_process);
          kernels_[i]->ProcessARate(source_bus->Channel(i)->Data(),
                                    destination_bus->Channel(i)->MutableData(),
                                    frames_to_process);
        }
      } else {
        for (unsigned i = 0; i < kernels_.size(); ++i) {
          CHECK(!delay_time_->IsAudioRate());
          kernels_[i]->SetDelayTime(delay_time_->FinalValue());
          kernels_[i]->ProcessKRate(source_bus->Channel(i)->Data(),
                                    destination_bus->Channel(i)->MutableData(),
                                    frames_to_process);
        }
      }
    } else {
      destination_bus->Zero();
    }
  }
}

void DelayHandler::ProcessOnlyAudioParams(uint32_t frames_to_process) {
  if (!IsInitialized()) {
    return;
  }
  // TODO(crbug.com/40637820): Eventually, the render quantum size will no
  // longer be hardcoded as 128. At that point, we'll need to switch from
  // stack allocation to heap allocation.
  constexpr unsigned render_quantum_frames_expected = 128;
  CHECK_EQ(render_quantum_frames_, render_quantum_frames_expected);
  DCHECK_LE(frames_to_process, render_quantum_frames_expected);
  float values[render_quantum_frames_expected];
  delay_time_->CalculateSampleAccurateValues(values, frames_to_process);
}

void DelayHandler::Initialize() {
  if (IsInitialized()) {
    return;
  }

  {
    base::AutoLock locker(process_lock_);
    DCHECK(!kernels_.size());

    // Create processing kernels, one per channel.
    for (unsigned i = 0; i < number_of_channels_; ++i) {
      kernels_.push_back(std::make_unique<Delay>(max_delay_time_, sample_rate_,
                                                 render_quantum_frames_));
    }
  }

  AudioHandler::Initialize();
}

void DelayHandler::Uninitialize() {
  if (!IsInitialized()) {
    return;
  }

  {
    base::AutoLock locker(process_lock_);
    kernels_.clear();
  }

  AudioHandler::Uninitialize();
}

void DelayHandler::CheckNumberOfChannelsForInput(AudioNodeInput* input) {
  DCHECK(Context()->IsAudioThread());
  Context()->AssertGraphOwner();
  DCHECK_EQ(input, &Input(0));

  // As soon as we know the channel count of our input, we can lazily
  // initialize.  Sometimes this may be called more than once with different
  // channel counts, in which case we must safely uninitialize and then
  // re-initialize with the new channel count.
  const unsigned number_of_channels = input->NumberOfChannels();

  if (IsInitialized() && number_of_channels != Output(0).NumberOfChannels()) {
    // We're already initialized but the channel count has changed.
    Uninitialize();
  }

  if (!IsInitialized()) {
    // This will propagate the channel count to any nodes connected further down
    // the chain...
    Output(0).SetNumberOfChannels(number_of_channels);

    // Re-initialize the processor with the new channel count.
    number_of_channels_ = number_of_channels;

    Initialize();
  }

  AudioHandler::CheckNumberOfChannelsForInput(input);
}

bool DelayHandler::RequiresTailProcessing() const {
  // Always return true even if the tail time and latency might both be
  // zero. This is for simplicity; most interesting delay nodes have non-zero
  // delay times anyway.  And it's ok to return true. It just means the node
  // lives a little longer than strictly necessary.
  return true;
}

double DelayHandler::TailTime() const {
  // Account for worst case delay.
  // Don't try to track actual delay time which can change dynamically.
  return max_delay_time_;
}

double DelayHandler::LatencyTime() const {
  // A "delay" effect is expected to delay the signal, and this is not
  // considered latency.
  return 0;
}

void DelayHandler::PullInputs(uint32_t frames_to_process) {
  // Render directly into output bus for in-place processing
  Input(0).Pull(Output(0).Bus(), frames_to_process);
}

}  // namespace blink
