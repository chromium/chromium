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
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/physical_fragment_link.h"
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
  if (!use_printing_layout_) {
    return 1;
  }

  auto* view = frame_->GetDocument()->GetLayoutView();
  const auto& fragments = view->GetPhysicalFragment(0)->Children();
  return ClampTo<wtf_size_t>(fragments.size());
}

gfx::Rect PrintContext::PageRect(wtf_size_t page_number) const {
  CHECK(IsFrameValid());
  DCHECK(is_printing_);
  DCHECK_LT(page_number, PageCount());
  const LayoutView& layout_view = *frame_->GetDocument()->GetLayoutView();

  if (!use_printing_layout_) {
    // Remote frames end up here.
    return ToPixelSnappedRect(layout_view.DocumentRect());
  }

  const auto& fragments = layout_view.GetPhysicalFragment(0)->Children();
  CHECK_GE(fragments.size(), 1u);
  DCHECK(fragments[0]->IsFragmentainerBox());

  const PhysicalFragmentLink& page = fragments[page_number];
  PhysicalRect physical_rect(page.offset, page->Size());
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

  use_printing_layout_ = print_params.use_printing_layout;

  const Settings* settings = frame_->GetSettings();
  DCHECK(settings);
  float maximum_shink_factor = settings->GetPrintingMaximumShrinkFactor();

  LayoutView& layout_view = *frame_->GetDocument()->GetLayoutView();
  layout_view.SetPageScaleFactor(1.0f / print_params.scale_factor);

  // This changes layout, so callers need to make sure that they don't paint to
  // screen while in printing mode.
  frame_->StartPrinting(print_params.default_page_description,
                        maximum_shink_factor);
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
  print_context->BeginPrintMode(WebPrintParams(gfx::SizeF(800, 1000)));
  const ComputedStyle* style = document->StyleForPage(page_number);

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

bool PrintContext::use_printing_layout() const {
  return use_printing_layout_;
}

ScopedPrintContext::ScopedPrintContext(LocalFrame* frame)
    : context_(MakeGarbageCollected<PrintContext>(frame)) {}

ScopedPrintContext::~ScopedPrintContext() {
  context_->EndPrintMode();
}

}  // namespace blink
