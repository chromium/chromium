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
#include "third_party/blink/renderer/core/frame/page_scale_constraints_set.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
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

PrintContext::PrintContext(LocalFrame* frame, bool use_printing_layout)
    : frame_(frame),
      is_printing_(false),
      use_printing_layout_(use_printing_layout),
      linked_destinations_valid_(false) {}

PrintContext::~PrintContext() {
  DCHECK(!is_printing_);
}

void PrintContext::ComputePageRects(const gfx::SizeF& print_size) {
  page_rects_.clear();

  if (!IsFrameValid())
    return;

  if (!use_printing_layout_) {
    gfx::Rect page_rect(0, 0, print_size.width(), print_size.height());
    page_rects_.push_back(page_rect);
    return;
  }

  auto* view = frame_->GetDocument()->GetLayoutView();
  gfx::Rect snapped_doc_rect = ToPixelSnappedRect(view->DocumentRect());
  LogicalSize page_size =
      view->PageSize().ConvertToLogical(view->StyleRef().GetWritingMode());

  bool is_horizontal = view->StyleRef().IsHorizontalWritingMode();

  int doc_logical_height =
      is_horizontal ? snapped_doc_rect.height() : snapped_doc_rect.width();
  int page_logical_height = page_size.block_size.ToInt();
  int page_logical_width = page_size.inline_size.ToInt();

  int inline_direction_start = snapped_doc_rect.x();
  int inline_direction_end = snapped_doc_rect.right();
  int block_direction_start = snapped_doc_rect.y();
  int block_direction_end = snapped_doc_rect.bottom();
  if (!is_horizontal) {
    std::swap(block_direction_start, inline_direction_start);
    std::swap(block_direction_end, inline_direction_end);
  }
  if (!view->StyleRef().IsLeftToRightDirection())
    std::swap(inline_direction_start, inline_direction_end);
  if (view->StyleRef().IsFlippedBlocksWritingMode())
    std::swap(block_direction_start, block_direction_end);

  unsigned page_count =
      ceilf(static_cast<float>(doc_logical_height) / page_logical_height);
  for (unsigned i = 0; i < page_count; ++i) {
    int page_logical_top =
        block_direction_end > block_direction_start
            ? block_direction_start + i * page_logical_height
            : block_direction_start - (i + 1) * page_logical_height;

    int page_logical_left = inline_direction_end > inline_direction_start
                                ? inline_direction_start
                                : inline_direction_start - page_logical_width;

    auto* scrollable_area = GetFrame()->View()->LayoutViewport();
    gfx::Rect page_rect(page_logical_left, page_logical_top, page_logical_width,
                        page_logical_height);
    if (!is_horizontal)
      page_rect.Transpose();
    page_rect.Offset(-scrollable_area->ScrollOffsetInt());
    page_rects_.push_back(page_rect);
  }
}

void PrintContext::BeginPrintMode(float width, float height) {
  DCHECK_GT(width, 0);
  DCHECK_GT(height, 0);

  // This function can be called multiple times to adjust printing parameters
  // without going back to screen mode.
  is_printing_ = true;

  gfx::SizeF aspect_ratio(width, height);
  gfx::SizeF floored_min_layout_size = frame_->ResizePageRectsKeepingRatio(
      aspect_ratio, gfx::SizeF(width * kPrintingMinimumShrinkFactor,
                               height * kPrintingMinimumShrinkFactor));

  const Settings* settings = frame_->GetSettings();
  DCHECK(settings);
  float printingMaximumShrinkFactor =
      settings->GetPrintingMaximumShrinkFactor();

  // This changes layout, so callers need to make sure that they don't paint to
  // screen while in printing mode.
  frame_->StartPrinting(
      floored_min_layout_size, aspect_ratio,
      printingMaximumShrinkFactor / kPrintingMinimumShrinkFactor);

  ComputePageRects(gfx::SizeF(width, height));
}

void PrintContext::EndPrintMode() {
  DCHECK(is_printing_);
  is_printing_ = false;
  if (IsFrameValid()) {
    frame_->EndPrinting();

    // Printing changes the viewport and content size which may result in
    // changing the page scale factor. Call SetNeedsReset() so that we reset
    // back to the initial page scale factor when we exit printing mode.
    frame_->GetPage()->GetPageScaleConstraintsSet().SetNeedsReset(true);
  }
  linked_destinations_.clear();
  linked_destinations_valid_ = false;
}

// static
int PrintContext::PageNumberForElement(Element* element,
                                       const gfx::SizeF& page_size_in_pixels) {
  element->GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kPrinting);

  LocalFrame* frame = element->GetDocument().GetFrame();
  gfx::RectF page_rect(page_size_in_pixels);
  ScopedPrintContext print_context(frame);
  print_context->BeginPrintMode(page_rect.width(), page_rect.height());

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
    String name = url.FragmentIdentifier();
    if (Node* target = node->GetDocument().FindAnchor(name))
      linked_destinations_.Set(name, target);
  }
}

void PrintContext::OutputLinkedDestinations(GraphicsContext& context,
                                            const gfx::Rect& page_rect) {
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
    if (page_rect.Contains(anchor_point))
      context.SetURLDestinationLocation(entry.key, anchor_point);
  }
}

// static
String PrintContext::PageProperty(LocalFrame* frame,
                                  const char* property_name,
                                  uint32_t page_number) {
  Document* document = frame->GetDocument();
  ScopedPrintContext print_context(frame);
  // Any non-zero size is OK here. We don't care about actual layout. We just
  // want to collect @page rules and figure out what declarations apply on a
  // given page (that may or may not exist).
  print_context->BeginPrintMode(800, 1000);
  scoped_refptr<const ComputedStyle> style =
      document->StyleForPage(page_number);

  // Implement formatters for properties we care about.
  if (!strcmp(property_name, "margin-left")) {
    if (style->MarginLeft().IsAuto())
      return String("auto");
    return String::Number(style->MarginLeft().Value());
  }
  if (!strcmp(property_name, "line-height"))
    return String::Number(style->LineHeight().Value());
  if (!strcmp(property_name, "font-size"))
    return String::Number(style->GetFontDescription().ComputedPixelSize());
  if (!strcmp(property_name, "font-family")) {
    return ComputedStyleUtils::ValueForFontFamily(
               style->GetFontDescription().Family())
        ->CssText();
  }
  if (!strcmp(property_name, "size")) {
    return String::Number(style->PageSize().width()) + ' ' +
           String::Number(style->PageSize().height());
  }
  return String("pageProperty() unimplemented for: ") + property_name;
}

bool PrintContext::IsPageBoxVisible(LocalFrame* frame, uint32_t page_number) {
  return frame->GetDocument()->IsPageBoxVisible(page_number);
}

String PrintContext::PageSizeAndMarginsInPixels(LocalFrame* frame,
                                                uint32_t page_number,
                                                int width,
                                                int height,
                                                int margin_top,
                                                int margin_right,
                                                int margin_bottom,
                                                int margin_left) {
  WebPrintPageDescription description;
  description.size.SetSize(width, height);
  description.margin_top = margin_top;
  description.margin_right = margin_right;
  description.margin_bottom = margin_bottom;
  description.margin_left = margin_left;
  frame->GetDocument()->GetPageDescription(page_number, &description);

  return "(" + String::Number(floor(description.size.width())) + ", " +
         String::Number(floor(description.size.height())) + ") " +
         String::Number(description.margin_top) + ' ' +
         String::Number(description.margin_right) + ' ' +
         String::Number(description.margin_bottom) + ' ' +
         String::Number(description.margin_left);
}

// static
int PrintContext::NumberOfPages(LocalFrame* frame,
                                const gfx::SizeF& page_size_in_pixels) {
  frame->GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kPrinting);

  ScopedPrintContext print_context(frame);
  print_context->BeginPrintMode(page_size_in_pixels.width(),
                                page_size_in_pixels.height());
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

bool PrintContext::use_printing_layout() const {
  return use_printing_layout_;
}

ScopedPrintContext::ScopedPrintContext(LocalFrame* frame)
    : context_(
          MakeGarbageCollected<PrintContext>(frame,
                                             /*use_printing_layout=*/true)) {}

ScopedPrintContext::~ScopedPrintContext() {
  context_->EndPrintMode();
}

}  // namespace blink
