// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_playback_stats.h"

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

AudioPlaybackStats::AudioPlaybackStats(AudioContext* context)
    : context_(context) {}

double AudioPlaybackStats::underrunDuration(ScriptState* script_state) {
  MaybeUpdateStats(script_state);
  return stats_.glitch_frames_duration().InSecondsF();
}

uint32_t AudioPlaybackStats::underrunEvents(ScriptState* script_state) {
  MaybeUpdateStats(script_state);
  return base::saturated_cast<uint32_t>(stats_.glitch_event_count());
}

double AudioPlaybackStats::totalDuration(ScriptState* script_state) {
  MaybeUpdateStats(script_state);
  return (stats_.glitch_frames_duration() + stats_.observed_frames_duration())
      .InSecondsF();
}

double AudioPlaybackStats::averageLatency(ScriptState* script_state) {
  MaybeUpdateStats(script_state);
  return stats_.average_latency().InSecondsF();
}

double AudioPlaybackStats::minimumLatency(ScriptState* script_state) {
  MaybeUpdateStats(script_state);
  return stats_.min_latency().InSecondsF();
}

double AudioPlaybackStats::maximumLatency(ScriptState* script_state) {
  MaybeUpdateStats(script_state);
  return stats_.max_latency().InSecondsF();
}

void AudioPlaybackStats::resetLatency(ScriptState* script_state) {
  MaybeUpdateStats(script_state);
  // Reset the latency stats correctly by having a temporary stats object absorb
  // them.
  AudioFrameStatsAccumulator temp_stats;
  temp_stats.Absorb(stats_);
}

ScriptObject AudioPlaybackStats::toJSON(ScriptState* script_state) {
  V8ObjectBuilder result(script_state);
  result.AddNumber("underrunDuration", underrunDuration(script_state));
  result.AddNumber("underrunEvents", underrunEvents(script_state));
  result.AddNumber("totalDuration", totalDuration(script_state));
  result.AddNumber("averageLatency", averageLatency(script_state));
  result.AddNumber("minimumLatency", minimumLatency(script_state));
  result.AddNumber("maximumLatency", maximumLatency(script_state));
  return result.ToScriptObject();
}

void AudioPlaybackStats::Trace(Visitor* visitor) const {
  visitor->Trace(context_);
  ScriptWrappable::Trace(visitor);
}

void AudioPlaybackStats::MaybeUpdateStats(ScriptState* script_state) {
  // We cache the stats in |stats_| in order to preserve the JavaScript
  // run-to-completion semantics. If the cached stats were updated in the
  // current task, we should not update them again.
  if (!context_ || is_stats_from_current_task_) {
    return;
  }
  // Get the latest stats, and remember that we now have stats from the current
  // task.
  context_->TransferAudioFrameStatsTo(stats_);
  is_stats_from_current_task_ = true;

  // Queue a microtask to let us know when we are on a new task again, ensuring
  // that we get fresh stats in the next task execution cycle.
  ToEventLoop(script_state)
      .EnqueueMicrotask(
          BindOnce(&AudioPlaybackStats::OnMicrotask, WrapWeakPersistent(this)));
}

void AudioPlaybackStats::OnMicrotask() {
  // Since this was queued on the older task when we got the current |stats_|,
  // the stats are no longer from the current task.
  is_stats_from_current_task_ = false;
}

}  // namespace blink
