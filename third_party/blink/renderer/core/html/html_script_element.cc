/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights
 * reserved.
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

#include "third_party/blink/renderer/core/html/html_script_element.h"

#include "third_party/blink/public/mojom/script/script_type.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/html_script_element_or_svg_script_element.h"
#include "third_party/blink/renderer/bindings/core/v8/script_event_listener.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_script.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/script/script_loader.h"
#include "third_party/blink/renderer/core/script/script_runner.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

HTMLScriptElement::HTMLScriptElement(Document& document,
                                     const CreateElementFlags flags)
    : HTMLElement(html_names::kScriptTag, document),
      loader_(InitializeScriptLoader(flags.IsCreatedByParser(),
                                     flags.WasAlreadyStarted())) {}

const AttrNameToTrustedType& HTMLScriptElement::GetCheckedAttributeTypes()
    const {
  DEFINE_STATIC_LOCAL(AttrNameToTrustedType, attribute_map,
                      ({{"src", SpecificTrustedType::kTrustedScriptURL}}));
  return attribute_map;
}

bool HTMLScriptElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == html_names::kSrcAttr ||
         HTMLElement::IsURLAttribute(attribute);
}

bool HTMLScriptElement::HasLegalLinkAttribute(const QualifiedName& name) const {
  return name == html_names::kSrcAttr ||
         HTMLElement::HasLegalLinkAttribute(name);
}

const QualifiedName& HTMLScriptElement::SubResourceAttributeName() const {
  return html_names::kSrcAttr;
}

void HTMLScriptElement::ChildrenChanged(const ChildrenChange& change) {
  HTMLElement::ChildrenChanged(change);
  if (change.IsChildInsertion())
    loader_->ChildrenChanged();
}

void HTMLScriptElement::DidMoveToNewDocument(Document& old_document) {
  ScriptRunner::MovePendingScript(old_document, GetDocument(), loader_.Get());
  HTMLElement::DidMoveToNewDocument(old_document);
}

void HTMLScriptElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kSrcAttr) {
    loader_->HandleSourceAttribute(params.new_value);
    LogUpdateAttributeIfIsolatedWorldAndInDocument("script", params);
  } else if (params.name == html_names::kAsyncAttr) {
    loader_->HandleAsyncAttribute();
  } else if (params.name == html_names::kImportanceAttr &&
             RuntimeEnabledFeatures::PriorityHintsEnabled(&GetDocument())) {
    // The only thing we need to do for the the importance attribute/Priority
    // Hints is count usage upon parsing. Processing the value happens when the
    // element loads.
    UseCounter::Count(GetDocument(), WebFeature::kPriorityHints);
  } else {
    HTMLElement::ParseAttribute(params);
  }
}

Node::InsertionNotificationRequest HTMLScriptElement::InsertedInto(
    ContainerNode& insertion_point) {
  if (insertion_point.isConnected() && HasSourceAttribute() &&
      !ScriptLoader::IsValidScriptTypeAndLanguage(
          TypeAttributeValue(), LanguageAttributeValue(),
          ScriptLoader::kDisallowLegacyTypeInTypeAttribute)) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kScriptElementWithInvalidTypeHasSrc);
  }
  HTMLElement::InsertedInto(insertion_point);
  LogAddElementIfIsolatedWorldAndInDocument("script", html_names::kSrcAttr);

  return kInsertionShouldCallDidNotifySubtreeInsertions;
}

void HTMLScriptElement::DidNotifySubtreeInsertionsToDocument() {
  loader_->DidNotifySubtreeInsertionsToDocument();
}

void HTMLScriptElement::setText(
    const StringOrTrustedScript& string_or_trusted_script,
    ExceptionState& exception_state) {
  setTextContent(string_or_trusted_script, exception_state);
}

void HTMLScriptElement::text(StringOrTrustedScript& result) {
  result.SetString(TextFromChildren());
}

void HTMLScriptElement::setInnerText(
    const StringOrTrustedScript& string_or_trusted_script,
    ExceptionState& exception_state) {
  String value = GetStringFromTrustedScript(string_or_trusted_script,
                                            &GetDocument(), exception_state);
  if (!exception_state.HadException()) {
    HTMLElement::setInnerText(value, exception_state);
  }
}

void HTMLScriptElement::setTextContent(
    const StringOrTrustedScript& string_or_trusted_script,
    ExceptionState& exception_state) {
  String value = GetStringFromTrustedScript(string_or_trusted_script,
                                            &GetDocument(), exception_state);
  if (!exception_state.HadException()) {
    Node::setTextContent(value);
  }
}

void HTMLScriptElement::setAsync(bool async) {
  SetBooleanAttribute(html_names::kAsyncAttr, async);
  loader_->HandleAsyncAttribute();
}

bool HTMLScriptElement::async() const {
  return FastHasAttribute(html_names::kAsyncAttr) || loader_->IsNonBlocking();
}

String HTMLScriptElement::SourceAttributeValue() const {
  return FastGetAttribute(html_names::kSrcAttr).GetString();
}

String HTMLScriptElement::CharsetAttributeValue() const {
  return FastGetAttribute(html_names::kCharsetAttr).GetString();
}

String HTMLScriptElement::TypeAttributeValue() const {
  return FastGetAttribute(html_names::kTypeAttr).GetString();
}

String HTMLScriptElement::LanguageAttributeValue() const {
  return FastGetAttribute(html_names::kLanguageAttr).GetString();
}

bool HTMLScriptElement::NomoduleAttributeValue() const {
  return FastHasAttribute(html_names::kNomoduleAttr);
}

String HTMLScriptElement::ForAttributeValue() const {
  return FastGetAttribute(html_names::kForAttr).GetString();
}

String HTMLScriptElement::EventAttributeValue() const {
  return FastGetAttribute(html_names::kEventAttr).GetString();
}

String HTMLScriptElement::CrossOriginAttributeValue() const {
  return FastGetAttribute(html_names::kCrossoriginAttr);
}

String HTMLScriptElement::IntegrityAttributeValue() const {
  return FastGetAttribute(html_names::kIntegrityAttr);
}

String HTMLScriptElement::ReferrerPolicyAttributeValue() const {
  return FastGetAttribute(html_names::kReferrerpolicyAttr);
}

String HTMLScriptElement::ImportanceAttributeValue() const {
  return FastGetAttribute(html_names::kImportanceAttr);
}

String HTMLScriptElement::TextFromChildren() {
  return Element::TextFromChildren();
}

bool HTMLScriptElement::AsyncAttributeValue() const {
  return FastHasAttribute(html_names::kAsyncAttr);
}

bool HTMLScriptElement::DeferAttributeValue() const {
  return FastHasAttribute(html_names::kDeferAttr);
}

bool HTMLScriptElement::HasSourceAttribute() const {
  return FastHasAttribute(html_names::kSrcAttr);
}

bool HTMLScriptElement::IsConnected() const {
  return Node::isConnected();
}

bool HTMLScriptElement::HasChildren() const {
  return Node::hasChildren();
}

const AtomicString& HTMLScriptElement::GetNonceForElement() const {
  return ContentSecurityPolicy::IsNonceableElement(this) ? nonce()
                                                         : g_null_atom;
}

bool HTMLScriptElement::AllowInlineScriptForCSP(
    const AtomicString& nonce,
    const WTF::OrdinalNumber& context_line,
    const String& script_content) {
  return GetDocument().GetContentSecurityPolicyForWorld()->AllowInline(
      ContentSecurityPolicy::InlineType::kScript, this, script_content, nonce,
      GetDocument().Url(), context_line);
}

Document& HTMLScriptElement::GetDocument() const {
  return Node::GetDocument();
}

void HTMLScriptElement::DispatchLoadEvent() {
  DispatchEvent(*Event::Create(event_type_names::kLoad));
}

void HTMLScriptElement::DispatchErrorEvent() {
  DispatchEvent(*Event::Create(event_type_names::kError));
}

void HTMLScriptElement::SetScriptElementForBinding(
    HTMLScriptElementOrSVGScriptElement& element) {
  if (!IsInV1ShadowTree())
    element.SetHTMLScriptElement(this);
}

Element& HTMLScriptElement::CloneWithoutAttributesAndChildren(
    Document& factory) const {
  CreateElementFlags flags =
      CreateElementFlags::ByCloneNode().SetAlreadyStarted(
          loader_->AlreadyStarted());
  return *factory.CreateElement(TagQName(), flags, IsValue());
}

void HTMLScriptElement::Trace(Visitor* visitor) {
  visitor->Trace(loader_);
  HTMLElement::Trace(visitor);
  ScriptElementBase::Trace(visitor);
}

}  // namespace blink
