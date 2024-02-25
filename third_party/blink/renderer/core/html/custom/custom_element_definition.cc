// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/custom_element_definition.h"
#include "third_party/blink/renderer/core/dom/attr.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/custom/custom_element.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_construction_stack.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_reaction.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_reaction_factory.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_reaction_stack.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_registry.h"
#include "third_party/blink/renderer/core/html/custom/element_internals.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_element_factory.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

CustomElementDefinition::CustomElementDefinition(
    CustomElementRegistry& registry,
    const CustomElementDescriptor& descriptor)
    : registry_(registry), descriptor_(descriptor) {}

CustomElementDefinition::CustomElementDefinition(
    CustomElementRegistry& registry,
    const CustomElementDescriptor& descriptor,
    const HashSet<AtomicString>& observed_attributes,
    const Vector<String>& disabled_features,
    FormAssociationFlag form_association_flag)
    : registry_(registry),
      descriptor_(descriptor),
      observed_attributes_(observed_attributes),
      has_style_attribute_changed_callback_(
          observed_attributes.Contains(html_names::kStyleAttr.LocalName())),
      disable_shadow_(disabled_features.Contains(String("shadow"))),
      disable_internals_(disabled_features.Contains(String("internals"))),
      is_form_associated_(form_association_flag == FormAssociationFlag::kYes) {}

CustomElementDefinition::~CustomElementDefinition() = default;

void CustomElementDefinition::Trace(Visitor* visitor) const {
  visitor->Trace(registry_);
  ElementRareDataField::Trace(visitor);
}

static String ErrorMessageForConstructorResult(Element& element,
                                               Document& document,
                                               const QualifiedName& tag_name) {
  // https://dom.spec.whatwg.org/#concept-create-element
  // 6.1.4. If result's attribute list is not empty, then throw a
  // NotSupportedError.
  if (element.hasAttributes())
    return "The result must not have attributes";
  // 6.1.5. If result has children, then throw a NotSupportedError.
  if (element.HasChildren())
    return "The result must not have children";
  // 6.1.6. If result's parent is not null, then throw a NotSupportedError.
  if (element.parentNode())
    return "The result must not have a parent";
  // 6.1.7. If result's node document is not document, then throw a
  // NotSupportedError.
  if (&element.GetDocument() != &document)
    return "The result must be in the same document";
  // 6.1.8. If result's namespace is not the HTML namespace, then throw a
  // NotSupportedError.
  if (element.namespaceURI() != html_names::xhtmlNamespaceURI)
    return "The result must have HTML namespace";
  // 6.1.9. If result's local name is not equal to localName, then throw a
  // NotSupportedError.
  if (element.localName() != tag_name.LocalName())
    return "The result must have the same localName";
  return String();
}

void CustomElementDefinition::CheckConstructorResult(
    Element* element,
    Document& document,
    const QualifiedName& tag_name,
    ExceptionState& exception_state) {
  // https://dom.spec.whatwg.org/#concept-create-element
  // 6.1.3. If result does not implement the HTMLElement interface, throw a
  // TypeError.
  // See https://github.com/whatwg/html/issues/1402 for more clarifications.
  if (!element || !element->IsHTMLElement()) {
    exception_state.ThrowTypeError(
        "The result must implement HTMLElement interface");
    return;
  }

  // 6.1.4. through 6.1.9.
  const String message =
      ErrorMessageForConstructorResult(*element, document, tag_name);
  if (!message.empty()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      message);
  }
}

HTMLElement* CustomElementDefinition::CreateElementForConstructor(
    Document& document) {
  HTMLElement* element =
      HTMLElementFactory::Create(Descriptor().LocalName(), document,
                                 CreateElementFlags::ByCreateElement());
  if (element) {
    element->SetIsValue(Descriptor().GetName());
  } else {
    element = MakeGarbageCollected<HTMLElement>(
        QualifiedName(g_null_atom, Descriptor().LocalName(),
                      html_names::xhtmlNamespaceURI),
        document);
  }
  // TODO(davaajav): write this as one call to setCustomElementState instead of
  // two.
  element->SetCustomElementState(CustomElementState::kUndefined);
  element->SetCustomElementDefinition(this);
  return element;
}

// A part of https://dom.spec.whatwg.org/#concept-create-element
HTMLElement* CustomElementDefinition::CreateElement(
    Document& document,
    const QualifiedName& tag_name,
    CreateElementFlags flags) {
  DCHECK(
      CustomElement::ShouldCreateCustomElement(tag_name) ||
      CustomElement::ShouldCreateCustomizedBuiltinElement(tag_name, document))
      << tag_name;

  // 5. If definition is non-null, and definition’s name is not equal to
  // its local name (i.e., definition represents a customized built-in
  // element), then:
  if (!descriptor_.IsAutonomous()) {
    // 5.1. Let interface be the element interface for localName and the
    // HTML namespace.
    // 5.2. Set result to a new element that implements interface, with
    // no attributes, namespace set to the HTML namespace, namespace
    // prefix set to prefix, local name set to localName, custom element
    // state set to "undefined", custom element definition set to null,
    // is value set to is, and node document set to document.
    auto* result = document.CreateRawElement(tag_name, flags);
    result->SetCustomElementState(CustomElementState::kUndefined);
    result->SetIsValue(Descriptor().GetName());

    if (!flags.IsAsyncCustomElements()) {
      // 5.3 If the synchronous custom elements flag is set, then run this step
      // while catching any exceptions:
      //   1. Upgrade element using definition.
      // If this step threw an exception, then:
      //   1. Report the exception.
      //   2. Set result's custom element state to "failed".
      Upgrade(*result);
    } else {
      // 5.4. Otherwise, enqueue a custom element upgrade reaction given
      // result and definition.
      EnqueueUpgradeReaction(*result);
    }
    return To<HTMLElement>(result);
  }

  // 6. If definition is non-null, then:
  // 6.1. If the synchronous custom elements flag is set, then run these
  // steps while catching any exceptions:
  if (!flags.IsAsyncCustomElements()) {
    // It's impossible to create a custom element with a scoped definition
    // without push the custom element construction stack. Make sure that
    // doesn't happen for synchrnous autonomous custom elements, which  don't
    // push the stack,
    // TODO(crbug.com/1304439): Alternatively, we can push the construction
    // stack only when using a scoped definition. Decide the exact behavior.
    CHECK(!RuntimeEnabledFeatures::ScopedCustomElementRegistryEnabled() ||
          registry_->IsGlobalRegistry());
    return CreateAutonomousCustomElementSync(document, tag_name);
  }

  // 6.2. Otherwise: (the synchronous custom elements flag is not set)
  // 6.2.1. Set result to a new element that implements the HTMLElement
  // interface, with no attributes, namespace set to the HTML namespace,
  // namespace prefix set to prefix, local name set to localName, custom
  // element state set to "undefined", and node document set to document.
  auto* element = MakeGarbageCollected<HTMLElement>(tag_name, document);
  element->SetCustomElementState(CustomElementState::kUndefined);
  // 6.2.2. Enqueue a custom element upgrade reaction given result and
  // definition.
  EnqueueUpgradeReaction(*element);
  return element;
}

// https://html.spec.whatwg.org/C/#concept-upgrade-an-element
void CustomElementDefinition::Upgrade(Element& element) {
  // 4.13.5.1 If element's custom element state is not "undefined" or
  // "uncustomized", then return.
  if (element.GetCustomElementState() != CustomElementState::kUndefined &&
      element.GetCustomElementState() != CustomElementState::kUncustomized) {
    return;
  }

  // 4.13.5.3. Set element's custom element state to "failed".
  element.SetCustomElementState(CustomElementState::kFailed);

  // 4.13.5.4: For each attribute in element's attribute list, in order, enqueue
  // a custom element callback reaction with element, callback name
  // "attributeChangedCallback", and an argument list containing attribute's
  // local name, null, attribute's value, and attribute's namespace.
  if (!observed_attributes_.empty())
    EnqueueAttributeChangedCallbackForAllAttributes(element);

  // 4.13.5.5: If element is connected, then enqueue a custom element callback
  // reaction with element, callback name "connectedCallback", and an empty
  // argument list.
  if (element.isConnected() && HasConnectedCallback())
    EnqueueConnectedCallback(element);

  bool succeeded = false;
  {
    // 4.13.5.6: Add element to the end of definition's construction stack.
    CustomElementConstructionStackScope construction_stack_scope(*this,
                                                                 element);
    // 4.13.5.8: Run the constructor, catching exceptions.
    succeeded = RunConstructor(element);
  }
  if (!succeeded) {
    // 4.13.5.?: If the above steps threw an exception, then element's custom
    // element state will remain "failed".
    CustomElementReactionStack::From(element.GetDocument().GetAgent())
        .ClearQueue(element);
    return;
  }

  element.SetCustomElementDefinition(this);

  // Setting the custom element definition changes the value of
  // IsFormAssociatedCustomElement(), which impacts whether HTMLElement calls
  // to the ListedElement when an attribute changes. Call the various change
  // methods now to ensure ListedElements state is correct.
  if (ListedElement* listed_element = ListedElement::From(element)) {
    if (element.FastHasAttribute(html_names::kReadonlyAttr))
      listed_element->ReadonlyAttributeChanged();
    if (element.FastHasAttribute(html_names::kDisabledAttr))
      listed_element->DisabledAttributeChanged();
  }

  if (IsFormAssociated())
    To<HTMLElement>(element).EnsureElementInternals().DidUpgrade();
}

bool CustomElementDefinition::HasAttributeChangedCallback(
    const QualifiedName& name) const {
  return observed_attributes_.Contains(name.LocalName());
}

bool CustomElementDefinition::HasStyleAttributeChangedCallback() const {
  return has_style_attribute_changed_callback_;
}

void CustomElementDefinition::EnqueueUpgradeReaction(Element& element) {
  CustomElement::Enqueue(element,
                         CustomElementReactionFactory::CreateUpgrade(*this));
}

void CustomElementDefinition::EnqueueConnectedCallback(Element& element) {
  CustomElement::Enqueue(element,
                         CustomElementReactionFactory::CreateConnected(*this));
}

void CustomElementDefinition::EnqueueDisconnectedCallback(Element& element) {
  CustomElement::Enqueue(
      element, CustomElementReactionFactory::CreateDisconnected(*this));
}

void CustomElementDefinition::EnqueueAdoptedCallback(Element& element,
                                                     Document& old_document,
                                                     Document& new_document) {
  CustomElement::Enqueue(element, CustomElementReactionFactory::CreateAdopted(
                                      *this, old_document, new_document));
}

void CustomElementDefinition::EnqueueAttributeChangedCallback(
    Element& element,
    const QualifiedName& name,
    const AtomicString& old_value,
    const AtomicString& new_value) {
  CustomElement::Enqueue(element,
                         CustomElementReactionFactory::CreateAttributeChanged(
                             *this, name, old_value, new_value));
}

void CustomElementDefinition::EnqueueAttributeChangedCallbackForAllAttributes(
    Element& element) {
  // Avoid synchronizing all attributes unless it is needed, while enqueing
  // callbacks "in order" as defined in the spec.
  // https://html.spec.whatwg.org/C/#concept-upgrade-an-element
  for (const AtomicString& name : observed_attributes_)
    element.SynchronizeAttribute(name);
  for (const auto& attribute : element.AttributesWithoutUpdate()) {
    if (HasAttributeChangedCallback(attribute.GetName())) {
      EnqueueAttributeChangedCallback(element, attribute.GetName(), g_null_atom,
                                      attribute.Value());
    }
  }
}

}  // namespace blink
