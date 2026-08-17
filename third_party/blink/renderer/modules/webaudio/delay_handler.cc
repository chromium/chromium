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
    : AudioHandler(NodeType::kNodeTypeDelay, node, sample_rate),
      number_of_channels_(kDefaultNumberOfChannels),
      sample_rate_(sample_rate),
      render_quantum_frames_(node.context()->renderQuantumSize()),
      delay_time_(&delay_time),
      max_delay_time_(max_delay_time),
      delay_time_sample_accurate_values_(render_quantum_frames_) {
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
          delay_time_->CalculateSampleAccurateValues(
              kernels_[i]->DelayTimes().first(frames_to_process));
          kernels_[i]->ProcessARate(source_bus->Channel(i)->Span(),
                                    destination_bus->Channel(i)->MutableSpan(),
                                    frames_to_process);
        }
      } else {
        for (unsigned i = 0; i < kernels_.size(); ++i) {
          CHECK(!delay_time_->IsAudioRate());
          kernels_[i]->SetDelayTime(delay_time_->FinalValue());
          kernels_[i]->ProcessKRate(source_bus->Channel(i)->Span(),
                                    destination_bus->Channel(i)->MutableSpan(),
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

  DCHECK_LE(frames_to_process, render_quantum_frames_);

  delay_time_->CalculateSampleAccurateValues(
      delay_time_sample_accurate_values_.as_span().first(frames_to_process));
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

  const unsigned number_of_channels = input->NumberOfChannels();

  if (number_of_channels != Output(0).NumberOfChannels()) {
    {
      base::AutoLock locker(process_lock_);

      const unsigned current_number_of_channels = kernels_.size();
      CHECK_GT(current_number_of_channels, 0u);
      CHECK_GT(number_of_channels, 0u);

      if (current_number_of_channels != number_of_channels) {
        const uint32_t buffer_length =
            base::checked_cast<uint32_t>(kernels_[0]->BufferSpan().size());
        const size_t write_index = kernels_[0]->WriteIndex();

        // Wrap the existing delay kernel buffers in a non-allocating AudioBus.
        scoped_refptr<AudioBus> old_bus =
            AudioBus::Create(current_number_of_channels, buffer_length, false);
        for (unsigned i = 0; i < current_number_of_channels; ++i) {
          old_bus->SetChannelMemory(i, kernels_[i]->BufferSpan());
        }

        // Allocate new Delay kernels for the destination channel layout.
        Vector<std::unique_ptr<Delay>> new_kernels;
        new_kernels.reserve(number_of_channels);
        for (unsigned i = 0; i < number_of_channels; ++i) {
          auto kernel = std::make_unique<Delay>(
              max_delay_time_, sample_rate_, render_quantum_frames_);
          kernel->SetWriteIndex(write_index);
          new_kernels.push_back(std::move(kernel));
        }

        // Wrap the new delay kernel buffers in a non-allocating AudioBus.
        scoped_refptr<AudioBus> new_bus =
            AudioBus::Create(number_of_channels, buffer_length, false);
        for (unsigned i = 0; i < number_of_channels; ++i) {
          new_bus->SetChannelMemory(i, new_kernels[i]->BufferSpan());
        }

        // Perform standard Web Audio up/down-mixing directly between kernel
        // buffers.
        new_bus->CopyFrom(*old_bus, InternalChannelInterpretation());

        // Clear non-allocating channel memory pointers before releasing buses.
        for (unsigned i = 0; i < current_number_of_channels; ++i) {
          old_bus->SetChannelMemory(i, base::span<float>());
        }
        for (unsigned i = 0; i < number_of_channels; ++i) {
          new_bus->SetChannelMemory(i, base::span<float>());
        }

        kernels_ = std::move(new_kernels);
      }
      number_of_channels_ = number_of_channels;
    }
    Output(0).SetNumberOfChannels(number_of_channels);
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

Vector<Delay*> DelayHandler::KernelsForTesting() const {
  base::AutoLock locker(process_lock_);
  Vector<Delay*> result;
  for (const auto& kernel : kernels_) {
    result.push_back(kernel.get());
  }
  return result;
}

}  // namespace blink
