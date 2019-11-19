/*
 * Copyright (C) 2006, 2007 Rob Buis
 * Copyright (C) 2008 Apple, Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/css/style_element.h"

#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/media_query_evaluator.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/svg/svg_style_element.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

static bool IsCSS(const Element& element, const AtomicString& type) {
  return type.IsEmpty() || (element.IsHTMLElement()
                                ? DeprecatedEqualIgnoringCase(type, "text/css")
                                : (type == "text/css"));
}

StyleElement::StyleElement(Document* document, bool created_by_parser)
    : created_by_parser_(created_by_parser),
      loading_(false),
      registered_as_candidate_(false),
      start_position_(TextPosition::BelowRangePosition()) {
  if (created_by_parser && document &&
      document->GetScriptableDocumentParser() &&
      !document->IsInDocumentWrite()) {
    start_position_ =
        document->GetScriptableDocumentParser()->GetTextPosition();
  }
}

StyleElement::~StyleElement() = default;

StyleElement::ProcessingResult StyleElement::ProcessStyleSheet(
    Document& document,
    Element& element) {
  TRACE_EVENT0("blink", "StyleElement::processStyleSheet");
  DCHECK(element.isConnected());

  registered_as_candidate_ = true;
  document.GetStyleEngine().AddStyleSheetCandidateNode(element);
  if (created_by_parser_)
    return kProcessingSuccessful;

  return Process(element);
}

void StyleElement::RemovedFrom(Element& element,
                               ContainerNode& insertion_point) {
  if (!insertion_point.isConnected())
    return;

  Document& document = element.GetDocument();
  if (registered_as_candidate_) {
    document.GetStyleEngine().RemoveStyleSheetCandidateNode(element,
                                                            insertion_point);
    registered_as_candidate_ = false;
  }

  if (sheet_)
    ClearSheet(element);
}

StyleElement::ProcessingResult StyleElement::ChildrenChanged(Element& element) {
  if (created_by_parser_)
    return kProcessingSuccessful;
  probe::WillChangeStyleElement(&element);
  return Process(element);
}

StyleElement::ProcessingResult StyleElement::FinishParsingChildren(
    Element& element) {
  ProcessingResult result = Process(element);
  created_by_parser_ = false;
  return result;
}

StyleElement::ProcessingResult StyleElement::Process(Element& element) {
  if (!element.isConnected())
    return kProcessingSuccessful;
  return CreateSheet(element, element.TextFromChildren());
}

void StyleElement::ClearSheet(Element& owner_element) {
  DCHECK(sheet_);

  if (sheet_->IsLoading()) {
    owner_element.GetDocument().GetStyleEngine().RemovePendingSheet(
        owner_element, style_engine_context_);
  }

  sheet_.Release()->ClearOwnerNode();
}

static bool IsInUserAgentShadowDOM(const Element& element) {
  ShadowRoot* root = element.ContainingShadowRoot();
  return root && root->IsUserAgent();
}

StyleElement::ProcessingResult StyleElement::CreateSheet(Element& element,
                                                         const String& text) {
  DCHECK(element.isConnected());
  Document& document = element.GetDocument();

  const ContentSecurityPolicy* csp =
      document.GetContentSecurityPolicyForWorld();

  // CSP is bypassed for style elements in user agent shadow DOM.
  bool passes_content_security_policy_checks =
      IsInUserAgentShadowDOM(element) ||
      csp->AllowInline(ContentSecurityPolicy::InlineType::kStyle, &element,
                       text, element.nonce(), document.Url(),
                       start_position_.line_);

  // Clearing the current sheet may remove the cache entry so create the new
  // sheet first
  CSSStyleSheet* new_sheet = nullptr;

  // If type is empty or CSS, this is a CSS style sheet.
  const AtomicString& type = this->type();
  if (IsCSS(element, type) && passes_content_security_policy_checks) {
    scoped_refptr<MediaQuerySet> media_queries;
    const AtomicString& media_string = media();
    if (!media_string.IsEmpty())
      media_queries = MediaQuerySet::Create(media_string);
    loading_ = true;
    TextPosition start_position =
        start_position_ == TextPosition::BelowRangePosition()
            ? TextPosition::MinimumPosition()
            : start_position_;
    new_sheet = document.GetStyleEngine().CreateSheet(
        element, text, start_position, style_engine_context_);
    new_sheet->SetMediaQueries(media_queries);
    loading_ = false;
  }

  if (sheet_)
    ClearSheet(element);

  sheet_ = new_sheet;
  if (sheet_)
    sheet_->Contents()->CheckLoaded();

  return passes_content_security_policy_checks ? kProcessingSuccessful
                                               : kProcessingFatalError;
}

bool StyleElement::IsLoading() const {
  if (loading_)
    return true;
  return sheet_ ? sheet_->IsLoading() : false;
}

bool StyleElement::SheetLoaded(Document& document) {
  if (IsLoading())
    return false;

  document.GetStyleEngine().RemovePendingSheet(*sheet_->ownerNode(),
                                               style_engine_context_);
  return true;
}

void StyleElement::StartLoadingDynamicSheet(Document& document) {
  document.GetStyleEngine().AddPendingSheet(style_engine_context_);
}

void StyleElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(sheet_);
}

}  // namespace blink
