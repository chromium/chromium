// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/frame_serializer_delegate_impl.h"

#include "third_party/blink/public/web/web_frame_serializer.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/html/link_rel_attribute.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/geometry/layout_point.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mhtml/mhtml_parser.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

namespace {

const int kPopupOverlayZIndexThreshold = 50;
const char kShadowModeAttributeName[] = "shadowmode";
const char kShadowDelegatesFocusAttributeName[] = "shadowdelegatesfocus";

}  // namespace

// static
String FrameSerializerDelegateImpl::GetContentID(Frame* frame) {
  DCHECK(frame);
  String frame_id = String(frame->GetFrameIdForTracing().data());
  return "<frame-" + frame_id + "@mhtml.blink>";
}

FrameSerializerDelegateImpl::FrameSerializerDelegateImpl(
    WebFrameSerializer::MHTMLPartsGenerationDelegate& web_delegate,
    HeapHashSet<WeakMember<const Element>>& shadow_template_elements)
    : web_delegate_(web_delegate),
      shadow_template_elements_(shadow_template_elements),
      popup_overlays_skipped_(false) {}

bool FrameSerializerDelegateImpl::ShouldIgnoreElement(const Element& element) {
  if (ShouldIgnoreHiddenElement(element))
    return true;
  if (ShouldIgnoreMetaElement(element))
    return true;
  if (web_delegate_.RemovePopupOverlay() &&
      ShouldIgnorePopupOverlayElement(element)) {
    return true;
  }
  // Remove <link> for stylesheets that do not load.
  auto* html_link_element = DynamicTo<HTMLLinkElement>(element);
  if (html_link_element && html_link_element->RelAttribute().IsStyleSheet() &&
      !html_link_element->sheet()) {
    return true;
  }
  return false;
}

bool FrameSerializerDelegateImpl::ShouldIgnoreHiddenElement(
    const Element& element) {
  // If an iframe is in the head, it will be moved to the body when the page is
  // being loaded. But if an iframe is injected into the head later, it will
  // stay there and not been displayed. To prevent it from being brought to the
  // saved page and cause it being displayed, we should not include it.
  if (IsA<HTMLIFrameElement>(element) &&
      Traversal<HTMLHeadElement>::FirstAncestor(element)) {
    return true;
  }

  // Do not include the element that is marked with hidden attribute.
  if (element.FastHasAttribute(html_names::kHiddenAttr))
    return true;

  // Do not include the hidden form element.
  auto* html_element_element = DynamicTo<HTMLInputElement>(&element);
  return html_element_element &&
         html_element_element->type() == input_type_names::kHidden;
}

bool FrameSerializerDelegateImpl::ShouldIgnoreMetaElement(
    const Element& element) {
  // Do not include meta elements that declare Content-Security-Policy
  // directives. They should have already been enforced when the original
  // document is loaded. Since only the rendered resources are encapsulated in
  // the saved MHTML page, there is no need to carry the directives. If they
  // are still kept in the MHTML, child frames that are referred to using cid:
  // scheme could be prevented from loading.
  if (!IsA<HTMLMetaElement>(element))
    return false;
  if (!element.FastHasAttribute(html_names::kContentAttr))
    return false;
  const AtomicString& http_equiv =
      element.FastGetAttribute(html_names::kHttpEquivAttr);
  return http_equiv == "Content-Security-Policy";
}

bool FrameSerializerDelegateImpl::ShouldIgnorePopupOverlayElement(
    const Element& element) {
  // The element should be visible.
  LayoutBox* box = element.GetLayoutBox();
  if (!box)
    return false;

  // The bounding box of the element should contain center point of the
  // viewport.
  LocalDOMWindow* window = element.GetDocument().domWindow();
  DCHECK(window);
  int center_x = window->innerWidth() / 2;
  int center_y = window->innerHeight() / 2;
  if (Page* page = element.GetDocument().GetPage()) {
    center_x = page->GetChromeClient().WindowToViewportScalar(
        window->GetFrame(), center_x);
    center_y = page->GetChromeClient().WindowToViewportScalar(
        window->GetFrame(), center_y);
  }
  LayoutPoint center_point(center_x, center_y);
  if (!box->FrameRect().Contains(center_point))
    return false;

  // The z-index should be greater than the threshold.
  if (box->Style()->EffectiveZIndex() < kPopupOverlayZIndexThreshold)
    return false;

  popup_overlays_skipped_ = true;

  return true;
}

bool FrameSerializerDelegateImpl::ShouldIgnoreAttribute(
    const Element& element,
    const Attribute& attribute) {
  // TODO(fgorski): Presence of srcset attribute causes MHTML to not display
  // images, as only the value of src is pulled into the archive. Discarding
  // srcset prevents the problem. Long term we should make sure to MHTML plays
  // nicely with srcset.
  if (IsA<HTMLImageElement>(element) &&
      (attribute.LocalName() == html_names::kSrcsetAttr ||
       attribute.LocalName() == html_names::kSizesAttr)) {
    return true;
  }

  // Do not save ping attribute since anyway the ping will be blocked from
  // MHTML.
  if (IsA<HTMLAnchorElement>(element) &&
      attribute.LocalName() == html_names::kPingAttr) {
    return true;
  }

  // The special attribute in a template element to denote the shadow DOM
  // should only be generated from MHTML serialization. If it is found in the
  // original page, it should be ignored.
  if (IsA<HTMLTemplateElement>(element) &&
      (attribute.LocalName() == kShadowModeAttributeName ||
       attribute.LocalName() == kShadowDelegatesFocusAttributeName) &&
      !shadow_template_elements_.Contains(&element)) {
    return true;
  }

  // If srcdoc attribute for frame elements will be rewritten as src attribute
  // containing link instead of html contents, don't ignore the attribute.
  // Bail out now to avoid the check in Element::isScriptingAttribute.
  bool is_src_doc_attribute = IsA<HTMLFrameElementBase>(element) &&
                              attribute.GetName() == html_names::kSrcdocAttr;
  String new_link_for_the_element;
  if (is_src_doc_attribute && RewriteLink(element, new_link_for_the_element))
    return false;

  //  Drop integrity attribute for those links with subresource loaded.
  auto* html_link_element = DynamicTo<HTMLLinkElement>(element);
  if (attribute.LocalName() == html_names::kIntegrityAttr &&
      html_link_element && html_link_element->sheet()) {
    return true;
  }

  // Do not include attributes that contain javascript. This is because the
  // script will not be executed when a MHTML page is being loaded.
  return element.IsScriptingAttribute(attribute);
}

bool FrameSerializerDelegateImpl::RewriteLink(const Element& element,
                                              String& rewritten_link) {
  auto* frame_owner = DynamicTo<HTMLFrameOwnerElement>(element);
  if (!frame_owner)
    return false;

  Frame* frame = frame_owner->ContentFrame();
  if (!frame)
    return false;

  WebString content_id = GetContentID(frame);
  KURL cid_uri = MHTMLParser::ConvertContentIDToURI(content_id);
  DCHECK(cid_uri.IsValid());
  rewritten_link = cid_uri.GetString();
  return true;
}

bool FrameSerializerDelegateImpl::ShouldSkipResourceWithURL(const KURL& url) {
  return web_delegate_.ShouldSkipResource(url);
}

Vector<Attribute> FrameSerializerDelegateImpl::GetCustomAttributes(
    const Element& element) {
  Vector<Attribute> attributes;

  if (auto* image = DynamicTo<HTMLImageElement>(element)) {
    GetCustomAttributesForImageElement(*image, &attributes);
  }

  return attributes;
}

void FrameSerializerDelegateImpl::GetCustomAttributesForImageElement(
    const HTMLImageElement& element,
    Vector<Attribute>* attributes) {
  // Currently only the value of src is pulled into the archive and the srcset
  // attribute is ignored (see shouldIgnoreAttribute() above). If the device
  // has a higher DPR, a different image from srcset could be loaded instead.
  // When this occurs, we should provide the rendering width and height for
  // <img> element if not set.

  // The image should be loaded and participate the layout.
  ImageResourceContent* image = element.CachedImage();
  if (!image || !image->HasImage() || image->ErrorOccurred() ||
      !element.GetLayoutObject()) {
    return;
  }

  // The width and height attributes should not be set.
  if (element.FastHasAttribute(html_names::kWidthAttr) ||
      element.FastHasAttribute(html_names::kHeightAttr)) {
    return;
  }

  // Check if different image is loaded. naturalWidth/naturalHeight will return
  // the image size adjusted with current DPR.
  if ((static_cast<int>(element.naturalWidth())) ==
          image->GetImage()->width() &&
      (static_cast<int>(element.naturalHeight())) ==
          image->GetImage()->height()) {
    return;
  }

  Attribute width_attribute(html_names::kWidthAttr,
                            AtomicString::Number(element.LayoutBoxWidth()));
  attributes->push_back(width_attribute);
  Attribute height_attribute(html_names::kHeightAttr,
                             AtomicString::Number(element.LayoutBoxHeight()));
  attributes->push_back(height_attribute);
}

std::pair<Node*, Element*> FrameSerializerDelegateImpl::GetAuxiliaryDOMTree(
    const Element& element) const {
  ShadowRoot* shadow_root = element.GetShadowRoot();
  if (!shadow_root)
    return std::pair<Node*, Element*>();

  String shadow_mode;
  switch (shadow_root->GetType()) {
    case ShadowRootType::kUserAgent:
      // No need to serialize.
      return std::pair<Node*, Element*>();
    case ShadowRootType::kOpen:
      shadow_mode = "open";
      break;
    case ShadowRootType::kClosed:
      shadow_mode = "closed";
      break;
  }

  // Put the shadow DOM content inside a template element. A special attribute
  // is set to tell the mode of the shadow DOM.
  auto* template_element = MakeGarbageCollected<Element>(
      html_names::kTemplateTag, &(element.GetDocument()));
  template_element->setAttribute(
      QualifiedName(g_null_atom, kShadowModeAttributeName, g_null_atom),
      AtomicString(shadow_mode));
  if (shadow_root->delegatesFocus()) {
    template_element->setAttribute(
        QualifiedName(g_null_atom, kShadowDelegatesFocusAttributeName,
                      g_null_atom),
        g_empty_atom);
  }
  shadow_template_elements_.insert(template_element);

  return std::pair<Node*, Element*>(shadow_root, template_element);
}

}  // namespace blink
