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

#include "third_party/blink/renderer/bindings/core/v8/html_script_element_or_svg_script_element.h"
#include "third_party/blink/renderer/bindings/core/v8/script_event_listener.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
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
      loader_(InitializeScriptLoader(flags.IsCreatedByParser(),
                                     flags.WasAlreadyStarted())) {}

void SVGScriptElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kOnerrorAttr) {
    SetAttributeEventListener(
        event_type_names::kError,
        CreateAttributeEventListener(
            this, params.name, params.new_value,
            JSEventHandler::HandlerType::kOnErrorEventHandler));
  } else {
    SVGElement::ParseAttribute(params);
  }
}

void SVGScriptElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  if (SVGURIReference::IsKnownAttribute(attr_name)) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    loader_->HandleSourceAttribute(HrefString());
    return;
  }

  SVGElement::SvgAttributeChanged(attr_name);
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
  loader_->ChildrenChanged();
}

void SVGScriptElement::DidMoveToNewDocument(Document& old_document) {
  ScriptRunner::MovePendingScript(old_document, GetDocument(), loader_.Get());
  SVGElement::DidMoveToNewDocument(old_document);
}

bool SVGScriptElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == AtomicString(SourceAttributeValue());
}

void SVGScriptElement::FinishParsingChildren() {
  SVGElement::FinishParsingChildren();
  have_fired_load_ = true;
}

bool SVGScriptElement::HaveLoadedRequiredResources() {
  return have_fired_load_;
}

String SVGScriptElement::SourceAttributeValue() const {
  return HrefString();
}

String SVGScriptElement::TypeAttributeValue() const {
  return getAttribute(svg_names::kTypeAttr).GetString();
}

String SVGScriptElement::TextFromChildren() {
  return Element::TextFromChildren();
}

bool SVGScriptElement::HasSourceAttribute() const {
  return href()->IsSpecified();
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
  return GetDocument().GetContentSecurityPolicyForWorld()->AllowInline(
      ContentSecurityPolicy::InlineType::kScript, this, script_content, nonce,
      GetDocument().Url(), context_line);
}

Document& SVGScriptElement::GetDocument() const {
  return Node::GetDocument();
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

void SVGScriptElement::SetScriptElementForBinding(
    HTMLScriptElementOrSVGScriptElement& element) {
  if (!IsInV1ShadowTree())
    element.SetSVGScriptElement(this);
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
  DEFINE_STATIC_LOCAL(AttrNameToTrustedType, attribute_map,
                      ({
                          {svg_names::kHrefAttr.LocalName(),
                           SpecificTrustedType::kTrustedScriptURL},
                      }));
  return attribute_map;
}

void SVGScriptElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(loader_);
  SVGElement::Trace(visitor);
  SVGURIReference::Trace(visitor);
  ScriptElementBase::Trace(visitor);
}

}  // namespace blink
