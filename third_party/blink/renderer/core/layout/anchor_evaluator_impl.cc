// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/anchor_evaluator_impl.h"

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/css/anchor_query.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/layout/anchor_query_map.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/logical_fragment_link.h"
#include "third_party/blink/renderer/core/style/anchor_specifier_value.h"
#include "third_party/blink/renderer/core/style/position_area.h"

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

// https://drafts.csswg.org/css-anchor-position-1/#valdef-anchor-inside
// https://drafts.csswg.org/css-anchor-position-1/#valdef-anchor-outside
CSSAnchorValue PhysicalAnchorValueFromInsideOutside(CSSAnchorValue anchor_value,
                                                    bool is_y_axis,
                                                    bool is_right_or_bottom) {
  switch (anchor_value) {
    case CSSAnchorValue::kInside: {
      if (is_y_axis) {
        return is_right_or_bottom ? CSSAnchorValue::kBottom
                                  : CSSAnchorValue::kTop;
      }
      return is_right_or_bottom ? CSSAnchorValue::kRight
                                : CSSAnchorValue::kLeft;
    }
    case CSSAnchorValue::kOutside: {
      if (is_y_axis) {
        return is_right_or_bottom ? CSSAnchorValue::kTop
                                  : CSSAnchorValue::kBottom;
      }
      return is_right_or_bottom ? CSSAnchorValue::kLeft
                                : CSSAnchorValue::kRight;
    }
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
      display_locks(logical_reference.display_locks),
      is_out_of_flow(logical_reference.is_out_of_flow) {}

void LogicalAnchorReference::InsertInReverseTreeOrderInto(
    Member<LogicalAnchorReference>* head_ptr) {
  for (;;) {
    LogicalAnchorReference* const head = *head_ptr;
    DCHECK(!head || head->layout_object);
    if (!head || head->layout_object->IsBeforeInPreOrder(*layout_object)) {
      next = head;
      *head_ptr = this;
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

namespace {

bool IsScopedByElement(const ScopedCSSName* lookup_name,
                       const Element& element) {
  const StyleAnchorScope& anchor_scope =
      element.ComputedStyleRef().AnchorScope();
  if (anchor_scope.IsNone()) {
    return false;
  }
  if (anchor_scope.IsAll()) {
    return anchor_scope.AllTreeScope() == lookup_name->GetTreeScope();
  }
  const ScopedCSSNameList* scoped_names = anchor_scope.Names();
  CHECK(scoped_names);
  for (const Member<const ScopedCSSName>& scoped_name :
       scoped_names->GetNames()) {
    if (*scoped_name == *lookup_name) {
      return true;
    }
  }
  return false;
}

// https://drafts.csswg.org/css-anchor-position-1/#anchor-scope
bool InSameAnchorScope(const AnchorKey& key,
                       const LayoutObject& query_object,
                       const LayoutObject& anchor_object) {
  const ScopedCSSName* const* name = absl::get_if<const ScopedCSSName*>(&key);
  if (!name) {
    // This is an implicit anchor reference, which is unaffected
    // by anchor-scope.
    return true;
  }
  auto anchor_scope_ancestor =
      [name](const LayoutObject& layout_object) -> const Element* {
    const Element* element = To<Element>(layout_object.GetNode());
    CHECK(element);
    while ((element = LayoutTreeBuilderTraversal::ParentElement(*element)) !=
           nullptr) {
      if (IsScopedByElement(*name, *element)) {
        break;
      }
    }
    return element;
  };
  return anchor_scope_ancestor(query_object) ==
         anchor_scope_ancestor(anchor_object);
}

}  // namespace

const LogicalAnchorReference* LogicalAnchorQuery::AnchorReference(
    const LayoutObject& query_object,
    const AnchorKey& key) const {
  if (const LogicalAnchorReference* reference = Base::GetAnchorReference(key)) {
    for (const LogicalAnchorReference* result = reference; result;
         result = result->next) {
      if ((!result->is_out_of_flow ||
           result->layout_object->IsBeforeInPreOrder(query_object)) &&
          InSameAnchorScope(key, query_object, *result->layout_object)) {
        return result;
      }
    }
  }
  return nullptr;
}

void LogicalAnchorQuery::Set(const AnchorKey& key,
                             const LayoutObject& layout_object,
                             const LogicalRect& rect,
                             SetOptions options,
                             Element* element_for_display_lock) {
  HeapHashSet<Member<Element>>* display_locks = nullptr;
  if (element_for_display_lock) {
    display_locks = MakeGarbageCollected<HeapHashSet<Member<Element>>>();
    display_locks->insert(element_for_display_lock);
  }
  Set(key, MakeGarbageCollected<LogicalAnchorReference>(
               layout_object, rect, options == SetOptions::kOutOfFlow,
               display_locks));
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
    SetOptions options,
    Element* element_for_display_lock) {
  for (auto entry : physical_query) {
    // For each key, only the last reference in tree order is reachable
    // under normal circumstances. However, the presence of anchor-scope
    // can make it necessary to skip past any number of references to reach
    // an earlier one. Therefore, all references must be propagated.
    //
    // See also InSameAnchorScope.
    for (PhysicalAnchorReference* reference = entry.value; reference;
         reference = reference->next) {
      LogicalRect rect = converter.ToLogical(reference->rect);
      rect.offset += additional_offset;

      HeapHashSet<Member<Element>>* display_locks = nullptr;
      if (reference->display_locks || element_for_display_lock) {
        display_locks = MakeGarbageCollected<HeapHashSet<Member<Element>>>();
      }
      if (reference->display_locks) {
        *display_locks = *reference->display_locks;
      }
      if (element_for_display_lock) {
        display_locks->insert(element_for_display_lock);
      }
      Set(entry.key, MakeGarbageCollected<LogicalAnchorReference>(
                         *reference->layout_object, rect,
                         options == SetOptions::kOutOfFlow, display_locks));
    }
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
  anchor_value = PhysicalAnchorValueFromInsideOutside(anchor_value, is_y_axis,
                                                      is_right_or_bottom);
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
    case CSSAnchorValue::kInside:
    case CSSAnchorValue::kOutside:
      // Should have been handled by `PhysicalAnchorValueFromInsideOutside`.
      [[fallthrough]];
    case CSSAnchorValue::kStart:
    case CSSAnchorValue::kEnd:
    case CSSAnchorValue::kSelfStart:
    case CSSAnchorValue::kSelfEnd:
      // These logical values should have been converted to corresponding
      // physical values in `PhysicalAnchorValueFromLogicalOrAuto`.
      NOTREACHED_IN_MIGRATION();
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
    case CSSAnchorSizeValue::kImplicit:
      break;
  }
  NOTREACHED_IN_MIGRATION();
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
    const std::optional<PositionAreaOffsets>& position_area_offsets) {
  switch (anchor_query.Type()) {
    case CSSAnchorQueryType::kAnchor:
      return EvaluateAnchor(anchor_query.AnchorSpecifier(),
                            anchor_query.AnchorSide(),
                            anchor_query.AnchorSidePercentageOrZero(),
                            position_anchor, position_area_offsets);
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
    case Mode::kWidth:
    case Mode::kHeight:
      return false;
  }
}

bool AnchorEvaluatorImpl::AllowAnchorSize() const {
  switch (GetMode()) {
    case Mode::kWidth:
    case Mode::kHeight:
    case Mode::kLeft:
    case Mode::kRight:
    case Mode::kTop:
    case Mode::kBottom:
      return true;
    case Mode::kNone:
      return false;
  }
}

bool AnchorEvaluatorImpl::IsYAxis() const {
  return GetMode() == Mode::kTop || GetMode() == Mode::kBottom ||
         GetMode() == Mode::kHeight;
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
    const std::optional<PositionAreaOffsets>& position_area_offsets) const {
  if (!AllowAnchor()) {
    return std::nullopt;
  }

  const LogicalAnchorReference* anchor_reference =
      ResolveAnchorReference(anchor_specifier, position_anchor);
  if (!anchor_reference) {
    return std::nullopt;
  }

  UpdateAccessibilityAnchor(anchor_reference->layout_object);

  if (anchor_reference->display_locks) {
    for (auto& display_lock : *anchor_reference->display_locks) {
      display_locks_affected_by_anchors_->insert(display_lock);
    }
  }

  PhysicalRect position_area_modified_containing_block_rect =
      PositionAreaModifiedContainingBlock(position_area_offsets);

  const bool is_y_axis = IsYAxis();

  DCHECK(AnchorQuery());
  if (std::optional<LayoutUnit> result = AnchorQuery()->EvaluateAnchor(
          *anchor_reference, anchor_value, percentage,
          AvailableSizeAlongAxis(position_area_modified_containing_block_rect),
          container_converter_, self_writing_direction_,
          position_area_modified_containing_block_rect.offset, is_y_axis,
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

  if (anchor_size_value == CSSAnchorSizeValue::kImplicit) {
    if (IsYAxis()) {
      anchor_size_value = CSSAnchorSizeValue::kHeight;
    } else {
      anchor_size_value = CSSAnchorSizeValue::kWidth;
    }
  }
  const LogicalAnchorReference* anchor_reference =
      ResolveAnchorReference(anchor_specifier, position_anchor);
  if (!anchor_reference) {
    return std::nullopt;
  }

  UpdateAccessibilityAnchor(anchor_reference->layout_object);

  if (anchor_reference->display_locks) {
    for (auto& display_lock : *anchor_reference->display_locks) {
      display_locks_affected_by_anchors_->insert(display_lock);
    }
  }

  DCHECK(AnchorQuery());
  return AnchorQuery()->EvaluateSize(*anchor_reference, anchor_size_value,
                                     container_converter_.GetWritingMode(),
                                     self_writing_direction_.GetWritingMode());
}

void AnchorEvaluatorImpl::UpdateAccessibilityAnchor(
    const LayoutObject* anchor) const {
  if (!anchor->GetDocument().ExistingAXObjectCache()) {
    return;
  }

  Element* anchor_element = To<Element>(anchor->GetNode());
  if (accessibility_anchor_ && accessibility_anchor_ != anchor_element) {
    has_multiple_accessibility_anchors_ = true;
  }
  accessibility_anchor_ = anchor_element;
}

Element* AnchorEvaluatorImpl::AccessibilityAnchor() const {
  if (has_multiple_accessibility_anchors_) {
    return nullptr;
  }
  return accessibility_anchor_;
}

void AnchorEvaluatorImpl::ClearAccessibilityAnchor() {
  accessibility_anchor_ = nullptr;
  has_multiple_accessibility_anchors_ = false;
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
    top =
        EvaluateAnchor(*AnchorSpecifierValue::Default(),
                       CSSAnchorValue::kCenter, dummy_percentage,
                       builder.PositionAnchor(), builder.PositionAreaOffsets());
  }
  {
    AnchorScope anchor_scope(AnchorScope::Mode::kLeft, this);
    left =
        EvaluateAnchor(*AnchorSpecifierValue::Default(),
                       CSSAnchorValue::kCenter, dummy_percentage,
                       builder.PositionAnchor(), builder.PositionAreaOffsets());
  }
  CHECK(top.has_value() == left.has_value());
  if (top.has_value()) {
    return PhysicalOffset(left.value(), top.value());
  }
  return std::nullopt;
}

std::optional<PositionAreaOffsets>
AnchorEvaluatorImpl::ComputePositionAreaOffsetsForLayout(
    const ScopedCSSName* position_anchor,
    PositionArea position_area) {
  CHECK(!position_area.IsNone());

  if (!DefaultAnchor(position_anchor)) {
    return std::nullopt;
  }
  PositionArea physical_position_area = position_area.ToPhysical(
      container_converter_.GetWritingDirection(), self_writing_direction_);

  std::optional<LayoutUnit> top;
  std::optional<LayoutUnit> bottom;
  std::optional<LayoutUnit> left;
  std::optional<LayoutUnit> right;

  // The PositionArea::Used*() methods returns either an anchor() function or
  // nullopt (representing a 0px length), using top/left/right/bottom, to adjust
  // the containing block to align with either of the physical edges of the
  // default anchor.
  //
  // Note that the inset adjustment is already set to zero above, so there's
  // nothing to do here for nullopt values.
  if (std::optional<blink::AnchorQuery> query =
          physical_position_area.UsedTop()) {
    AnchorScope anchor_scope(AnchorScope::Mode::kTop, this);
    top = Evaluate(query.value(), position_anchor,
                   /* position_area_offsets */ std::nullopt);
  }
  if (std::optional<blink::AnchorQuery> query =
          physical_position_area.UsedBottom()) {
    AnchorScope anchor_scope(AnchorScope::Mode::kBottom, this);
    bottom = Evaluate(query.value(), position_anchor,
                      /* position_area_offsets */ std::nullopt);
  }
  if (std::optional<blink::AnchorQuery> query =
          physical_position_area.UsedLeft()) {
    AnchorScope anchor_scope(AnchorScope::Mode::kLeft, this);
    left = Evaluate(query.value(), position_anchor,
                    /* position_area_offsets */ std::nullopt);
  }
  if (std::optional<blink::AnchorQuery> query =
          physical_position_area.UsedRight()) {
    AnchorScope anchor_scope(AnchorScope::Mode::kRight, this);
    right = Evaluate(query.value(), position_anchor,
                     /* position_area_offsets */ std::nullopt);
  }
  return PositionAreaOffsets(top, bottom, left, right);
}

PhysicalRect AnchorEvaluatorImpl::PositionAreaModifiedContainingBlock(
    const std::optional<PositionAreaOffsets>& position_area_offsets) const {
  return cached_position_area_modified_containing_block_.Get(
      position_area_offsets, [&]() {
        if (!position_area_offsets.has_value()) {
          return containing_block_rect_;
        }

        PhysicalRect position_area_modified_containing_block_rect =
            containing_block_rect_;

        LayoutUnit top = position_area_offsets->top.value_or(LayoutUnit());
        LayoutUnit bottom =
            position_area_offsets->bottom.value_or(LayoutUnit());
        LayoutUnit left = position_area_offsets->left.value_or(LayoutUnit());
        LayoutUnit right = position_area_offsets->right.value_or(LayoutUnit());

        // Reduce the container size and adjust the offset based on the
        // position-area.
        position_area_modified_containing_block_rect.ContractEdges(
            top, right, bottom, left);

        // For 'center' values (aligned with start and end anchor sides), the
        // containing block is aligned and sized with the anchor, regardless of
        // whether it's inside the original containing block or not. Otherwise,
        // ContractEdges above might have created a negative size if the
        // position-area is aligned with an anchor side outside the containing
        // block.
        if (position_area_modified_containing_block_rect.size.width <
            LayoutUnit()) {
          DCHECK(left == LayoutUnit() || right == LayoutUnit())
              << "If aligned to both anchor edges, the size should never be "
                 "negative.";
          // Collapse the inline size to 0 and align with the single anchor edge
          // defined by the position-area.
          if (left == LayoutUnit()) {
            DCHECK(right != LayoutUnit());
            position_area_modified_containing_block_rect.offset.left +=
                position_area_modified_containing_block_rect.size.width;
          }
          position_area_modified_containing_block_rect.size.width =
              LayoutUnit();
        }
        if (position_area_modified_containing_block_rect.size.height <
            LayoutUnit()) {
          DCHECK(top == LayoutUnit() || bottom == LayoutUnit())
              << "If aligned to both anchor edges, the size should never be "
                 "negative.";
          // Collapse the block size to 0 and align with the single anchor edge
          // defined by the position-area.
          if (top == LayoutUnit()) {
            DCHECK(bottom != LayoutUnit());
            position_area_modified_containing_block_rect.offset.top +=
                position_area_modified_containing_block_rect.size.height;
          }
          position_area_modified_containing_block_rect.size.height =
              LayoutUnit();
        }

        return position_area_modified_containing_block_rect;
      });
}

void LogicalAnchorReference::Trace(Visitor* visitor) const {
  visitor->Trace(layout_object);
  visitor->Trace(next);
  visitor->Trace(display_locks);
}

void PhysicalAnchorReference::Trace(Visitor* visitor) const {
  visitor->Trace(layout_object);
  visitor->Trace(next);
  visitor->Trace(display_locks);
}

}  // namespace blink
