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

#include "third_party/blink/renderer/modules/webaudio/audio_scheduled_source_node.h"

#include <algorithm>

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

const double AudioScheduledSourceHandler::kUnknownTime = -1;

AudioScheduledSourceHandler::AudioScheduledSourceHandler(NodeType node_type,
                                                         AudioNode& node,
                                                         float sample_rate)
    : AudioHandler(node_type, node, sample_rate),
      start_time_(0),
      end_time_(kUnknownTime),
      playback_state_(UNSCHEDULED_STATE) {
  if (Context()->GetExecutionContext()) {
    task_runner_ = Context()->GetExecutionContext()->GetTaskRunner(
        TaskType::kMediaElementEvent);
  }
}

std::tuple<size_t, size_t, double>
AudioScheduledSourceHandler::UpdateSchedulingInfo(size_t quantum_frame_size,
                                                  AudioBus* output_bus) {
  // Set up default values for the three return values.
  size_t quantum_frame_offset = 0;
  size_t non_silent_frames_to_process = 0;
  double start_frame_offset = 0;

  DCHECK(output_bus);
  DCHECK_EQ(quantum_frame_size,
            static_cast<size_t>(audio_utilities::kRenderQuantumFrames));

  double sample_rate = Context()->sampleRate();

  // quantumStartFrame     : Start frame of the current time quantum.
  // quantumEndFrame       : End frame of the current time quantum.
  // startFrame            : Start frame for this source.
  // endFrame              : End frame for this source.
  size_t quantum_start_frame = Context()->CurrentSampleFrame();
  size_t quantum_end_frame = quantum_start_frame + quantum_frame_size;

  // Round up if the start_time isn't on a frame boundary so we don't start too
  // early.
  size_t start_frame = audio_utilities::TimeToSampleFrame(
      start_time_, sample_rate, audio_utilities::kRoundUp);
  size_t end_frame = 0;

  if (end_time_ == kUnknownTime) {
    end_frame = 0;
  } else {
    // The end frame is the end time rounded up because it is an exclusive upper
    // bound of the end time.  We also need to take care to handle huge end
    // times and clamp the corresponding frame to the largest size_t value.
    end_frame = audio_utilities::TimeToSampleFrame(end_time_, sample_rate,
                                                   audio_utilities::kRoundUp);
  }

  // If we know the end time and it's already passed, then don't bother doing
  // any more rendering this cycle.
  if (end_time_ != kUnknownTime && end_frame <= quantum_start_frame)
    Finish();

  PlaybackState state = GetPlaybackState();

  if (state == UNSCHEDULED_STATE || state == FINISHED_STATE ||
      start_frame >= quantum_end_frame) {
    // Output silence.
    output_bus->Zero();
    non_silent_frames_to_process = 0;
    return std::make_tuple(quantum_frame_offset, non_silent_frames_to_process,
                           start_frame_offset);
  }

  // Check if it's time to start playing.
  if (state == SCHEDULED_STATE) {
    // Increment the active source count only if we're transitioning from
    // SCHEDULED_STATE to PLAYING_STATE.
    SetPlaybackState(PLAYING_STATE);
    // Determine the offset of the true start time from the starting frame.
    // NOTE: start_frame_offset is usually negative, but may not be because of
    // the rounding that may happen in computing |start_frame| above.
    start_frame_offset = start_time_ * sample_rate - start_frame;
  } else {
    start_frame_offset = 0;
  }

  quantum_frame_offset =
      start_frame > quantum_start_frame ? start_frame - quantum_start_frame : 0;
  quantum_frame_offset = std::min(quantum_frame_offset,
                                  quantum_frame_size);  // clamp to valid range
  non_silent_frames_to_process = quantum_frame_size - quantum_frame_offset;

  if (!non_silent_frames_to_process) {
    // Output silence.
    output_bus->Zero();
    return std::make_tuple(quantum_frame_offset, non_silent_frames_to_process,
                           start_frame_offset);
  }

  // Handle silence before we start playing.
  // Zero any initial frames representing silence leading up to a rendering
  // start time in the middle of the quantum.
  if (quantum_frame_offset) {
    for (unsigned i = 0; i < output_bus->NumberOfChannels(); ++i)
      memset(output_bus->Channel(i)->MutableData(), 0,
             sizeof(float) * quantum_frame_offset);
  }

  // Handle silence after we're done playing.
  // If the end time is somewhere in the middle of this time quantum, then zero
  // out the frames from the end time to the very end of the quantum.
  if (end_time_ != kUnknownTime && end_frame >= quantum_start_frame &&
      end_frame < quantum_end_frame) {
    size_t zero_start_frame = end_frame - quantum_start_frame;
    size_t frames_to_zero = quantum_frame_size - zero_start_frame;

    DCHECK_LT(zero_start_frame, quantum_frame_size);
    DCHECK_LE(frames_to_zero, quantum_frame_size);
    DCHECK_LE(zero_start_frame + frames_to_zero, quantum_frame_size);

    bool is_safe = zero_start_frame < quantum_frame_size &&
                   frames_to_zero <= quantum_frame_size &&
                   zero_start_frame + frames_to_zero <= quantum_frame_size;
    if (is_safe) {
      if (frames_to_zero > non_silent_frames_to_process)
        non_silent_frames_to_process = 0;
      else
        non_silent_frames_to_process -= frames_to_zero;

      for (unsigned i = 0; i < output_bus->NumberOfChannels(); ++i)
        memset(output_bus->Channel(i)->MutableData() + zero_start_frame, 0,
               sizeof(float) * frames_to_zero);
    }

    Finish();
  }

  return std::make_tuple(quantum_frame_offset, non_silent_frames_to_process,
                         start_frame_offset);
}

void AudioScheduledSourceHandler::Start(double when,
                                        ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  Context()->NotifySourceNodeStart();

  if (GetPlaybackState() != UNSCHEDULED_STATE) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "cannot call start more than once.");
    return;
  }

  if (when < 0) {
    exception_state.ThrowRangeError(
        ExceptionMessages::IndexExceedsMinimumBound("start time", when, 0.0));
    return;
  }

  // The node is started. Add a reference to keep us alive so that audio will
  // eventually get played even if Javascript should drop all references to this
  // node. The reference will get dropped when the source has finished playing.
  Context()->NotifySourceNodeStartedProcessing(GetNode());

  // This synchronizes with process(). updateSchedulingInfo will read some of
  // the variables being set here.
  MutexLocker process_locker(process_lock_);

  // If |when| < currentTime, the source must start now according to the spec.
  // So just set startTime to currentTime in this case to start the source now.
  start_time_ = std::max(when, Context()->currentTime());

  SetPlaybackState(SCHEDULED_STATE);
}

void AudioScheduledSourceHandler::Stop(double when,
                                       ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (GetPlaybackState() == UNSCHEDULED_STATE) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "cannot call stop without calling start first.");
    return;
  }

  if (when < 0) {
    exception_state.ThrowRangeError(
        ExceptionMessages::IndexExceedsMinimumBound("stop time", when, 0.0));
    return;
  }

  // This synchronizes with process()
  MutexLocker process_locker(process_lock_);

  // stop() can be called more than once, with the last call to stop taking
  // effect, unless the source has already stopped due to earlier calls to stop.
  // No exceptions are thrown in any case.
  when = std::max(0.0, when);
  end_time_ = when;
}

void AudioScheduledSourceHandler::FinishWithoutOnEnded() {
  if (GetPlaybackState() != FINISHED_STATE) {
    // Let the context dereference this AudioNode.
    Context()->NotifySourceNodeFinishedProcessing(this);
    SetPlaybackState(FINISHED_STATE);
  }
}

void AudioScheduledSourceHandler::Finish() {
  FinishWithoutOnEnded();

  PostCrossThreadTask(
      *task_runner_, FROM_HERE,
      CrossThreadBindOnce(&AudioScheduledSourceHandler::NotifyEnded,
                          WrapRefCounted(this)));
}

void AudioScheduledSourceHandler::NotifyEnded() {
  DCHECK(IsMainThread());
  if (!Context() || !Context()->GetExecutionContext())
    return;
  if (GetNode())
    GetNode()->DispatchEvent(*Event::Create(event_type_names::kEnded));
}

// ----------------------------------------------------------------

AudioScheduledSourceNode::AudioScheduledSourceNode(BaseAudioContext& context)
    : AudioNode(context) {}

AudioScheduledSourceHandler&
AudioScheduledSourceNode::GetAudioScheduledSourceHandler() const {
  return static_cast<AudioScheduledSourceHandler&>(Handler());
}

void AudioScheduledSourceNode::start(ExceptionState& exception_state) {
  start(0, exception_state);
}

void AudioScheduledSourceNode::start(double when,
                                     ExceptionState& exception_state) {
  GetAudioScheduledSourceHandler().Start(when, exception_state);
}

void AudioScheduledSourceNode::stop(ExceptionState& exception_state) {
  stop(0, exception_state);
}

void AudioScheduledSourceNode::stop(double when,
                                    ExceptionState& exception_state) {
  GetAudioScheduledSourceHandler().Stop(when, exception_state);
}

EventListener* AudioScheduledSourceNode::onended() {
  return GetAttributeEventListener(event_type_names::kEnded);
}

void AudioScheduledSourceNode::setOnended(EventListener* listener) {
  SetAttributeEventListener(event_type_names::kEnded, listener);
}

bool AudioScheduledSourceNode::HasPendingActivity() const {
  // To avoid the leak, a node should be collected regardless of its
  // playback state if the context is closed.
  if (context()->ContextState() == BaseAudioContext::kClosed) {
    return false;
  }

  // If a node is scheduled or playing, do not collect the node prematurely
  // even its reference is out of scope. Then fire onended event if assigned.
  return ContainsHandler() &&
         GetAudioScheduledSourceHandler().IsPlayingOrScheduled();
}

}  // namespace blink
