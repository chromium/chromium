// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SNAP_COORDINATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SNAP_COORDINATOR_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class LayoutBox;

// Snap Coordinator keeps track of snap containers and all of their associated
// snap areas. It also contains the logic to generate the list of valid snap
// positions for a given snap container.
//
// Snap container:
//   An scroll container that has 'scroll-snap-type' value other
//   than 'none'.
// Snap area:
//   A snap container's descendant that contributes snap positions. An element
//   only contributes snap positions to its nearest ancestor (on the element’s
//   containing block chain) scroll container.
//
// For more information see spec: https://drafts.csswg.org/css-snappoints/
class CORE_EXPORT SnapCoordinator final
    : public GarbageCollectedFinalized<SnapCoordinator> {
 public:
  static SnapCoordinator* Create();
  ~SnapCoordinator();
  void Trace(blink::Visitor* visitor) {}

  void SnapContainerDidChange(LayoutBox&, ScrollSnapType);
  void SnapAreaDidChange(LayoutBox&, ScrollSnapAlign);

  // Returns the SnapContainerData if the snap container has one.
  base::Optional<SnapContainerData> GetSnapContainerData(
      const LayoutBox& snap_container) const;

  // Calculate the SnapAreaData for the specific snap area in its snap
  // container.
  SnapAreaData CalculateSnapAreaData(const LayoutBox& snap_area,
                                     const LayoutBox& snap_container);

  // Called by LocalFrameView::PerformPostLayoutTasks(), so that the snap data
  // are updated whenever a layout happens.
  void UpdateAllSnapContainerData();
  void UpdateSnapContainerData(const LayoutBox&);

  // SnapForEndPosition() and SnapForDirection() return true if snapping was
  // performed, and false otherwise.
  bool SnapForEndPosition(const LayoutBox& snap_container,
                          bool scrolled_x,
                          bool scrolled_y) const;
  bool SnapForDirection(const LayoutBox& snap_container,
                        const ScrollOffset& delta) const;

  base::Optional<FloatPoint> GetSnapPosition(
      const LayoutBox& snap_container,
      const SnapSelectionStrategy& strategy) const;

#ifndef NDEBUG
  void ShowSnapAreaMap();
  void ShowSnapAreasFor(const LayoutBox*);
  void ShowSnapDataFor(const LayoutBox*);
#endif

 private:
  friend class SnapCoordinatorTest;
  explicit SnapCoordinator();
  bool PerformSnapping(const LayoutBox& snap_container,
                       const SnapSelectionStrategy& strategy) const;

  HashMap<const LayoutBox*, SnapContainerData> snap_container_map_;
  DISALLOW_COPY_AND_ASSIGN(SnapCoordinator);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SNAP_COORDINATOR_H_
