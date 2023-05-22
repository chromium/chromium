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
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/serializers/markup_accumulator.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_image_loader.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/html/image_document.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/loader/resource/font_resource.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/style/style_fetched_image.h"
#include "third_party/blink/renderer/core/style/style_image.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/mhtml/serialized_resource.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"

namespace blink {

class SerializerMarkupAccumulator : public MarkupAccumulator {
  STACK_ALLOCATED();

 public:
  SerializerMarkupAccumulator(FrameSerializer::Delegate&,
                              FrameSerializerResourceDelegate&,
                              Document&);
  ~SerializerMarkupAccumulator() override;

 protected:
  void AppendCustomAttributes(const Element&) override;
  bool ShouldIgnoreAttribute(const Element&, const Attribute&) const override;
  bool ShouldIgnoreElement(const Element&) const override;
  AtomicString AppendElement(const Element&) override;
  void AppendAttribute(const Element&, const Attribute&) override;
  std::pair<Node*, Element*> GetAuxiliaryDOMTree(const Element&) const override;

 private:
  void AppendAttributeValue(const String& attribute_value);
  void AppendRewrittenAttribute(const Element&,
                                const String& attribute_name,
                                const String& attribute_value);
  void AppendExtraForHeadElement(const Element&);
  void AppendStylesheets(Document* document, bool style_element_only);

  FrameSerializer::Delegate& delegate_;
  FrameSerializerResourceDelegate& resource_delegate_;
  Document* document_;

  // Elements with links rewritten via appendAttribute method.
  HeapHashSet<Member<const Element>> elements_with_rewritten_links_;
};

SerializerMarkupAccumulator::SerializerMarkupAccumulator(
    FrameSerializer::Delegate& delegate,
    FrameSerializerResourceDelegate& resource_delegate,
    Document& document)
    : MarkupAccumulator(kResolveAllURLs,
                        IsA<HTMLDocument>(document) ? SerializationType::kHTML
                                                    : SerializationType::kXML,
                        kNoShadowRoots),
      delegate_(delegate),
      resource_delegate_(resource_delegate),
      document_(&document) {}

SerializerMarkupAccumulator::~SerializerMarkupAccumulator() = default;

void SerializerMarkupAccumulator::AppendCustomAttributes(
    const Element& element) {
  Vector<Attribute> attributes = delegate_.GetCustomAttributes(element);
  for (const auto& attribute : attributes)
    AppendAttribute(element, attribute);
}

bool SerializerMarkupAccumulator::ShouldIgnoreAttribute(
    const Element& element,
    const Attribute& attribute) const {
  return delegate_.ShouldIgnoreAttribute(element, attribute);
}

bool SerializerMarkupAccumulator::ShouldIgnoreElement(
    const Element& element) const {
  if (IsA<HTMLScriptElement>(element))
    return true;
  if (IsA<HTMLNoScriptElement>(element))
    return true;
  auto* meta = DynamicTo<HTMLMetaElement>(element);
  if (meta && meta->ComputeEncoding().IsValid()) {
    return true;
  }
  // This is done in serializing document.StyleSheets.
  if (IsA<HTMLStyleElement>(element))
    return true;
  return delegate_.ShouldIgnoreElement(element);
}

AtomicString SerializerMarkupAccumulator::AppendElement(
    const Element& element) {
  AtomicString prefix = MarkupAccumulator::AppendElement(element);

  if (IsA<HTMLHeadElement>(element))
    AppendExtraForHeadElement(element);

  resource_delegate_.AddResourceForElement(*document_, element);

  // FIXME: For object (plugins) tags and video tag we could replace them by an
  // image of their current contents.

  return prefix;
}

void SerializerMarkupAccumulator::AppendExtraForHeadElement(
    const Element& element) {
  DCHECK(IsA<HTMLHeadElement>(element));

  // TODO(tiger): Refactor MarkupAccumulator so it is easier to append an
  // element like this, without special cases for XHTML
  markup_.Append("<meta http-equiv=\"Content-Type\" content=\"");
  AppendAttributeValue(document_->SuggestedMIMEType());
  markup_.Append("; charset=");
  AppendAttributeValue(document_->characterSet());
  if (document_->IsXHTMLDocument())
    markup_.Append("\" />");
  else
    markup_.Append("\">");

  // The CSS rules of a style element can be updated dynamically independent of
  // the CSS text included in the style element. So we can't use the inline
  // CSS text defined in the style element. To solve this, we serialize the
  // working CSS rules in document.stylesheets and wrap them in link elements.
  AppendStylesheets(document_, true /*style_element_only*/);
}

void SerializerMarkupAccumulator::AppendStylesheets(Document* document,
                                                    bool style_element_only) {
  StyleSheetList& sheets = document->StyleSheets();
  for (unsigned i = 0; i < sheets.length(); ++i) {
    StyleSheet* sheet = sheets.item(i);
    if (!sheet->IsCSSStyleSheet() || sheet->disabled())
      continue;
    if (style_element_only && !IsA<HTMLStyleElement>(sheet->ownerNode()))
      continue;

    StringBuilder pseudo_sheet_url_builder;
    pseudo_sheet_url_builder.Append("cid:css-");
    pseudo_sheet_url_builder.Append(WTF::CreateCanonicalUUIDString());
    pseudo_sheet_url_builder.Append("@mhtml.blink");
    KURL pseudo_sheet_url = KURL(pseudo_sheet_url_builder.ToString());

    markup_.Append("<link rel=\"stylesheet\" type=\"text/css\" href=\"");
    markup_.Append(pseudo_sheet_url.GetString());
    markup_.Append("\" />");

    resource_delegate_.SerializeCSSStyleSheet(
        static_cast<CSSStyleSheet&>(*sheet), pseudo_sheet_url);
  }
}

void SerializerMarkupAccumulator::AppendAttribute(const Element& element,
                                                  const Attribute& attribute) {
  // Check if link rewriting can affect the attribute.
  bool is_link_attribute = element.HasLegalLinkAttribute(attribute.GetName());
  bool is_src_doc_attribute = IsA<HTMLFrameElementBase>(element) &&
                              attribute.GetName() == html_names::kSrcdocAttr;
  if (is_link_attribute || is_src_doc_attribute) {
    // Check if the delegate wants to do link rewriting for the element.
    String new_link_for_the_element;
    if (delegate_.RewriteLink(element, new_link_for_the_element)) {
      if (is_link_attribute) {
        // Rewrite element links.
        AppendRewrittenAttribute(element, attribute.GetName().ToString(),
                                 new_link_for_the_element);
      } else {
        DCHECK(is_src_doc_attribute);
        // Emit src instead of srcdoc attribute for frame elements - we want the
        // serialized subframe to use html contents from the link provided by
        // Delegate::rewriteLink rather than html contents from srcdoc
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

std::pair<Node*, Element*> SerializerMarkupAccumulator::GetAuxiliaryDOMTree(
    const Element& element) const {
  return delegate_.GetAuxiliaryDOMTree(element);
}

void SerializerMarkupAccumulator::AppendAttributeValue(
    const String& attribute_value) {
  MarkupFormatter::AppendAttributeValue(
      markup_, attribute_value, IsA<HTMLDocument>(document_), *document_);
}

void SerializerMarkupAccumulator::AppendRewrittenAttribute(
    const Element& element,
    const String& attribute_name,
    const String& attribute_value) {
  if (elements_with_rewritten_links_.Contains(&element))
    return;
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

// TODO(tiger): Right now there is no support for rewriting URLs inside CSS
// documents which leads to bugs like <https://crbug.com/251898>. Not being
// able to rewrite URLs inside CSS documents means that resources imported from
// url(...) statements in CSS might not work when rewriting links for the
// "Webpage, Complete" method of saving a page. It will take some work but it
// needs to be done if we want to continue to support non-MHTML saved pages.

FrameSerializer::FrameSerializer(Deque<SerializedResource>& resources,
                                 Delegate& delegate)
    : resources_(&resources), delegate_(delegate) {}

void FrameSerializer::SerializeFrame(const LocalFrame& frame) {
  TRACE_EVENT0("page-serialization", "FrameSerializer::serializeFrame");
  DCHECK(frame.GetDocument());
  Document& document = *frame.GetDocument();
  KURL url = document.Url();

  // If frame is an image document, add the image and don't continue
  if (auto* image_document = DynamicTo<ImageDocument>(document)) {
    AddImageToResources(image_document->CachedImage(), url);
    return;
  }

  {
    TRACE_EVENT0("page-serialization", "FrameSerializer::serializeFrame HTML");
    SerializerMarkupAccumulator accumulator(delegate_, *this, document);
    String text =
        accumulator.SerializeNodes<EditingStrategy>(document, kIncludeNode);

    std::string frame_html =
        document.Encoding().Encode(text, WTF::kEntitiesForUnencodables);
    // Note that the frame has to be 1st resource.
    resources_->push_front(SerializedResource(
        url, document.SuggestedMIMEType(),
        SharedBuffer::Create(frame_html.c_str(), frame_html.length())));
  }
}

void FrameSerializer::AddResourceForElement(Document& document,
                                            const Element& element) {
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
    AddImageToResources(cached_image, document.CompleteURL(image_url_value));
  } else if (const auto* input = DynamicTo<HTMLInputElement>(element)) {
    if (input->type() == input_type_names::kImage && input->ImageLoader()) {
      KURL image_url = input->Src();
      ImageResourceContent* cached_image = input->ImageLoader()->GetContent();
      AddImageToResources(cached_image, image_url);
    }
  } else if (const auto* link = DynamicTo<HTMLLinkElement>(element)) {
    if (CSSStyleSheet* sheet = link->sheet()) {
      KURL sheet_url =
          document.CompleteURL(link->FastGetAttribute(html_names::kHrefAttr));
      SerializeCSSStyleSheet(*sheet, sheet_url);
    }
  } else if (const auto* style = DynamicTo<HTMLStyleElement>(element)) {
    if (CSSStyleSheet* sheet = style->sheet())
      SerializeCSSStyleSheet(*sheet, NullURL());
  } else if (const auto* plugin = DynamicTo<HTMLPlugInElement>(&element)) {
    if (plugin->IsImageType() && plugin->ImageLoader()) {
      KURL image_url = document.CompleteURL(plugin->Url());
      ImageResourceContent* cached_image = plugin->ImageLoader()->GetContent();
      AddImageToResources(cached_image, image_url);
    }
  }
}

void FrameSerializer::SerializeCSSStyleSheet(CSSStyleSheet& style_sheet,
                                             const KURL& url) {
  // If the URL is invalid or if it is a data URL this means that this CSS is
  // defined inline, respectively in a <style> tag or in the data URL itself.
  bool is_inline_css = !url.IsValid() || url.ProtocolIsData();
  // If this CSS is not inline then it is identifiable by its URL. So just skip
  // it if it has already been analyzed before.
  if (!is_inline_css && (resource_urls_.Contains(url) ||
                         delegate_.ShouldSkipResourceWithURL(url))) {
    return;
  }
  if (!is_inline_css)
    resource_urls_.insert(url);

  TRACE_EVENT2("page-serialization", "FrameSerializer::serializeCSSStyleSheet",
               "type", "CSS", "url", url.ElidedString().Utf8());

  // If this CSS is inlined its definition was already serialized with the frame
  // HTML code that was previously generated. No need to regenerate it here.
  if (!is_inline_css) {
    StringBuilder css_text;
    css_text.Append("@charset \"");
    css_text.Append(
        String(style_sheet.Contents()->Charset().GetName()).DeprecatedLower());
    css_text.Append("\";\n\n");

    for (unsigned i = 0; i < style_sheet.length(); ++i) {
      CSSRule* rule = style_sheet.item(i);
      String item_text = rule->cssText();
      if (!item_text.empty()) {
        css_text.Append(item_text);
        if (i < style_sheet.length() - 1)
          css_text.Append("\n\n");
      }
    }

    WTF::TextEncoding text_encoding(style_sheet.Contents()->Charset());
    DCHECK(text_encoding.IsValid());
    String text_string = css_text.ToString();
    std::string text = text_encoding.Encode(
        text_string, WTF::kCSSEncodedEntitiesForUnencodables);
    resources_->push_back(
        SerializedResource(url, String("text/css"),
                           SharedBuffer::Create(text.c_str(), text.length())));
  }

  // Sub resources need to be serialized even if the CSS definition doesn't
  // need to be.
  for (unsigned i = 0; i < style_sheet.length(); ++i)
    SerializeCSSRule(style_sheet.item(i));
}

void FrameSerializer::SerializeCSSRule(CSSRule* rule) {
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
      if (import_rule->styleSheet())
        SerializeCSSStyleSheet(*import_rule->styleSheet(), import_url);
      break;
    }

    // Rules inheriting CSSGroupingRule
    case CSSRule::kMediaRule:
    case CSSRule::kSupportsRule:
    case CSSRule::kContainerRule:
    case CSSRule::kLayerBlockRule:
    case CSSRule::kScopeRule:
    case CSSRule::kStartingStyleRule: {
      CSSRuleList* rule_list = rule->cssRules();
      for (unsigned i = 0; i < rule_list->length(); ++i)
        SerializeCSSRule(rule_list->item(i));
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

    // Rules in which no external resources can be referenced
    case CSSRule::kCharsetRule:
    case CSSRule::kFontPaletteValuesRule:
    case CSSRule::kFontFeatureRule:
    case CSSRule::kFontFeatureValuesRule:
    case CSSRule::kPageRule:
    case CSSRule::kPropertyRule:
    case CSSRule::kKeyframesRule:
    case CSSRule::kKeyframeRule:
    case CSSRule::kNamespaceRule:
    case CSSRule::kViewportRule:
    case CSSRule::kLayerStatementRule:
    case CSSRule::kPositionFallbackRule:
    case CSSRule::kTryRule:
      break;
  }
}

bool FrameSerializer::ShouldAddURL(const KURL& url) {
  return url.IsValid() && !resource_urls_.Contains(url) &&
         !url.ProtocolIsData() && !delegate_.ShouldSkipResourceWithURL(url);
}

void FrameSerializer::AddToResources(
    const String& mime_type,
    scoped_refptr<const SharedBuffer> data,
    const KURL& url) {
  if (!data) {
    DLOG(ERROR) << "No data for resource " << url.GetString();
    return;
  }

  resources_->push_back(SerializedResource(url, mime_type, std::move(data)));
}

void FrameSerializer::AddImageToResources(ImageResourceContent* image,
                                          const KURL& url) {
  if (!ShouldAddURL(url))
    return;
  resource_urls_.insert(url);
  if (!image || !image->HasImage() || image->ErrorOccurred())
    return;

  TRACE_EVENT2("page-serialization", "FrameSerializer::addImageToResources",
               "type", "image", "url", url.ElidedString().Utf8());
  scoped_refptr<const SharedBuffer> data = image->GetImage()->Data();
  AddToResources(image->GetResponse().MimeType(), data, url);
}

void FrameSerializer::AddFontToResources(FontResource& font) {
  if (!ShouldAddURL(font.Url()))
    return;
  resource_urls_.insert(font.Url());
  if (!font.IsLoaded() || !font.ResourceBuffer())
    return;

  scoped_refptr<const SharedBuffer> data(font.ResourceBuffer());

  AddToResources(font.GetResponse().MimeType(), data, font.Url());
}

void FrameSerializer::RetrieveResourcesForProperties(
    const CSSPropertyValueSet* style_declaration,
    Document& document) {
  if (!style_declaration)
    return;

  // The background-image and list-style-image (for ul or ol) are the CSS
  // properties that make use of images. We iterate to make sure we include any
  // other image properties there might be.
  unsigned property_count = style_declaration->PropertyCount();
  for (unsigned i = 0; i < property_count; ++i) {
    const CSSValue& css_value = style_declaration->PropertyAt(i).Value();
    RetrieveResourcesForCSSValue(css_value, document);
  }
}

void FrameSerializer::RetrieveResourcesForCSSValue(const CSSValue& css_value,
                                                   Document& document) {
  if (const auto* image_value = DynamicTo<CSSImageValue>(css_value)) {
    if (image_value->IsCachePending())
      return;
    StyleImage* style_image = image_value->CachedImage();
    if (!style_image || !style_image->IsImageResource())
      return;

    AddImageToResources(style_image->CachedImage(),
                        style_image->CachedImage()->Url());
  } else if (const auto* font_face_src_value =
                 DynamicTo<CSSFontFaceSrcValue>(css_value)) {
    if (font_face_src_value->IsLocal())
      return;

    AddFontToResources(
        font_face_src_value->Fetch(document.GetExecutionContext(), nullptr));
  } else if (const auto* css_value_list = DynamicTo<CSSValueList>(css_value)) {
    for (unsigned i = 0; i < css_value_list->length(); i++)
      RetrieveResourcesForCSSValue(css_value_list->Item(i), document);
  }
}

// Returns MOTW (Mark of the Web) declaration before html tag which is in
// HTML comment, e.g. "<!-- saved from url=(%04d)%s -->"
// See http://msdn2.microsoft.com/en-us/library/ms537628(VS.85).aspx.
String FrameSerializer::MarkOfTheWebDeclaration(const KURL& url) {
  StringBuilder builder;
  bool emits_minus = false;
  std::string orignal_url = url.GetString().Ascii();
  for (const char* string = orignal_url.c_str(); *string; ++string) {
    const char ch = *string;
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

}  // namespace blink
