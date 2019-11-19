// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TOP_DOCUMENT_ROOT_SCROLLER_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TOP_DOCUMENT_ROOT_SCROLLER_CONTROLLER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/page/scrolling/root_scroller_controller.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class LocalFrameView;
class Node;
class Page;
class RootFrameViewport;
class ScrollStateCallback;
class ScrollableArea;
class ViewportScrollCallback;

// This class manages the the page level aspects of the root scroller.  That
// is, given all the iframes on a page and their individual root scrollers,
// this class will determine which ultimate Node should be used as the root
// scroller and ensures that Node is used to scroll browser controls and
// provide overscroll effects. High level details are available in README.md.
// TODO(bokan): This class is currently OOPIF unaware. crbug.com/642378.
class CORE_EXPORT TopDocumentRootScrollerController
    : public GarbageCollected<TopDocumentRootScrollerController> {
 public:
  explicit TopDocumentRootScrollerController(Page&);

  void Trace(blink::Visitor*);

  // PaintLayerScrollableAreas need to notify this class when they're being
  // disposed so that we can remove them as the root scroller.
  void DidDisposeScrollableArea(ScrollableArea&);

  // This method needs to be called to create a ViewportScrollCallback that
  // will be used to apply viewport scrolling actions like browser controls
  // movement and overscroll glow.
  void InitializeViewportScrollCallback(RootFrameViewport&, Document&);

  // Returns true if the given ScrollStateCallback is the
  // ViewportScrollCallback managed by this class.
  // TODO(bokan): Temporarily needed to allow ScrollCustomization to
  // differentiate between real custom callback and the built-in viewport
  // apply scroll. crbug.com/623079.
  bool IsViewportScrollCallback(const ScrollStateCallback*) const;

  // Returns the Node that's the global root scroller.  See README.md for the
  // difference between this and the root scroller types in
  // RootScrollerController.
  Node* GlobalRootScroller() const;

  // Called when the root scroller in any frames on the page has changed.
  void DidChangeRootScroller();

  void DidResizeViewport();

  // Returns the ScrollableArea associated with the globalRootScroller().
  ScrollableArea* RootScrollerArea() const;

  // Returns the size we should use for the root scroller, accounting for
  // browser controls adjustment and using the root LocalFrameView.
  IntSize RootScrollerVisibleArea() const;

 private:
  // Calculates the Node that should be the global root scroller. On a simple
  // page, this will be the root frame's effective root scroller. If the
  // effective root scroller is an iframe, this will then recursively descend
  // into the iframe to find its effective root scroller.
  Node* FindGlobalRootScroller();

  // Should be called to set a new node as the global root scroller and that
  // all appropriate state changes are made if it changes.
  void UpdateGlobalRootScroller(Node* new_global_root_scroller);

  // Updates the is_global_root_scroller bits in all the necessary places when
  // the global root scroller changes.
  void UpdateCachedBits(Node* old_global, Node* new_global);

  Document* TopDocument() const;

  // The apply-scroll callback that moves browser controls and produces
  // overscroll effects. This class makes sure this callback is set on the
  // global root scroller element.
  Member<ViewportScrollCallback> viewport_apply_scroll_;

  // The page level root scroller. i.e. The actual node for which scrolling
  // should move browser controls and produce overscroll glow. Once an
  // |viewport_apply_scroll_| has been created, it will always be set on this
  // Node.
  WeakMember<Node> global_root_scroller_;

  Member<Page> page_;
};

}  // namespace blink

#endif  // RootScrollerController_h
