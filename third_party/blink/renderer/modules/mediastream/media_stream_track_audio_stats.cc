// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_track_audio_stats.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track_impl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

MediaStreamTrackAudioStats::MediaStreamTrackAudioStats(
    MediaStreamTrackImpl* track)
    : track_(track) {}

uint64_t MediaStreamTrackAudioStats::deliveredFrames(
    ScriptState* script_state) {
  MaybeUpdateStats(script_state);
  return stats_.DeliveredFrames();
}

DOMHighResTimeStamp MediaStreamTrackAudioStats::deliveredFramesDuration(
    ScriptState* script_state) {
  MaybeUpdateStats(script_state);
  return stats_.DeliveredFramesDuration().InMillisecondsF();
}

uint64_t MediaStreamTrackAudioStats::totalFrames(ScriptState* script_state) {
  MaybeUpdateStats(script_state);
  return stats_.TotalFrames();
}

DOMHighResTimeStamp MediaStreamTrackAudioStats::totalFramesDuration(
    ScriptState* script_state) {
  MaybeUpdateStats(script_state);
  return stats_.TotalFramesDuration().InMillisecondsF();
}

DOMHighResTimeStamp MediaStreamTrackAudioStats::latency(
    ScriptState* script_state) {
  MaybeUpdateStats(script_state);
  return stats_.Latency().InMillisecondsF();
}

DOMHighResTimeStamp MediaStreamTrackAudioStats::averageLatency(
    ScriptState* script_state) {
  MaybeUpdateStats(script_state);
  return stats_.AverageLatency().InMillisecondsF();
}

DOMHighResTimeStamp MediaStreamTrackAudioStats::minimumLatency(
    ScriptState* script_state) {
  MaybeUpdateStats(script_state);
  return stats_.MinimumLatency().InMillisecondsF();
}

DOMHighResTimeStamp MediaStreamTrackAudioStats::maximumLatency(
    ScriptState* script_state) {
  MaybeUpdateStats(script_state);
  return stats_.MaximumLatency().InMillisecondsF();
}

void MediaStreamTrackAudioStats::resetLatency(ScriptState* script_state) {
  MaybeUpdateStats(script_state);
  // Reset the latency stats correctly by having a temporary stats object absorb
  // them.
  MediaStreamTrackPlatform::AudioFrameStats temp_stats;
  temp_stats.Absorb(stats_);
}

ScriptValue MediaStreamTrackAudioStats::toJSON(ScriptState* script_state) {
  V8ObjectBuilder result(script_state);
  result.AddNumber("deliveredFrames", deliveredFrames(script_state));
  result.AddNumber("deliveredFramesDuration",
                   deliveredFramesDuration(script_state));
  result.AddNumber("totalFrames", totalFrames(script_state));
  result.AddNumber("totalFramesDuration", totalFramesDuration(script_state));
  result.AddNumber("latency", latency(script_state));
  result.AddNumber("averageLatency", averageLatency(script_state));
  result.AddNumber("minimumLatency", minimumLatency(script_state));
  result.AddNumber("maximumLatency", maximumLatency(script_state));
  return result.GetScriptValue();
}

void MediaStreamTrackAudioStats::Trace(Visitor* visitor) const {
  visitor->Trace(track_);
  ScriptWrappable::Trace(visitor);
}

void MediaStreamTrackAudioStats::MaybeUpdateStats(ScriptState* script_state) {
  // We cache the stats in |stats_| in order to preserve the JavaScript
  // run-to-completion semantics. If the cached stats were updated in the
  // current task, we should not update them again.
  if (!track_ || stats_are_from_current_task_) {
    return;
  }
  // Get the latest stats, and remember that we now have stats from the current
  // task.
  track_->TransferAudioFrameStatsTo(stats_);
  stats_are_from_current_task_ = true;

  // Queue a microtask to let us know when we are on a new task again, ensuring
  // that we get fresh stats in the next task execution cycle.
  ToEventLoop(script_state)
      .EnqueueMicrotask(WTF::BindOnce(&MediaStreamTrackAudioStats::OnMicrotask,
                                      WrapWeakPersistent(this)));
}

void MediaStreamTrackAudioStats::OnMicrotask() {
  // Since this was queued on the older task when we got the current |stats_|,
  // the stats are no longer from the current task.
  stats_are_from_current_task_ = false;
}

}  // namespace blink
