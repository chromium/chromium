// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/scroll_timeline_offset.h"

#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/css_numeric_value_or_string_or_css_keyword_value_or_scroll_timeline_element_based_offset.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_timeline_element_based_offset.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/cssom/css_keyword_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_numeric_value.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/style/data_equivalency.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace blink {

namespace {

bool ValidateElementBasedOffset(ScrollTimelineElementBasedOffset* offset) {
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
  return (o1->edge() == o2->edge()) && (o1->target() == o2->target()) &&
         (o1->threshold() == o2->threshold());
}

CSSKeywordValue* GetCSSKeywordValue(const ScrollTimelineOffsetValue& offset) {
  if (offset.IsCSSKeywordValue())
    return offset.GetAsCSSKeywordValue();
  // CSSKeywordish:
  if (offset.IsString() && !offset.GetAsString().IsEmpty())
    return CSSKeywordValue::Create(offset.GetAsString());
  return nullptr;
}

}  // namespace

// static
ScrollTimelineOffset* ScrollTimelineOffset::Create(
    const ScrollTimelineOffsetValue& input_offset) {
  if (input_offset.IsCSSNumericValue()) {
    auto* numeric = input_offset.GetAsCSSNumericValue();
    const auto& offset = To<CSSPrimitiveValue>(*numeric->ToCSSValue());
    bool matches_length_percentage = offset.IsLength() ||
                                     offset.IsPercentage() ||
                                     offset.IsCalculatedPercentageWithLength();
    if (!matches_length_percentage)
      return nullptr;
    return MakeGarbageCollected<ScrollTimelineOffset>(&offset);
  }

  if (input_offset.IsScrollTimelineElementBasedOffset()) {
    auto* offset = input_offset.GetAsScrollTimelineElementBasedOffset();
    if (!ValidateElementBasedOffset(offset))
      return nullptr;

    return MakeGarbageCollected<ScrollTimelineOffset>(offset);
  }

  if (auto* keyword = GetCSSKeywordValue(input_offset)) {
    if (keyword->KeywordValueID() != CSSValueID::kAuto)
      return nullptr;
    return MakeGarbageCollected<ScrollTimelineOffset>();
  }

  return nullptr;
}

base::Optional<double> ScrollTimelineOffset::ResolveOffset(
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

    CSSToLengthConversionData conversion_data = CSSToLengthConversionData(
        &computed_style, root_style, document.GetLayoutView(),
        computed_style.EffectiveZoom());
    double resolved = FloatValueForLength(
        length_based_offset_->ConvertToLength(conversion_data), max_offset);

    return resolved;
  } else if (element_based_offset_) {
    if (!element_based_offset_->hasTarget())
      return base::nullopt;
    Element* target = element_based_offset_->target();
    const LayoutBox* target_box = target->GetLayoutBox();

    // It is possible for target to not have a layout box e.g., if it is an
    // unattached element. In which case we return the default offset for now.
    //
    // See the spec discussion here:
    // https://github.com/w3c/csswg-drafts/issues/4337#issuecomment-610997231
    if (!target_box)
      return base::nullopt;

    if (!IsContainingBlockChainDescendant(target_box, root_box))
      return base::nullopt;

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

ScrollTimelineOffsetValue ScrollTimelineOffset::ToScrollTimelineOffsetValue()
    const {
  ScrollTimelineOffsetValue result;
  if (length_based_offset_) {
    result.SetCSSNumericValue(
        CSSNumericValue::FromCSSValue(*length_based_offset_.Get()));
  } else if (element_based_offset_) {
    result.SetScrollTimelineElementBasedOffset(element_based_offset_);
  } else {
    // This is the default value (i.e., 'auto' value)
    result.SetCSSKeywordValue(CSSKeywordValue::Create("auto"));
  }

  return result;
}

bool ScrollTimelineOffset::operator==(const ScrollTimelineOffset& o) const {
  return DataEquivalent(length_based_offset_, o.length_based_offset_) &&
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
