// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints_set.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/overscroll_controller.h"
#include "third_party/blink/renderer/core/page/scrolling/root_scroller_util.h"
#include "third_party/blink/renderer/core/page/scrolling/viewport_scroll_callback.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"

namespace blink {

namespace {

ScrollableArea* GetScrollableArea(Node* node) {
  if (!node || !node->GetLayoutObject() ||
      !node->GetLayoutObject()->IsBoxModelObject())
    return nullptr;

  return ToLayoutBoxModelObject(node->GetLayoutObject())->GetScrollableArea();
}

}  // namespace

// static
TopDocumentRootScrollerController* TopDocumentRootScrollerController::Create(
    Page& page) {
  return new TopDocumentRootScrollerController(page);
}

TopDocumentRootScrollerController::TopDocumentRootScrollerController(Page& page)
    : page_(&page) {}

void TopDocumentRootScrollerController::Trace(blink::Visitor* visitor) {
  visitor->Trace(viewport_apply_scroll_);
  visitor->Trace(global_root_scroller_);
  visitor->Trace(page_);
}

void TopDocumentRootScrollerController::DidChangeRootScroller() {
  RecomputeGlobalRootScroller();
}

void TopDocumentRootScrollerController::DidResizeViewport() {
  if (!GlobalRootScroller() || !GlobalRootScroller()->GetDocument().IsActive())
    return;

  if (!GlobalRootScroller()->GetLayoutObject())
    return;

  DCHECK(GlobalRootScroller()->GetLayoutObject()->IsBoxModelObject());

  LayoutBoxModelObject* layout_object =
      ToLayoutBoxModelObject(GlobalRootScroller()->GetLayoutObject());

  // Top controls can resize the viewport without invalidating compositing or
  // paint so we need to do that manually here.
  if (layout_object->HasLayer()) {
    layout_object->Layer()->SetNeedsCompositingInputsUpdate();
    layout_object->Layer()->UpdateSelfPaintingLayer();
  }

  layout_object->SetNeedsPaintPropertyUpdate();
}

ScrollableArea* TopDocumentRootScrollerController::RootScrollerArea() const {
  return GetScrollableArea(GlobalRootScroller());
}

IntSize TopDocumentRootScrollerController::RootScrollerVisibleArea() const {
  if (!TopDocument() || !TopDocument()->View())
    return IntSize();

  float minimum_page_scale =
      page_->GetPageScaleConstraintsSet().FinalConstraints().minimum_scale;
  int browser_controls_adjustment =
      ceilf(page_->GetVisualViewport().BrowserControlsAdjustment() /
            minimum_page_scale);

  return TopDocument()
             ->View()
             ->LayoutViewport()
             ->VisibleContentRect(kExcludeScrollbars)
             .Size() +
         IntSize(0, browser_controls_adjustment);
}

Node* TopDocumentRootScrollerController::FindGlobalRootScroller() {
  if (!TopDocument())
    return nullptr;

  Node* root_scroller =
      &TopDocument()->GetRootScrollerController().EffectiveRootScroller();

  while (root_scroller && root_scroller->IsFrameOwnerElement()) {
    HTMLFrameOwnerElement* frame_owner = ToHTMLFrameOwnerElement(root_scroller);
    DCHECK(frame_owner);

    Document* iframe_document = frame_owner->contentDocument();
    if (!iframe_document)
      return root_scroller;

    root_scroller =
        &iframe_document->GetRootScrollerController().EffectiveRootScroller();
  }

  return root_scroller;
}

void SetNeedsCompositingUpdateOnAncestors(Node* node) {
  if (!node || !node->GetDocument().IsActive())
    return;

  ScrollableArea* area = GetScrollableArea(node);

  if (!area || !area->Layer())
    return;

  Frame* frame = area->Layer()->GetLayoutObject().GetFrame();
  for (; frame; frame = frame->Tree().Parent()) {
    if (!frame->IsLocalFrame())
      continue;

    LayoutView* layout_view = ToLocalFrame(frame)->View()->GetLayoutView();
    PaintLayer* frame_root_layer = layout_view->Layer();
    DCHECK(frame_root_layer);
    frame_root_layer->SetNeedsCompositingInputsUpdate();
  }
}

void TopDocumentRootScrollerController::RecomputeGlobalRootScroller() {
  if (!viewport_apply_scroll_)
    return;

  Node* target = FindGlobalRootScroller();
  if (target == global_root_scroller_)
    return;

  ScrollableArea* target_scroller = GetScrollableArea(target);

  if (!target_scroller)
    return;

  if (global_root_scroller_)
    global_root_scroller_->RemoveApplyScroll();

  // Use disable-native-scroll since the ViewportScrollCallback needs to
  // apply scroll actions both before (BrowserControls) and after (overscroll)
  // scrolling the element so it will apply scroll to the element itself.
  target->SetApplyScroll(viewport_apply_scroll_);

  Node* old_root_scroller = global_root_scroller_;

  global_root_scroller_ = target;

  // Ideally, scroll customization would pass the current element to scroll to
  // the apply scroll callback but this doesn't happen today so we set it
  // through a back door here. This is also needed by the
  // ViewportScrollCallback to swap the target into the layout viewport
  // in RootFrameViewport.
  viewport_apply_scroll_->SetScroller(target_scroller);

  SetNeedsCompositingUpdateOnAncestors(old_root_scroller);
  SetNeedsCompositingUpdateOnAncestors(target);

  if (ScrollableArea* area = GetScrollableArea(old_root_scroller)) {
    if (old_root_scroller->GetDocument().IsActive())
      area->DidChangeGlobalRootScroller();
  }

  target_scroller->DidChangeGlobalRootScroller();
}

Document* TopDocumentRootScrollerController::TopDocument() const {
  if (!page_ || !page_->MainFrame() || !page_->MainFrame()->IsLocalFrame())
    return nullptr;

  return ToLocalFrame(page_->MainFrame())->GetDocument();
}

void TopDocumentRootScrollerController::DidUpdateCompositing(
    const LocalFrameView& frame_view) {
  if (!page_)
    return;

  // The only other way to get here is from a local root OOPIF but we ignore
  // that case since the global root can't cross remote frames today.
  if (!frame_view.GetFrame().IsMainFrame())
    return;

  // Let the compositor-side counterpart know about this change.
  page_->GetChromeClient().RegisterViewportLayers();
}

void TopDocumentRootScrollerController::DidDisposeScrollableArea(
    ScrollableArea& area) {
  if (!TopDocument() || !TopDocument()->View())
    return;

  // If the document is tearing down, we may no longer have a layoutViewport to
  // fallback to.
  if (TopDocument()->Lifecycle().GetState() >= DocumentLifecycle::kStopping)
    return;

  LocalFrameView* frame_view = TopDocument()->View();

  RootFrameViewport* rfv = frame_view->GetRootFrameViewport();

  if (rfv && &area == &rfv->LayoutViewport()) {
    DCHECK(frame_view->LayoutViewport());
    rfv->SetLayoutViewport(*frame_view->LayoutViewport());
  }
}

void TopDocumentRootScrollerController::InitializeViewportScrollCallback(
    RootFrameViewport& root_frame_viewport) {
  DCHECK(page_);
  viewport_apply_scroll_ = ViewportScrollCallback::Create(
      &page_->GetBrowserControls(), &page_->GetOverscrollController(),
      root_frame_viewport);

  RecomputeGlobalRootScroller();
}

bool TopDocumentRootScrollerController::IsViewportScrollCallback(
    const ScrollStateCallback* callback) const {
  if (!callback)
    return false;

  return callback == viewport_apply_scroll_.Get();
}

GraphicsLayer* TopDocumentRootScrollerController::RootScrollerLayer() const {
  ScrollableArea* area = GetScrollableArea(global_root_scroller_);

  if (!area)
    return nullptr;

  GraphicsLayer* graphics_layer = area->LayerForScrolling();

  // TODO(bokan): We should assert graphicsLayer here and
  // RootScrollerController should do whatever needs to happen to ensure
  // the root scroller gets composited.

  return graphics_layer;
}

GraphicsLayer* TopDocumentRootScrollerController::RootContainerLayer() const {
  ScrollableArea* area = GetScrollableArea(global_root_scroller_);
  return area ? area->LayerForContainer() : nullptr;
}

PaintLayer* TopDocumentRootScrollerController::RootScrollerPaintLayer() const {
  return root_scroller_util::PaintLayerForRootScroller(global_root_scroller_);
}

Node* TopDocumentRootScrollerController::GlobalRootScroller() const {
  return global_root_scroller_.Get();
}

}  // namespace blink
