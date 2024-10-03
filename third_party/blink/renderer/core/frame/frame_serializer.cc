/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/frame/frame_serializer.h"

#include "third_party/blink/public/web/web_frame_serializer.h"
#include "third_party/blink/renderer/core/css/css_font_face_rule.h"
#include "third_party/blink/renderer/core/css/css_font_face_src_value.h"
#include "third_party/blink/renderer/core/css/css_image_value.h"
#include "third_party/blink/renderer/core/css/css_import_rule.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/serializers/markup_accumulator.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_image_loader.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
#include "third_party/blink/renderer/core/html/html_no_script_element.h"
#include "third_party/blink/renderer/core/html/html_picture_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/html/image_document.h"
#include "third_party/blink/renderer/core/html/link_rel_attribute.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/resource/font_resource.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/style/style_fetched_image.h"
#include "third_party/blink/renderer/core/style/style_image.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/mhtml/mhtml_parser.h"
#include "third_party/blink/renderer/platform/mhtml/serialized_resource.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"

namespace blink {

namespace {

const int kPopupOverlayZIndexThreshold = 50;
// Note that this is *not* the open web's declarative shadow DOM attribute,
// which is <template shadowrootmode>. This is a special attribute used by
// MHTML archive files to represent shadow roots.
const char kShadowModeAttributeName[] = "shadowmode";
const char kShadowDelegatesFocusAttributeName[] = "shadowdelegatesfocus";

using mojom::blink::FormControlType;

KURL MakePseudoCSSUrl() {
  StringBuilder pseudo_sheet_url_builder;
  pseudo_sheet_url_builder.Append("cid:css-");
  pseudo_sheet_url_builder.Append(WTF::CreateCanonicalUUIDString());
  pseudo_sheet_url_builder.Append("@mhtml.blink");
  return KURL(pseudo_sheet_url_builder.ToString());
}

void AppendLinkElement(StringBuilder& markup, const KURL& url) {
  markup.Append(R"(<link rel="stylesheet" type="text/css" href=")");
  markup.Append(url.GetString());
  markup.Append("\" />");
}

}  // namespace

// Stores the list of serialized resources which constitute the frame. The
// first resource should be the frame's content (usually HTML).
class MultiResourcePacker {
 public:
  MultiResourcePacker(
      WebFrameSerializer::MHTMLPartsGenerationDelegate* web_delegate)
      : web_delegate_(web_delegate) {}

  bool HasResource(const KURL& url) const {
    return resource_urls_.Contains(url);
  }

  void AddMainResource(const String& mime_type,
                       scoped_refptr<const SharedBuffer> data,
                       const KURL& url) {
    // The main resource must be first.
    // We do not call `ShouldAddURL()` for the main resource.
    resources_.push_front(SerializedResource(url, mime_type, std::move(data)));
  }

  void AddToResources(const String& mime_type,
                      scoped_refptr<const SharedBuffer> data,
                      const KURL& url) {
    if (!data) {
      DLOG(ERROR) << "No data for resource " << url.GetString();
      return;
    }
    CHECK(resource_urls_.Contains(url))
        << "ShouldAddURL() not called before AddToResources";
    resources_.push_back(SerializedResource(url, mime_type, std::move(data)));
  }

  void AddImageToResources(ImageResourceContent* image, const KURL& url) {
    if (!image || !image->HasImage() || image->ErrorOccurred() ||
        !ShouldAddURL(url)) {
      return;
    }

    TRACE_EVENT2("page-serialization", "FrameSerializer::addImageToResources",
                 "type", "image", "url", url.ElidedString().Utf8());
    AddToResources(image->GetResponse().MimeType(), image->GetImage()->Data(),
                   url);
  }

  // Returns whether the resource for `url` should be added. This will return
  // true only once for a `url`, because we only want to store each resource
  // once.
  bool ShouldAddURL(const KURL& url) {
    bool should_add = url.IsValid() && !resource_urls_.Contains(url) &&
                      !url.ProtocolIsData() &&
                      !web_delegate_->ShouldSkipResource(url);
    if (should_add) {
      // Make sure that `ShouldAddURL()` returns true only once for any given
      // URL. This is done because `ShouldSkipResource()` has the hidden
      // behavior of tracking which resources are being added. This is why we
      // must call it only once per url.
      resource_urls_.insert(url);
    }
    return should_add;
  }

  void AddFontToResources(FontResource& font) {
    if (!font.IsLoaded() || !font.ResourceBuffer()) {
      return;
    }
    if (!ShouldAddURL(font.Url())) {
      return;
    }

    AddToResources(font.GetResponse().MimeType(), font.ResourceBuffer(),
                   font.Url());
  }

  Deque<SerializedResource> FinishAndTakeResources() && {
    return std::move(resources_);
  }

 private:
  // This hashset is only used for de-duplicating resources to be serialized.
  HashSet<KURL> resource_urls_;

  Deque<SerializedResource> resources_;
  WebFrameSerializer::MHTMLPartsGenerationDelegate* web_delegate_;
};

class SerializerMarkupAccumulator : public MarkupAccumulator {
  STACK_ALLOCATED();

 public:
  SerializerMarkupAccumulator(
      MultiResourcePacker* resource_serializer,
      WebFrameSerializer::MHTMLPartsGenerationDelegate* web_delegate,
      Document& document)
      : MarkupAccumulator(kResolveAllURLs,
                          IsA<HTMLDocument>(document) ? SerializationType::kHTML
                                                      : SerializationType::kXML,
                          ShadowRootInclusion()),
        resource_serializer_(resource_serializer),
        web_delegate_(web_delegate),
        document_(&document) {}
  ~SerializerMarkupAccumulator() override = default;

 private:
  bool ShouldIgnoreHiddenElement(const Element& element) const {
    // If an iframe is in the head, it will be moved to the body when the page
    // is being loaded. But if an iframe is injected into the head later, it
    // will stay there and not been displayed. To prevent it from being brought
    // to the saved page and cause it being displayed, we should not include it.
    if (IsA<HTMLIFrameElement>(element) &&
        Traversal<HTMLHeadElement>::FirstAncestor(element)) {
      return true;
    }

    // Do not include the element that is marked with hidden attribute.
    if (element.FastHasAttribute(html_names::kHiddenAttr)) {
      return true;
    }

    // Do not include the hidden form element.
    auto* html_element_element = DynamicTo<HTMLInputElement>(&element);
    return html_element_element && html_element_element->FormControlType() ==
                                       FormControlType::kInputHidden;
  }

  bool ShouldIgnoreMetaElement(const Element& element) const {
    // Do not include meta elements that declare Content-Security-Policy
    // directives. They should have already been enforced when the original
    // document is loaded. Since only the rendered resources are encapsulated in
    // the saved MHTML page, there is no need to carry the directives. If they
    // are still kept in the MHTML, child frames that are referred to using cid:
    // scheme could be prevented from loading.
    if (!IsA<HTMLMetaElement>(element)) {
      return false;
    }
    if (!element.FastHasAttribute(html_names::kContentAttr)) {
      return false;
    }
    const AtomicString& http_equiv =
        element.FastGetAttribute(html_names::kHttpEquivAttr);
    return http_equiv == "Content-Security-Policy";
  }

  bool ShouldIgnorePopupOverlayElement(const Element& element) const {
    // The element should be visible.
    LayoutBox* box = element.GetLayoutBox();
    if (!box) {
      return false;
    }

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
    if (!PhysicalRect(box->PhysicalLocation(), box->Size())
             .Contains(LayoutUnit(center_x), LayoutUnit(center_y))) {
      return false;
    }

    // The z-index should be greater than the threshold.
    if (box->Style()->EffectiveZIndex() < kPopupOverlayZIndexThreshold) {
      return false;
    }

    popup_overlays_skipped_ = true;

    return true;
  }

  EmitChoice WillProcessAttribute(const Element& element,
                                  const Attribute& attribute) const override {
    // TODO(fgorski): Presence of srcset attribute causes MHTML to not display
    // images, as only the value of src is pulled into the archive. Discarding
    // srcset prevents the problem. Long term we should make sure to MHTML plays
    // nicely with srcset.
    if (IsA<HTMLImageElement>(element) &&
        (attribute.LocalName() == html_names::kSrcsetAttr ||
         attribute.LocalName() == html_names::kSizesAttr)) {
      return EmitChoice::kIgnore;
    }

    // Do not save ping attribute since anyway the ping will be blocked from
    // MHTML.
    // TODO(crbug.com/369219144): Should this be IsA<HTMLAnchorElementBase>?
    if (IsA<HTMLAnchorElement>(element) &&
        attribute.LocalName() == html_names::kPingAttr) {
      return EmitChoice::kIgnore;
    }

    // The special attribute in a template element to denote the shadow DOM
    // should only be generated from MHTML serialization. If it is found in the
    // original page, it should be ignored.
    if (IsA<HTMLTemplateElement>(element) &&
        (attribute.LocalName() == kShadowModeAttributeName ||
         attribute.LocalName() == kShadowDelegatesFocusAttributeName) &&
        !shadow_template_elements_.Contains(&element)) {
      return EmitChoice::kIgnore;
    }

    // If srcdoc attribute for frame elements will be rewritten as src attribute
    // containing link instead of html contents, don't ignore the attribute.
    // Bail out now to avoid the check in Element::isScriptingAttribute.
    bool is_src_doc_attribute = IsA<HTMLFrameElementBase>(element) &&
                                attribute.GetName() == html_names::kSrcdocAttr;
    String new_link_for_the_element;
    if (is_src_doc_attribute &&
        RewriteLink(element, new_link_for_the_element)) {
      return EmitChoice::kEmit;
    }

    //  Drop integrity attribute for those links with subresource loaded.
    auto* html_link_element = DynamicTo<HTMLLinkElement>(element);
    if (attribute.LocalName() == html_names::kIntegrityAttr &&
        html_link_element && html_link_element->sheet()) {
      return EmitChoice::kIgnore;
    }

    // Do not include attributes that contain javascript. This is because the
    // script will not be executed when a MHTML page is being loaded.
    if (element.IsScriptingAttribute(attribute)) {
      return EmitChoice::kIgnore;
    }
    return EmitChoice::kEmit;
  }

  bool RewriteLink(const Element& element, String& rewritten_link) const {
    auto* frame_owner = DynamicTo<HTMLFrameOwnerElement>(element);
    if (!frame_owner) {
      return false;
    }

    Frame* frame = frame_owner->ContentFrame();
    if (!frame) {
      return false;
    }

    WebString content_id = FrameSerializer::GetContentID(frame);
    KURL cid_uri = MHTMLParser::ConvertContentIDToURI(content_id);
    DCHECK(cid_uri.IsValid());
    rewritten_link = cid_uri.GetString();
    return true;
  }

  Vector<Attribute> GetCustomAttributes(const Element& element) {
    Vector<Attribute> attributes;

    if (auto* image = DynamicTo<HTMLImageElement>(element)) {
      GetCustomAttributesForImageElement(*image, &attributes);
    }

    return attributes;
  }

  void GetCustomAttributesForImageElement(const HTMLImageElement& element,
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

    // Check if different image is loaded. naturalWidth/naturalHeight will
    // return the image size adjusted with current DPR.
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

  std::pair<ShadowRoot*, HTMLTemplateElement*> GetShadowTree(
      const Element& element) const override {
    ShadowRoot* shadow_root = element.GetShadowRoot();
    if (!shadow_root || shadow_root->GetMode() == ShadowRootMode::kUserAgent) {
      return std::pair<ShadowRoot*, HTMLTemplateElement*>();
    }

    // Put the shadow DOM content inside a template element. A special attribute
    // is set to tell the mode of the shadow DOM.
    HTMLTemplateElement* template_element =
        MakeGarbageCollected<HTMLTemplateElement>(element.GetDocument());
    template_element->setAttribute(
        QualifiedName(AtomicString(kShadowModeAttributeName)),
        AtomicString(shadow_root->GetMode() == ShadowRootMode::kOpen
                         ? "open"
                         : "closed"));
    if (shadow_root->delegatesFocus()) {
      template_element->setAttribute(
          QualifiedName(AtomicString(kShadowDelegatesFocusAttributeName)),
          g_empty_atom);
    }
    shadow_template_elements_.insert(template_element);

    return std::pair<ShadowRoot*, HTMLTemplateElement*>(shadow_root,
                                                        template_element);
  }

  void AppendCustomAttributes(const Element& element) override {
    Vector<Attribute> attributes = GetCustomAttributes(element);
    for (const auto& attribute : attributes) {
      AppendAttribute(element, attribute);
    }
  }

  EmitChoice WillProcessElement(const Element& element) override {
    if (IsA<HTMLScriptElement>(element)) {
      return EmitChoice::kIgnore;
    }
    if (IsA<HTMLNoScriptElement>(element)) {
      return EmitChoice::kIgnore;
    }
    auto* meta = DynamicTo<HTMLMetaElement>(element);
    if (meta && meta->ComputeEncoding().IsValid()) {
      return EmitChoice::kIgnore;
    }

    if (MHTMLImprovementsEnabled()) {
      // When `MHTMLImprovementsEnabled()`, we replace <style> with a <link> to
      // the serialized style sheet.
      if (const HTMLStyleElement* style_element =
              DynamicTo<HTMLStyleElement>(element)) {
        CSSStyleSheet* sheet = style_element->sheet();
        if (sheet) {
          AppendStylesheet(*sheet);
          return EmitChoice::kIgnore;
        }
      }
    } else {
      // A <link> element is inserted in `AppendExtraForHeadElement()` as a
      // substitute for this element.
      if (IsA<HTMLStyleElement>(element)) {
        return EmitChoice::kIgnore;
      }
    }

    if (ShouldIgnoreHiddenElement(element)) {
      return EmitChoice::kIgnore;
    }
    if (ShouldIgnoreMetaElement(element)) {
      return EmitChoice::kIgnore;
    }
    if (web_delegate_->RemovePopupOverlay() &&
        ShouldIgnorePopupOverlayElement(element)) {
      return EmitChoice::kIgnore;
    }
    // Remove <link> for stylesheets that do not load.
    auto* html_link_element = DynamicTo<HTMLLinkElement>(element);
    if (html_link_element && html_link_element->RelAttribute().IsStyleSheet() &&
        !html_link_element->sheet()) {
      return EmitChoice::kIgnore;
    }
    return MarkupAccumulator::WillProcessElement(element);
  }

  void WillCloseSyntheticTemplateElement(ShadowRoot& auxiliary_tree) override {
    if (MHTMLImprovementsEnabled()) {
      AppendAdoptedStyleSheets(&auxiliary_tree);
    }
  }

  AtomicString AppendElement(const Element& element) override {
    AtomicString prefix = MarkupAccumulator::AppendElement(element);

    if (IsA<HTMLHeadElement>(element)) {
      AppendExtraForHeadElement(element);
    }
    AddResourceForElement(*document_, element);

    // FIXME: For object (plugins) tags and video tag we could replace them by
    // an image of their current contents.

    return prefix;
  }

  void AppendEndTag(const Element& element,
                    const AtomicString& prefix) override {
    if (MHTMLImprovementsEnabled()) {
      // Add adopted stylesheets to the very end of the document, so they
      // processed after other stylesheets.
      if (IsA<HTMLHtmlElement>(element)) {
        AppendAdoptedStyleSheets(document_);
      }
    }
    MarkupAccumulator::AppendEndTag(element, prefix);
  }

  void AppendExtraForHeadElement(const Element& element) {
    DCHECK(IsA<HTMLHeadElement>(element));

    // TODO(tiger): Refactor MarkupAccumulator so it is easier to append an
    // element like this, without special cases for XHTML
    markup_.Append("<meta http-equiv=\"Content-Type\" content=\"");
    AppendAttributeValue(document_->SuggestedMIMEType());
    markup_.Append("; charset=");
    AppendAttributeValue(document_->characterSet());
    if (document_->IsXHTMLDocument()) {
      markup_.Append("\" />");
    } else {
      markup_.Append("\">");
    }

    // The CSS rules of a style element can be updated dynamically independent
    // of the CSS text included in the style element. So we can't use the inline
    // CSS text defined in the style element. To solve this, we serialize the
    // working CSS rules in document.stylesheets and document.adoptedStyleSheets
    // and wrap them in link elements.
    // Adopted stylesheets are evaluated last, so we append them last.
    if (!MHTMLImprovementsEnabled()) {
      AppendStylesheets(document_->StyleSheets(), true /*style_element_only*/);
    }
  }

  // Add `sheet` as a new resource and emit a <link> element to load it.
  void AppendStylesheet(StyleSheet& sheet) {
    if (!sheet.IsCSSStyleSheet() || sheet.disabled()) {
      return;
    }

    KURL pseudo_sheet_url = MakePseudoCSSUrl();
    AppendLinkElement(markup_, pseudo_sheet_url);
    SerializeCSSStyleSheet(static_cast<CSSStyleSheet&>(sheet),
                           pseudo_sheet_url);
  }

  void AppendStylesheets(StyleSheetList& sheets, bool style_element_only) {
    for (unsigned i = 0; i < sheets.length(); ++i) {
      StyleSheet* sheet = sheets.item(i);
      if (style_element_only && !IsA<HTMLStyleElement>(sheet->ownerNode())) {
        continue;
      }
      AppendStylesheet(*sheet);
    }
  }

  // Appends <link> elements to construct the same styles from `root`'s
  // `AdoptedStyleSheets()`.
  void AppendAdoptedStyleSheets(TreeScope* root) {
    auto* sheets = root->AdoptedStyleSheets();
    if (!sheets) {
      return;
    }

    for (blink::CSSStyleSheet* sheet : *sheets) {
      // Serialize the stylesheet only the first time it's visited.
      KURL pseudo_sheet_url;
      auto iter = stylesheet_pseudo_urls_.find(sheet);
      if (iter != stylesheet_pseudo_urls_.end()) {
        pseudo_sheet_url = iter->value;
      } else {
        pseudo_sheet_url = MakePseudoCSSUrl();
        SerializeCSSStyleSheet(static_cast<CSSStyleSheet&>(*sheet),
                               pseudo_sheet_url);
        stylesheet_pseudo_urls_.insert(sheet, pseudo_sheet_url);
      }

      AppendLinkElement(markup_, pseudo_sheet_url);
    }
  }

  void AppendStylesheets(Document* document, bool style_element_only) {
    StyleSheetList& sheets = document->StyleSheets();
    for (unsigned i = 0; i < sheets.length(); ++i) {
      StyleSheet* sheet = sheets.item(i);
      if (!sheet->IsCSSStyleSheet() || sheet->disabled()) {
        continue;
      }
      if (style_element_only && !IsA<HTMLStyleElement>(sheet->ownerNode())) {
        continue;
      }

      KURL pseudo_sheet_url = MakePseudoCSSUrl();
      AppendLinkElement(markup_, pseudo_sheet_url);
      SerializeCSSStyleSheet(static_cast<CSSStyleSheet&>(*sheet),
                             pseudo_sheet_url);
    }
  }

  void AppendAttribute(const Element& element,
                       const Attribute& attribute) override {
    // Check if link rewriting can affect the attribute.
    bool is_link_attribute = element.HasLegalLinkAttribute(attribute.GetName());
    bool is_src_doc_attribute = IsA<HTMLFrameElementBase>(element) &&
                                attribute.GetName() == html_names::kSrcdocAttr;
    if (is_link_attribute || is_src_doc_attribute) {
      // Check if the delegate wants to do link rewriting for the element.
      String new_link_for_the_element;
      if (RewriteLink(element, new_link_for_the_element)) {
        if (is_link_attribute) {
          // Rewrite element links.
          AppendRewrittenAttribute(element, attribute.GetName().ToString(),
                                   new_link_for_the_element);
        } else {
          DCHECK(is_src_doc_attribute);
          // Emit src instead of srcdoc attribute for frame elements - we want
          // the serialized subframe to use html contents from the link provided
          // by Delegate::rewriteLink rather than html contents from srcdoc
          // attribute.
          AppendRewrittenAttribute(element, html_names::kSrcAttr.LocalName(),
                                   new_link_for_the_element);
        }
        return;
      }
    }

    // Fallback to appending the original attribute.
    MarkupAccumulator::AppendAttribute(element, attribute);
  }

  void AppendAttributeValue(const String& attribute_value) {
    MarkupFormatter::AppendAttributeValue(
        markup_, attribute_value, IsA<HTMLDocument>(document_), *document_);
  }

  void AppendRewrittenAttribute(const Element& element,
                                const String& attribute_name,
                                const String& attribute_value) {
    if (elements_with_rewritten_links_.Contains(&element)) {
      return;
    }
    elements_with_rewritten_links_.insert(&element);

    // Append the rewritten attribute.
    // TODO(tiger): Refactor MarkupAccumulator so it is easier to append an
    // attribute like this.
    markup_.Append(' ');
    markup_.Append(attribute_name);
    markup_.Append("=\"");
    AppendAttributeValue(attribute_value);
    markup_.Append("\"");
  }

  void AddResourceForElement(Document& document, const Element& element) {
    // We have to process in-line style as it might contain some resources
    // (typically background images).
    if (element.IsStyledElement()) {
      RetrieveResourcesForProperties(element.InlineStyle(), document);
      RetrieveResourcesForProperties(
          const_cast<Element&>(element).PresentationAttributeStyle(), document);
    }

    if (const auto* image = DynamicTo<HTMLImageElement>(element)) {
      AtomicString image_url_value;
      const Element* parent = element.parentElement();
      if (parent && IsA<HTMLPictureElement>(parent)) {
        // If parent element is <picture>, use ImageSourceURL() to get best fit
        // image URL from sibling source.
        image_url_value = image->ImageSourceURL();
      } else {
        // Otherwise, it is single <img> element. We should get image url
        // contained in href attribute. ImageSourceURL() may return a different
        // URL from srcset attribute.
        image_url_value = image->FastGetAttribute(html_names::kSrcAttr);
      }
      ImageResourceContent* cached_image = image->CachedImage();
      resource_serializer_->AddImageToResources(
          cached_image, document.CompleteURL(image_url_value));
    } else if (const auto* input = DynamicTo<HTMLInputElement>(element)) {
      if (input->FormControlType() == FormControlType::kInputImage &&
          input->ImageLoader()) {
        KURL image_url = input->Src();
        ImageResourceContent* cached_image = input->ImageLoader()->GetContent();
        resource_serializer_->AddImageToResources(cached_image, image_url);
      }
    } else if (const auto* link = DynamicTo<HTMLLinkElement>(element)) {
      if (CSSStyleSheet* sheet = link->sheet()) {
        KURL sheet_url =
            document.CompleteURL(link->FastGetAttribute(html_names::kHrefAttr));
        SerializeCSSStyleSheet(*sheet, sheet_url);
      }
    } else if (const auto* style = DynamicTo<HTMLStyleElement>(element)) {
      if (CSSStyleSheet* sheet = style->sheet()) {
        SerializeCSSStyleSheet(*sheet, NullURL());
      }
    } else if (const auto* plugin = DynamicTo<HTMLPlugInElement>(&element)) {
      if (plugin->IsImageType() && plugin->ImageLoader()) {
        KURL image_url = document.CompleteURL(plugin->Url());
        ImageResourceContent* cached_image =
            plugin->ImageLoader()->GetContent();
        resource_serializer_->AddImageToResources(cached_image, image_url);
      }
    }
  }

  void SerializeCSSStyleSheet(CSSStyleSheet& style_sheet, const KURL& url) {
    // If the URL is invalid or if it is a data URL this means that this CSS is
    // defined inline, respectively in a <style> tag or in the data URL itself.
    bool is_inline_css = !url.IsValid() || url.ProtocolIsData();
    // If this CSS is not inline then it is identifiable by its URL. So just
    // skip it if it has already been analyzed before.
    if (!is_inline_css && !resource_serializer_->ShouldAddURL(url)) {
      return;
    }

    TRACE_EVENT2("page-serialization",
                 "FrameSerializer::serializeCSSStyleSheet", "type", "CSS",
                 "url", url.ElidedString().Utf8());

    const auto& charset = style_sheet.Contents()->Charset();

    // If this CSS is inlined its definition was already serialized with the
    // frame HTML code that was previously generated. No need to regenerate it
    // here.
    if (!is_inline_css) {
      StringBuilder css_text;
      // Adopted stylesheets may not have a defined charset, so use UTF-8 in
      // that case.
      css_text.Append("@charset \"");
      css_text.Append(charset.IsValid()
                          ? charset.GetName().GetString().DeprecatedLower()
                          : "utf-8");
      css_text.Append("\";\n\n");

      for (unsigned i = 0; i < style_sheet.length(); ++i) {
        CSSRule* rule = style_sheet.ItemInternal(i);
        String item_text = rule->cssText();
        if (!item_text.empty()) {
          css_text.Append(item_text);
          if (i < style_sheet.length() - 1) {
            css_text.Append("\n\n");
          }
        }
      }

      String text_string = css_text.ToString();
      std::string text;
      if (charset.IsValid()) {
        WTF::TextEncoding text_encoding(charset);
        text = text_encoding.Encode(text_string,
                                    WTF::kCSSEncodedEntitiesForUnencodables);
      } else {
        text = WTF::UTF8Encoding().Encode(
            text_string, WTF::kCSSEncodedEntitiesForUnencodables);
      }

      resource_serializer_->AddToResources(
          String("text/css"), SharedBuffer::Create(text.c_str(), text.length()),
          url);
    }

    // Sub resources need to be serialized even if the CSS definition doesn't
    // need to be.
    for (unsigned i = 0; i < style_sheet.length(); ++i) {
      SerializeCSSRule(style_sheet.ItemInternal(i));
    }
  }

  void SerializeCSSRule(CSSRule* rule) {
    DCHECK(rule->parentStyleSheet()->OwnerDocument());
    Document& document = *rule->parentStyleSheet()->OwnerDocument();

    switch (rule->GetType()) {
      case CSSRule::kStyleRule:
        RetrieveResourcesForProperties(
            &To<CSSStyleRule>(rule)->GetStyleRule()->Properties(), document);
        break;

      case CSSRule::kImportRule: {
        CSSImportRule* import_rule = To<CSSImportRule>(rule);
        KURL sheet_base_url = rule->parentStyleSheet()->BaseURL();
        DCHECK(sheet_base_url.IsValid());
        KURL import_url = KURL(sheet_base_url, import_rule->href());
        if (import_rule->styleSheet()) {
          SerializeCSSStyleSheet(*import_rule->styleSheet(), import_url);
        }
        break;
      }

      // Rules inheriting CSSGroupingRule
      case CSSRule::kNestedDeclarationsRule:
      case CSSRule::kMediaRule:
      case CSSRule::kSupportsRule:
      case CSSRule::kContainerRule:
      case CSSRule::kLayerBlockRule:
      case CSSRule::kScopeRule:
      case CSSRule::kStartingStyleRule: {
        CSSRuleList* rule_list = rule->cssRules();
        for (unsigned i = 0; i < rule_list->length(); ++i) {
          SerializeCSSRule(rule_list->item(i));
        }
        break;
      }

      case CSSRule::kFontFaceRule:
        RetrieveResourcesForProperties(
            &To<CSSFontFaceRule>(rule)->StyleRule()->Properties(), document);
        break;

      case CSSRule::kCounterStyleRule:
        // TODO(crbug.com/1176323): Handle image symbols in @counter-style rules
        // when we implement it.
        break;

      case CSSRule::kMarginRule:
      case CSSRule::kPageRule:
        // TODO(crbug.com/40341678): Both page and margin rules may contain
        // external resources (e.g. via background-image). FrameSerializer is at
        // the mercy of whatever resource loading has already been triggered (by
        // regular lifecycle updates). See crbug.com/364331857 . As such, unless
        // the user has actually tried to print the page, resources inside @page
        // rules won't have been loaded. Rather than introducing flaky behavior
        // (sometimes @page resources are loaded, sometimes not), let's wait for
        // that bug to be fixed.
        break;

      // Rules in which no external resources can be referenced
      case CSSRule::kCharsetRule:
      case CSSRule::kFontPaletteValuesRule:
      case CSSRule::kFontFeatureRule:
      case CSSRule::kFontFeatureValuesRule:
      case CSSRule::kPropertyRule:
      case CSSRule::kKeyframesRule:
      case CSSRule::kKeyframeRule:
      case CSSRule::kNamespaceRule:
      case CSSRule::kLayerStatementRule:
      case CSSRule::kViewTransitionRule:
      case CSSRule::kPositionTryRule:
        break;
    }
  }

  void RetrieveResourcesForProperties(
      const CSSPropertyValueSet* style_declaration,
      Document& document) {
    if (!style_declaration) {
      return;
    }

    // The background-image and list-style-image (for ul or ol) are the CSS
    // properties that make use of images. We iterate to make sure we include
    // any other image properties there might be.
    unsigned property_count = style_declaration->PropertyCount();
    for (unsigned i = 0; i < property_count; ++i) {
      const CSSValue& css_value = style_declaration->PropertyAt(i).Value();
      RetrieveResourcesForCSSValue(css_value, document);
    }
  }

  void RetrieveResourcesForCSSValue(const CSSValue& css_value,
                                    Document& document) {
    if (const auto* image_value = DynamicTo<CSSImageValue>(css_value)) {
      if (image_value->IsCachePending()) {
        return;
      }
      StyleImage* style_image = image_value->CachedImage();
      if (!style_image || !style_image->IsImageResource()) {
        return;
      }

      resource_serializer_->AddImageToResources(
          style_image->CachedImage(), style_image->CachedImage()->Url());
    } else if (const auto* font_face_src_value =
                   DynamicTo<CSSFontFaceSrcValue>(css_value)) {
      if (font_face_src_value->IsLocal()) {
        return;
      }

      resource_serializer_->AddFontToResources(
          font_face_src_value->Fetch(document.GetExecutionContext(), nullptr));
    } else if (const auto* css_value_list =
                   DynamicTo<CSSValueList>(css_value)) {
      for (unsigned i = 0; i < css_value_list->length(); i++) {
        RetrieveResourcesForCSSValue(css_value_list->Item(i), document);
      }
    }
  }

  // There are several improvements being added behind this flag. So far, it
  // covers:
  // * Serialize adopted stylesheets
  // * Serialize styleSheets on shadow roots
  // * Retain stylesheet order, previously order of stylesheets
  //   was sometimes wrong.
  bool MHTMLImprovementsEnabled() const {
    return base::FeatureList::IsEnabled(blink::features::kMHTML_Improvements);
  }

  MultiResourcePacker* resource_serializer_;
  WebFrameSerializer::MHTMLPartsGenerationDelegate* web_delegate_;
  Document* document_;

  mutable HeapHashSet<WeakMember<const Element>> shadow_template_elements_;
  mutable bool popup_overlays_skipped_ = false;

  // Elements with links rewritten via appendAttribute method.
  HeapHashSet<Member<const Element>> elements_with_rewritten_links_;
  // Adopted stylesheets can be reused. This stores the set of stylesheets
  // already serialized as resources, along with their URL.
  HeapHashMap<Member<blink::CSSStyleSheet>, KURL> stylesheet_pseudo_urls_;
};

// TODO(tiger): Right now there is no support for rewriting URLs inside CSS
// documents which leads to bugs like <https://crbug.com/251898>. Not being
// able to rewrite URLs inside CSS documents means that resources imported from
// url(...) statements in CSS might not work when rewriting links for the
// "Webpage, Complete" method of saving a page. It will take some work but it
// needs to be done if we want to continue to support non-MHTML saved pages.

// static
void FrameSerializer::SerializeFrame(
    WebFrameSerializer::MHTMLPartsGenerationDelegate& web_delegate,
    LocalFrame& frame,
    base::OnceCallback<void(Deque<SerializedResource>)> done_callback) {
  TRACE_EVENT0("page-serialization", "FrameSerializer::serializeFrame");
  DCHECK(frame.GetDocument());
  Document& document = *frame.GetDocument();
  KURL url = document.Url();
  MultiResourcePacker resource_serializer(&web_delegate);
  // If frame is an image document, add the image and don't continue
  if (auto* image_document = DynamicTo<ImageDocument>(document)) {
    resource_serializer.AddImageToResources(image_document->CachedImage(), url);
    std::move(done_callback)
        .Run(std::move(resource_serializer).FinishAndTakeResources());
    return;
  }

  {
    TRACE_EVENT0("page-serialization", "FrameSerializer::serializeFrame HTML");
    SerializerMarkupAccumulator accumulator(&resource_serializer, &web_delegate,
                                            document);
    String text =
        accumulator.SerializeNodes<EditingStrategy>(document, kIncludeNode);

    std::string frame_html =
        document.Encoding().Encode(text, WTF::kEntitiesForUnencodables);
    resource_serializer.AddMainResource(
        document.SuggestedMIMEType(),
        SharedBuffer::Create(frame_html.c_str(), frame_html.length()), url);
    // TODO(crbug.com/363289333): Add async fetching of fonts.
    std::move(done_callback)
        .Run(std::move(resource_serializer).FinishAndTakeResources());
  }
}

// Returns MOTW (Mark of the Web) declaration before html tag which is in
// HTML comment, e.g. "<!-- saved from url=(%04d)%s -->"
// See http://msdn2.microsoft.com/en-us/library/ms537628(VS.85).aspx.
// static
String FrameSerializer::MarkOfTheWebDeclaration(const KURL& url) {
  StringBuilder builder;
  bool emits_minus = false;
  for (const char ch : url.GetString().Ascii()) {
    if (ch == '-' && emits_minus) {
      builder.Append("%2D");
      emits_minus = false;
      continue;
    }
    emits_minus = ch == '-';
    builder.Append(ch);
  }
  std::string escaped_url = builder.ToString().Ascii();
  return String::Format("saved from url=(%04d)%s",
                        static_cast<int>(escaped_url.length()),
                        escaped_url.c_str());
}

// static
String FrameSerializer::GetContentID(Frame* frame) {
  DCHECK(frame);
  const String& frame_id = frame->GetFrameIdForTracing();
  return "<frame-" + frame_id + "@mhtml.blink>";
}

}  // namespace blink
