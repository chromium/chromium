/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
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
 *
 */

#include "third_party/blink/renderer/core/html/forms/image_input_type.h"

#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_image_fallback_helper.h"
#include "third_party/blink/renderer/core/html/html_image_loader.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_object_factory.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

ImageInputType::ImageInputType(HTMLInputElement& element)
    : BaseButtonInputType(element), use_fallback_content_(false) {}

void ImageInputType::CountUsage() {
  CountUsageIfVisible(WebFeature::kInputTypeImage);
}

const AtomicString& ImageInputType::FormControlType() const {
  return input_type_names::kImage;
}

bool ImageInputType::IsFormDataAppendable() const {
  return true;
}

void ImageInputType::AppendToFormData(FormData& form_data) const {
  if (!GetElement().IsActivatedSubmit())
    return;
  const AtomicString& name = GetElement().GetName();
  if (name.IsEmpty()) {
    form_data.AppendFromElement("x", click_location_.X());
    form_data.AppendFromElement("y", click_location_.Y());
    return;
  }

  DEFINE_STATIC_LOCAL(String, dot_x_string, (".x"));
  DEFINE_STATIC_LOCAL(String, dot_y_string, (".y"));
  form_data.AppendFromElement(name + dot_x_string, click_location_.X());
  form_data.AppendFromElement(name + dot_y_string, click_location_.Y());
}

String ImageInputType::ResultForDialogSubmit() const {
  StringBuilder result;
  result.AppendNumber(click_location_.X());
  result.Append(',');
  result.AppendNumber(click_location_.Y());
  return result.ToString();
}

bool ImageInputType::SupportsValidation() const {
  return false;
}

static IntPoint ExtractClickLocation(const Event& event) {
  if (!event.UnderlyingEvent() || !event.UnderlyingEvent()->IsMouseEvent())
    return IntPoint();
  auto& mouse_event = *ToMouseEvent(event.UnderlyingEvent());
  if (!mouse_event.HasPosition())
    return IntPoint();
  return IntPoint(mouse_event.offsetX(), mouse_event.offsetY());
}

void ImageInputType::HandleDOMActivateEvent(Event& event) {
  if (GetElement().IsDisabledFormControl() || !GetElement().Form())
    return;
  click_location_ = ExtractClickLocation(event);
  // Event handlers can run.
  GetElement().Form()->PrepareForSubmission(&event, &GetElement());
  event.SetDefaultHandled();
}

LayoutObject* ImageInputType::CreateLayoutObject(const ComputedStyle& style,
                                                 LegacyLayout legacy) const {
  if (use_fallback_content_)
    return LayoutObjectFactory::CreateBlockFlow(GetElement(), style, legacy);
  LayoutImage* image = new LayoutImage(&GetElement());
  image->SetImageResource(MakeGarbageCollected<LayoutImageResource>());
  return image;
}

void ImageInputType::AltAttributeChanged() {
  if (GetElement().UserAgentShadowRoot()) {
    Element* text =
        GetElement().UserAgentShadowRoot()->getElementById("alttext");
    String value = GetElement().AltText();
    if (text && text->textContent() != value)
      text->setTextContent(GetElement().AltText());
  }
}

void ImageInputType::SrcAttributeChanged() {
  if (!GetElement().GetLayoutObject())
    return;
  GetElement().EnsureImageLoader().UpdateFromElement(
      ImageLoader::kUpdateIgnorePreviousError);
}

void ImageInputType::ValueAttributeChanged() {
  if (use_fallback_content_)
    return;
  BaseButtonInputType::ValueAttributeChanged();
}

void ImageInputType::StartResourceLoading() {
  BaseButtonInputType::StartResourceLoading();

  HTMLImageLoader& image_loader = GetElement().EnsureImageLoader();
  image_loader.UpdateFromElement();
}

void ImageInputType::OnAttachWithLayoutObject() {
  LayoutObject* layout_object = GetElement().GetLayoutObject();
  DCHECK(layout_object);
  if (!layout_object->IsLayoutImage())
    return;

  HTMLImageLoader& image_loader = GetElement().EnsureImageLoader();
  LayoutImageResource* image_resource =
      ToLayoutImage(layout_object)->ImageResource();
  image_resource->SetImageResource(image_loader.GetContent());
}

bool ImageInputType::ShouldRespectAlignAttribute() {
  return true;
}

bool ImageInputType::CanBeSuccessfulSubmitButton() {
  return true;
}

bool ImageInputType::IsEnumeratable() {
  return false;
}

bool ImageInputType::ShouldRespectHeightAndWidthAttributes() {
  return true;
}

unsigned ImageInputType::Height() const {
  if (!GetElement().GetLayoutObject()) {
    // Check the attribute first for an explicit pixel value.
    unsigned height;
    if (ParseHTMLNonNegativeInteger(
            GetElement().FastGetAttribute(html_names::kHeightAttr), height))
      return height;

    // If the image is available, use its height.
    HTMLImageLoader* image_loader = GetElement().ImageLoader();
    if (image_loader && image_loader->GetContent()) {
      return image_loader->GetContent()
          ->IntrinsicSize(LayoutObject::ShouldRespectImageOrientation(nullptr))
          .Height();
    }
  }

  GetElement().GetDocument().UpdateStyleAndLayout();

  LayoutBox* box = GetElement().GetLayoutBox();
  return box ? AdjustForAbsoluteZoom::AdjustInt(box->ContentHeight().ToInt(),
                                                box)
             : 0;
}

unsigned ImageInputType::Width() const {
  if (!GetElement().GetLayoutObject()) {
    // Check the attribute first for an explicit pixel value.
    unsigned width;
    if (ParseHTMLNonNegativeInteger(
            GetElement().FastGetAttribute(html_names::kWidthAttr), width))
      return width;

    // If the image is available, use its width.
    HTMLImageLoader* image_loader = GetElement().ImageLoader();
    if (image_loader && image_loader->GetContent()) {
      return image_loader->GetContent()
          ->IntrinsicSize(LayoutObject::ShouldRespectImageOrientation(nullptr))
          .Width();
    }
  }

  GetElement().GetDocument().UpdateStyleAndLayout();

  LayoutBox* box = GetElement().GetLayoutBox();
  return box ? AdjustForAbsoluteZoom::AdjustInt(box->ContentWidth().ToInt(),
                                                box)
             : 0;
}

bool ImageInputType::HasLegalLinkAttribute(const QualifiedName& name) const {
  return name == html_names::kSrcAttr ||
         BaseButtonInputType::HasLegalLinkAttribute(name);
}

const QualifiedName& ImageInputType::SubResourceAttributeName() const {
  return html_names::kSrcAttr;
}

void ImageInputType::EnsureFallbackContent() {
  if (use_fallback_content_)
    return;
  SetUseFallbackContent();
  ReattachFallbackContent();
}

void ImageInputType::SetUseFallbackContent() {
  if (use_fallback_content_)
    return;
  use_fallback_content_ = true;
  if (GetElement().GetDocument().InStyleRecalc())
    return;
  if (ShadowRoot* root = GetElement().UserAgentShadowRoot())
    root->RemoveChildren();
  CreateShadowSubtree();
}

void ImageInputType::EnsurePrimaryContent() {
  if (!use_fallback_content_)
    return;
  use_fallback_content_ = false;
  if (ShadowRoot* root = GetElement().UserAgentShadowRoot())
    root->RemoveChildren();
  CreateShadowSubtree();
  ReattachFallbackContent();
}

void ImageInputType::ReattachFallbackContent() {
  if (!GetElement().GetDocument().InStyleRecalc()) {
    // ComputedStyle depends on use_fallback_content_. Trigger recalc.
    GetElement().SetNeedsStyleRecalc(
        kLocalStyleChange,
        StyleChangeReasonForTracing::Create(style_change_reason::kUseFallback));
    // LayoutObject type depends on use_fallback_content_. Trigger re-attach.
    GetElement().SetForceReattachLayoutTree();
  }
}

void ImageInputType::CreateShadowSubtree() {
  if (!use_fallback_content_) {
    BaseButtonInputType::CreateShadowSubtree();
    return;
  }
  HTMLImageFallbackHelper::CreateAltTextShadowTree(GetElement());
}

scoped_refptr<ComputedStyle> ImageInputType::CustomStyleForLayoutObject(
    scoped_refptr<ComputedStyle> new_style) {
  if (!use_fallback_content_)
    return new_style;

  return HTMLImageFallbackHelper::CustomStyleForAltText(GetElement(),
                                                        std::move(new_style));
}

}  // namespace blink
