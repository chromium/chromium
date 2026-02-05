// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_PLAYBACK_STATS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_PLAYBACK_STATS_H_

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webaudio/audio_context.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

// https://webaudio.github.io/web-audio-api/#AudioPlaybackStats
class MODULES_EXPORT AudioPlaybackStats : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit AudioPlaybackStats(AudioContext*);

  double underrunDuration(ScriptState* script_state);
  uint32_t underrunEvents(ScriptState* script_state);
  double totalDuration(ScriptState* script_state);
  double averageLatency(ScriptState* script_state);
  double minimumLatency(ScriptState* script_state);
  double maximumLatency(ScriptState* script_state);
  void resetLatency(ScriptState* script_state);

  ScriptObject toJSON(ScriptState* script_state);

  void Trace(Visitor*) const override;

 private:
  // Updates the stats if they have not yet been updated during the current
  // task.
  void MaybeUpdateStats(ScriptState* script_state);
  void OnMicrotask();

  WeakMember<AudioContext> context_;
  AudioFrameStatsAccumulator stats_;
  bool is_stats_from_current_task_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_PLAYBACK_STATS_H_
