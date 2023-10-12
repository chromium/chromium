// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints_set.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"

namespace blink {

namespace {

ScrollableArea* GetScrollableArea(Node* node) {
  if (!node || !node->GetLayoutObject() ||
      !node->GetLayoutObject()->IsBoxModelObject())
    return nullptr;

  return To<LayoutBoxModelObject>(node->GetLayoutObject())->GetScrollableArea();
}

}  // namespace

TopDocumentRootScrollerController::TopDocumentRootScrollerController(Page& page)
    : page_(&page) {}

void TopDocumentRootScrollerController::Trace(Visitor* visitor) const {
  visitor->Trace(root_frame_viewport_);
  visitor->Trace(global_root_scroller_);
  visitor->Trace(page_);
}

void TopDocumentRootScrollerController::DidChangeRootScroller() {
  Node* target = FindGlobalRootScroller();
  UpdateGlobalRootScroller(target);
}

void TopDocumentRootScrollerController::DidResizeViewport() {
  if (!GlobalRootScroller() || !GlobalRootScroller()->GetDocument().IsActive())
    return;

  if (!GlobalRootScroller()->GetLayoutObject())
    return;

  auto* layout_object =
      To<LayoutBoxModelObject>(GlobalRootScroller()->GetLayoutObject());

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

gfx::Size TopDocumentRootScrollerController::RootScrollerVisibleArea() const {
  if (!TopDocument() || !TopDocument()->View())
    return gfx::Size();

  float minimum_page_scale =
      page_->GetPageScaleConstraintsSet().FinalConstraints().minimum_scale;
  int browser_controls_adjustment =
      ceilf(page_->GetVisualViewport().BrowserControlsAdjustment() /
            minimum_page_scale);

  gfx::Size layout_size = TopDocument()
                              ->View()
                              ->LayoutViewport()
                              ->VisibleContentRect(kExcludeScrollbars)
                              .size();
  return gfx::Size(layout_size.width(),
                   layout_size.height() + browser_controls_adjustment);
}

void TopDocumentRootScrollerController::Reset() {
  global_root_scroller_.Clear();
  root_frame_viewport_.Clear();
}

Node* TopDocumentRootScrollerController::FindGlobalRootScroller() {
  if (!TopDocument())
    return nullptr;

  Node* root_scroller =
      &TopDocument()->GetRootScrollerController().EffectiveRootScroller();

  while (auto* frame_owner = DynamicTo<HTMLFrameOwnerElement>(root_scroller)) {
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
    auto* local_frame = DynamicTo<LocalFrame>(frame);
    if (!local_frame)
      continue;

    LayoutView* layout_view = local_frame->View()->GetLayoutView();
    PaintLayer* frame_root_layer = layout_view->Layer();
    DCHECK(frame_root_layer);
    frame_root_layer->SetNeedsCompositingInputsUpdate();
  }
}

void TopDocumentRootScrollerController::UpdateGlobalRootScroller(
    Node* new_global_root_scroller) {
  if (!root_frame_viewport_) {
    return;
  }

  // Note, the layout object can be replaced during a rebuild. In that case,
  // re-run process even if the element itself is the same.
  if (new_global_root_scroller == global_root_scroller_ &&
      global_root_scroller_->GetLayoutObject()->IsGlobalRootScroller())
    return;

  ScrollableArea* target_scroller = GetScrollableArea(new_global_root_scroller);

  if (!target_scroller)
    return;

  Node* old_root_scroller = global_root_scroller_;

  global_root_scroller_ = new_global_root_scroller;

  // Swap the new global root scroller into the layout viewport.
  root_frame_viewport_->SetLayoutViewport(*target_scroller);

  SetNeedsCompositingUpdateOnAncestors(old_root_scroller);
  SetNeedsCompositingUpdateOnAncestors(new_global_root_scroller);

  UpdateCachedBits(old_root_scroller, new_global_root_scroller);
  if (ScrollableArea* area = GetScrollableArea(old_root_scroller)) {
    if (old_root_scroller->GetDocument().IsActive())
      area->DidChangeGlobalRootScroller();
  }

  target_scroller->DidChangeGlobalRootScroller();
}

void TopDocumentRootScrollerController::UpdateCachedBits(Node* old_global,
                                                         Node* new_global) {
  if (old_global) {
    if (LayoutObject* object = old_global->GetLayoutObject())
      object->SetIsGlobalRootScroller(false);
  }

  if (new_global) {
    if (LayoutObject* object = new_global->GetLayoutObject())
      object->SetIsGlobalRootScroller(true);
  }
}

Document* TopDocumentRootScrollerController::TopDocument() const {
  if (!page_)
    return nullptr;
  auto* main_local_frame = DynamicTo<LocalFrame>(page_->MainFrame());
  return main_local_frame ? main_local_frame->GetDocument() : nullptr;
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

void TopDocumentRootScrollerController::Initialize(
    RootFrameViewport& root_frame_viewport,
    Document& main_document) {
  DCHECK(page_);
  root_frame_viewport_ = root_frame_viewport;

  // Initialize global_root_scroller_ to the default; the main document node.
  // We can't yet reliably compute this because the frame we're loading may not
  // be swapped into the main frame yet so TopDocument returns nullptr.
  UpdateGlobalRootScroller(&main_document);
}

Node* TopDocumentRootScrollerController::GlobalRootScroller() const {
  return global_root_scroller_.Get();
}

}  // namespace blink
