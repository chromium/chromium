// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/root_scroller_util.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/root_scroller_controller.h"
#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"

namespace blink {

namespace root_scroller_util {

PaintLayer* PaintLayerForRootScroller(const Node* node) {
  if (!node)
    return nullptr;

  if (!node->GetLayoutObject() || !node->GetLayoutObject()->IsBox())
    return nullptr;

  LayoutBox* box = ToLayoutBox(node->GetLayoutObject());
  return box->Layer();
}

bool IsGlobal(const LayoutBox& box) {
  if (!box.GetNode() || !box.GetNode()->GetDocument().GetPage())
    return false;

  return box.GetNode() == box.GetDocument()
                              .GetPage()
                              ->GlobalRootScrollerController()
                              .GlobalRootScroller();
}

bool IsGlobal(const PaintLayer& layer) {
  if (!layer.GetLayoutBox())
    return false;

  PaintLayer* root_scroller_layer =
      PaintLayerForRootScroller(layer.GetLayoutBox()
                                    ->GetDocument()
                                    .GetPage()
                                    ->GlobalRootScrollerController()
                                    .GlobalRootScroller());

  return &layer == root_scroller_layer;
}

bool IsGlobal(const Node* node) {
  return node->GetDocument()
             .GetPage()
             ->GlobalRootScrollerController()
             .GlobalRootScroller() == node;
}

}  // namespace root_scroller_util

}  // namespace blink
