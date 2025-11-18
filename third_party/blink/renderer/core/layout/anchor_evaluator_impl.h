// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_ANCHOR_EVALUATOR_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_ANCHOR_EVALUATOR_IMPL_H_

#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/anchor_evaluator.h"
#include "third_party/blink/renderer/core/css/css_anchor_query_enums.h"
#include "third_party/blink/renderer/core/css/out_of_flow_data.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/style/style_position_anchor.h"
#include "third_party/blink/renderer/platform/geometry/physical_offset.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"

namespace blink {

class AnchorMap;
class AnchorQuery;
class AnchorSpecifierValue;
class Element;
class LayoutBox;
class LayoutObject;
class PaintLayer;
class PhysicalAnchorReference;

class CORE_EXPORT AnchorEvaluatorImpl : public AnchorEvaluator {
  STACK_ALLOCATED();

 public:
  // An empty evaluator that always return `nullopt`. This instance can still
  // compute `HasAnchorFunctions()` and is used to keep the container's
  // writing-direction for logical flip-* operations.
  explicit AnchorEvaluatorImpl(WritingDirectionMode container_writing_direction)
      : container_writing_direction_(container_writing_direction) {}

  AnchorEvaluatorImpl(const LayoutBox& query_box,
                      const AnchorMap& anchor_map,
                      const LayoutObject* implicit_anchor,
                      const LayoutObject* css_containing_block,
                      WritingDirectionMode container_writing_direction,
                      const PhysicalRect& container_rect,
                      const std::optional<PhysicalRect>& scroll_rect)
      : query_box_(&query_box),
        anchor_map_(&anchor_map),
        implicit_anchor_(implicit_anchor),
        query_box_actual_containing_block_(css_containing_block),
        container_writing_direction_(container_writing_direction),
        container_rect_(container_rect),
        scroll_rect_(scroll_rect),
        display_locks_affected_by_anchors_(
            MakeGarbageCollected<GCedHeapHashSet<Member<Element>>>()) {
    DCHECK(anchor_map_);
  }

  // Returns true if any anchor reference in the axis is in the same scroll
  // container as the default anchor, in which case we need scroll adjustment in
  // the axis after layout.
  bool NeedsScrollAdjustmentInX() const {
    return needs_scroll_adjustment_in_x_;
  }
  bool NeedsScrollAdjustmentInY() const {
    return needs_scroll_adjustment_in_y_;
  }

  // Evaluates the given anchor query. Returns nullopt if the query invalid
  // (e.g., no target or wrong axis).
  std::optional<LayoutUnit> Evaluate(
      const AnchorQuery&,
      const StylePositionAnchor& position_anchor,
      const std::optional<PositionAreaOffsets>&) override;

  std::optional<PositionAreaOffsets> ComputePositionAreaOffsetsForLayout(
      const StylePositionAnchor& position_anchor,
      PositionArea position_area) override;

  std::optional<PhysicalOffset> ComputeAnchorCenterOffsets(
      const ComputedStyleBuilder&) override;

  WritingDirectionMode GetContainerWritingDirection() const override {
    return container_writing_direction_;
  }

  const AnchorMap* GetAnchorMap() const { return anchor_map_; }

  // Given the computed value of `position-anchor`, returns the default anchor.
  const LayoutObject* DefaultAnchor(
      const StylePositionAnchor& position_anchor) const;

  // Returns the most recent anchor evaluated. If more than one anchor has been
  // evaluated so far, nullptr is returned. This is done to avoid extra noise
  // for assistive tech.
  Element* AccessibilityAnchor() const;
  void ClearAccessibilityAnchor();

  GCedHeapHashSet<Member<Element>>* GetDisplayLocksAffectedByAnchors() const {
    return display_locks_affected_by_anchors_;
  }

  const OutOfFlowData::RememberedScrollOffsets* LastUsedScrollOffsets() {
    return used_scroll_offsets_;
  }
  void ClearLastUsedScrollOffsets() { used_scroll_offsets_ = nullptr; }
  void SetRememberedScrollOffsets(
      const OutOfFlowData::RememberedScrollOffsets* offsets) {
    remembered_scroll_offsets_ = offsets;
  }
  void ClearRememberedScrollOffsets() { remembered_scroll_offsets_ = nullptr; }

  bool DidResolveAnchorWithRunningTransformAnimation() const {
    return did_resolve_anchor_with_running_transform_animation_;
  }

 private:
  // Unless nullptr is returned, the returned anchor reference is guaranteed to
  // have a valid LayoutObject.
  const PhysicalAnchorReference* ResolveAnchorReference(
      const AnchorSpecifierValue& anchor_specifier,
      const StylePositionAnchor& position_anchor) const;

  bool ShouldUseScrollAdjustmentFor(
      const LayoutObject* anchor,
      const StylePositionAnchor& position_anchor) const;

  std::optional<LayoutUnit> EvaluateAnchor(
      const AnchorSpecifierValue& anchor_specifier,
      CSSAnchorValue anchor_value,
      float percentage,
      const StylePositionAnchor& position_anchor,
      const std::optional<PositionAreaOffsets>&);
  std::optional<LayoutUnit> EvaluateAnchorSize(
      const AnchorSpecifierValue& anchor_specifier,
      CSSAnchorSizeValue anchor_size_value,
      const StylePositionAnchor& position_anchor);
  const PhysicalAnchorReference* ResolveAnchorForEvaluation(
      const AnchorSpecifierValue&,
      const StylePositionAnchor& position_anchor);
  PhysicalRect CalculateAnchorRectWithScrollOffset(
      const PhysicalAnchorReference&);

  void UpdateAccessibilityAnchor(const LayoutObject* anchor);

  const PaintLayer* DefaultAnchorScrollContainerLayer(
      const StylePositionAnchor& position_anchor) const;

  bool AllowAnchor() const;
  bool AllowAnchorSize() const;
  bool IsYAxis() const;
  bool IsRightOrBottom() const;

  LayoutUnit AvailableSizeAlongAxis(
      const PhysicalRect& position_area_modified_containing_block_rect) const {
    return IsYAxis() ? position_area_modified_containing_block_rect.Height()
                     : position_area_modified_containing_block_rect.Width();
  }

  // Returns the containing block, further constrained by the position-area.
  // Not to be confused with the inset-modified containing block.
  PhysicalRect PositionAreaModifiedContainingBlock(
      const std::optional<PositionAreaOffsets>&,
      bool has_default_anchor) const;

  const LayoutBox* query_box_ = nullptr;
  const AnchorMap* anchor_map_ = nullptr;
  const LayoutObject* implicit_anchor_ = nullptr;

  // The (CSS) containing block of the querying element. This should only be set
  // if the containing block in the physical fragment tree is not the same as
  // this. This inconsistency happens when OOFs participate in block
  // fragmentation. If specified, some additional tree-walking will be performed
  // when looking for acceptable anchors.
  const LayoutObject* query_box_actual_containing_block_ = nullptr;

  WritingDirectionMode container_writing_direction_{WritingMode::kHorizontalTb,
                                                    TextDirection::kLtr};

  // Either width or height will be used, depending on IsYAxis().
  const PhysicalRect container_rect_;
  const std::optional<PhysicalRect> scroll_rect_;

  // A single-value cache. If a call to Get has the same key as the last call,
  // then the cached result it returned. Otherwise, the value is created using
  // CreationFunc, then returned.
  template <typename KeyType, typename ValueType>
  class CachedValue {
    STACK_ALLOCATED();

   public:
    template <typename CreationFunc>
    ValueType Get(KeyType key, CreationFunc create) {
      if (key_ && *key_ == key) {
        DCHECK_EQ(value_.value(), create());
        return value_.value();
      }
      key_ = key;
      value_ = create();
      return value_.value();
    }

   private:
    std::optional<KeyType> key_;
    std::optional<ValueType> value_;
  };

  // Caches most recent result of DefaultAnchor.
  mutable CachedValue<StylePositionAnchor, const LayoutObject*>
      cached_default_anchor_;

  // Caches most recent result of DefaultAnchorScrollContainerLayer.
  mutable CachedValue<StylePositionAnchor, const PaintLayer*>
      cached_default_anchor_scroll_container_layer_;

  bool needs_scroll_adjustment_in_x_ = false;
  bool needs_scroll_adjustment_in_y_ = false;

  // Most recent anchor evaluated, used for accessibility. This value is cleared
  // before a @position-try rule is applied.
  Element* accessibility_anchor_ = nullptr;

  // True if more than one anchor has been evaluated so far. This value is
  // cleared before a @position-try rule is applied.
  bool has_multiple_accessibility_anchors_ = false;

  bool did_resolve_anchor_with_running_transform_animation_ = false;

  // A set of elements whose display locks' skipping status are potentially
  // impacted by anchors found by this evaluator.
  GCedHeapHashSet<Member<Element>>* display_locks_affected_by_anchors_ =
      nullptr;

  const OutOfFlowData::RememberedScrollOffsets* remembered_scroll_offsets_ =
      nullptr;

  OutOfFlowData::RememberedScrollOffsets* used_scroll_offsets_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_ANCHOR_EVALUATOR_IMPL_H_
