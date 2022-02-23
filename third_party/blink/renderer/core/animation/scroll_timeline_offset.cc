// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/scroll_timeline_offset.h"

#include "base/memory/values_equivalent.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_timeline_element_based_offset.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_csskeywordvalue_cssnumericvalue_scrolltimelineelementbasedoffset_string.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/cssom/css_keyword_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_numeric_value.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace blink {

namespace {

bool ValidateElementBasedOffset(
    const ScrollTimelineElementBasedOffset* offset) {
  if (!offset->hasTarget())
    return false;

  if (offset->hasThreshold()) {
    if (offset->threshold() < 0 || offset->threshold() > 1)
      return false;
  }

  return true;
}

// TODO(majidvp): Dedup. This is a copy of the function in
// third_party/blink/renderer/core/intersection_observer/intersection_geometry.cc
// http://crbug.com/1023375

// Return true if ancestor is in the containing block chain above descendant.
bool IsContainingBlockChainDescendant(const LayoutObject* descendant,
                                      const LayoutObject* ancestor) {
  if (!ancestor || !descendant)
    return false;
  LocalFrame* ancestor_frame = ancestor->GetDocument().GetFrame();
  LocalFrame* descendant_frame = descendant->GetDocument().GetFrame();
  if (ancestor_frame != descendant_frame)
    return false;

  while (descendant && descendant != ancestor)
    descendant = descendant->ContainingBlock();
  return descendant;
}

bool ElementBasedOffsetsEqual(ScrollTimelineElementBasedOffset* o1,
                              ScrollTimelineElementBasedOffset* o2) {
  if (o1 == o2)
    return true;
  if (!o1 || !o2)
    return false;
  // TODO(crbug.com/1070871): Use targetOr(nullptr) after migration is done.
  Element* target_or_null1 = o1->hasTarget() ? o1->target() : nullptr;
  Element* target_or_null2 = o2->hasTarget() ? o2->target() : nullptr;
  return target_or_null1 == target_or_null2 && o1->edge() == o2->edge() &&
         o1->threshold() == o2->threshold();
}

}  // namespace

// static
ScrollTimelineOffset* ScrollTimelineOffset::Create(
    const V8ScrollTimelineOffset* offset) {
  switch (offset->GetContentType()) {
    case V8ScrollTimelineOffset::ContentType::kCSSKeywordValue: {
      const auto* keyword = offset->GetAsCSSKeywordValue();
      if (keyword->KeywordValueID() != CSSValueID::kAuto)
        return nullptr;
      return MakeGarbageCollected<ScrollTimelineOffset>();
    }
    case V8ScrollTimelineOffset::ContentType::kCSSNumericValue: {
      const auto* value =
          To<CSSPrimitiveValue>(offset->GetAsCSSNumericValue()->ToCSSValue());
      bool matches_length_percentage =
          !value || value->IsLength() || value->IsPercentage() ||
          value->IsCalculatedPercentageWithLength();
      if (!matches_length_percentage)
        return nullptr;
      return MakeGarbageCollected<ScrollTimelineOffset>(value);
    }
    case V8ScrollTimelineOffset::ContentType::
        kScrollTimelineElementBasedOffset: {
      auto* value = offset->GetAsScrollTimelineElementBasedOffset();
      if (!ValidateElementBasedOffset(value))
        return nullptr;
      return MakeGarbageCollected<ScrollTimelineOffset>(value);
    }
    case V8ScrollTimelineOffset::ContentType::kString: {
      if (offset->GetAsString().IsEmpty())
        return nullptr;
      const auto* keyword = CSSKeywordValue::Create(offset->GetAsString());
      if (keyword->KeywordValueID() != CSSValueID::kAuto)
        return nullptr;
      return MakeGarbageCollected<ScrollTimelineOffset>();
    }
  }
  NOTREACHED();
  return nullptr;
}

absl::optional<double> ScrollTimelineOffset::ResolveOffset(
    Node* scroll_source,
    ScrollOrientation orientation,
    double max_offset,
    double default_offset) {
  const LayoutBox* root_box = scroll_source->GetLayoutBox();
  DCHECK(root_box);
  Document& document = root_box->GetDocument();

  if (length_based_offset_) {
    // Resolve scroll based offset.
    const ComputedStyle& computed_style = root_box->StyleRef();
    const ComputedStyle* root_style =
        document.documentElement()
            ? document.documentElement()->GetComputedStyle()
            : document.GetComputedStyle();

    // TOOD(crbug.com/1223030): Handle container relative units.
    CSSToLengthConversionData conversion_data = CSSToLengthConversionData(
        &computed_style, root_style, document.GetLayoutView(),
        /* nearest_container */ nullptr, computed_style.EffectiveZoom());
    double resolved = FloatValueForLength(
        length_based_offset_->ConvertToLength(conversion_data), max_offset);

    return resolved;
  } else if (element_based_offset_) {
    if (!element_based_offset_->hasTarget())
      return absl::nullopt;
    Element* target = element_based_offset_->target();
    const LayoutBox* target_box = target->GetLayoutBox();

    // It is possible for target to not have a layout box e.g., if it is an
    // unattached element. In which case we return the default offset for now.
    //
    // See the spec discussion here:
    // https://github.com/w3c/csswg-drafts/issues/4337#issuecomment-610997231
    if (!target_box)
      return absl::nullopt;

    if (!IsContainingBlockChainDescendant(target_box, root_box))
      return absl::nullopt;

    PhysicalRect target_rect = target_box->PhysicalBorderBoxRect();
    target_rect = target_box->LocalToAncestorRect(
        target_rect, root_box,
        kTraverseDocumentBoundaries | kIgnoreScrollOffset);

    PhysicalRect root_rect(root_box->PhysicalBorderBoxRect());

    LayoutUnit root_edge;
    LayoutUnit target_edge;

    // Here is the simple diagram that shows the computation.
    //
    //                 +-----+
    //                 |     |     +------+
    //                 |     |     |      |
    // edge:start +----+-----+-------------------+-----+-------+
    //            |                |xxxxxx|      |xxxxx|       |
    //            |                +------+      |xxxxx|       |
    //            |                              +-----+       |
    //            |                                            |
    // threshold: |    A) 0       B) 0.5         C) 1          |
    //            |                                            |
    //            |                              +-----+       |
    //            |                +------+      |xxxxx|       |
    //            |                |xxxxxx|      |xxxxx|       |
    // edge: end  +----+-----+-------------------+-----+-------+
    //                 |     |     |      |
    //                 |     |     +------+
    //                 +-----+
    //
    // We always take the target top edge and compute the distance to the
    // root's selected edge. This give us (C) in start edge case and (A) in
    // end edge case.
    //
    // To take threshold into account we simply add (1-threshold) or threshold
    // in start and end edge cases respectively.
    bool is_start = element_based_offset_->edge() == "start";
    float threshold_adjustment = is_start
                                     ? (1 - element_based_offset_->threshold())
                                     : element_based_offset_->threshold();

    if (orientation == kVerticalScroll) {
      root_edge = is_start ? root_rect.Y() : root_rect.Bottom();
      target_edge = target_rect.Y();
      // Note that threshold is considered as a portion of target and not as a
      // portion of root. IntersectionObserver has option to allow both.
      target_edge += (threshold_adjustment * target_rect.Height());
    } else {  // kHorizontalScroll
      root_edge = is_start ? root_rect.X() : root_rect.Right();
      target_edge = target_rect.X();
      target_edge += (threshold_adjustment * target_rect.Width());
    }

    LayoutUnit offset = target_edge - root_edge;
    return std::min(std::max(offset.ToDouble(), 0.0), max_offset);
  } else {
    // Resolve the default case (i.e., 'auto' value)
    return default_offset;
  }
}

V8ScrollTimelineOffset* ScrollTimelineOffset::ToV8ScrollTimelineOffset() const {
  if (length_based_offset_) {
    return MakeGarbageCollected<V8ScrollTimelineOffset>(
        CSSNumericValue::FromCSSValue(*length_based_offset_.Get()));
  } else if (element_based_offset_) {
    return MakeGarbageCollected<V8ScrollTimelineOffset>(element_based_offset_);
  }
  // This is the default value (i.e., 'auto' value)
  return MakeGarbageCollected<V8ScrollTimelineOffset>(
      CSSKeywordValue::Create("auto"));
}

bool ScrollTimelineOffset::operator==(const ScrollTimelineOffset& o) const {
  return base::ValuesEquivalent(length_based_offset_, o.length_based_offset_) &&
         ElementBasedOffsetsEqual(element_based_offset_,
                                  o.element_based_offset_);
}

ScrollTimelineOffset::ScrollTimelineOffset(const CSSPrimitiveValue* offset)
    : length_based_offset_(offset), element_based_offset_(nullptr) {}

ScrollTimelineOffset::ScrollTimelineOffset(
    ScrollTimelineElementBasedOffset* offset)
    : length_based_offset_(nullptr), element_based_offset_(offset) {}

void ScrollTimelineOffset::Trace(blink::Visitor* visitor) const {
  visitor->Trace(length_based_offset_);
  visitor->Trace(element_based_offset_);
}

}  // namespace blink
