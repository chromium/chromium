// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SNAP_COORDINATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SNAP_COORDINATOR_H_

#include "base/macros.h"
#include "cc/input/scroll_snap_data.h"
#include "cc/input/snap_selection_strategy.h"
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
//   A scroll container that has 'scroll-snap-type' value other
//   than 'none'.
//   However, we maintain a snap container entry for a scrollable area even if
//   its snap type is 'none'. This is because while the scroller does not snap,
//   it still captures the snap areas in its subtree.
// Snap area:
//   A snap container's descendant that contributes snap positions. An element
//   only contributes snap positions to its nearest ancestor (on the element’s
//   containing block chain) scroll container.
//
// For more information see spec: https://drafts.csswg.org/css-snappoints/
class CORE_EXPORT SnapCoordinator final
    : public GarbageCollected<SnapCoordinator> {
 public:
  explicit SnapCoordinator();
  ~SnapCoordinator();
  void Trace(blink::Visitor* visitor) {}

  void AddSnapContainer(LayoutBox& snap_container);
  void RemoveSnapContainer(LayoutBox& snap_container);

  void SnapContainerDidChange(LayoutBox&);
  void SnapAreaDidChange(LayoutBox&, cc::ScrollSnapAlign);

  // Calculate the SnapAreaData for the specific snap area in its snap
  // container.
  cc::SnapAreaData CalculateSnapAreaData(const LayoutBox& snap_area,
                                         const LayoutBox& snap_container);

  // Called by LocalFrameView::PerformPostLayoutTasks(), so that the snap data
  // are updated whenever a layout happens.
  void UpdateAllSnapContainerData();
  void UpdateSnapContainerData(LayoutBox&);

#ifndef NDEBUG
  void ShowSnapAreaMap();
  void ShowSnapAreasFor(const LayoutBox*);
  void ShowSnapDataFor(const LayoutBox*);
#endif

 private:
  friend class SnapCoordinatorTest;

  HashSet<LayoutBox*> snap_containers_;
  DISALLOW_COPY_AND_ASSIGN(SnapCoordinator);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SNAP_COORDINATOR_H_
