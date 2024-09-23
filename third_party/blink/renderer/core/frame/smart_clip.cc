/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/frame/smart_clip.h"

#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

static gfx::Rect ConvertToContentCoordinatesWithoutCollapsingToZero(
    const gfx::Rect& rect_in_viewport,
    const LocalFrameView* view) {
  gfx::Rect rect_in_contents = view->ViewportToFrame(rect_in_viewport);
  if (rect_in_viewport.width() > 0 && !rect_in_contents.width())
    rect_in_contents.set_width(1);
  if (rect_in_viewport.height() > 0 && !rect_in_contents.height())
    rect_in_contents.set_height(1);
  return rect_in_contents;
}

static Node* NodeInsideFrame(Node* node) {
  if (auto* frame_owner_element = DynamicTo<HTMLFrameOwnerElement>(node))
    return frame_owner_element->contentDocument();
  return nullptr;
}

SmartClip::SmartClip(LocalFrame* frame) : frame_(frame) {}

SmartClipData SmartClip::DataForRect(const gfx::Rect& crop_rect_in_viewport) {
  Node* best_node =
      FindBestOverlappingNode(frame_->GetDocument(), crop_rect_in_viewport);
  if (!best_node)
    return SmartClipData();

  if (Node* node_from_frame = NodeInsideFrame(best_node)) {
    // FIXME: This code only hit-tests a single iframe. It seems like we ought
    // support nested frames.
    if (Node* best_node_in_frame =
            FindBestOverlappingNode(node_from_frame, crop_rect_in_viewport))
      best_node = best_node_in_frame;
  }

  HeapVector<Member<Node>> hit_nodes;
  CollectOverlappingChildNodes(best_node, crop_rect_in_viewport, hit_nodes);

  if (hit_nodes.empty() || hit_nodes.size() == best_node->CountChildren()) {
    hit_nodes.clear();
    hit_nodes.push_back(best_node);
  }

  // Union won't work with the empty rect, so we initialize to the first rect.
  gfx::Rect united_rects = hit_nodes[0]->PixelSnappedBoundingBox();
  StringBuilder collected_text;
  for (wtf_size_t i = 0; i < hit_nodes.size(); ++i) {
    collected_text.Append(ExtractTextFromNode(hit_nodes[i]));
    united_rects.Union(hit_nodes[i]->PixelSnappedBoundingBox());
  }

  return SmartClipData(
      frame_->GetDocument()->View()->FrameToViewport(united_rects),
      collected_text.ToString());
}

float SmartClip::PageScaleFactor() {
  return frame_->GetPage()->PageScaleFactor();
}

// This function is a bit of a mystery. If you understand what it does, please
// consider adding a more descriptive name.
Node* SmartClip::MinNodeContainsNodes(Node* min_node, Node* new_node) {
  if (!new_node)
    return min_node;
  if (!min_node)
    return new_node;

  gfx::Rect min_node_rect = min_node->PixelSnappedBoundingBox();
  gfx::Rect new_node_rect = new_node->PixelSnappedBoundingBox();

  Node* parent_min_node = min_node->parentNode();
  Node* parent_new_node = new_node->parentNode();

  if (min_node_rect.Contains(new_node_rect)) {
    if (parent_min_node && parent_new_node &&
        parent_new_node->parentNode() == parent_min_node)
      return parent_min_node;
    return min_node;
  }

  if (new_node_rect.Contains(min_node_rect)) {
    if (parent_min_node && parent_new_node &&
        parent_min_node->parentNode() == parent_new_node)
      return parent_new_node;
    return new_node;
  }

  // This loop appears to find the nearest ancestor of minNode (in DOM order)
  // that contains the newNodeRect. It's very unclear to me why that's an
  // interesting node to find. Presumably this loop will often just return
  // the documentElement.
  Node* node = min_node;
  while (node) {
    if (node->GetLayoutObject()) {
      gfx::Rect node_rect = node->PixelSnappedBoundingBox();
      if (node_rect.Contains(new_node_rect)) {
        return node;
      }
    }
    node = node->parentNode();
  }

  return nullptr;
}

Node* SmartClip::FindBestOverlappingNode(
    Node* root_node,
    const gfx::Rect& crop_rect_in_viewport) {
  if (!root_node)
    return nullptr;

  gfx::Rect resized_crop_rect =
      ConvertToContentCoordinatesWithoutCollapsingToZero(
          crop_rect_in_viewport, root_node->GetDocument().View());

  Node* node = root_node;
  Node* min_node = nullptr;

  while (node) {
    gfx::Rect node_rect = node->PixelSnappedBoundingBox();
    auto* element = DynamicTo<Element>(node);
    if (element &&
        EqualIgnoringASCIICase(
            element->FastGetAttribute(html_names::kAriaHiddenAttr), "true")) {
      node = NodeTraversal::NextSkippingChildren(*node, root_node);
      continue;
    }

    LayoutObject* layout_object = node->GetLayoutObject();
    if (layout_object && !node_rect.IsEmpty()) {
      if (layout_object->IsText() || layout_object->IsLayoutImage() ||
          node->IsFrameOwnerElement() ||
          (layout_object->StyleRef().HasBackgroundImage() &&
           !ShouldSkipBackgroundImage(node))) {
        if (resized_crop_rect.Intersects(node_rect)) {
          min_node = MinNodeContainsNodes(min_node, node);
        } else {
          node = NodeTraversal::NextSkippingChildren(*node, root_node);
          continue;
        }
      }
    }
    node = NodeTraversal::Next(*node, root_node);
  }

  return min_node;
}

// This function appears to heuristically guess whether to include a background
// image in the smart clip. It seems to want to include sprites created from
// CSS background images but to skip actual backgrounds.
bool SmartClip::ShouldSkipBackgroundImage(Node* node) {
  DCHECK(node);
  // Apparently we're only interested in background images on spans and divs.
  if (!IsA<HTMLSpanElement>(*node) && !IsA<HTMLDivElement>(*node))
    return true;

  // This check actually makes a bit of sense. If you're going to sprite an
  // image out of a CSS background, you're probably going to specify a height
  // or a width. On the other hand, if we've got a legit background image,
  // it's very likely the height or the width will be set to auto.
  LayoutObject* layout_object = node->GetLayoutObject();
  if (layout_object && (layout_object->StyleRef()
                            .LogicalHeight()
                            .HasAutoOrContentOrIntrinsic() ||
                        layout_object->StyleRef()
                            .LogicalWidth()
                            .HasAutoOrContentOrIntrinsic())) {
    return true;
  }

  return false;
}

void SmartClip::CollectOverlappingChildNodes(
    Node* parent_node,
    const gfx::Rect& crop_rect_in_viewport,
    HeapVector<Member<Node>>& hit_nodes) {
  if (!parent_node)
    return;
  gfx::Rect resized_crop_rect =
      ConvertToContentCoordinatesWithoutCollapsingToZero(
          crop_rect_in_viewport, parent_node->GetDocument().View());
  for (Node* child = parent_node->firstChild(); child;
       child = child->nextSibling()) {
    gfx::Rect child_rect = child->PixelSnappedBoundingBox();
    if (resized_crop_rect.Intersects(child_rect))
      hit_nodes.push_back(child);
  }
}

String SmartClip::ExtractTextFromNode(Node* node) {
  // Science has proven that no text nodes are ever positioned at y == -99999.
  int prev_y_pos = -99999;

  StringBuilder result;
  for (Node& current_node : NodeTraversal::InclusiveDescendantsOf(*node)) {
    LayoutObject* layout_object = current_node.GetLayoutObject();

    if (!layout_object ||
        layout_object->StyleRef().UsedUserSelect() == EUserSelect::kNone) {
      continue;
    }
    if (Node* node_from_frame = NodeInsideFrame(&current_node)) {
      result.Append(ExtractTextFromNode(node_from_frame));
      continue;
    }
    if (!layout_object->IsText()) {
      continue;
    }
    gfx::Rect node_rect = current_node.PixelSnappedBoundingBox();
    if (node_rect.IsEmpty()) {
      continue;
    }

    String node_value = current_node.nodeValue();

    // It's unclear why we disallowed solitary "\n" node values.
    // Maybe we're trying to ignore <br> tags somehow?
    if (node_value == "\n") {
      node_value = "";
    }
    if (node_rect.y() != prev_y_pos) {
      prev_y_pos = node_rect.y();
      result.Append('\n');
    }
    result.Append(node_value);
  }

  return result.ToString();
}

}  // namespace blink
