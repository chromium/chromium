// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TIMELINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TIMELINE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class Animation;
class Document;

class CORE_EXPORT AnimationTimeline : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~AnimationTimeline() override = default;

  virtual double currentTime(bool&) = 0;

  base::Optional<double> CurrentTime() {
    bool is_null;
    double current_time_ms = currentTime(is_null);
    return is_null ? base::nullopt : base::make_optional(current_time_ms);
  }

  base::Optional<double> CurrentTimeSeconds() {
    base::Optional<double> current_time_ms = CurrentTime();
    if (current_time_ms)
      return current_time_ms.value() / 1000;
    return current_time_ms;
  }

  virtual bool IsDocumentTimeline() const { return false; }
  virtual bool IsScrollTimeline() const { return false; }
  virtual bool IsActive() const = 0;
  // Returns the initial start time for animations that are linked to this
  // timeline. This method gets invoked when initializing the start time of an
  // animation on this timeline for the first time. It exists because the
  // initial start time for scroll-linked and time-linked animations are
  // different.
  //
  // Changing scroll-linked animation start_time initialization is under
  // consideration here: https://github.com/w3c/csswg-drafts/issues/2075.
  virtual base::Optional<base::TimeDelta> InitialStartTimeForAnimations() = 0;
  virtual Document* GetDocument() = 0;
  virtual void AnimationAttached(Animation*) = 0;
  virtual void AnimationDetached(Animation*) = 0;
};

}  // namespace blink

#endif
