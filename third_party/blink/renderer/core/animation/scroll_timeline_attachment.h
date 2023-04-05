// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SCROLL_TIMELINE_ATTACHMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SCROLL_TIMELINE_ATTACHMENT_H_

#include "cc/animation/scroll_timeline.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_axis.h"
#include "third_party/blink/renderer/core/animation/timing.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class Element;

// A scroll timeline has an "attachment", which provides the appropriate
// scrolling element and axis/inset configuration. Attachments can appear
// dynamically at any time *after* timeline creation, which makes it possible
// for a given scrolling element to affect animations on preceding elements.
//
// https://github.com/w3c/csswg-drafts/issues/7759
class CORE_EXPORT ScrollTimelineAttachment
    : public GarbageCollected<ScrollTimelineAttachment> {
 public:
  using ScrollOffsets = cc::ScrollTimeline::ScrollOffsets;
  using ScrollAxis = V8ScrollAxis::Enum;

  enum class Type { kScroll, kView };

  virtual Type GetType() const { return Type::kScroll; }

  // Indicates the relation between the reference element and source of the
  // scroll timeline.
  enum class ReferenceType {
    kSource,          // The reference element matches the source.
    kNearestAncestor  // The source is the nearest scrollable ancestor to the
                      // reference element.
  };

  ScrollTimelineAttachment(ReferenceType,
                           Element* reference_element,
                           ScrollAxis);

  ReferenceType GetReferenceType() const { return reference_type_; }
  Element* GetReferenceElement() const { return reference_element_.Get(); }
  ScrollAxis GetAxis() const { return axis_; }

  // Determines the source for the scroll timeline. It may be the reference
  // element or its nearest scrollable ancestor, depending on |reference_type_|.
  Element* ComputeSource() const;
  // This version does not force a style update and is therefore safe to call
  // during lifecycle update.
  Element* ComputeSourceNoLayout() const;

  void Trace(Visitor*) const;

 private:
  ReferenceType reference_type_;
  Member<Element> reference_element_;
  ScrollAxis axis_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SCROLL_TIMELINE_ATTACHMENT_H_
