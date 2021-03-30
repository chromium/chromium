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
// snap areas.
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
  void Trace(Visitor* visitor) const {}

  void AddSnapContainer(LayoutBox& snap_container);
  void RemoveSnapContainer(LayoutBox& snap_container);

  void SnapContainerDidChange(LayoutBox&);
  void SnapAreaDidChange(LayoutBox&, cc::ScrollSnapAlign);

  // Calculate the SnapAreaData for the specific snap area in its snap
  // container.
  cc::SnapAreaData CalculateSnapAreaData(const LayoutBox& snap_area,
                                         const LayoutBox& snap_container);

  bool AnySnapContainerDataNeedsUpdate() const {
    return any_snap_container_data_needs_update_;
  }
  void SetAnySnapContainerDataNeedsUpdate(bool needs_update) {
    any_snap_container_data_needs_update_ = needs_update;
  }
  // Called by Document::PerformScrollSnappingTasks() whenever a style or layout
  // change happens. This will update all snap container data that was affected
  // by the style/layout change.
  void UpdateAllSnapContainerDataIfNeeded();

  // Resnaps all snap containers to their current snap target, or to the
  // closest snap point if there is no target (e.g. on the initial layout or if
  // the previous snapped target was removed).
  void ResnapAllContainersIfNeeded();

  void UpdateSnapContainerData(LayoutBox&);

#ifndef NDEBUG
  void ShowSnapAreaMap();
  void ShowSnapAreasFor(const LayoutBox*);
  void ShowSnapDataFor(const LayoutBox*);
#endif

 private:
  friend class SnapCoordinatorTest;

  HashSet<LayoutBox*> snap_containers_;
  bool any_snap_container_data_needs_update_ = true;

  // Used for reporting to UMA when snapping on the initial layout affects the
  // initial scroll position.
  bool did_first_resnap_all_containers_ = false;

  DISALLOW_COPY_AND_ASSIGN(SnapCoordinator);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SNAP_COORDINATOR_H_
