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

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"

namespace blink {

namespace {

LayoutBoxModelObject* EnclosingBoxModelObject(LayoutObject* object) {
  while (object && !object->IsBoxModelObject())
    object = object->Parent();
  if (!object)
    return nullptr;
  return ToLayoutBoxModelObject(object);
}

bool IsCoordinateInPage(int top, int left, const IntRect& page) {
  return page.X() <= left && left < page.MaxX() && page.Y() <= top &&
         top < page.MaxY();
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

void PrintContext::ComputePageRects(const FloatSize& print_size) {
  page_rects_.clear();

  if (!IsFrameValid())
    return;

  if (!use_printing_layout_) {
    IntRect page_rect(0, 0, print_size.Width(), print_size.Height());
    page_rects_.push_back(page_rect);
    return;
  }

  auto* view = frame_->GetDocument()->GetLayoutView();
  const PhysicalRect& document_rect = view->DocumentRect();
  FloatSize page_size = frame_->ResizePageRectsKeepingRatio(
      print_size, FloatSize(document_rect.Width(), document_rect.Height()));
  ComputePageRectsWithPageSizeInternal(page_size);
}

void PrintContext::ComputePageRectsWithPageSize(
    const FloatSize& page_size_in_pixels) {
  page_rects_.clear();
  ComputePageRectsWithPageSizeInternal(page_size_in_pixels);
}

void PrintContext::ComputePageRectsWithPageSizeInternal(
    const FloatSize& page_size_in_pixels) {
  if (!IsFrameValid())
    return;

  auto* view = frame_->GetDocument()->GetLayoutView();

  IntRect snapped_doc_rect = PixelSnappedIntRect(view->DocumentRect());

  int page_width = page_size_in_pixels.Width();
  // We scaled with floating point arithmetic and need to ensure results like
  // 13329.99 are treated as 13330 so that we don't mistakenly assign an extra
  // page for the stray pixel.
  int page_height = page_size_in_pixels.Height() + LayoutUnit::Epsilon();

  bool is_horizontal = view->StyleRef().IsHorizontalWritingMode();

  int doc_logical_height =
      is_horizontal ? snapped_doc_rect.Height() : snapped_doc_rect.Width();
  int page_logical_height = is_horizontal ? page_height : page_width;
  int page_logical_width = is_horizontal ? page_width : page_height;

  int inline_direction_start = snapped_doc_rect.X();
  int inline_direction_end = snapped_doc_rect.MaxX();
  int block_direction_start = snapped_doc_rect.Y();
  int block_direction_end = snapped_doc_rect.MaxY();
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
    IntSize frame_scroll = scrollable_area->ScrollOffsetInt();
    page_logical_left -= frame_scroll.Width();
    page_logical_top -= frame_scroll.Height();
    IntRect page_rect(page_logical_left, page_logical_top, page_logical_width,
                      page_logical_height);
    if (!is_horizontal)
      page_rect = page_rect.TransposedRect();
    page_rects_.push_back(page_rect);
  }
}

void PrintContext::BeginPrintMode(float width, float height) {
  DCHECK_GT(width, 0);
  DCHECK_GT(height, 0);

  // This function can be called multiple times to adjust printing parameters
  // without going back to screen mode.
  is_printing_ = true;

  FloatSize original_page_size = FloatSize(width, height);
  FloatSize min_layout_size = frame_->ResizePageRectsKeepingRatio(
      original_page_size, FloatSize(width * kPrintingMinimumShrinkFactor,
                                    height * kPrintingMinimumShrinkFactor));

  // This changes layout, so callers need to make sure that they don't paint to
  // screen while in printing mode.
  frame_->StartPrinting(
      min_layout_size, original_page_size,
      kPrintingMaximumShrinkFactor / kPrintingMinimumShrinkFactor);
}

void PrintContext::EndPrintMode() {
  DCHECK(is_printing_);
  is_printing_ = false;
  if (IsFrameValid())
    frame_->EndPrinting();
  linked_destinations_.clear();
  linked_destinations_valid_ = false;
}

// static
int PrintContext::PageNumberForElement(Element* element,
                                       const FloatSize& page_size_in_pixels) {
  element->GetDocument().UpdateStyleAndLayout();

  LocalFrame* frame = element->GetDocument().GetFrame();
  FloatRect page_rect(FloatPoint(0, 0), page_size_in_pixels);
  ScopedPrintContext print_context(frame);
  print_context->BeginPrintMode(page_rect.Width(), page_rect.Height());

  LayoutBoxModelObject* box =
      EnclosingBoxModelObject(element->GetLayoutObject());
  if (!box)
    return -1;

  FloatSize scaled_page_size = page_size_in_pixels;
  scaled_page_size.Scale(
      frame->View()->LayoutViewport()->ContentsSize().Width() /
      page_rect.Width());
  print_context->ComputePageRectsWithPageSize(scaled_page_size);

  int top = box->PixelSnappedOffsetTop(box->OffsetParent());
  int left = box->PixelSnappedOffsetLeft(box->OffsetParent());
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
    if (Element* element = node->GetDocument().FindAnchor(name))
      linked_destinations_.Set(name, element);
  }
}

void PrintContext::OutputLinkedDestinations(GraphicsContext& context,
                                            const IntRect& page_rect) {
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
    IntPoint anchor_point = layout_object->AbsoluteBoundingBoxRect().Location();
    if (page_rect.Contains(anchor_point))
      context.SetURLDestinationLocation(entry.key, anchor_point);
  }
}

// static
String PrintContext::PageProperty(LocalFrame* frame,
                                  const char* property_name,
                                  int page_number) {
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
  if (!strcmp(property_name, "font-family"))
    return style->GetFontDescription().Family().Family().GetString();
  if (!strcmp(property_name, "size"))
    return String::Number(style->PageSize().Width()) + ' ' +
           String::Number(style->PageSize().Height());

  return String("pageProperty() unimplemented for: ") + property_name;
}

bool PrintContext::IsPageBoxVisible(LocalFrame* frame, int page_number) {
  return frame->GetDocument()->IsPageBoxVisible(page_number);
}

String PrintContext::PageSizeAndMarginsInPixels(LocalFrame* frame,
                                                int page_number,
                                                int width,
                                                int height,
                                                int margin_top,
                                                int margin_right,
                                                int margin_bottom,
                                                int margin_left) {
  DoubleSize page_size(width, height);
  frame->GetDocument()->PageSizeAndMarginsInPixels(page_number, page_size,
                                                   margin_top, margin_right,
                                                   margin_bottom, margin_left);

  return "(" + String::Number(floor(page_size.Width())) + ", " +
         String::Number(floor(page_size.Height())) + ") " +
         String::Number(margin_top) + ' ' + String::Number(margin_right) + ' ' +
         String::Number(margin_bottom) + ' ' + String::Number(margin_left);
}

// static
int PrintContext::NumberOfPages(LocalFrame* frame,
                                const FloatSize& page_size_in_pixels) {
  frame->GetDocument()->UpdateStyleAndLayout();

  FloatRect page_rect(FloatPoint(0, 0), page_size_in_pixels);
  ScopedPrintContext print_context(frame);
  print_context->BeginPrintMode(page_rect.Width(), page_rect.Height());
  // Account for shrink-to-fit.
  FloatSize scaled_page_size = page_size_in_pixels;
  scaled_page_size.Scale(
      frame->View()->LayoutViewport()->ContentsSize().Width() /
      page_rect.Width());
  print_context->ComputePageRectsWithPageSize(scaled_page_size);
  return print_context->PageCount();
}

bool PrintContext::IsFrameValid() const {
  return frame_->View() && frame_->GetDocument() &&
         frame_->GetDocument()->GetLayoutView();
}

void PrintContext::Trace(blink::Visitor* visitor) {
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
