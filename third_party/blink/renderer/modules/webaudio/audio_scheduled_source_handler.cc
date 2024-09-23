// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webaudio/audio_scheduled_source_handler.h"

#include <algorithm>

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

AudioScheduledSourceHandler::AudioScheduledSourceHandler(NodeType node_type,
                                                         AudioNode& node,
                                                         float sample_rate)
    : AudioHandler(node_type, node, sample_rate),
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
  DCHECK_EQ(
      quantum_frame_size,
      static_cast<size_t>(GetDeferredTaskHandler().RenderQuantumFrames()));

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
  if (end_time_ != kUnknownTime && end_frame <= quantum_start_frame) {
    Finish();
  }

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
    // the rounding that may happen in computing `start_frame` above.
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
    for (unsigned i = 0; i < output_bus->NumberOfChannels(); ++i) {
      memset(output_bus->Channel(i)->MutableData(), 0,
             sizeof(float) * quantum_frame_offset);
    }
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
      if (frames_to_zero > non_silent_frames_to_process) {
        non_silent_frames_to_process = 0;
      } else {
        non_silent_frames_to_process -= frames_to_zero;
      }

      for (unsigned i = 0; i < output_bus->NumberOfChannels(); ++i) {
        memset(output_bus->Channel(i)->MutableData() + zero_start_frame, 0,
               sizeof(float) * frames_to_zero);
      }
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

  SetOnEndedNotificationPending();

  // This synchronizes with process(). updateSchedulingInfo will read some of
  // the variables being set here.
  base::AutoLock process_locker(process_lock_);

  // If `when` < `currentTime()`, the source must start now according to the
  // spec. So just set `start_time_` to `currentTime()` in this case to start
  // the source now.
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
  base::AutoLock process_locker(process_lock_);

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
                          AsWeakPtr()));
}

void AudioScheduledSourceHandler::NotifyEnded() {
  // NotifyEnded is always called when the node is finished, even if
  // there are no event listeners.  We always dispatch the event and
  // let DispatchEvent take are of sending the event to the right
  // place,
  DCHECK(IsMainThread());

  if (GetNode()) {
    DispatchEventResult result =
        GetNode()->DispatchEvent(*Event::Create(event_type_names::kEnded));
    if (result == DispatchEventResult::kCanceledBeforeDispatch) {
      return;
    }
  }
  on_ended_notification_pending_ = false;
}

}  // namespace blink
