// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_ANIMATION_PLAYBACK_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_ANIMATION_PLAYBACK_EVENT_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/dom/events/event.h"

namespace blink {

class AnimationPlaybackEventInit;

class AnimationPlaybackEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AnimationPlaybackEvent* Create(
      const AtomicString& type,
      const AnimationPlaybackEventInit* initializer) {
    return MakeGarbageCollected<AnimationPlaybackEvent>(type, initializer);
  }

  AnimationPlaybackEvent(const AtomicString& type,
                         absl::optional<double> current_time,
                         absl::optional<double> timeline_time);
  AnimationPlaybackEvent(const AtomicString&,
                         const AnimationPlaybackEventInit*);
  ~AnimationPlaybackEvent() override;

  absl::optional<double> currentTime() const { return current_time_; }
  absl::optional<double> timelineTime() const { return timeline_time_; }

  const AtomicString& InterfaceName() const override;

  void Trace(Visitor*) const override;

 private:
  absl::optional<double> current_time_;
  absl::optional<double> timeline_time_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_ANIMATION_PLAYBACK_EVENT_H_
