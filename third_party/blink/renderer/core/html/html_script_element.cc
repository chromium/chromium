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

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_htmlscriptelement_svgscriptelement.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/attribution_src_loader.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/loader/render_blocking_resource_manager.h"
#include "third_party/blink/renderer/core/script/script_loader.h"
#include "third_party/blink/renderer/core/script/script_runner.h"
#include "third_party/blink/renderer/core/script_type_names.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

HTMLScriptElement::HTMLScriptElement(Document& document,
                                     const CreateElementFlags flags)
    : HTMLElement(html_names::kScriptTag, document),
      children_changed_by_api_(false),
      blocking_attribute_(MakeGarbageCollected<BlockingAttribute>(this)),
      loader_(InitializeScriptLoader(flags)) {}

const AttrNameToTrustedType& HTMLScriptElement::GetCheckedAttributeTypes()
    const {
  DEFINE_STATIC_LOCAL(AttrNameToTrustedType, attribute_map,
                      ({{"src", SpecificTrustedType::kScriptURL}}));
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

void HTMLScriptElement::ChildrenChanged(const ChildrenChange& change) {
  HTMLElement::ChildrenChanged(change);
  loader_->ChildrenChanged(change);

  // We'll record whether the script element children were ever changed by
  // the API (as opposed to the parser).
  children_changed_by_api_ |= !change.ByParser();
}

void HTMLScriptElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kSrcAttr) {
    loader_->HandleSourceAttribute(params.new_value);
    LogUpdateAttributeIfIsolatedWorldAndInDocument("script", params);
  } else if (params.name == html_names::kAsyncAttr) {
    // https://html.spec.whatwg.org/C/#non-blocking
    // "In addition, whenever a script element whose |non-blocking|
    // flag is set has an async content attribute added, the element's
    // |non-blocking| flag must be unset."
    loader_->HandleAsyncAttribute();
  } else if (params.name == html_names::kFetchpriorityAttr) {
    // The only thing we need to do for the the fetchPriority attribute/Priority
    // Hints is count usage upon parsing. Processing the value happens when the
    // element loads.
    UseCounter::Count(GetDocument(), WebFeature::kPriorityHints);
  } else if (params.name == html_names::kBlockingAttr) {
    blocking_attribute_->OnAttributeValueChanged(params.old_value,
                                                 params.new_value);
    if (GetDocument().GetRenderBlockingResourceManager() &&
        !IsPotentiallyRenderBlocking()) {
      GetDocument().GetRenderBlockingResourceManager()->RemovePendingScript(
          *this);
    }
  } else if (params.name == html_names::kAttributionsrcAttr) {
    if (GetDocument().GetFrame()) {
      // Copied from `ScriptLoader::PrepareScript()`.
      String referrerpolicy_attr = ReferrerPolicyAttributeValue();
      network::mojom::ReferrerPolicy referrer_policy =
          network::mojom::ReferrerPolicy::kDefault;
      if (!referrerpolicy_attr.empty()) {
        SecurityPolicy::ReferrerPolicyFromString(
            referrerpolicy_attr, kDoNotSupportReferrerPolicyLegacyKeywords,
            &referrer_policy);
      }

      GetDocument().GetFrame()->GetAttributionSrcLoader()->Register(
          params.new_value, /*element=*/this, referrer_policy);
    }
  } else {
    HTMLElement::ParseAttribute(params);
  }
}

Node::InsertionNotificationRequest HTMLScriptElement::InsertedInto(
    ContainerNode& insertion_point) {
  if (insertion_point.isConnected() && HasSourceAttribute() &&
      ScriptLoader::GetScriptTypeAtPrepare(TypeAttributeValue(),
                                           LanguageAttributeValue()) ==
          ScriptLoader::ScriptTypeAtPrepare::kInvalid) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kScriptElementWithInvalidTypeHasSrc);
  }
  HTMLElement::InsertedInto(insertion_point);
  LogAddElementIfIsolatedWorldAndInDocument("script", html_names::kSrcAttr);

  return kInsertionShouldCallDidNotifySubtreeInsertions;
}

void HTMLScriptElement::RemovedFrom(ContainerNode& insertion_point) {
  HTMLElement::RemovedFrom(insertion_point);
  loader_->Removed();
  if (GetDocument().GetRenderBlockingResourceManager() &&
      !GetDocument().StatePreservingAtomicMoveInProgress()) {
    GetDocument().GetRenderBlockingResourceManager()->RemovePendingScript(
        *this);
  }
}

void HTMLScriptElement::DidNotifySubtreeInsertionsToDocument() {
  loader_->DidNotifySubtreeInsertionsToDocument();
}

void HTMLScriptElement::setText(const String& string) {
  setTextContent(string);
}

void HTMLScriptElement::setInnerTextForBinding(
    const V8UnionStringLegacyNullToEmptyStringOrTrustedScript*
        string_or_trusted_script,
    ExceptionState& exception_state) {
  const String& value = TrustedTypesCheckForScript(
      string_or_trusted_script, GetExecutionContext(), "HTMLScriptElement",
      "innerText", exception_state);
  if (exception_state.HadException())
    return;
  // https://w3c.github.io/trusted-types/dist/spec/#setting-slot-values
  // "On setting the innerText [...]: Set [[ScriptText]] internal slot value to
  // the stringified attribute value. Perform the usual attribute setter steps."
  script_text_internal_slot_ = ParkableString(value.Impl());
  HTMLElement::setInnerText(value);
}

void HTMLScriptElement::setTextContentForBinding(
    const V8UnionStringOrTrustedScript* value,
    ExceptionState& exception_state) {
  const String& string = TrustedTypesCheckForScript(
      value, GetExecutionContext(), "HTMLScriptElement", "textContent",
      exception_state);
  if (exception_state.HadException())
    return;
  setTextContent(string);
}

void HTMLScriptElement::setTextContent(const String& string) {
  // https://w3c.github.io/trusted-types/dist/spec/#setting-slot-values
  // "On setting [.. textContent ..]: Set [[ScriptText]] internal slot value to
  // the stringified attribute value. Perform the usual attribute setter steps."
  script_text_internal_slot_ = ParkableString(string.Impl());
  Node::setTextContent(string);
}

void HTMLScriptElement::setAsync(bool async) {
  // https://html.spec.whatwg.org/multipage/scripting.html#dom-script-async
  SetBooleanAttribute(html_names::kAsyncAttr, async);
  loader_->HandleAsyncAttribute();
}

void HTMLScriptElement::FinishParsingChildren() {
  Element::FinishParsingChildren();

  // We normally expect the parser to finish parsing before any script gets
  // a chance to manipulate the script. However, if script parsing gets
  // deferrred (or similar; see crbug.com/1033101) then a script might get
  // access to the HTMLScriptElement before. In this case, we cannot blindly
  // accept the current TextFromChildren as a parser result.
  DCHECK(children_changed_by_api_ || !script_text_internal_slot_.length());
  if (!children_changed_by_api_)
    script_text_internal_slot_ = ParkableString(TextFromChildren().Impl());
}

bool HTMLScriptElement::async() const {
  return FastHasAttribute(html_names::kAsyncAttr) || loader_->IsForceAsync();
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

String HTMLScriptElement::FetchPriorityAttributeValue() const {
  return FastGetAttribute(html_names::kFetchpriorityAttr);
}

String HTMLScriptElement::ChildTextContent() {
  return TextFromChildren();
}

String HTMLScriptElement::ScriptTextInternalSlot() const {
  return script_text_internal_slot_.ToString();
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

bool HTMLScriptElement::HasAttributionsrcAttribute() const {
  return FastHasAttribute(html_names::kAttributionsrcAttr);
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
  // Support 'inline-speculation-rules' source.
  // https://wicg.github.io/nav-speculation/speculation-rules.html#content-security-policy
  DCHECK(loader_);
  ContentSecurityPolicy::InlineType inline_type =
      loader_->GetScriptType() ==
              ScriptLoader::ScriptTypeAtPrepare::kSpeculationRules
          ? ContentSecurityPolicy::InlineType::kScriptSpeculationRules
          : ContentSecurityPolicy::InlineType::kScript;
  return GetExecutionContext()
      ->GetContentSecurityPolicyForCurrentWorld()
      ->AllowInline(inline_type, this, script_content, nonce,
                    GetDocument().Url(), context_line);
}

Document& HTMLScriptElement::GetDocument() const {
  return Node::GetDocument();
}

ExecutionContext* HTMLScriptElement::GetExecutionContext() const {
  return Node::GetExecutionContext();
}

V8HTMLOrSVGScriptElement* HTMLScriptElement::AsV8HTMLOrSVGScriptElement() {
  if (IsInShadowTree())
    return nullptr;
  return MakeGarbageCollected<V8HTMLOrSVGScriptElement>(this);
}

DOMNodeId HTMLScriptElement::GetDOMNodeId() {
  return this->GetDomNodeId();
}

void HTMLScriptElement::DispatchLoadEvent() {
  DispatchEvent(*Event::Create(event_type_names::kLoad));
}

void HTMLScriptElement::DispatchErrorEvent() {
  DispatchEvent(*Event::Create(event_type_names::kError));
}

ScriptElementBase::Type HTMLScriptElement::GetScriptElementType() {
  return ScriptElementBase::Type::kHTMLScriptElement;
}

Element& HTMLScriptElement::CloneWithoutAttributesAndChildren(
    Document& factory) const {
  CreateElementFlags flags =
      CreateElementFlags::ByCloneNode().SetAlreadyStarted(
          loader_->AlreadyStarted());
  return *factory.CreateElement(TagQName(), flags, IsValue());
}

bool HTMLScriptElement::IsPotentiallyRenderBlocking() const {
  if (blocking_attribute_->HasRenderToken())
    return true;

  if (loader_->IsParserInserted() &&
      loader_->GetScriptType() == ScriptLoader::ScriptTypeAtPrepare::kClassic) {
    // If ForceInOrderScript is enabled, treat the script having src attribute
    // as non-render blocking even if it has neither async nor defer attribute.
    // Because the script is force-in-order'ed, which behaves like the scripts
    // categorized ScriptSchedulingType::kInOrder. Those're not render blocking.
    if (base::FeatureList::IsEnabled(features::kForceInOrderScript) &&
        HasSourceAttribute())
      return false;
    return !AsyncAttributeValue() && !DeferAttributeValue();
  }

  return false;
}

// static
bool HTMLScriptElement::supports(const AtomicString& type) {
  if (type == script_type_names::kClassic)
    return true;
  if (type == script_type_names::kModule)
    return true;
  if (type == script_type_names::kImportmap)
    return true;

  if (type == script_type_names::kSpeculationrules) {
    return true;
  }
  if (type == script_type_names::kWebbundle)
    return true;

  return false;
}

void HTMLScriptElement::Trace(Visitor* visitor) const {
  visitor->Trace(blocking_attribute_);
  visitor->Trace(loader_);
  HTMLElement::Trace(visitor);
  ScriptElementBase::Trace(visitor);
}

}  // namespace blink
