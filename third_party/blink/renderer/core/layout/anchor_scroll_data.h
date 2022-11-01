// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_ANCHOR_SCROLL_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_ANCHOR_SCROLL_DATA_H_

#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/scroll/scroll_snapshot_client.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {

class Element;
class PaintLayer;

// Created for each anchor-positioned element that uses anchor-scroll.
// Stores a snapshot of all the scroll containers of the anchor up to the
// containing block (exclusively) for use by layout, paint and compositing.
// The snapshot is updated once per frame update on top of animation frame to
// avoid layout cycling.
class AnchorScrollData : public GarbageCollected<AnchorScrollData>,
                         public ScrollSnapshotClient {
 public:
  explicit AnchorScrollData(Element*);

  Element* OwnerElement() const { return owner_; }

  bool HasTranslation() const { return scroll_container_layers_.size(); }
  gfx::Vector2dF AccumulatedScrollOffset() const {
    return accumulated_scroll_offset_;
  }
  gfx::Vector2d AccumulatedScrollOrigin() const {
    return accumulated_scroll_origin_;
  }
  const HeapVector<Member<const PaintLayer>>& ScrollContainerLayers() const {
    return scroll_container_layers_;
  }

  // Utility function that returns accumulated_scroll_offset_ rounded as a
  // PhysicalOffset.
  PhysicalOffset TranslationAsPhysicalOffset() const {
    return -PhysicalOffset::FromVector2dFFloor(accumulated_scroll_offset_);
  }

  // Returns whether |owner_| is still an anchor-positioned element using |this|
  // as its AnchroScrollData.
  bool IsActive() const;

  // ScrollSnapshotClient:
  void UpdateSnapshot() override;
  bool ValidateSnapshot() override;
  bool ShouldScheduleNextService() override;

  void Trace(Visitor*) const override;

 private:
  enum class SnapshotDiff { kNone, kScrollers, kOffsetOnly };
  // Takes an up-to-date snapshot, and compares it with the existing one.
  // If |update| is true, also rewrites the existing snapshot.
  SnapshotDiff TakeAndCompareSnapshot(bool update);

  void InvalidateLayout();
  void InvalidatePaint();

  // The anchor-positioned element.
  Member<Element> owner_;

  // Paint layers of the ancestor scroll containers of the anchor element, up to
  // the containing block of |owner_| (exclusively).
  HeapVector<Member<const PaintLayer>> scroll_container_layers_;

  // Sum of the scroll offsets of the above scroll containers. This is the
  // offset that the element should be translated in position-fallback choosing
  // and paint.
  gfx::Vector2dF accumulated_scroll_offset_;

  // Sum of the scroll origins of the above scroll containers. Used by
  // compositor to deal with writing modes.
  gfx::Vector2d accumulated_scroll_origin_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_ANCHOR_SCROLL_DATA_H_
