/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "third_party/blink/public/web/web_frame_serializer.h"

#include "base/macros.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_frame_serializer_client.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/exported/web_remote_frame_impl.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/frame_serializer.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/web_frame_serializer_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_all_collection.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/html/html_table_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/mhtml/mhtml_archive.h"
#include "third_party/blink/renderer/platform/mhtml/mhtml_parser.h"
#include "third_party/blink/renderer/platform/mhtml/serialized_resource.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_concatenate.h"

#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

const int kPopupOverlayZIndexThreshold = 50;
const char kShadowModeAttributeName[] = "shadowmode";
const char kShadowDelegatesFocusAttributeName[] = "shadowdelegatesfocus";

// Returns a Content-ID to be used for the given frame.
// See rfc2557 - section 8.3 - "Use of the Content-ID header and CID URLs".
// Format note - the returned string should be of the form "<foo@bar.com>"
// (i.e. the strings should include the angle brackets).
String GetContentID(Frame* frame) {
  String frame_id = String(ToTraceValue(frame).data());
  return "<frame-" + frame_id + "@mhtml.blink>";
}

class MHTMLFrameSerializerDelegate final : public FrameSerializer::Delegate {
  STACK_ALLOCATED();

 public:
  MHTMLFrameSerializerDelegate(
      WebFrameSerializer::MHTMLPartsGenerationDelegate&,
      HeapHashSet<WeakMember<const Element>>&);
  ~MHTMLFrameSerializerDelegate() override = default;
  bool ShouldIgnoreElement(const Element&) override;
  bool ShouldIgnoreAttribute(const Element&, const Attribute&) override;
  bool RewriteLink(const Element&, String& rewritten_link) override;
  bool ShouldSkipResourceWithURL(const KURL&) override;
  Vector<Attribute> GetCustomAttributes(const Element&) override;
  std::pair<Node*, Element*> GetAuxiliaryDOMTree(const Element&) const override;
  bool ShouldCollectProblemMetric() override;

 private:
  bool ShouldIgnoreHiddenElement(const Element&);
  bool ShouldIgnoreMetaElement(const Element&);
  bool ShouldIgnorePopupOverlayElement(const Element&);
  void GetCustomAttributesForImageElement(const HTMLImageElement&,
                                          Vector<Attribute>*);

  WebFrameSerializer::MHTMLPartsGenerationDelegate& web_delegate_;
  HeapHashSet<WeakMember<const Element>>& shadow_template_elements_;
  bool popup_overlays_skipped_;

  DISALLOW_COPY_AND_ASSIGN(MHTMLFrameSerializerDelegate);
};

MHTMLFrameSerializerDelegate::MHTMLFrameSerializerDelegate(
    WebFrameSerializer::MHTMLPartsGenerationDelegate& web_delegate,
    HeapHashSet<WeakMember<const Element>>& shadow_template_elements)
    : web_delegate_(web_delegate),
      shadow_template_elements_(shadow_template_elements),
      popup_overlays_skipped_(false) {}

bool MHTMLFrameSerializerDelegate::ShouldIgnoreElement(const Element& element) {
  if (ShouldIgnoreHiddenElement(element))
    return true;
  if (ShouldIgnoreMetaElement(element))
    return true;
  if (web_delegate_.RemovePopupOverlay() &&
      ShouldIgnorePopupOverlayElement(element)) {
    return true;
  }
  // Remove <link> for stylesheets that do not load.
  if (IsHTMLLinkElement(element) &&
      ToHTMLLinkElement(element).RelAttribute().IsStyleSheet() &&
      !ToHTMLLinkElement(element).sheet()) {
    return true;
  }
  return false;
}

bool MHTMLFrameSerializerDelegate::ShouldIgnoreHiddenElement(
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
  return IsHTMLInputElement(element) &&
         ToHTMLInputElement(&element)->type() == input_type_names::kHidden;
}

bool MHTMLFrameSerializerDelegate::ShouldIgnoreMetaElement(
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

bool MHTMLFrameSerializerDelegate::ShouldIgnorePopupOverlayElement(
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
  if (box->Style()->ZIndex() < kPopupOverlayZIndexThreshold)
    return false;

  popup_overlays_skipped_ = true;

  return true;
}

bool MHTMLFrameSerializerDelegate::ShouldIgnoreAttribute(
    const Element& element,
    const Attribute& attribute) {
  // TODO(fgorski): Presence of srcset attribute causes MHTML to not display
  // images, as only the value of src is pulled into the archive. Discarding
  // srcset prevents the problem. Long term we should make sure to MHTML plays
  // nicely with srcset.
  if (IsHTMLImageElement(element) &&
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
  bool is_src_doc_attribute = IsHTMLFrameElementBase(element) &&
                              attribute.GetName() == html_names::kSrcdocAttr;
  String new_link_for_the_element;
  if (is_src_doc_attribute && RewriteLink(element, new_link_for_the_element))
    return false;

  //  Drop integrity attribute for those links with subresource loaded.
  if (attribute.LocalName() == html_names::kIntegrityAttr &&
      IsHTMLLinkElement(element) && ToHTMLLinkElement(element).sheet()) {
    return true;
  }

  // Do not include attributes that contain javascript. This is because the
  // script will not be executed when a MHTML page is being loaded.
  return element.IsScriptingAttribute(attribute);
}

bool MHTMLFrameSerializerDelegate::RewriteLink(const Element& element,
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

bool MHTMLFrameSerializerDelegate::ShouldSkipResourceWithURL(const KURL& url) {
  return web_delegate_.ShouldSkipResource(url);
}

Vector<Attribute> MHTMLFrameSerializerDelegate::GetCustomAttributes(
    const Element& element) {
  Vector<Attribute> attributes;

  if (auto* image = ToHTMLImageElementOrNull(element)) {
    GetCustomAttributesForImageElement(*image, &attributes);
  }

  return attributes;
}

bool MHTMLFrameSerializerDelegate::ShouldCollectProblemMetric() {
  return web_delegate_.UsePageProblemDetectors();
}

void MHTMLFrameSerializerDelegate::GetCustomAttributesForImageElement(
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
  if (((int)element.naturalWidth()) == image->GetImage()->width() &&
      ((int)element.naturalHeight()) == image->GetImage()->height()) {
    return;
  }

  Attribute width_attribute(html_names::kWidthAttr,
                            AtomicString::Number(element.LayoutBoxWidth()));
  attributes->push_back(width_attribute);
  Attribute height_attribute(html_names::kHeightAttr,
                             AtomicString::Number(element.LayoutBoxHeight()));
  attributes->push_back(height_attribute);
}

std::pair<Node*, Element*> MHTMLFrameSerializerDelegate::GetAuxiliaryDOMTree(
    const Element& element) const {
  ShadowRoot* shadow_root = element.GetShadowRoot();
  if (!shadow_root)
    return std::pair<Node*, Element*>();

  String shadow_mode;
  switch (shadow_root->GetType()) {
    case ShadowRootType::kUserAgent:
      // No need to serialize.
      return std::pair<Node*, Element*>();
    case ShadowRootType::V0:
      shadow_mode = "v0";
      break;
    case ShadowRootType::kOpen:
      shadow_mode = "open";
      break;
    case ShadowRootType::kClosed:
      shadow_mode = "closed";
      break;
  }

  // Put the shadow DOM content inside a template element. A special attribute
  // is set to tell the mode of the shadow DOM.
  Element* template_element =
      Element::Create(html_names::kTemplateTag, &(element.GetDocument()));
  template_element->setAttribute(
      QualifiedName(g_null_atom, kShadowModeAttributeName, g_null_atom),
      AtomicString(shadow_mode));
  if (shadow_root->GetType() != ShadowRootType::V0 &&
      shadow_root->delegatesFocus()) {
    template_element->setAttribute(
        QualifiedName(g_null_atom, kShadowDelegatesFocusAttributeName,
                      g_null_atom),
        g_empty_atom);
  }
  shadow_template_elements_.insert(template_element);

  return std::pair<Node*, Element*>(shadow_root, template_element);
}

}  // namespace

WebThreadSafeData WebFrameSerializer::GenerateMHTMLHeader(
    const WebString& boundary,
    WebLocalFrame* frame,
    MHTMLPartsGenerationDelegate* delegate) {
  TRACE_EVENT0("page-serialization", "WebFrameSerializer::generateMHTMLHeader");
  DCHECK(frame);
  DCHECK(delegate);

  auto* web_local_frame = To<WebLocalFrameImpl>(frame);

  Document* document = web_local_frame->GetFrame()->GetDocument();

  scoped_refptr<RawData> buffer = RawData::Create();
  MHTMLArchive::GenerateMHTMLHeader(
      boundary, document->Url(), document->title(),
      document->SuggestedMIMEType(), base::Time::Now(), *buffer->MutableData());
  return WebThreadSafeData(buffer);
}

WebThreadSafeData WebFrameSerializer::GenerateMHTMLParts(
    const WebString& boundary,
    WebLocalFrame* web_frame,
    MHTMLPartsGenerationDelegate* web_delegate) {
  TRACE_EVENT0("page-serialization", "WebFrameSerializer::generateMHTMLParts");
  DCHECK(web_frame);
  DCHECK(web_delegate);

  // Translate arguments from public to internal blink APIs.
  LocalFrame* frame = To<WebLocalFrameImpl>(web_frame)->GetFrame();
  MHTMLArchive::EncodingPolicy encoding_policy =
      web_delegate->UseBinaryEncoding()
          ? MHTMLArchive::EncodingPolicy::kUseBinaryEncoding
          : MHTMLArchive::EncodingPolicy::kUseDefaultEncoding;

  // Serialize.
  TRACE_EVENT_BEGIN0("page-serialization",
                     "WebFrameSerializer::generateMHTMLParts serializing");
  Deque<SerializedResource> resources;
  {
    SCOPED_BLINK_UMA_HISTOGRAM_TIMER(
        "PageSerialization.MhtmlGeneration.SerializationTime.SingleFrame");
    HeapHashSet<WeakMember<const Element>> shadow_template_elements;
    MHTMLFrameSerializerDelegate core_delegate(*web_delegate,
                                               shadow_template_elements);
    FrameSerializer serializer(resources, core_delegate);
    serializer.SerializeFrame(*frame);
  }

  TRACE_EVENT_END1("page-serialization",
                   "WebFrameSerializer::generateMHTMLParts serializing",
                   "resource count", static_cast<uint64_t>(resources.size()));

  // There was an error serializing the frame (e.g. of an image resource).
  if (resources.IsEmpty())
    return WebThreadSafeData();

  // Encode serialized resources as MHTML.
  scoped_refptr<RawData> output = RawData::Create();
  {
    SCOPED_BLINK_UMA_HISTOGRAM_TIMER(
        "PageSerialization.MhtmlGeneration.EncodingTime.SingleFrame");
    // Frame is the 1st resource (see FrameSerializer::serializeFrame doc
    // comment). Frames get a Content-ID header.
    MHTMLArchive::GenerateMHTMLPart(boundary, GetContentID(frame),
                                    encoding_policy, resources.TakeFirst(),
                                    *output->MutableData());
    while (!resources.IsEmpty()) {
      TRACE_EVENT0("page-serialization",
                   "WebFrameSerializer::generateMHTMLParts encoding");
      MHTMLArchive::GenerateMHTMLPart(boundary, String(), encoding_policy,
                                      resources.TakeFirst(),
                                      *output->MutableData());
    }
  }
  return WebThreadSafeData(output);
}

bool WebFrameSerializer::Serialize(
    WebLocalFrame* frame,
    WebFrameSerializerClient* client,
    WebFrameSerializer::LinkRewritingDelegate* delegate,
    bool save_with_empty_url) {
  WebFrameSerializerImpl serializer_impl(frame, client, delegate,
                                         save_with_empty_url);
  return serializer_impl.Serialize();
}

WebString WebFrameSerializer::GenerateMetaCharsetDeclaration(
    const WebString& charset) {
  // TODO(yosin) We should call |FrameSerializer::metaCharsetDeclarationOf()|.
  String charset_string =
      "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=" +
      static_cast<const String&>(charset) + "\">";
  return charset_string;
}

WebString WebFrameSerializer::GenerateMarkOfTheWebDeclaration(
    const WebURL& url) {
  StringBuilder builder;
  builder.Append("\n<!-- ");
  builder.Append(FrameSerializer::MarkOfTheWebDeclaration(url));
  builder.Append(" -->\n");
  return builder.ToString();
}

}  // namespace blink
