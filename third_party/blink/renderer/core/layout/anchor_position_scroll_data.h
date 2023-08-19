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

class Element;

// https://drafts.csswg.org/css-anchor-position-1/#scroll
//
// Created for each anchor-positioned element that needs to track the scroll
// offset of another element (its default anchor or the additional
// fallback-bounds rect).
//
// Stores a snapshot of all the scroll containers of the anchor up to the
// containing block (exclusively) for use by layout, paint and compositing.
// Also stores a similar snapshot for the target of the
// 'position-fallback-bounds' property.
//
// The snapshot is passed as input to the position fallback algorithm.
//
// The snapshot is updated once per frame update on top of animation frame to
// avoid layout cycling. If there is any change, we trigger an update to
// layout and/or paint.
class AnchorPositionScrollData
    : public GarbageCollected<AnchorPositionScrollData>,
      public ScrollSnapshotClient,
      public ElementRareDataField {
 public:
  explicit AnchorPositionScrollData(Element*);
  virtual ~AnchorPositionScrollData();

  struct ScrollContainersData {
    DISALLOW_NEW();

    // Compositor element ids of the ancestor scroll containers of some element
    // (anchor or position-fallback-bounds), up to the containing block of
    // `owner_` (exclusively).
    Vector<CompositorElementId> scroll_container_ids;

    // Sum of the scroll offsets of the above scroll containers. This is the
    // snapshotted scroll offset when tracking the anchor element, or the offset
    // applied to additional fallback-bounds rect.
    gfx::Vector2dF accumulated_scroll_offset;

    // Sum of the scroll origins of the above scroll containers.
    gfx::Vector2d accumulated_scroll_origin;

    // Whether viewport is in `scroll_container_ids`.
    bool scroll_containers_include_viewport = false;
  };

  Element* OwnerElement() const { return owner_; }

  bool HasTranslation() const { return scroll_container_ids_.size(); }
  gfx::Vector2dF AccumulatedScrollOffset() const {
    return accumulated_scroll_offset_;
  }
  gfx::Vector2d AccumulatedScrollOrigin() const {
    return accumulated_scroll_origin_;
  }
  const Vector<CompositorElementId>& ScrollContainerIds() const {
    return scroll_container_ids_;
  }
  gfx::Vector2dF AdditionalBoundsScrollOffset() const {
    return additional_bounds_scroll_offset_;
  }

  // Returns true if the snapshotted scroll offset is affected by viewport's
  // scroll offset.
  bool IsAffectedByViewportScrolling() const {
    return is_affected_by_viewport_scrolling_;
  }

  // Utility function that returns accumulated_scroll_offset_ rounded as a
  // PhysicalOffset.
  // TODO(crbug.com/1309178): It's conceptually wrong to use
  // Physical/LogicalOffset, which only represents the location of a box within
  // a container, to represent a scroll offset. Stop using this function.
  PhysicalOffset TranslationAsPhysicalOffset() const {
    return -PhysicalOffset::FromVector2dFFloor(accumulated_scroll_offset_);
  }

  // Returns whether `owner_` is still an anchor-positioned element using `this`
  // as its AnchroScrollData.
  bool IsActive() const;

  // ScrollSnapshotClient:
  void UpdateSnapshot() override;
  bool ValidateSnapshot() override;
  bool ShouldScheduleNextService() override;
  bool IsAnchorPositionScrollData() const override { return true; }

  void Trace(Visitor*) const override;

 private:
  enum class SnapshotDiff { kNone, kScrollersOrFallbackPosition, kOffsetOnly };
  // Takes an up-to-date snapshot, and compares it with the existing one.
  // If `update` is true, also rewrites the existing snapshot.
  SnapshotDiff TakeAndCompareSnapshot(bool update);
  bool IsFallbackPositionValid(
      const gfx::Vector2dF& new_accumulated_scroll_offset,
      const gfx::Vector2dF& new_additional_bounds_scroll_offset) const;

  void InvalidateLayoutAndPaint();
  void InvalidatePaint();

  // ValidateSnapshot is called every frame, but AnchorPositionScrollData only
  // needs to perform the validation once (during the frame it was created).
  bool is_snapshot_validated_ = false;

  // The anchor-positioned element.
  Member<Element> owner_;

  // Compositor element ids of the ancestor scroll containers of the anchor, up
  // to the containing block of `owner_` (exclusively).
  Vector<CompositorElementId> scroll_container_ids_;

  // The snapshotted scroll offset, calculated as the sum of the scroll offsets
  // of the above scroll containers.
  gfx::Vector2dF accumulated_scroll_offset_;

  // Sum of the scroll origins of the above scroll containers. Used by
  // compositor to deal with writing modes.
  gfx::Vector2d accumulated_scroll_origin_;

  // The scroll offset applied to the additional fallback-bounds rect.
  gfx::Vector2dF additional_bounds_scroll_offset_;

  bool is_affected_by_viewport_scrolling_ = false;
};

template <>
struct DowncastTraits<AnchorPositionScrollData> {
  static bool AllowFrom(const ScrollSnapshotClient& client) {
    return client.IsAnchorPositionScrollData();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_ANCHOR_POSITION_SCROLL_DATA_H_
