// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/custom_element.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/custom/ce_reactions_scope.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_definition.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_reaction_factory.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_reaction_stack.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_registry.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_registration_context.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_unknown_element.h"
#include "third_party/blink/renderer/core/html_element_factory.h"
#include "third_party/blink/renderer/core/html_element_type_helpers.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

CustomElementRegistry* CustomElement::Registry(const Element& element) {
  return Registry(element.GetDocument());
}

CustomElementRegistry* CustomElement::Registry(const Document& document) {
  if (LocalDOMWindow* window = document.ExecutingWindow())
    return window->customElements();
  return nullptr;
}

static CustomElementDefinition* DefinitionForElementWithoutCheck(
    const Element& element) {
  DCHECK_EQ(element.GetCustomElementState(), CustomElementState::kCustom);
  return element.GetCustomElementDefinition();
}

CustomElementDefinition* CustomElement::DefinitionForElement(
    const Element* element) {
  if (!element ||
      element->GetCustomElementState() != CustomElementState::kCustom)
    return nullptr;
  return DefinitionForElementWithoutCheck(*element);
}

Vector<AtomicString>& CustomElement::EmbedderCustomElementNames() {
  DEFINE_STATIC_LOCAL(Vector<AtomicString>, names, ());
  return names;
}

void CustomElement::AddEmbedderCustomElementName(const AtomicString& name) {
  DCHECK_EQ(name, name.LowerASCII());
  DCHECK(Document::IsValidName(name)) << name;
  DCHECK_EQ(HTMLElementType::kHTMLUnknownElement, htmlElementTypeForTag(name))
      << name;
  DCHECK(!IsValidName(name, false)) << name;

  if (EmbedderCustomElementNames().Contains(name))
    return;
  EmbedderCustomElementNames().push_back(name);
}

void CustomElement::AddEmbedderCustomElementNameForTesting(
    const AtomicString& name,
    ExceptionState& exception_state) {
  if (name != name.LowerASCII() || !Document::IsValidName(name) ||
      HTMLElementType::kHTMLUnknownElement != htmlElementTypeForTag(name) ||
      IsValidName(name, false)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Name cannot be used");
    return;
  }

  AddEmbedderCustomElementName(name);
}

bool CustomElement::IsHyphenatedSpecElementName(const AtomicString& name) {
  // Even if Blink does not implement one of the related specs, (for
  // example annotation-xml is from MathML, which Blink does not
  // implement) we must prohibit using the name because that is
  // required by the HTML spec which we *do* implement. Don't remove
  // names from this list without removing them from the HTML spec
  // first.
  DEFINE_STATIC_LOCAL(HashSet<AtomicString>, hyphenated_spec_element_names,
                      ({
                          "annotation-xml", "color-profile", "font-face",
                          "font-face-src", "font-face-uri", "font-face-format",
                          "font-face-name", "missing-glyph",
                      }));
  return hyphenated_spec_element_names.Contains(name);
}

bool CustomElement::ShouldCreateCustomElement(const AtomicString& name) {
  return IsValidName(name);
}

bool CustomElement::ShouldCreateCustomElement(const QualifiedName& tag_name) {
  return ShouldCreateCustomElement(tag_name.LocalName()) &&
         tag_name.NamespaceURI() == html_names::xhtmlNamespaceURI;
}

bool CustomElement::ShouldCreateCustomizedBuiltinElement(
    const AtomicString& local_name) {
  return htmlElementTypeForTag(local_name) !=
         HTMLElementType::kHTMLUnknownElement;
}

bool CustomElement::ShouldCreateCustomizedBuiltinElement(
    const QualifiedName& tag_name) {
  return ShouldCreateCustomizedBuiltinElement(tag_name.LocalName()) &&
         tag_name.NamespaceURI() == html_names::xhtmlNamespaceURI;
}

static CustomElementDefinition* DefinitionFor(
    const Document& document,
    const CustomElementDescriptor desc) {
  if (CustomElementRegistry* registry = CustomElement::Registry(document))
    return registry->DefinitionFor(desc);
  return nullptr;
}

// https://dom.spec.whatwg.org/#concept-create-element
HTMLElement* CustomElement::CreateCustomElement(Document& document,
                                                const QualifiedName& tag_name,
                                                CreateElementFlags flags) {
  DCHECK(ShouldCreateCustomElement(tag_name)) << tag_name;
  // 4. Let definition be the result of looking up a custom element
  // definition given document, namespace, localName, and is.
  if (auto* definition = DefinitionFor(
          document, CustomElementDescriptor(tag_name.LocalName(),
                                            tag_name.LocalName()))) {
    DCHECK(definition->Descriptor().IsAutonomous());
    // 6. Otherwise, if definition is non-null, then:
    return definition->CreateElement(document, tag_name, flags);
  }
  // 7. Otherwise:
  return To<HTMLElement>(
      CreateUncustomizedOrUndefinedElementTemplate<kQNameIsValid>(
          document, tag_name, flags, g_null_atom));
}

// Step 7 of https://dom.spec.whatwg.org/#concept-create-element in
// addition to Custom Element V0 handling.
template <CustomElement::CreateUUCheckLevel level>
Element* CustomElement::CreateUncustomizedOrUndefinedElementTemplate(
    Document& document,
    const QualifiedName& tag_name,
    const CreateElementFlags flags,
    const AtomicString& is_value) {
  if (level == kQNameIsValid) {
    DCHECK(is_value.IsNull());
    DCHECK(ShouldCreateCustomElement(tag_name)) << tag_name;
  }

  Element* element;
  if (RuntimeEnabledFeatures::CustomElementsV0Enabled(&document)) {
    if (V0CustomElement::IsValidName(tag_name.LocalName()) &&
        document.RegistrationContext()) {
      element = document.RegistrationContext()->CreateCustomTagElement(
          document, tag_name);
    } else {
      element = document.CreateRawElement(tag_name, flags);
      if (level == kCheckAll && !is_value.IsNull()) {
        element->SetIsValue(is_value);
        if (flags.IsCustomElementsV0()) {
          V0CustomElementRegistrationContext::SetTypeExtension(element,
                                                               is_value);
        }
      }
    }
  } else {
    // 7.1. Let interface be the element interface for localName and namespace.
    // 7.2. Set result to a new element that implements interface, with ...
    element = document.CreateRawElement(tag_name, flags);
    if (level == kCheckAll && !is_value.IsNull())
      element->SetIsValue(is_value);
  }

  // 7.3. If namespace is the HTML namespace, and either localName is a
  // valid custom element name or is is non-null, then set result’s
  // custom element state to "undefined".
  if (level == kQNameIsValid)
    element->SetCustomElementState(CustomElementState::kUndefined);
  else if (tag_name.NamespaceURI() == html_names::xhtmlNamespaceURI &&
           (CustomElement::IsValidName(tag_name.LocalName()) ||
            !is_value.IsNull()))
    element->SetCustomElementState(CustomElementState::kUndefined);

  return element;
}

Element* CustomElement::CreateUncustomizedOrUndefinedElement(
    Document& document,
    const QualifiedName& tag_name,
    const CreateElementFlags flags,
    const AtomicString& is_value) {
  return CreateUncustomizedOrUndefinedElementTemplate<kCheckAll>(
      document, tag_name, flags, is_value);
}

HTMLElement* CustomElement::CreateFailedElement(Document& document,
                                                const QualifiedName& tag_name) {
  DCHECK(ShouldCreateCustomElement(tag_name));

  // "create an element for a token":
  // https://html.spec.whatwg.org/C/#create-an-element-for-the-token

  // 7. If this step throws an exception, let element be instead a new element
  // that implements HTMLUnknownElement, with no attributes, namespace set to
  // given namespace, namespace prefix set to null, custom element state set
  // to "failed", and node document set to document.

  auto* element = MakeGarbageCollected<HTMLUnknownElement>(tag_name, document);
  element->SetCustomElementState(CustomElementState::kFailed);
  return element;
}

void CustomElement::Enqueue(Element& element, CustomElementReaction& reaction) {
  // To enqueue an element on the appropriate element queue
  // https://html.spec.whatwg.org/C/#enqueue-an-element-on-the-appropriate-element-queue

  // If the custom element reactions stack is not empty, then
  // Add element to the current element queue.
  if (CEReactionsScope* current = CEReactionsScope::Current()) {
    current->EnqueueToCurrentQueue(element, reaction);
    return;
  }

  // If the custom element reactions stack is empty, then
  // Add element to the backup element queue.
  CustomElementReactionStack::Current().EnqueueToBackupQueue(element, reaction);
}

void CustomElement::EnqueueConnectedCallback(Element& element) {
  auto* definition = DefinitionForElementWithoutCheck(element);
  if (definition->HasConnectedCallback())
    definition->EnqueueConnectedCallback(element);
}

void CustomElement::EnqueueDisconnectedCallback(Element& element) {
  auto* definition = DefinitionForElementWithoutCheck(element);
  if (definition->HasDisconnectedCallback())
    definition->EnqueueDisconnectedCallback(element);
}

void CustomElement::EnqueueAdoptedCallback(Element& element,
                                           Document& old_owner,
                                           Document& new_owner) {
  auto* definition = DefinitionForElementWithoutCheck(element);
  if (definition->HasAdoptedCallback())
    definition->EnqueueAdoptedCallback(element, old_owner, new_owner);
}

void CustomElement::EnqueueAttributeChangedCallback(
    Element& element,
    const QualifiedName& name,
    const AtomicString& old_value,
    const AtomicString& new_value) {
  auto* definition = DefinitionForElementWithoutCheck(element);
  if (definition->HasAttributeChangedCallback(name))
    definition->EnqueueAttributeChangedCallback(element, name, old_value,
                                                new_value);
}

void CustomElement::EnqueueFormAssociatedCallback(
    Element& element,
    HTMLFormElement* nullable_form) {
  auto& definition = *DefinitionForElementWithoutCheck(element);
  if (definition.HasFormAssociatedCallback()) {
    Enqueue(element, CustomElementReactionFactory::CreateFormAssociated(
                         definition, nullable_form));
  }
}

void CustomElement::EnqueueFormResetCallback(Element& element) {
  auto& definition = *DefinitionForElementWithoutCheck(element);
  if (definition.HasFormResetCallback()) {
    Enqueue(element, CustomElementReactionFactory::CreateFormReset(definition));
  }
}

void CustomElement::EnqueueFormDisabledCallback(Element& element,
                                                bool is_disabled) {
  auto& definition = *DefinitionForElementWithoutCheck(element);
  if (definition.HasFormDisabledCallback()) {
    Enqueue(element, CustomElementReactionFactory::CreateFormDisabled(
                         definition, is_disabled));
  }
}

void CustomElement::EnqueueFormStateRestoreCallback(
    Element& element,
    const FileOrUSVStringOrFormData& value,
    const String& mode) {
  auto& definition = *DefinitionForElementWithoutCheck(element);
  if (definition.HasFormStateRestoreCallback()) {
    Enqueue(element, CustomElementReactionFactory::CreateFormStateRestore(
                         definition, value, mode));
  }
}

void CustomElement::TryToUpgrade(Element& element,
                                 bool upgrade_invisible_elements) {
  // Try to upgrade an element
  // https://html.spec.whatwg.org/C/#concept-try-upgrade

  DCHECK_EQ(element.GetCustomElementState(), CustomElementState::kUndefined);

  CustomElementRegistry* registry = CustomElement::Registry(element);
  if (!registry)
    return;
  const AtomicString& is_value = element.IsValue();
  if (CustomElementDefinition* definition =
          registry->DefinitionFor(CustomElementDescriptor(
              is_value.IsNull() ? element.localName() : is_value,
              element.localName())))
    definition->EnqueueUpgradeReaction(element, upgrade_invisible_elements);
  else
    registry->AddCandidate(element);
}

}  // namespace blink
