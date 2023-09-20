// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_track_video_stats.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track_impl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

MediaStreamTrackVideoStats::MediaStreamTrackVideoStats(
    MediaStreamTrackImpl* track)
    : track_(track) {}

uint64_t MediaStreamTrackVideoStats::deliveredFrames(
    ScriptState* script_state) {
  PopulateStatsCache(script_state);
  return stats_.deliverable_frames;
}

uint64_t MediaStreamTrackVideoStats::discardedFrames(
    ScriptState* script_state) {
  PopulateStatsCache(script_state);
  return stats_.discarded_frames;
}

uint64_t MediaStreamTrackVideoStats::totalFrames(ScriptState* script_state) {
  PopulateStatsCache(script_state);
  return stats_.deliverable_frames + stats_.discarded_frames +
         stats_.dropped_frames;
}

ScriptValue MediaStreamTrackVideoStats::toJSON(ScriptState* script_state) {
  V8ObjectBuilder result(script_state);
  result.AddNumber("deliveredFrames", deliveredFrames(script_state));
  result.AddNumber("discardedFrames", discardedFrames(script_state));
  result.AddNumber("totalFrames", totalFrames(script_state));
  return result.GetScriptValue();
}

void MediaStreamTrackVideoStats::Trace(Visitor* visitor) const {
  visitor->Trace(track_);
  ScriptWrappable::Trace(visitor);
}

void MediaStreamTrackVideoStats::PopulateStatsCache(ScriptState* script_state) {
  if (!track_ || !stats_invalidated_) {
    return;
  }
  stats_ = track_->GetVideoFrameStats();
  // We cache in order to preserve the JavaScript run-to-completion semantics.
  // Queue a microtask to invalidate the stats cache, ensuring that we get fresh
  // stats in the next task execution cycle.
  stats_invalidated_ = false;
  ToEventLoop(script_state)
      .EnqueueMicrotask(
          WTF::BindOnce(&MediaStreamTrackVideoStats::InvalidateStatsCache,
                        WrapWeakPersistent(this)));
}

void MediaStreamTrackVideoStats::InvalidateStatsCache() {
  stats_invalidated_ = true;
}

}  // namespace blink
