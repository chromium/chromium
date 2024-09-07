// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_playout_stats.h"

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

AudioPlayoutStats::AudioPlayoutStats(AudioContext* context)
    : context_(context) {}

DOMHighResTimeStamp AudioPlayoutStats::fallbackFramesDuration(
    ScriptState* script_state) {
  MaybeUpdateStats(script_state);
  return stats_.glitch_frames_duration().InMillisecondsF();
}

uint32_t AudioPlayoutStats::fallbackFramesEvents(ScriptState* script_state) {
  MaybeUpdateStats(script_state);
  return base::saturated_cast<uint32_t>(stats_.glitch_event_count());
}

DOMHighResTimeStamp AudioPlayoutStats::totalFramesDuration(
    ScriptState* script_state) {
  MaybeUpdateStats(script_state);
  return (stats_.glitch_frames_duration() + stats_.observed_frames_duration())
      .InMillisecondsF();
}

DOMHighResTimeStamp AudioPlayoutStats::averageLatency(
    ScriptState* script_state) {
  MaybeUpdateStats(script_state);
  return stats_.average_latency().InMillisecondsF();
}

DOMHighResTimeStamp AudioPlayoutStats::minimumLatency(
    ScriptState* script_state) {
  MaybeUpdateStats(script_state);
  return stats_.min_latency().InMillisecondsF();
}

DOMHighResTimeStamp AudioPlayoutStats::maximumLatency(
    ScriptState* script_state) {
  MaybeUpdateStats(script_state);
  return stats_.max_latency().InMillisecondsF();
}

void AudioPlayoutStats::resetLatency(ScriptState* script_state) {
  MaybeUpdateStats(script_state);
  // Reset the latency stats correctly by having a temporary stats object absorb
  // them.
  AudioFrameStatsAccumulator temp_stats;
  temp_stats.Absorb(stats_);
}

ScriptValue AudioPlayoutStats::toJSON(ScriptState* script_state) {
  V8ObjectBuilder result(script_state);
  result.AddNumber("fallbackFramesDuration",
                   fallbackFramesDuration(script_state));
  result.AddNumber("fallbackFramesEvents", fallbackFramesEvents(script_state));
  result.AddNumber("totalFramesDuration", totalFramesDuration(script_state));
  result.AddNumber("averageLatency", averageLatency(script_state));
  result.AddNumber("minimumLatency", minimumLatency(script_state));
  result.AddNumber("maximumLatency", maximumLatency(script_state));
  return result.GetScriptValue();
}

void AudioPlayoutStats::Trace(Visitor* visitor) const {
  visitor->Trace(context_);
  ScriptWrappable::Trace(visitor);
}

void AudioPlayoutStats::MaybeUpdateStats(ScriptState* script_state) {
  // We cache the stats in |stats_| in order to preserve the JavaScript
  // run-to-completion semantics. If the cached stats were updated in the
  // current task, we should not update them again.
  if (!context_ || stats_are_from_current_task_) {
    return;
  }
  // Get the latest stats, and remember that we now have stats from the current
  // task.
  context_->TransferAudioFrameStatsTo(stats_);
  stats_are_from_current_task_ = true;

  // Queue a microtask to let us know when we are on a new task again, ensuring
  // that we get fresh stats in the next task execution cycle.
  ToEventLoop(script_state)
      .EnqueueMicrotask(WTF::BindOnce(&AudioPlayoutStats::OnMicrotask,
                                      WrapWeakPersistent(this)));
}

void AudioPlayoutStats::OnMicrotask() {
  // Since this was queued on the older task when we got the current |stats_|,
  // the stats are no longer from the current task.
  stats_are_from_current_task_ = false;
}

}  // namespace blink
