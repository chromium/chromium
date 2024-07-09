// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webaudio/constant_source_handler.h"

#include <tuple>

#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"

namespace blink {

namespace {

// A ConstantSource is always mono.
constexpr unsigned kNumberOfOutputChannels = 1;

}  // namespace

ConstantSourceHandler::ConstantSourceHandler(AudioNode& node,
                                             float sample_rate,
                                             AudioParamHandler& offset)
    : AudioScheduledSourceHandler(kNodeTypeConstantSource, node, sample_rate),
      offset_(&offset),
      sample_accurate_values_(GetDeferredTaskHandler().RenderQuantumFrames()) {
  AddOutput(kNumberOfOutputChannels);

  Initialize();
}

scoped_refptr<ConstantSourceHandler> ConstantSourceHandler::Create(
    AudioNode& node,
    float sample_rate,
    AudioParamHandler& offset) {
  return base::AdoptRef(new ConstantSourceHandler(node, sample_rate, offset));
}

ConstantSourceHandler::~ConstantSourceHandler() {
  Uninitialize();
}

void ConstantSourceHandler::Process(uint32_t frames_to_process) {
  AudioBus* output_bus = Output(0).Bus();
  DCHECK(output_bus);

  if (!IsInitialized() || !output_bus->NumberOfChannels()) {
    output_bus->Zero();
    return;
  }

  // The audio thread can't block on this lock, so we call tryLock() instead.
  base::AutoTryLock try_locker(process_lock_);
  if (!try_locker.is_acquired()) {
    // Too bad - the tryLock() failed.
    output_bus->Zero();
    return;
  }

  size_t quantum_frame_offset;
  size_t non_silent_frames_to_process;
  double start_frame_offset;

  // Figure out where in the current rendering quantum that the source is
  // active and for how many frames.
  std::tie(quantum_frame_offset, non_silent_frames_to_process,
           start_frame_offset) =
      UpdateSchedulingInfo(frames_to_process, output_bus);

  if (!non_silent_frames_to_process) {
    output_bus->Zero();
    return;
  }

  bool is_sample_accurate = offset_->HasSampleAccurateValues();

  if (is_sample_accurate && offset_->IsAudioRate()) {
    DCHECK_LE(frames_to_process, sample_accurate_values_.size());
    float* offsets = sample_accurate_values_.Data();
    offset_->CalculateSampleAccurateValues(offsets, frames_to_process);
    if (non_silent_frames_to_process > 0) {
      memcpy(output_bus->Channel(0)->MutableData() + quantum_frame_offset,
             offsets + quantum_frame_offset,
             non_silent_frames_to_process * sizeof(*offsets));
      output_bus->ClearSilentFlag();
    } else {
      output_bus->Zero();
    }

    return;
  }

  float value = is_sample_accurate ? offset_->FinalValue() : offset_->Value();
  if (value == 0) {
    output_bus->Zero();
  } else {
    float* dest = output_bus->Channel(0)->MutableData();
    dest += quantum_frame_offset;
    for (unsigned k = 0; k < non_silent_frames_to_process; ++k) {
      dest[k] = value;
    }
    output_bus->ClearSilentFlag();
  }
}

bool ConstantSourceHandler::PropagatesSilence() const {
  return !IsPlayingOrScheduled() || HasFinished();
}

void ConstantSourceHandler::HandleStoppableSourceNode() {
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

base::WeakPtr<AudioScheduledSourceHandler> ConstantSourceHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace blink
