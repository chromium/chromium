// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_ANCHOR_POSITION_SCROLL_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_ANCHOR_POSITION_SCROLL_DATA_H_

#include "third_party/blink/renderer/core/dom/element_rare_data_field.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/scroll/scroll_snapshot_client.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {

class AnchorPositionVisibilityObserver;
class Element;
class LayoutObject;

// Created for each anchor-positioned element.
//
// https://drafts.csswg.org/css-anchor-position-1/#scroll
//
// To adjust the location of the anchor-positioned element based on scroll,
// sticky and anchor-positioning offsets between this element and the anchor
// (the default anchor and the target of the `position-fallback-bounds`
// property, respectively), this class stores a snapshot of all the scroll
// adjustment containers [1] of the anchor up to the containing block
// (exclusively) of the anchor-positioned element, along the containing block
// hierarchy. Note that "containing block" is in the spec meaning, which
// corresponds to LayoutObject::Container() instead of ContainingBlock().
//
// [1] An element is a scroll adjustment container if it is a scroll container,
// has sticky position, or is anchor-positioned.
//
// https://drafts.csswg.org/css-anchor-position-1/#fallback
//
// Also stores a snapshot of the scroll offset of the scroll container of the
// anchor-positioned element, which affects position fallback.
//
// The snapshot passed as input to the position fallback and position visibility
// algorithm.
//
// The snapshot is updated once per frame update on top of animation frame to
// avoid layout cycling. If there is any change, we trigger an update to
// layout and/or paint.
class AnchorPositionScrollData
    : public GarbageCollected<AnchorPositionScrollData>,
      public ScrollSnapshotClient,
      public ElementRareDataField {
 public:
  explicit AnchorPositionScrollData(Element* anchored_element);
  virtual ~AnchorPositionScrollData();

  Element* AnchoredElement() const { return anchored_element_.Get(); }

  bool NeedsScrollAdjustment() const {
    return !default_anchor_adjustment_data_.adjustment_container_ids.empty();
  }
  bool NeedsScrollAdjustmentInX() const {
    return default_anchor_adjustment_data_.needs_scroll_adjustment_in_x;
  }
  bool NeedsScrollAdjustmentInY() const {
    return default_anchor_adjustment_data_.needs_scroll_adjustment_in_y;
  }

  gfx::Vector2dF TotalOffset() const {
    return default_anchor_adjustment_data_.accumulated_adjustment +
           default_anchor_adjustment_data_
               .anchored_element_container_scroll_offset;
  }
  gfx::Vector2dF AccumulatedAdjustment() const {
    return default_anchor_adjustment_data_.accumulated_adjustment;
  }
  gfx::Vector2d AccumulatedAdjustmentScrollOrigin() const {
    return default_anchor_adjustment_data_.accumulated_adjustment_scroll_origin;
  }
  const Vector<CompositorElementId>& AdjustmentContainerIds() const {
    return default_anchor_adjustment_data_.adjustment_container_ids;
  }
  gfx::Vector2dF AdditionalBoundsOffset() const {
    return additional_bounds_offset_;
  }

  // Returns true if the snapshotted scroll offset is affected by viewport's
  // scroll offset.
  bool IsAffectedByViewportScrolling() const {
    return default_anchor_adjustment_data_.containers_include_viewport;
  }

  // Utility function that returns AccumulatedAdjustment() rounded as a
  // PhysicalOffset.
  // TODO(crbug.com/1309178): It's conceptually wrong to use
  // Physical/LogicalOffset, which only represents the location of a box within
  // a container, to represent a scroll offset. Stop using this function.
  PhysicalOffset TranslationAsPhysicalOffset() const {
    return -PhysicalOffset::FromVector2dFFloor(AccumulatedAdjustment());
  }

  // Returns whether `anchored_element_` is still an anchor-positioned element
  // using `this` as its AnchroScrollData.
  bool IsActive() const;

  // ScrollSnapshotClient:
  void UpdateSnapshot() override;
  bool ValidateSnapshot() override;
  bool ShouldScheduleNextService() override;
  bool IsAnchorPositionScrollData() const override { return true; }

  AnchorPositionVisibilityObserver& EnsureAnchorPositionVisibilityObserver();
  AnchorPositionVisibilityObserver* GetAnchorPositionVisibilityObserver()
      const {
    return position_visibility_observer_;
  }

  // Whether the default anchor or any ancestor of the default anchor (until
  // the container of the `anchored_element_`, not included) is also anchor
  // positioned.
  bool DefaultAnchorHasChainedAnchor() const {
    return default_anchor_adjustment_data_.has_chained_anchor;
  }

  void Trace(Visitor*) const override;

 private:
  enum class SnapshotDiff { kNone, kScrollersOrFallbackPosition, kOffsetOnly };

  struct AdjustmentData {
    DISALLOW_NEW();

    // Compositor element ids of the ancestor scroll adjustment containers
    // (see the class documentation) of some element (anchor or
    // position-fallback-bounds), up to the containing block of
    // `anchored_element_` (exclusively), along the containing block hierarchy.
    Vector<CompositorElementId> adjustment_container_ids;

    // Sum of the adjustment offsets of the above containers. This includes
    // snapshots of
    // - scroll offsets of scroll containers,
    // - opposite of sticky offsets of stick-positioned containers,
    // - `accumulated_adjustment` of anchor-positioned containers.
    gfx::Vector2dF accumulated_adjustment;

    // Sum of the scroll origins of scroll containers in the above containers.
    // Used by the compositor to deal with writing modes.
    gfx::Vector2d accumulated_adjustment_scroll_origin;

    // The scroll offset of the containing block of `anchored_element_` if it's
    // a scroll container. The offset doesn't contribute to the adjustment, but
    // may affect the results of position fallback and position visibility.
    gfx::Vector2dF anchored_element_container_scroll_offset;

    // Whether viewport is in `container_ids`.
    bool containers_include_viewport = false;

    // These fields are for the default anchor only. The values are from the
    // fields with the same names in OutOfFlowLayoutPart::OffsetInfo. See
    // documentation there for their meanings.
    bool needs_scroll_adjustment_in_x = false;
    bool needs_scroll_adjustment_in_y = false;

    bool has_chained_anchor = false;
  };

  AdjustmentData ComputeAdjustmentContainersData(
      const LayoutObject& anchor_or_bounds) const;
  AdjustmentData ComputeDefaultAnchorAdjustmentData() const;
  gfx::Vector2dF ComputeAdditionalBoundsOffset() const;
  // Takes an up-to-date snapshot, and compares it with the existing one.
  // If `update` is true, also rewrites the existing snapshot.
  SnapshotDiff TakeAndCompareSnapshot(bool update);
  bool IsFallbackPositionValid(
      const gfx::Vector2dF& new_accumulated_adjustment,
      const gfx::Vector2dF& new_anchored_element_container_scroll_offset,
      const gfx::Vector2dF& new_additional_bounds_offset) const;

  void InvalidateLayoutAndPaint();
  void InvalidatePaint();

  // ValidateSnapshot is called every frame, but AnchorPositionScrollData only
  // needs to perform the validation once (during the frame it was created).
  bool is_snapshot_validated_ = false;

  // The anchor-positioned element.
  Member<Element> anchored_element_;

  AdjustmentData default_anchor_adjustment_data_;

  // The accumulated adjustment applied to the additional fallback-bounds rect.
  gfx::Vector2dF additional_bounds_offset_;

  Member<AnchorPositionVisibilityObserver> position_visibility_observer_;
};

template <>
struct DowncastTraits<AnchorPositionScrollData> {
  static bool AllowFrom(const ScrollSnapshotClient& client) {
    return client.IsAnchorPositionScrollData();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_ANCHOR_POSITION_SCROLL_DATA_H_
