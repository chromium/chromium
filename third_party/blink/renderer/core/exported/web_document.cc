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
#include "net/storage_access_api/status.h"
#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/platform/web_distillability.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_dom_event.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/renderer/core/css/css_selector_watch.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_statistics_collector.h"
#include "third_party/blink/renderer/core/dom/document_type.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
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
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/speculation_rules/document_speculation_rules.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace {

static const blink::WebStyleSheetKey GenerateStyleSheetKey() {
  static unsigned counter = 0;
  return String::Number(++counter);
}

}  // namespace

namespace blink {

const DocumentToken& WebDocument::Token() const {
  return ConstUnwrap<Document>()->Token();
}

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

std::optional<SkColor> WebDocument::ThemeColor() {
  std::optional<Color> color = Unwrap<Document>()->ThemeColor();
  if (color)
    return color->Rgb();
  return std::nullopt;
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

net::StorageAccessApiStatus WebDocument::StorageAccessApiStatus() const {
  return ConstUnwrap<Document>()
      ->GetExecutionContext()
      ->GetStorageAccessApiStatus();
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

WebElementCollection WebDocument::All() const {
  return WebElementCollection(
      const_cast<Document*>(ConstUnwrap<Document>())->all());
}

WebVector<WebFormControlElement> WebDocument::UnassociatedFormControls() const {
  Vector<WebFormControlElement> unassociated_form_controls;
  for (const auto& element :
       ConstUnwrap<Document>()->UnassociatedListedElements()) {
    if (auto* form_control =
            blink::DynamicTo<HTMLFormControlElement>(element.Get())) {
      unassociated_form_controls.push_back(form_control);
    }
  }
  return unassociated_form_controls;
}

WebVector<WebFormElement> WebDocument::Forms() const {
  HTMLCollection* forms =
      const_cast<Document*>(ConstUnwrap<Document>())->forms();

  Vector<WebFormElement> form_elements;
  form_elements.reserve(forms->length());
  for (Element* element : *forms) {
    form_elements.emplace_back(blink::To<HTMLFormElement>(element));
  }
  return form_elements;
}

WebVector<WebFormElement> WebDocument::GetTopLevelForms() const {
  Vector<WebFormElement> web_forms;
  HeapVector<Member<HTMLFormElement>> forms =
      const_cast<Document*>(ConstUnwrap<Document>())->GetTopLevelForms();
  web_forms.reserve(forms.size());
  for (auto& form : forms) {
    web_forms.push_back(form.Get());
  }
  return web_forms;
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

WebStyleSheetKey WebDocument::InsertStyleSheet(
    const WebString& source_code,
    const WebStyleSheetKey* key,
    WebCssOrigin origin,
    BackForwardCacheAware back_forward_cache_aware) {
  Document* document = Unwrap<Document>();
  DCHECK(document);
  if (back_forward_cache_aware == BackForwardCacheAware::kPossiblyDisallow) {
    document->GetFrame()->GetFrameScheduler()->RegisterStickyFeature(
        SchedulingPolicy::Feature::kInjectedStyleSheet,
        {SchedulingPolicy::DisableBackForwardCache()});
  }
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
                                           WebCssOrigin origin) {
  Unwrap<Document>()->GetStyleEngine().RemoveInjectedSheet(key, origin);
}

void WebDocument::WatchCSSSelectors(const WebVector<WebString>& web_selectors) {
  Document* document = Unwrap<Document>();
  CSSSelectorWatch* watch = CSSSelectorWatch::FromIfExists(*document);
  if (!watch && web_selectors.empty())
    return;
  Vector<String> selectors;
  selectors.AppendSpan(base::span(web_selectors));
  CSSSelectorWatch::From(*document).WatchCSSSelectors(selectors);
}

WebVector<WebDraggableRegion> WebDocument::DraggableRegions() const {
  WebVector<WebDraggableRegion> draggable_regions;
  const Document* document = ConstUnwrap<Document>();
  if (document->HasDraggableRegions()) {
    const Vector<DraggableRegionValue>& regions = document->DraggableRegions();
    draggable_regions = WebVector<WebDraggableRegion>(regions.size());
    for (wtf_size_t i = 0; i < regions.size(); i++) {
      const DraggableRegionValue& value = regions[i];
      draggable_regions[i].draggable = value.draggable;
      draggable_regions[i].bounds = ToPixelSnappedRect(value.bounds);
    }
  }
  return draggable_regions;
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

cc::ElementId WebDocument::GetVisualViewportScrollingElementIdForTesting() {
  return blink::To<Document>(private_.Get())
      ->GetPage()
      ->GetVisualViewport()
      .GetScrollElementId();
}

bool WebDocument::IsLoaded() {
  return !ConstUnwrap<Document>()->Parser();
}

bool WebDocument::IsPrerendering() {
  return ConstUnwrap<Document>()->IsPrerendering();
}

bool WebDocument::HasDocumentPictureInPictureWindow() const {
  return ConstUnwrap<Document>()->HasDocumentPictureInPictureWindow();
}

void WebDocument::AddPostPrerenderingActivationStep(
    base::OnceClosure callback) {
  return Unwrap<Document>()->AddPostPrerenderingActivationStep(
      std::move(callback));
}

void WebDocument::SetCookieManager(
    CrossVariantMojoRemote<network::mojom::RestrictedCookieManagerInterfaceBase>
        cookie_manager) {
  Unwrap<Document>()->SetCookieManager(std::move(cookie_manager));
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

net::ReferrerPolicy WebDocument::GetReferrerPolicy() const {
  network::mojom::ReferrerPolicy policy =
      ConstUnwrap<Document>()->GetExecutionContext()->GetReferrerPolicy();
  if (policy == network::mojom::ReferrerPolicy::kDefault) {
    return blink::ReferrerUtils::GetDefaultNetReferrerPolicy();
  } else {
    return network::ReferrerPolicyForUrlRequest(policy);
  }
}

WebString WebDocument::OutgoingReferrer() const {
  return WebString(ConstUnwrap<Document>()->domWindow()->OutgoingReferrer());
}

void WebDocument::InitiatePreview(const WebURL& url) {
  if (!url.IsValid()) {
    return;
  }

  Document* document = blink::To<Document>(private_.Get());
  if (!document) {
    return;
  }

  KURL kurl(url);
  DocumentSpeculationRules::From(*document).InitiatePreview(kurl);
}

}  // namespace blink
