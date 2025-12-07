/*
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2007 Apple Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/page/print_context.h"

#include <utility>

#include "third_party/blink/public/web/web_print_page_description.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/pagination_utils.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/physical_fragment_link.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

namespace {

LayoutBoxModelObject* EnclosingBoxModelObject(LayoutObject* object) {
  while (object && !object->IsBoxModelObject())
    object = object->Parent();
  return To<LayoutBoxModelObject>(object);
}

bool IsCoordinateInPage(int top, int left, const gfx::Rect& page) {
  return page.x() <= left && left < page.right() && page.y() <= top &&
         top < page.bottom();
}

}  // namespace

PrintContext::PrintContext(LocalFrame* frame)
    : frame_(frame), is_printing_(false), linked_destinations_valid_(false) {}

PrintContext::~PrintContext() {
  DCHECK(!is_printing_);
}

wtf_size_t PrintContext::PageCount() const {
  DCHECK(is_printing_);
  if (!IsFrameValid()) {
    return 0;
  }
  if (!use_paginated_layout_) {
    return 1;
  }

  return ::blink::PageCount(*frame_->GetDocument()->GetLayoutView());
}

gfx::Rect PrintContext::PageRect(wtf_size_t page_index) const {
  CHECK(IsFrameValid());
  DCHECK(is_printing_);
  DCHECK_LT(page_index, PageCount());
  const LayoutView& layout_view = *frame_->GetDocument()->GetLayoutView();

  if (!use_paginated_layout_) {
    // Remote frames (and the special per-page headers+footers document) end up
    // here.
    return ToPixelSnappedRect(layout_view.DocumentRect());
  }

  PhysicalRect physical_rect = StitchedPageContentRect(layout_view, page_index);
  gfx::Rect page_rect = ToEnclosingRect(physical_rect);

  // There's code to avoid fractional page sizes, so we shouldn't have to worry
  // about that here.
  DCHECK_EQ(gfx::RectF(physical_rect), gfx::RectF(page_rect));

  page_rect.Offset(-frame_->View()->LayoutViewport()->ScrollOffsetInt());

  return page_rect;
}

void PrintContext::BeginPrintMode(const WebPrintParams& print_params) {
  DCHECK_GT(print_params.default_page_description.size.width(), 0);
  DCHECK_GT(print_params.default_page_description.size.height(), 0);

  // This function can be called multiple times to adjust printing parameters
  // without going back to screen mode.
  is_printing_ = true;

  use_paginated_layout_ = print_params.use_paginated_layout;

  const Settings* settings = frame_->GetSettings();
  DCHECK(settings);
  float maximum_shink_factor = settings->GetPrintingMaximumShrinkFactor();

  LayoutView& layout_view = *frame_->GetDocument()->GetLayoutView();
  layout_view.SetPaginationScaleFactor(1.0f / print_params.scale_factor);

  // This changes layout, so callers need to make sure that they don't paint to
  // screen while in printing mode.
  frame_->StartPrinting(print_params, maximum_shink_factor);
}

void PrintContext::EndPrintMode() {
  DCHECK(is_printing_);
  is_printing_ = false;
  if (IsFrameValid()) {
    frame_->EndPrinting();
  }
  linked_destinations_.clear();
  linked_destinations_valid_ = false;
}

// static
int PrintContext::PageNumberForElement(Element* element,
                                       const gfx::SizeF& page_size_in_pixels) {
  element->GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kPrinting);

  LocalFrame* frame = element->GetDocument().GetFrame();
  ScopedPrintContext print_context(frame);
  print_context->BeginPrintMode(WebPrintParams(page_size_in_pixels));

  LayoutBoxModelObject* box =
      EnclosingBoxModelObject(element->GetLayoutObject());
  if (!box)
    return -1;

  int top = box->OffsetTop(box->OffsetParent()).ToInt();
  int left = box->OffsetLeft(box->OffsetParent()).ToInt();
  for (wtf_size_t page_number = 0; page_number < print_context->PageCount();
       ++page_number) {
    if (IsCoordinateInPage(top, left, print_context->PageRect(page_number)))
      return static_cast<int>(page_number);
  }
  return -1;
}

void PrintContext::CollectLinkedDestinations(Node* node) {
  for (Node* i = node->firstChild(); i; i = i->nextSibling())
    CollectLinkedDestinations(i);

  auto* element = DynamicTo<Element>(node);
  if (!node->IsLink() || !element)
    return;
  const AtomicString& href = element->getAttribute(html_names::kHrefAttr);
  if (href.IsNull())
    return;
  KURL url = node->GetDocument().CompleteURL(href);
  if (!url.IsValid())
    return;

  if (url.HasFragmentIdentifier() &&
      EqualIgnoringFragmentIdentifier(url, node->GetDocument().BaseURL())) {
    String name = url.FragmentIdentifier().ToString();
    if (Node* target = node->GetDocument().FindAnchor(name))
      linked_destinations_.Set(name, target);
  }
}

void PrintContext::OutputLinkedDestinations(
    GraphicsContext& context,
    const PropertyTreeStateOrAlias& property_tree_state,
    const gfx::Rect& page_rect) {
  DEFINE_STATIC_DISPLAY_ITEM_CLIENT(client, "PrintedLinkedDestinations");
  ScopedPaintChunkProperties scoped_paint_chunk_properties(
      context.GetPaintController(), property_tree_state, *client,
      DisplayItem::kPrintedContentDestinationLocations);
  DrawingRecorder line_boundary_recorder(
      context, *client, DisplayItem::kPrintedContentDestinationLocations);

  if (!linked_destinations_valid_) {
    // Collect anchors in the top-level frame only because our PrintContext
    // supports only one namespace for the anchors.
    CollectLinkedDestinations(GetFrame()->GetDocument());
    linked_destinations_valid_ = true;
  }

  for (const auto& entry : linked_destinations_) {
    LayoutObject* layout_object = entry.value->GetLayoutObject();
    if (!layout_object || !layout_object->GetFrameView())
      continue;
    gfx::Point anchor_point = layout_object->AbsoluteBoundingBoxRect().origin();
    if (page_rect.Contains(anchor_point)) {
      // The linked destination location is relative to the current page (in
      // fact just like everything else that's painted, but the linked
      // destination code is tacked on the outside of the paint code, so extra
      // awareness is required).
      context.SetURLDestinationLocation(
          entry.key, anchor_point - page_rect.OffsetFromOrigin());
    }
  }
}

// static
int PrintContext::NumberOfPages(LocalFrame* frame,
                                const gfx::SizeF& page_size_in_pixels) {
  frame->GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kPrinting);

  ScopedPrintContext print_context(frame);
  print_context->BeginPrintMode(WebPrintParams(page_size_in_pixels));
  return print_context->PageCount();
}

bool PrintContext::IsFrameValid() const {
  return frame_->View() && frame_->GetDocument() &&
         frame_->GetDocument()->GetLayoutView();
}

void PrintContext::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(linked_destinations_);
}

ScopedPrintContext::ScopedPrintContext(LocalFrame* frame)
    : context_(MakeGarbageCollected<PrintContext>(frame)) {}

ScopedPrintContext::~ScopedPrintContext() {
  context_->EndPrintMode();
}

}  // namespace blink
