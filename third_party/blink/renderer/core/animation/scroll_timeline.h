// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SCROLL_TIMELINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SCROLL_TIMELINE_H_

#include "third_party/blink/renderer/core/animation/animation_timeline.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline_options.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// Implements the ScrollTimeline concept from the Scroll-linked Animations spec.
//
// A ScrollTimeline is a special form of AnimationTimeline whose time values are
// not determined by wall-clock time but instead the progress of scrolling in a
// scroll container. The user is able to specify which scroll container to
// track, the direction of scroll they care about, and various attributes to
// control the conversion of scroll amount to time output.
//
// Spec: https://wicg.github.io/scroll-animations/#scroll-timelines
class CORE_EXPORT ScrollTimeline final : public AnimationTimeline {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum ScrollDirection {
    Block,
    Inline,
    Horizontal,
    Vertical,
  };

  static ScrollTimeline* Create(Document&,
                                ScrollTimelineOptions,
                                ExceptionState&);

  // AnimationTimeline implementation.
  double currentTime(bool& is_null) final;
  bool IsScrollTimeline() const override { return true; }

  // IDL API implementation.
  Element* scrollSource();
  String orientation();
  String startScrollOffset();
  String endScrollOffset();
  void timeRange(DoubleOrScrollTimelineAutoKeyword&);

  // Returns the Node that should actually have the ScrollableArea (if one
  // exists). This can differ from |scrollSource| when |scroll_source_| is the
  // Document's scrollingElement.
  Node* ResolvedScrollSource() const;

  ScrollDirection GetOrientation() const { return orientation_; }

  void GetCurrentAndMaxOffset(const LayoutBox*,
                              double& current_offset,
                              double& max_offset) const;
  void ResolveScrollStartAndEnd(const LayoutBox*,
                                double max_offset,
                                double& resolved_start_scroll_offset,
                                double& resolved_end_scroll_offset) const;

  // Must be called when this ScrollTimeline is attached/detached from an
  // animation.
  void AttachAnimation();
  void DetachAnimation();

  void Trace(blink::Visitor*) override;

  // For the AnimationWorklet origin trial, we need to automatically composite
  // elements that are targets of ScrollTimelines (http://crbug.com/776533). We
  // expose a static lookup method to enable this.
  //
  // TODO(crbug.com/839341): Remove once WorkletAnimations can run on main.
  static bool HasActiveScrollTimeline(Node* node);

 private:
  ScrollTimeline(Element*,
                 ScrollDirection,
                 CSSPrimitiveValue*,
                 CSSPrimitiveValue*,
                 double);

  Member<Element> scroll_source_;
  ScrollDirection orientation_;
  Member<CSSPrimitiveValue> start_scroll_offset_;
  Member<CSSPrimitiveValue> end_scroll_offset_;
  double time_range_;
};

DEFINE_TYPE_CASTS(ScrollTimeline,
                  AnimationTimeline,
                  value,
                  value->IsScrollTimeline(),
                  value.IsScrollTimeline());

}  // namespace blink

#endif
