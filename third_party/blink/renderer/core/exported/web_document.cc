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

#include "third_party/blink/public/web/web_document.h"

#include "base/memory/scoped_refptr.h"
#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "third_party/blink/public/platform/web_distillability.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_dom_event.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/renderer/core/css/css_selector_watch.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_statistics_collector.h"
#include "third_party/blink/renderer/core/dom/document_type.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/html_all_collection.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/html/plugin_document.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace {

static const blink::WebStyleSheetKey GenerateStyleSheetKey() {
  static unsigned counter = 0;
  return String::Number(++counter);
}

}  // namespace

namespace blink {

WebURL WebDocument::Url() const {
  return ConstUnwrap<Document>()->Url();
}

WebSecurityOrigin WebDocument::GetSecurityOrigin() const {
  if (!ConstUnwrap<Document>())
    return WebSecurityOrigin();
  ExecutionContext* context = ConstUnwrap<Document>()->GetExecutionContext();
  if (!context)
    return WebSecurityOrigin();
  return WebSecurityOrigin(context->GetSecurityOrigin());
}

bool WebDocument::IsSecureContext() const {
  const Document* document = ConstUnwrap<Document>();
  ExecutionContext* context =
      document ? document->GetExecutionContext() : nullptr;
  return context && context->IsSecureContext();
}

WebString WebDocument::Encoding() const {
  return ConstUnwrap<Document>()->EncodingName();
}

WebString WebDocument::ContentLanguage() const {
  return ConstUnwrap<Document>()->ContentLanguage();
}

WebString WebDocument::GetReferrer() const {
  return ConstUnwrap<Document>()->referrer();
}

base::Optional<SkColor> WebDocument::ThemeColor() const {
  base::Optional<Color> color = ConstUnwrap<Document>()->ThemeColor();
  if (color)
    return color->Rgb();
  return base::nullopt;
}

WebURL WebDocument::OpenSearchDescriptionURL() const {
  return const_cast<Document*>(ConstUnwrap<Document>())
      ->OpenSearchDescriptionURL();
}

WebLocalFrame* WebDocument::GetFrame() const {
  return WebLocalFrameImpl::FromFrame(ConstUnwrap<Document>()->GetFrame());
}

bool WebDocument::IsHTMLDocument() const {
  return IsA<HTMLDocument>(ConstUnwrap<Document>());
}

bool WebDocument::IsXHTMLDocument() const {
  return ConstUnwrap<Document>()->IsXHTMLDocument();
}

bool WebDocument::IsPluginDocument() const {
  return IsA<PluginDocument>(ConstUnwrap<Document>());
}

WebURL WebDocument::BaseURL() const {
  return ConstUnwrap<Document>()->BaseURL();
}

ukm::SourceId WebDocument::GetUkmSourceId() const {
  return ConstUnwrap<Document>()->UkmSourceID();
}

net::SiteForCookies WebDocument::SiteForCookies() const {
  return ConstUnwrap<Document>()->SiteForCookies();
}

WebSecurityOrigin WebDocument::TopFrameOrigin() const {
  return ConstUnwrap<Document>()->TopFrameOrigin();
}

WebElement WebDocument::DocumentElement() const {
  return WebElement(ConstUnwrap<Document>()->documentElement());
}

WebElement WebDocument::Body() const {
  return WebElement(ConstUnwrap<Document>()->body());
}

WebElement WebDocument::Head() {
  return WebElement(Unwrap<Document>()->head());
}

WebString WebDocument::Title() const {
  return WebString(ConstUnwrap<Document>()->title());
}

WebString WebDocument::ContentAsTextForTesting() const {
  Element* document_element = ConstUnwrap<Document>()->documentElement();
  if (!document_element)
    return WebString();
  return document_element->innerText();
}

WebElementCollection WebDocument::All() {
  return WebElementCollection(Unwrap<Document>()->all());
}

WebVector<WebFormElement> WebDocument::Forms() const {
  HTMLCollection* forms =
      const_cast<Document*>(ConstUnwrap<Document>())->forms();

  Vector<WebFormElement> form_elements;
  form_elements.ReserveCapacity(forms->length());
  for (Element* element : *forms) {
    // Strange but true, sometimes node can be 0.
    if (auto* html_form_element = DynamicTo<HTMLFormElement>(element))
      form_elements.emplace_back(html_form_element);
  }
  return form_elements;
}

WebURL WebDocument::CompleteURL(const WebString& partial_url) const {
  return ConstUnwrap<Document>()->CompleteURL(partial_url);
}

WebElement WebDocument::GetElementById(const WebString& id) const {
  return WebElement(ConstUnwrap<Document>()->getElementById(id));
}

WebElement WebDocument::FocusedElement() const {
  return WebElement(ConstUnwrap<Document>()->FocusedElement());
}

WebStyleSheetKey WebDocument::InsertStyleSheet(const WebString& source_code,
                                               const WebStyleSheetKey* key,
                                               CSSOrigin origin) {
  Document* document = Unwrap<Document>();
  DCHECK(document);
  auto* parsed_sheet = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(*document));
  parsed_sheet->ParseString(source_code);
  const WebStyleSheetKey& injection_key =
      key && !key->IsNull() ? *key : GenerateStyleSheetKey();
  DCHECK(!injection_key.IsEmpty());
  document->GetStyleEngine().InjectSheet(injection_key, parsed_sheet, origin);
  return injection_key;
}

void WebDocument::RemoveInsertedStyleSheet(const WebStyleSheetKey& key,
                                           CSSOrigin origin) {
  Unwrap<Document>()->GetStyleEngine().RemoveInjectedSheet(key, origin);
}

void WebDocument::WatchCSSSelectors(const WebVector<WebString>& web_selectors) {
  Document* document = Unwrap<Document>();
  CSSSelectorWatch* watch = CSSSelectorWatch::FromIfExists(*document);
  if (!watch && web_selectors.empty())
    return;
  Vector<String> selectors;
  selectors.Append(web_selectors.Data(), web_selectors.size());
  CSSSelectorWatch::From(*document).WatchCSSSelectors(selectors);
}

WebVector<WebDraggableRegion> WebDocument::DraggableRegions() const {
  WebVector<WebDraggableRegion> draggable_regions;
  const Document* document = ConstUnwrap<Document>();
  if (document->HasAnnotatedRegions()) {
    const Vector<AnnotatedRegionValue>& regions = document->AnnotatedRegions();
    draggable_regions = WebVector<WebDraggableRegion>(regions.size());
    for (size_t i = 0; i < regions.size(); i++) {
      const AnnotatedRegionValue& value = regions[i];
      draggable_regions[i].draggable = value.draggable;
      draggable_regions[i].bounds = PixelSnappedIntRect(value.bounds);
    }
  }
  return draggable_regions;
}

WebURL WebDocument::CanonicalUrlForSharing() const {
  const Document* document = ConstUnwrap<Document>();
  HTMLLinkElement* link_element = document->LinkCanonical();
  if (!link_element)
    return WebURL();
  return link_element->Href();
}

WebDistillabilityFeatures WebDocument::DistillabilityFeatures() {
  return DocumentStatisticsCollector::CollectStatistics(*Unwrap<Document>());
}

void WebDocument::SetShowBeforeUnloadDialog(bool show_dialog) {
  if (!IsHTMLDocument())
    return;

  Document* doc = Unwrap<Document>();
  doc->SetShowBeforeUnloadDialog(show_dialog);
}

uint64_t WebDocument::GetVisualViewportScrollingElementIdForTesting() {
  return blink::To<Document>(private_.Get())
      ->GetPage()
      ->GetVisualViewport()
      .GetScrollElementId()
      .GetStableId();
}

bool WebDocument::IsLoaded() {
  return !ConstUnwrap<Document>()->Parser();
}

WebDocument::WebDocument(Document* elem) : WebNode(elem) {}

DEFINE_WEB_NODE_TYPE_CASTS(WebDocument, ConstUnwrap<Node>()->IsDocumentNode())

WebDocument& WebDocument::operator=(Document* elem) {
  private_ = elem;
  return *this;
}

WebDocument::operator Document*() const {
  return blink::To<Document>(private_.Get());
}

}  // namespace blink
