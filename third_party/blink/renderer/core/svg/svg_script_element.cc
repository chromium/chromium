/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
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

#include "third_party/blink/renderer/core/svg/svg_script_element.h"

#include "third_party/blink/renderer/bindings/core/v8/js_event_handler_for_content_attribute.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_htmlscriptelement_svgscriptelement.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/script/script_loader.h"
#include "third_party/blink/renderer/core/script/script_runner.h"
#include "third_party/blink/renderer/core/xlink_names.h"

namespace blink {

SVGScriptElement::SVGScriptElement(Document& document,
                                   const CreateElementFlags flags)
    : SVGElement(svg_names::kScriptTag, document),
      SVGURIReference(this),
      loader_(InitializeScriptLoader(flags)) {}

void SVGScriptElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kOnerrorAttr) {
    SetAttributeEventListener(
        event_type_names::kError,
        JSEventHandlerForContentAttribute::Create(
            GetExecutionContext(), params.name, params.new_value,
            JSEventHandler::HandlerType::kOnErrorEventHandler));
  } else {
    SVGElement::ParseAttribute(params);
  }
}

void SVGScriptElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  if (SVGURIReference::IsKnownAttribute(params.name)) {
    loader_->HandleSourceAttribute(LegacyHrefString(*this));
    return;
  }

  SVGElement::SvgAttributeChanged(params);
}

Node::InsertionNotificationRequest SVGScriptElement::InsertedInto(
    ContainerNode& root_parent) {
  SVGElement::InsertedInto(root_parent);
  return kInsertionShouldCallDidNotifySubtreeInsertions;
}

void SVGScriptElement::DidNotifySubtreeInsertionsToDocument() {
  loader_->DidNotifySubtreeInsertionsToDocument();

  if (!loader_->IsParserInserted())
    have_fired_load_ = true;
}

void SVGScriptElement::ChildrenChanged(const ChildrenChange& change) {
  SVGElement::ChildrenChanged(change);
  loader_->ChildrenChanged(change);

  // We'll record whether the script element children were ever changed by
  // the API (as opposed to the parser).
  children_changed_by_api_ |= !change.ByParser();
}

bool SVGScriptElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == AtomicString(SourceAttributeValue());
}

void SVGScriptElement::FinishParsingChildren() {
  SVGElement::FinishParsingChildren();
  have_fired_load_ = true;

  // We normally expect the parser to finish parsing before any script gets
  // a chance to manipulate the script. However, if script parsing gets
  // deferred (or similar; see crbug.com/1033101) then a script might get
  // access to the script element before. In this case, we cannot blindly
  // accept the current TextFromChildren as a parser result.
  // This matches the logic in HTMLScriptElement.
  DCHECK(children_changed_by_api_ || !script_text_internal_slot_.length());
  if (!children_changed_by_api_) {
    script_text_internal_slot_ = ParkableString(TextFromChildren().Impl());
  }
}

bool SVGScriptElement::HaveLoadedRequiredResources() {
  return have_fired_load_;
}

String SVGScriptElement::SourceAttributeValue() const {
  return LegacyHrefString(*this);
}

String SVGScriptElement::TypeAttributeValue() const {
  return getAttribute(svg_names::kTypeAttr).GetString();
}

String SVGScriptElement::ChildTextContent() {
  return TextFromChildren();
}

String SVGScriptElement::ScriptTextInternalSlot() const {
  return script_text_internal_slot_.ToString();
}

bool SVGScriptElement::HasSourceAttribute() const {
  return !LegacyHrefString(*this).IsNull();
}

bool SVGScriptElement::IsConnected() const {
  return Node::isConnected();
}

bool SVGScriptElement::HasChildren() const {
  return Node::hasChildren();
}

const AtomicString& SVGScriptElement::GetNonceForElement() const {
  return ContentSecurityPolicy::IsNonceableElement(this) ? nonce()
                                                         : g_null_atom;
}

bool SVGScriptElement::AllowInlineScriptForCSP(
    const AtomicString& nonce,
    const WTF::OrdinalNumber& context_line,
    const String& script_content) {
  return GetExecutionContext()
      ->GetContentSecurityPolicyForCurrentWorld()
      ->AllowInline(ContentSecurityPolicy::InlineType::kScript, this,
                    script_content, nonce, GetDocument().Url(), context_line);
}

Document& SVGScriptElement::GetDocument() const {
  return Node::GetDocument();
}

ExecutionContext* SVGScriptElement::GetExecutionContext() const {
  return Node::GetExecutionContext();
}

Element& SVGScriptElement::CloneWithoutAttributesAndChildren(
    Document& factory) const {
  CreateElementFlags flags =
      CreateElementFlags::ByCloneNode().SetAlreadyStarted(
          loader_->AlreadyStarted());
  return *factory.CreateElement(TagQName(), flags, IsValue());
}

void SVGScriptElement::DispatchLoadEvent() {
  DispatchEvent(*Event::Create(event_type_names::kLoad));
  have_fired_load_ = true;
}

void SVGScriptElement::DispatchErrorEvent() {
  DispatchEvent(*Event::Create(event_type_names::kError));
}

ScriptElementBase::Type SVGScriptElement::GetScriptElementType() {
  return ScriptElementBase::Type::kSVGScriptElement;
}

#if DCHECK_IS_ON()
bool SVGScriptElement::IsAnimatableAttribute(const QualifiedName& name) const {
  if (name == svg_names::kTypeAttr || name == svg_names::kHrefAttr ||
      name == xlink_names::kHrefAttr)
    return false;
  return SVGElement::IsAnimatableAttribute(name);
}
#endif

const AttrNameToTrustedType& SVGScriptElement::GetCheckedAttributeTypes()
    const {
  DEFINE_STATIC_LOCAL(
      AttrNameToTrustedType, attribute_map,
      ({
          {svg_names::kHrefAttr.LocalName(), SpecificTrustedType::kScriptURL},
      }));
  return attribute_map;
}

V8HTMLOrSVGScriptElement* SVGScriptElement::AsV8HTMLOrSVGScriptElement() {
  if (IsInShadowTree())
    return nullptr;
  return MakeGarbageCollected<V8HTMLOrSVGScriptElement>(this);
}

DOMNodeId SVGScriptElement::GetDOMNodeId() {
  return this->GetDomNodeId();
}

SVGAnimatedPropertyBase* SVGScriptElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (SVGAnimatedPropertyBase* ret =
          SVGURIReference::PropertyFromAttribute(attribute_name);
      ret) {
    return ret;
  }
  return SVGElement::PropertyFromAttribute(attribute_name);
}

void SVGScriptElement::SynchronizeAllSVGAttributes() const {
  SVGURIReference::SynchronizeAllSVGAttributes();
  SVGElement::SynchronizeAllSVGAttributes();
}

void SVGScriptElement::Trace(Visitor* visitor) const {
  visitor->Trace(loader_);
  SVGElement::Trace(visitor);
  SVGURIReference::Trace(visitor);
  ScriptElementBase::Trace(visitor);
}

}  // namespace blink
