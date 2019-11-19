// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/constant_source_node.h"

#include <algorithm>

#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/constant_source_options.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

ConstantSourceHandler::ConstantSourceHandler(AudioNode& node,
                                             float sample_rate,
                                             AudioParamHandler& offset)
    : AudioScheduledSourceHandler(kNodeTypeConstantSource, node, sample_rate),
      offset_(&offset),
      sample_accurate_values_(audio_utilities::kRenderQuantumFrames) {
  // A ConstantSource is always mono.
  AddOutput(1);

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
  MutexTryLocker try_locker(process_lock_);
  if (!try_locker.Locked()) {
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

  if (offset_->HasSampleAccurateValues()) {
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
  } else {
    float value = offset_->Value();

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
}

bool ConstantSourceHandler::PropagatesSilence() const {
  return !IsPlayingOrScheduled() || HasFinished();
}

void ConstantSourceHandler::HandleStoppableSourceNode() {
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
ConstantSourceNode::ConstantSourceNode(BaseAudioContext& context)
    : AudioScheduledSourceNode(context),
      offset_(AudioParam::Create(
          context,
          Uuid(),
          AudioParamHandler::kParamTypeConstantSourceOffset,
          1,
          AudioParamHandler::AutomationRate::kAudio,
          AudioParamHandler::AutomationRateMode::kVariable)) {
  SetHandler(ConstantSourceHandler::Create(*this, context.sampleRate(),
                                           offset_->Handler()));
}

ConstantSourceNode* ConstantSourceNode::Create(
    BaseAudioContext& context,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return MakeGarbageCollected<ConstantSourceNode>(context);
}

ConstantSourceNode* ConstantSourceNode::Create(
    BaseAudioContext* context,
    const ConstantSourceOptions* options,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  ConstantSourceNode* node = Create(*context, exception_state);

  if (!node)
    return nullptr;

  node->offset()->setValue(options->offset());

  return node;
}

void ConstantSourceNode::Trace(blink::Visitor* visitor) {
  visitor->Trace(offset_);
  AudioScheduledSourceNode::Trace(visitor);
}

ConstantSourceHandler& ConstantSourceNode::GetConstantSourceHandler() const {
  return static_cast<ConstantSourceHandler&>(Handler());
}

AudioParam* ConstantSourceNode::offset() {
  return offset_;
}

void ConstantSourceNode::ReportDidCreate() {
  GraphTracer().DidCreateAudioNode(this);
  GraphTracer().DidCreateAudioParam(offset_);
}

void ConstantSourceNode::ReportWillBeDestroyed() {
  GraphTracer().WillDestroyAudioParam(offset_);
  GraphTracer().WillDestroyAudioNode(this);
}

}  // namespace blink
