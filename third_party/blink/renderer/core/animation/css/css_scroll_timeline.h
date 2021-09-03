// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_SCROLL_TIMELINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_SCROLL_TIMELINE_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/dom/id_target_observer.h"

namespace blink {

class Document;
class Element;
class StyleRuleScrollTimeline;

// A CSSScrollTimeline is like a ScrollTimeline, except it originates from
// an @scroll-timeline rule.
class CORE_EXPORT CSSScrollTimeline : public ScrollTimeline {
 public:
  struct Options {
    STACK_ALLOCATED();

   public:
    Options(Document&, StyleRuleScrollTimeline&);

   private:
    friend class CSSScrollTimeline;

    absl::optional<Element*> source_;
    ScrollTimeline::ScrollDirection direction_;
    HeapVector<Member<ScrollTimelineOffset>> offsets_;
    absl::optional<double> time_range_;
    StyleRuleScrollTimeline* rule_;
  };

  CSSScrollTimeline(Document*, Options&&);

  const AtomicString& Name() const;

  StyleRuleScrollTimeline* GetRule() const { return rule_; }

  bool Matches(const Options&) const;

  // AnimationTimeline implementation.
  bool IsCSSScrollTimeline() const override { return true; }
  void AnimationAttached(Animation*) override;
  void AnimationDetached(Animation*) override;

  // If a CSSScrollTimeline matching |options| already exists, return that
  // timeline. Otherwise returns nullptr.
  static CSSScrollTimeline* FindMatchingTimeline(const Options&);

  void Trace(Visitor*) const override;

 private:
  void SetObservers(HeapVector<Member<IdTargetObserver>>);

  Member<StyleRuleScrollTimeline> rule_;
  HeapVector<Member<IdTargetObserver>> observers_;
};

template <>
struct DowncastTraits<CSSScrollTimeline> {
  static bool AllowFrom(const AnimationTimeline& value) {
    return value.IsCSSScrollTimeline();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_SCROLL_TIMELINE_H_
