// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_SCROLL_PAINT_PROPERTY_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_SCROLL_PAINT_PROPERTY_NODE_H_

#include "base/optional.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_property_node.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scroll/main_thread_scrolling_reason.h"
#include "third_party/blink/renderer/platform/scroll/overscroll_behavior.h"
#include "third_party/blink/renderer/platform/scroll/scroll_snap_data.h"

namespace blink {

using MainThreadScrollingReasons = uint32_t;

// A scroll node contains auxiliary scrolling information which includes how far
// an area can be scrolled, main thread scrolling reasons, etc. Scroll nodes
// are referenced by TransformPaintPropertyNodes that are used for the scroll
// offset translation, though scroll offset translation can exist without a
// scroll node (e.g., overflow: hidden).
//
// Main thread scrolling reasons force scroll updates to go to the main thread
// and can have dependencies on other nodes. For example, all parents of a
// scroll node with background attachment fixed set should also have it set.
//
// The scroll tree differs from the other trees because it does not affect
// geometry directly.
class PLATFORM_EXPORT ScrollPaintPropertyNode
    : public PaintPropertyNode<ScrollPaintPropertyNode> {
 public:
  // To make it less verbose and more readable to construct and update a node,
  // a struct with default values is used to represent the state.
  struct State {
    IntRect container_rect;
    IntSize contents_size;
    bool user_scrollable_horizontal = false;
    bool user_scrollable_vertical = false;
    bool scrolls_inner_viewport = false;
    bool scrolls_outer_viewport = false;
    bool max_scroll_offset_affected_by_page_scale = false;
    MainThreadScrollingReasons main_thread_scrolling_reasons =
        MainThreadScrollingReason::kNotScrollingOnMain;
    // The scrolling element id is stored directly on the scroll node and not
    // on the associated TransformPaintPropertyNode used for scroll offset.
    CompositorElementId compositor_element_id;
    OverscrollBehavior overscroll_behavior = blink::OverscrollBehavior(
        blink::OverscrollBehavior::kOverscrollBehaviorTypeAuto);
    base::Optional<SnapContainerData> snap_container_data;

    bool operator==(const State& o) const {
      return container_rect == o.container_rect &&
             contents_size == o.contents_size &&
             user_scrollable_horizontal == o.user_scrollable_horizontal &&
             user_scrollable_vertical == o.user_scrollable_vertical &&
             scrolls_inner_viewport == o.scrolls_inner_viewport &&
             scrolls_outer_viewport == o.scrolls_outer_viewport &&
             max_scroll_offset_affected_by_page_scale ==
                 o.max_scroll_offset_affected_by_page_scale &&
             main_thread_scrolling_reasons == o.main_thread_scrolling_reasons &&
             compositor_element_id == o.compositor_element_id &&
             overscroll_behavior == o.overscroll_behavior &&
             snap_container_data == o.snap_container_data;
    }
  };

  // This node is really a sentinel, and does not represent a real scroll.
  static const ScrollPaintPropertyNode& Root();

  static scoped_refptr<ScrollPaintPropertyNode> Create(
      const ScrollPaintPropertyNode& parent,
      State&& state) {
    return base::AdoptRef(
        new ScrollPaintPropertyNode(&parent, std::move(state)));
  }
  static scoped_refptr<ScrollPaintPropertyNode> CreateAlias(
      const ScrollPaintPropertyNode&) {
    // ScrollPaintPropertyNodes cannot be aliases.
    NOTREACHED();
    return nullptr;
  }

  bool Update(const ScrollPaintPropertyNode& parent, State&& state) {
    bool parent_changed = SetParent(&parent);
    if (state == state_)
      return parent_changed;

    state_ = std::move(state);
    Validate();
    SetChanged();
    return true;
  }

  OverscrollBehavior::OverscrollBehaviorType OverscrollBehaviorX() const {
    return state_.overscroll_behavior.x;
  }

  OverscrollBehavior::OverscrollBehaviorType OverscrollBehaviorY() const {
    return state_.overscroll_behavior.y;
  }

  base::Optional<SnapContainerData> GetSnapContainerData() const {
    return state_.snap_container_data;
  }

  // Rect of the container area that the contents scrolls in, in the space of
  // the parent of the associated transform node (ScrollTranslation).
  // It doesn't include non-overlay scrollbars. Overlay scrollbars do not affect
  // the rect.
  const IntRect& ContainerRect() const { return state_.container_rect; }

  // Size of the contents that is scrolled within the container rect.
  const IntSize& ContentsSize() const { return state_.contents_size; }

  bool UserScrollableHorizontal() const {
    return state_.user_scrollable_horizontal;
  }
  bool UserScrollableVertical() const {
    return state_.user_scrollable_vertical;
  }
  bool ScrollsInnerViewport() const { return state_.scrolls_inner_viewport; }
  bool ScrollsOuterViewport() const { return state_.scrolls_outer_viewport; }
  bool MaxScrollOffsetAffectedByPageScale() const {
    return state_.max_scroll_offset_affected_by_page_scale;
  }

  // Return reason bitfield with values from cc::MainThreadScrollingReason.
  MainThreadScrollingReasons GetMainThreadScrollingReasons() const {
    return state_.main_thread_scrolling_reasons;
  }

  // Main thread scrolling reason for the threaded scrolling disabled setting.
  bool ThreadedScrollingDisabled() const {
    return state_.main_thread_scrolling_reasons &
           MainThreadScrollingReason::kThreadedScrollingDisabled;
  }

  // Main thread scrolling reason for background attachment fixed descendants.
  bool HasBackgroundAttachmentFixedDescendants() const {
    return state_.main_thread_scrolling_reasons &
           MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects;
  }

  const CompositorElementId& GetCompositorElementId() const {
    return state_.compositor_element_id;
  }

  std::unique_ptr<JSONObject> ToJSON() const;

 private:
  ScrollPaintPropertyNode(const ScrollPaintPropertyNode* parent, State&& state)
      : PaintPropertyNode(parent), state_(std::move(state)) {
    Validate();
  }

  void Validate() const {
#if DCHECK_IS_ON()
    DCHECK(!state_.compositor_element_id ||
           NamespaceFromCompositorElementId(state_.compositor_element_id) ==
               CompositorElementIdNamespace::kScroll);
#endif
  }

  State state_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_SCROLL_PAINT_PROPERTY_NODE_H_
