/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#include "third_party/blink/renderer/core/html/custom/v0_custom_element_registration_context.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_definition.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_scheduler.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_unknown_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/svg/svg_unknown_element.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

V0CustomElementRegistrationContext::V0CustomElementRegistrationContext()
    : candidates_(MakeGarbageCollected<V0CustomElementUpgradeCandidateMap>()) {}

void V0CustomElementRegistrationContext::RegisterElement(
    Document* document,
    V0CustomElementConstructorBuilder* constructor_builder,
    const AtomicString& type,
    ExceptionState& exception_state) {
  V0CustomElementDefinition* definition = registry_.RegisterElement(
      document, constructor_builder, type, exception_state);

  if (!definition)
    return;

  // Upgrade elements that were waiting for this definition.
  V0CustomElementUpgradeCandidateMap::ElementSet* upgrade_candidates =
      candidates_->TakeUpgradeCandidatesFor(definition->Descriptor());

  if (!upgrade_candidates)
    return;

  for (const auto& candidate : *upgrade_candidates)
    V0CustomElement::Define(candidate, definition);
}

Element* V0CustomElementRegistrationContext::CreateCustomTagElement(
    Document& document,
    const QualifiedName& tag_name) {
  DCHECK(V0CustomElement::IsValidName(tag_name.LocalName()));

  Element* element;

  if (html_names::xhtmlNamespaceURI == tag_name.NamespaceURI()) {
    element = MakeGarbageCollected<HTMLElement>(tag_name, document);
  } else if (svg_names::kNamespaceURI == tag_name.NamespaceURI()) {
    element = MakeGarbageCollected<SVGUnknownElement>(tag_name, document);
  } else {
    // XML elements are not custom elements, so return early.
    return Element::Create(tag_name, &document);
  }

  element->SetV0CustomElementState(Element::kV0WaitingForUpgrade);
  ResolveOrScheduleResolution(element, g_null_atom);
  return element;
}

void V0CustomElementRegistrationContext::DidGiveTypeExtension(
    Element* element,
    const AtomicString& type) {
  ResolveOrScheduleResolution(element, type);
}

void V0CustomElementRegistrationContext::ResolveOrScheduleResolution(
    Element* element,
    const AtomicString& type_extension) {
  // If an element has a custom tag name it takes precedence over
  // the "is" attribute (if any).
  const AtomicString& type = V0CustomElement::IsValidName(element->localName())
                                 ? element->localName()
                                 : type_extension;
  DCHECK(!type.IsNull());

  V0CustomElementDescriptor descriptor(type, element->namespaceURI(),
                                       element->localName());
  DCHECK_EQ(element->GetV0CustomElementState(), Element::kV0WaitingForUpgrade);

  V0CustomElementScheduler::ResolveOrScheduleResolution(this, element,
                                                        descriptor);
}

void V0CustomElementRegistrationContext::Resolve(
    Element* element,
    const V0CustomElementDescriptor& descriptor) {
  V0CustomElementDefinition* definition = registry_.Find(descriptor);
  if (definition) {
    V0CustomElement::Define(element, definition);
  } else {
    DCHECK_EQ(element->GetV0CustomElementState(),
              Element::kV0WaitingForUpgrade);
    candidates_->Add(descriptor, element);
  }
}

void V0CustomElementRegistrationContext::SetIsAttributeAndTypeExtension(
    Element* element,
    const AtomicString& type) {
  DCHECK(element);
  DCHECK(!type.IsEmpty());
  element->setAttribute(html_names::kIsAttr, type);
  SetTypeExtension(element, type);
}

void V0CustomElementRegistrationContext::SetTypeExtension(
    Element* element,
    const AtomicString& type) {
  if (!element->IsHTMLElement() && !element->IsSVGElement())
    return;

  V0CustomElementRegistrationContext* context =
      element->GetDocument().RegistrationContext();
  if (!context)
    return;

  if (element->IsV0CustomElement()) {
    // This can happen if:
    // 1. The element has a custom tag, which takes precedence over
    //    type extensions.
    // 2. Undoing a command (eg ReplaceNodeWithSpan) recycles an
    //    element but tries to overwrite its attribute list.
    return;
  }

  // Custom tags take precedence over type extensions
  DCHECK(!V0CustomElement::IsValidName(element->localName()));

  if (!V0CustomElement::IsValidName(type))
    return;

  element->SetV0CustomElementState(Element::kV0WaitingForUpgrade);
  context->DidGiveTypeExtension(element,
                                element->GetDocument().ConvertLocalName(type));
}

bool V0CustomElementRegistrationContext::NameIsDefined(
    const AtomicString& name) const {
  return registry_.NameIsDefined(name);
}

void V0CustomElementRegistrationContext::SetV1(
    const CustomElementRegistry* v1) {
  registry_.SetV1(v1);
}

void V0CustomElementRegistrationContext::Trace(Visitor* visitor) {
  visitor->Trace(candidates_);
  visitor->Trace(registry_);
}

}  // namespace blink
