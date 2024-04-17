// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/anchor_evaluator_impl.h"

#include "third_party/blink/renderer/core/css/anchor_query.h"
#include "third_party/blink/renderer/core/layout/anchor_query_map.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/logical_fragment_link.h"
#include "third_party/blink/renderer/core/style/anchor_specifier_value.h"
#include "third_party/blink/renderer/core/style/inset_area.h"

namespace blink {

namespace {

CSSAnchorValue PhysicalAnchorValueUsing(CSSAnchorValue x,
                                        CSSAnchorValue flipped_x,
                                        CSSAnchorValue y,
                                        CSSAnchorValue flipped_y,
                                        WritingDirectionMode writing_direction,
                                        bool is_y_axis) {
  if (is_y_axis)
    return writing_direction.IsFlippedY() ? flipped_y : y;
  return writing_direction.IsFlippedX() ? flipped_x : x;
}

// The logical <anchor-side> keywords map to one of the physical keywords
// depending on the property the function is being used in and the writing mode.
// https://drafts.csswg.org/css-anchor-1/#anchor-pos
CSSAnchorValue PhysicalAnchorValueFromLogicalOrAuto(
    CSSAnchorValue anchor_value,
    WritingDirectionMode writing_direction,
    WritingDirectionMode self_writing_direction,
    bool is_y_axis) {
  switch (anchor_value) {
    case CSSAnchorValue::kSelfStart:
      writing_direction = self_writing_direction;
      [[fallthrough]];
    case CSSAnchorValue::kStart:
      return PhysicalAnchorValueUsing(
          CSSAnchorValue::kLeft, CSSAnchorValue::kRight, CSSAnchorValue::kTop,
          CSSAnchorValue::kBottom, writing_direction, is_y_axis);
    case CSSAnchorValue::kSelfEnd:
      writing_direction = self_writing_direction;
      [[fallthrough]];
    case CSSAnchorValue::kEnd:
      return PhysicalAnchorValueUsing(
          CSSAnchorValue::kRight, CSSAnchorValue::kLeft,
          CSSAnchorValue::kBottom, CSSAnchorValue::kTop, writing_direction,
          is_y_axis);
    default:
      return anchor_value;
  }
}

}  // namespace

PhysicalAnchorReference::PhysicalAnchorReference(
    const LogicalAnchorReference& logical_reference,
    const WritingModeConverter& converter)
    : rect(converter.ToPhysical(logical_reference.rect)),
      layout_object(logical_reference.layout_object),
      is_out_of_flow(logical_reference.is_out_of_flow) {}

void LogicalAnchorReference::InsertInReverseTreeOrderInto(
    Member<LogicalAnchorReference>* head_ptr) {
  for (;;) {
    LogicalAnchorReference* const head = *head_ptr;
    DCHECK(!head || head->layout_object);
    if (!head || head->layout_object->IsBeforeInPreOrder(*layout_object)) {
      // An in-flow reference has higher precedence than any other reference
      // before it in tree order, in which case there's no need to keep the
      // other references.
      if (is_out_of_flow) {
        next = head;
      }
      *head_ptr = this;
      break;
    }

    // Skip adding if there is already an in-flow reference that is after in
    // the tree order, which always has higher precedence than |this|.
    if (!head->is_out_of_flow) {
      break;
    }

    head_ptr = &head->next;
  }
}

// static
const LogicalAnchorQuery& LogicalAnchorQuery::Empty() {
  DEFINE_STATIC_LOCAL(Persistent<LogicalAnchorQuery>, empty,
                      (MakeGarbageCollected<LogicalAnchorQuery>()));
  return *empty;
}

const PhysicalAnchorReference* PhysicalAnchorQuery::AnchorReference(
    const LayoutObject& query_object,
    const AnchorKey& key) const {
  if (const PhysicalAnchorReference* reference =
          Base::GetAnchorReference(key)) {
    for (const PhysicalAnchorReference* result = reference; result;
         result = result->next) {
      if (!result->is_out_of_flow ||
          result->layout_object->IsBeforeInPreOrder(query_object)) {
        return result;
      }
    }
  }
  return nullptr;
}

const LayoutObject* PhysicalAnchorQuery::AnchorLayoutObject(
    const LayoutObject& query_object,
    const AnchorKey& key) const {
  if (const PhysicalAnchorReference* reference =
          AnchorReference(query_object, key)) {
    return reference->layout_object.Get();
  }
  return nullptr;
}

const LogicalAnchorReference* LogicalAnchorQuery::AnchorReference(
    const LayoutObject& query_object,
    const AnchorKey& key) const {
  if (const LogicalAnchorReference* reference = Base::GetAnchorReference(key)) {
    for (const LogicalAnchorReference* result = reference; result;
         result = result->next) {
      if ((!result->is_out_of_flow ||
           result->layout_object->IsBeforeInPreOrder(query_object))) {
        return result;
      }
    }
  }
  return nullptr;
}

void LogicalAnchorQuery::Set(const AnchorKey& key,
                             const LayoutObject& layout_object,
                             const LogicalRect& rect,
                             SetOptions options) {
  Set(key, MakeGarbageCollected<LogicalAnchorReference>(
               layout_object, rect, options == SetOptions::kOutOfFlow));
}

void LogicalAnchorQuery::Set(const AnchorKey& key,
                             LogicalAnchorReference* reference) {
  DCHECK(reference);
  DCHECK(!reference->next);
  const auto result = Base::insert(key, reference);
  if (result.is_new_entry)
    return;

  // If this is a fragment of the existing |LayoutObject|, unite the rect.
  Member<LogicalAnchorReference>* const existing_head_ptr = result.stored_value;
  LogicalAnchorReference* const existing_head = *existing_head_ptr;
  DCHECK(existing_head);
  const LayoutObject* new_object = reference->layout_object;
  DCHECK(new_object);
  for (LogicalAnchorReference* existing = existing_head; existing;
       existing = existing->next) {
    const LayoutObject* existing_object = existing->layout_object;
    DCHECK(existing_object);
    if (existing_object == new_object) {
      existing->rect.Unite(reference->rect);
      return;
    }
  }

  // When out-of-flow objects are involved, callers can't guarantee the call
  // order. Insert into the list in the reverse tree order.
  reference->InsertInReverseTreeOrderInto(existing_head_ptr);
}

void PhysicalAnchorQuery::SetFromLogical(
    const LogicalAnchorQuery& logical_query,
    const WritingModeConverter& converter) {
  // This function assumes |this| is empty on the entry. Merging multiple
  // references is not supported.
  DCHECK(IsEmpty());
  for (const auto entry : logical_query) {
    auto* head =
        MakeGarbageCollected<PhysicalAnchorReference>(*entry.value, converter);
    PhysicalAnchorReference* tail = head;
    for (LogicalAnchorReference* runner = entry.value->next; runner;
         runner = runner->next) {
      tail->next =
          MakeGarbageCollected<PhysicalAnchorReference>(*runner, converter);
      tail = tail->next;
    }
    const auto result = Base::insert(entry.key, head);
    DCHECK(result.is_new_entry);
  }
}

void LogicalAnchorQuery::SetFromPhysical(
    const PhysicalAnchorQuery& physical_query,
    const WritingModeConverter& converter,
    const LogicalOffset& additional_offset,
    SetOptions options) {
  for (auto entry : physical_query) {
    // For each key, only the last one in the tree order, in or out of flow, is
    // needed to be propagated, because whether it's in flow is re-computed for
    // each containing block.
    LogicalRect rect = converter.ToLogical(entry.value->rect);
    rect.offset += additional_offset;
    Set(entry.key, MakeGarbageCollected<LogicalAnchorReference>(
                       *entry.value->layout_object, rect,
                       options == SetOptions::kOutOfFlow));
  }
}

std::optional<LayoutUnit> LogicalAnchorQuery::EvaluateAnchor(
    const LogicalAnchorReference& reference,
    CSSAnchorValue anchor_value,
    float percentage,
    LayoutUnit available_size,
    const WritingModeConverter& container_converter,
    WritingDirectionMode self_writing_direction,
    const PhysicalOffset& offset_to_padding_box,
    bool is_y_axis,
    bool is_right_or_bottom) const {
  const PhysicalRect anchor = container_converter.ToPhysical(reference.rect);
  anchor_value = PhysicalAnchorValueFromLogicalOrAuto(
      anchor_value, container_converter.GetWritingDirection(),
      self_writing_direction, is_y_axis);
  LayoutUnit value;
  switch (anchor_value) {
    case CSSAnchorValue::kCenter: {
      const LayoutUnit start = is_y_axis
                                   ? anchor.Y() - offset_to_padding_box.top
                                   : anchor.X() - offset_to_padding_box.left;
      const LayoutUnit end = is_y_axis
                                 ? anchor.Bottom() - offset_to_padding_box.top
                                 : anchor.Right() - offset_to_padding_box.left;
      value = start + LayoutUnit::FromFloatRound((end - start) * 0.5);
      break;
    }
    case CSSAnchorValue::kLeft:
      if (is_y_axis)
        return std::nullopt;  // Wrong axis.
      // Make the offset relative to the padding box, because the containing
      // block is formed by the padding edge.
      // https://www.w3.org/TR/CSS21/visudet.html#containing-block-details
      value = anchor.X() - offset_to_padding_box.left;
      break;
    case CSSAnchorValue::kRight:
      if (is_y_axis)
        return std::nullopt;  // Wrong axis.
      // See |CSSAnchorValue::kLeft|.
      value = anchor.Right() - offset_to_padding_box.left;
      break;
    case CSSAnchorValue::kTop:
      if (!is_y_axis)
        return std::nullopt;  // Wrong axis.
      // See |CSSAnchorValue::kLeft|.
      value = anchor.Y() - offset_to_padding_box.top;
      break;
    case CSSAnchorValue::kBottom:
      if (!is_y_axis)
        return std::nullopt;  // Wrong axis.
      // See |CSSAnchorValue::kLeft|.
      value = anchor.Bottom() - offset_to_padding_box.top;
      break;
    case CSSAnchorValue::kPercentage: {
      LayoutUnit size;
      if (is_y_axis) {
        value = anchor.Y() - offset_to_padding_box.top;
        size = anchor.Height();
        // The percentage is logical, between the `start` and `end` sides.
        // Convert to the physical percentage.
        // https://drafts.csswg.org/css-anchor-1/#anchor-pos
        if (container_converter.GetWritingDirection().IsFlippedY())
          percentage = 100 - percentage;
      } else {
        value = anchor.X() - offset_to_padding_box.left;
        size = anchor.Width();
        // Convert the logical percentage to physical. See above.
        if (container_converter.GetWritingDirection().IsFlippedX())
          percentage = 100 - percentage;
      }
      value += LayoutUnit::FromFloatRound(size * percentage / 100);
      break;
    }
    case CSSAnchorValue::kStart:
    case CSSAnchorValue::kEnd:
    case CSSAnchorValue::kSelfStart:
    case CSSAnchorValue::kSelfEnd:
      // These logical values should have been converted to corresponding
      // physical values in `PhysicalAnchorValueFromLogicalOrAuto`.
      NOTREACHED();
      return std::nullopt;
  }

  // The |value| is for the "start" side of insets. For the "end" side of
  // insets, return the distance from |available_size|.
  if (is_right_or_bottom)
    return available_size - value;
  return value;
}

LayoutUnit LogicalAnchorQuery::EvaluateSize(
    const LogicalAnchorReference& reference,
    CSSAnchorSizeValue anchor_size_value,
    WritingMode container_writing_mode,
    WritingMode self_writing_mode) const {
  const LogicalSize& anchor = reference.rect.size;
  switch (anchor_size_value) {
    case CSSAnchorSizeValue::kInline:
      return anchor.inline_size;
    case CSSAnchorSizeValue::kBlock:
      return anchor.block_size;
    case CSSAnchorSizeValue::kWidth:
      return IsHorizontalWritingMode(container_writing_mode)
                 ? anchor.inline_size
                 : anchor.block_size;
    case CSSAnchorSizeValue::kHeight:
      return IsHorizontalWritingMode(container_writing_mode)
                 ? anchor.block_size
                 : anchor.inline_size;
    case CSSAnchorSizeValue::kSelfInline:
      return IsHorizontalWritingMode(container_writing_mode) ==
                     IsHorizontalWritingMode(self_writing_mode)
                 ? anchor.inline_size
                 : anchor.block_size;
    case CSSAnchorSizeValue::kSelfBlock:
      return IsHorizontalWritingMode(container_writing_mode) ==
                     IsHorizontalWritingMode(self_writing_mode)
                 ? anchor.block_size
                 : anchor.inline_size;
  }
  NOTREACHED();
  return LayoutUnit();
}

const LogicalAnchorQuery* AnchorEvaluatorImpl::AnchorQuery() const {
  if (anchor_query_)
    return anchor_query_;
  if (anchor_queries_) {
    DCHECK(containing_block_);
    anchor_query_ = &anchor_queries_->AnchorQuery(*containing_block_);
    DCHECK(anchor_query_);
    return anchor_query_;
  }
  return nullptr;
}

std::optional<LayoutUnit> AnchorEvaluatorImpl::Evaluate(
    const class AnchorQuery& anchor_query,
    const ScopedCSSName* position_anchor,
    const std::optional<InsetAreaOffsets>& inset_area_offsets) {
  switch (anchor_query.Type()) {
    case CSSAnchorQueryType::kAnchor:
      return EvaluateAnchor(anchor_query.AnchorSpecifier(),
                            anchor_query.AnchorSide(),
                            anchor_query.AnchorSidePercentageOrZero(),
                            position_anchor, inset_area_offsets);
    case CSSAnchorQueryType::kAnchorSize:
      return EvaluateAnchorSize(anchor_query.AnchorSpecifier(),
                                anchor_query.AnchorSize(), position_anchor);
  }
}

const LogicalAnchorReference* AnchorEvaluatorImpl::ResolveAnchorReference(
    const AnchorSpecifierValue& anchor_specifier,
    const ScopedCSSName* position_anchor) const {
  if (!anchor_specifier.IsNamed() && !position_anchor && !implicit_anchor_) {
    return nullptr;
  }
  const LogicalAnchorQuery* anchor_query = AnchorQuery();
  if (!anchor_query) {
    return nullptr;
  }
  if (anchor_specifier.IsNamed()) {
    return anchor_query->AnchorReference(*query_object_,
                                         &anchor_specifier.GetName());
  }
  if (anchor_specifier.IsDefault() && position_anchor) {
    return anchor_query->AnchorReference(*query_object_, position_anchor);
  }
  return anchor_query->AnchorReference(*query_object_, implicit_anchor_);
}

const LayoutObject* AnchorEvaluatorImpl::DefaultAnchor(
    const ScopedCSSName* position_anchor) const {
  return cached_default_anchor_.Get(position_anchor, [&]() {
    const LogicalAnchorReference* reference = ResolveAnchorReference(
        *AnchorSpecifierValue::Default(), position_anchor);
    return reference ? reference->layout_object : nullptr;
  });
}

const PaintLayer* AnchorEvaluatorImpl::DefaultAnchorScrollContainerLayer(
    const ScopedCSSName* position_anchor) const {
  return cached_default_anchor_scroll_container_layer_.Get(
      position_anchor, [&]() {
        return DefaultAnchor(position_anchor)
            ->ContainingScrollContainerLayer(
                true /*ignore_layout_view_for_fixed_pos*/);
      });
}

bool AnchorEvaluatorImpl::AllowAnchor() const {
  switch (GetMode()) {
    case Mode::kLeft:
    case Mode::kRight:
    case Mode::kTop:
    case Mode::kBottom:
      return true;
    case Mode::kNone:
    case Mode::kSize:
      return false;
  }
}

bool AnchorEvaluatorImpl::AllowAnchorSize() const {
  switch (GetMode()) {
    case Mode::kSize:
      return true;
    case Mode::kNone:
    case Mode::kLeft:
    case Mode::kRight:
    case Mode::kTop:
    case Mode::kBottom:
      return false;
  }
}

bool AnchorEvaluatorImpl::IsYAxis() const {
  return GetMode() == Mode::kTop || GetMode() == Mode::kBottom;
}

bool AnchorEvaluatorImpl::IsRightOrBottom() const {
  return GetMode() == Mode::kRight || GetMode() == Mode::kBottom;
}

bool AnchorEvaluatorImpl::ShouldUseScrollAdjustmentFor(
    const LayoutObject* anchor,
    const ScopedCSSName* position_anchor) const {
  if (!DefaultAnchor(position_anchor)) {
    return false;
  }
  if (anchor == DefaultAnchor(position_anchor)) {
    return true;
  }
  return anchor->ContainingScrollContainerLayer(
             true /*ignore_layout_view_for_fixed_pos*/) ==
         DefaultAnchorScrollContainerLayer(position_anchor);
}

std::optional<LayoutUnit> AnchorEvaluatorImpl::EvaluateAnchor(
    const AnchorSpecifierValue& anchor_specifier,
    CSSAnchorValue anchor_value,
    float percentage,
    const ScopedCSSName* position_anchor,
    const std::optional<InsetAreaOffsets>& inset_area_offsets) const {
  if (!AllowAnchor()) {
    return std::nullopt;
  }

  const LogicalAnchorReference* anchor_reference =
      ResolveAnchorReference(anchor_specifier, position_anchor);
  if (!anchor_reference) {
    return std::nullopt;
  }

  PhysicalRect inset_area_modified_containing_block_rect =
      InsetAreaModifiedContainingBlock(inset_area_offsets);

  const bool is_y_axis = IsYAxis();

  DCHECK(AnchorQuery());
  if (std::optional<LayoutUnit> result = AnchorQuery()->EvaluateAnchor(
          *anchor_reference, anchor_value, percentage,
          AvailableSizeAlongAxis(inset_area_modified_containing_block_rect),
          container_converter_, self_writing_direction_,
          inset_area_modified_containing_block_rect.offset, is_y_axis,
          IsRightOrBottom())) {
    bool& needs_scroll_adjustment = is_y_axis ? needs_scroll_adjustment_in_y_
                                              : needs_scroll_adjustment_in_x_;
    if (!needs_scroll_adjustment &&
        ShouldUseScrollAdjustmentFor(anchor_reference->layout_object,
                                     position_anchor)) {
      needs_scroll_adjustment = true;
    }
    return result;
  }
  return std::nullopt;
}

std::optional<LayoutUnit> AnchorEvaluatorImpl::EvaluateAnchorSize(
    const AnchorSpecifierValue& anchor_specifier,
    CSSAnchorSizeValue anchor_size_value,
    const ScopedCSSName* position_anchor) const {
  if (!AllowAnchorSize()) {
    return std::nullopt;
  }

  const LogicalAnchorReference* anchor_reference =
      ResolveAnchorReference(anchor_specifier, position_anchor);
  if (!anchor_reference) {
    return std::nullopt;
  }

  DCHECK(AnchorQuery());
  return AnchorQuery()->EvaluateSize(*anchor_reference, anchor_size_value,
                                     container_converter_.GetWritingMode(),
                                     self_writing_direction_.GetWritingMode());
}

std::optional<PhysicalOffset> AnchorEvaluatorImpl::ComputeAnchorCenterOffsets(
    const ComputedStyleBuilder& builder) {
  // Parameter `percentage` is unused for any non-percentage anchor value.
  const double dummy_percentage = 0;

  // Do not let the pre-computation of anchor-center offsets mark for needing
  // scroll adjustments. It is not known at this point if anchor-center will be
  // used at all, and allowing this marking could cause unnecessary work and
  // paint invalidations.
  base::AutoReset<bool> reset_adjust_x(&needs_scroll_adjustment_in_x_, true);
  base::AutoReset<bool> reset_adjust_y(&needs_scroll_adjustment_in_y_, true);
  std::optional<LayoutUnit> top;
  std::optional<LayoutUnit> left;
  {
    AnchorScope anchor_scope(AnchorScope::Mode::kTop, this);
    top = EvaluateAnchor(*AnchorSpecifierValue::Default(),
                         CSSAnchorValue::kCenter, dummy_percentage,
                         builder.PositionAnchor(), builder.InsetAreaOffsets());
  }
  {
    AnchorScope anchor_scope(AnchorScope::Mode::kLeft, this);
    left = EvaluateAnchor(*AnchorSpecifierValue::Default(),
                          CSSAnchorValue::kCenter, dummy_percentage,
                          builder.PositionAnchor(), builder.InsetAreaOffsets());
  }
  CHECK(top.has_value() == left.has_value());
  if (top.has_value()) {
    return PhysicalOffset(left.value(), top.value());
  }
  return std::nullopt;
}

std::optional<InsetAreaOffsets>
AnchorEvaluatorImpl::ComputeInsetAreaOffsetsForLayout(
    const ScopedCSSName* position_anchor,
    InsetArea inset_area) {
  CHECK(!inset_area.IsNone());

  if (!DefaultAnchor(position_anchor)) {
    return std::nullopt;
  }
  InsetArea physical_inset_area = inset_area.ToPhysical(
      container_converter_.GetWritingDirection(), self_writing_direction_);

  std::optional<LayoutUnit> top;
  std::optional<LayoutUnit> bottom;
  std::optional<LayoutUnit> left;
  std::optional<LayoutUnit> right;

  // The InsetArea::Used*() methods returns either an anchor() function or
  // nullopt (representing a 0px length), using top/left/right/bottom, to adjust
  // the containing block to align with either of the physical edges of the
  // default anchor.
  //
  // Note that the inset adjustment is already set to zero above, so there's
  // nothing to do here for nullopt values.
  if (std::optional<blink::AnchorQuery> query = physical_inset_area.UsedTop()) {
    AnchorScope anchor_scope(AnchorScope::Mode::kTop, this);
    top = Evaluate(query.value(), position_anchor,
                   /* inset_area_offsets */ std::nullopt);
  }
  if (std::optional<blink::AnchorQuery> query =
          physical_inset_area.UsedBottom()) {
    AnchorScope anchor_scope(AnchorScope::Mode::kBottom, this);
    bottom = Evaluate(query.value(), position_anchor,
                      /* inset_area_offsets */ std::nullopt);
  }
  if (std::optional<blink::AnchorQuery> query =
          physical_inset_area.UsedLeft()) {
    AnchorScope anchor_scope(AnchorScope::Mode::kLeft, this);
    left = Evaluate(query.value(), position_anchor,
                    /* inset_area_offsets */ std::nullopt);
  }
  if (std::optional<blink::AnchorQuery> query =
          physical_inset_area.UsedRight()) {
    AnchorScope anchor_scope(AnchorScope::Mode::kRight, this);
    right = Evaluate(query.value(), position_anchor,
                     /* inset_area_offsets */ std::nullopt);
  }
  return InsetAreaOffsets(top, bottom, left, right);
}

PhysicalRect AnchorEvaluatorImpl::InsetAreaModifiedContainingBlock(
    const std::optional<InsetAreaOffsets>& inset_area_offsets) const {
  return cached_inset_area_modified_containing_block_.Get(
      inset_area_offsets, [&]() {
        if (!inset_area_offsets.has_value()) {
          return containing_block_rect_;
        }

        PhysicalRect inset_area_modified_containing_block_rect =
            containing_block_rect_;

        LayoutUnit top = inset_area_offsets->top.value_or(LayoutUnit());
        LayoutUnit bottom = inset_area_offsets->bottom.value_or(LayoutUnit());
        LayoutUnit left = inset_area_offsets->left.value_or(LayoutUnit());
        LayoutUnit right = inset_area_offsets->right.value_or(LayoutUnit());

        // Reduce the container size and adjust the offset based on the
        // inset-area.
        inset_area_modified_containing_block_rect.ContractEdges(top, right,
                                                                bottom, left);

        // For 'center' values (aligned with start and end anchor sides), the
        // containing block is aligned and sized with the anchor, regardless of
        // whether it's inside the original containing block or not. Otherwise,
        // ContractEdges above might have created a negative size if the
        // inset-area is aligned with an anchor side outside the containing
        // block.
        if (inset_area_modified_containing_block_rect.size.width <
            LayoutUnit()) {
          DCHECK(left == LayoutUnit() || right == LayoutUnit())
              << "If aligned to both anchor edges, the size should never be "
                 "negative.";
          // Collapse the inline size to 0 and align with the single anchor edge
          // defined by the inset-area.
          if (left == LayoutUnit()) {
            DCHECK(right != LayoutUnit());
            inset_area_modified_containing_block_rect.offset.left +=
                inset_area_modified_containing_block_rect.size.width;
          }
          inset_area_modified_containing_block_rect.size.width = LayoutUnit();
        }
        if (inset_area_modified_containing_block_rect.size.height <
            LayoutUnit()) {
          DCHECK(top == LayoutUnit() || bottom == LayoutUnit())
              << "If aligned to both anchor edges, the size should never be "
                 "negative.";
          // Collapse the block size to 0 and align with the single anchor edge
          // defined by the inset-area.
          if (top == LayoutUnit()) {
            DCHECK(bottom != LayoutUnit());
            inset_area_modified_containing_block_rect.offset.top +=
                inset_area_modified_containing_block_rect.size.height;
          }
          inset_area_modified_containing_block_rect.size.height = LayoutUnit();
        }

        return inset_area_modified_containing_block_rect;
      });
}

void LogicalAnchorReference::Trace(Visitor* visitor) const {
  visitor->Trace(layout_object);
  visitor->Trace(next);
}

void PhysicalAnchorReference::Trace(Visitor* visitor) const {
  visitor->Trace(layout_object);
  visitor->Trace(next);
}

}  // namespace blink
