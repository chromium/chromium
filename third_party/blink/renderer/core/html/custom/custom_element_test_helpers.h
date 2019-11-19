// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CUSTOM_ELEMENT_TEST_HELPERS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CUSTOM_ELEMENT_TEST_HELPERS_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_definition_options.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/html/custom/ce_reactions_scope.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_definition.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_definition_builder.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

#include <utility>

namespace blink {

class CustomElementDescriptor;

class TestCustomElementDefinitionBuilder
    : public CustomElementDefinitionBuilder {
  STACK_ALLOCATED();

 public:
  TestCustomElementDefinitionBuilder() = default;

  bool CheckConstructorIntrinsics() override { return true; }
  bool CheckConstructorNotRegistered() override { return true; }
  bool RememberOriginalProperties() override { return true; }
  CustomElementDefinition* Build(const CustomElementDescriptor&,
                                 CustomElementDefinition::Id) override;

 private:

  DISALLOW_COPY_AND_ASSIGN(TestCustomElementDefinitionBuilder);
};

class TestCustomElementDefinition : public CustomElementDefinition {
 public:
  TestCustomElementDefinition(const CustomElementDescriptor& descriptor)
      : CustomElementDefinition(descriptor) {}

  TestCustomElementDefinition(const CustomElementDescriptor& descriptor,
                              HashSet<AtomicString>&& observed_attributes,
                              const Vector<String>& disabled_features)
      : CustomElementDefinition(descriptor,
                                std::move(observed_attributes),
                                disabled_features,
                                FormAssociationFlag::kNo) {}

  ~TestCustomElementDefinition() override = default;

  ScriptValue GetConstructorForScript() override { return ScriptValue(); }

  bool RunConstructor(Element& element) override {
    if (GetConstructionStack().IsEmpty() ||
        GetConstructionStack().back() != &element)
      return false;
    GetConstructionStack().back().Clear();
    return true;
  }

  HTMLElement* CreateAutonomousCustomElementSync(
      Document& document,
      const QualifiedName&) override {
    return CreateElementForConstructor(document);
  }

  bool HasConnectedCallback() const override { return false; }
  bool HasDisconnectedCallback() const override { return false; }
  bool HasAdoptedCallback() const override { return false; }
  bool HasFormAssociatedCallback() const override { return false; }
  bool HasFormResetCallback() const override { return false; }
  bool HasFormDisabledCallback() const override { return false; }
  bool HasFormStateRestoreCallback() const override { return false; }

  void RunConnectedCallback(Element&) override {
    NOTREACHED() << "definition does not have connected callback";
  }

  void RunDisconnectedCallback(Element&) override {
    NOTREACHED() << "definition does not have disconnected callback";
  }

  void RunAdoptedCallback(Element&,
                          Document& old_owner,
                          Document& new_owner) override {
    NOTREACHED() << "definition does not have adopted callback";
  }

  void RunAttributeChangedCallback(Element&,
                                   const QualifiedName&,
                                   const AtomicString& old_value,
                                   const AtomicString& new_value) override {
    NOTREACHED() << "definition does not have attribute changed callback";
  }
  void RunFormAssociatedCallback(Element& element,
                                 HTMLFormElement* nullable_form) override {
    NOTREACHED() << "definition does not have formAssociatedCallback";
  }

  void RunFormResetCallback(Element& element) override {
    NOTREACHED() << "definition does not have formResetCallback";
  }

  void RunFormDisabledCallback(Element& element, bool is_disabled) override {
    NOTREACHED() << "definition does not have disabledStateChangedCallback";
  }

  void RunFormStateRestoreCallback(Element& element,
                                   const FileOrUSVStringOrFormData& value,
                                   const String& mode) override {
    NOTREACHED() << "definition does not have restoreValueCallback";
  }

  DISALLOW_COPY_AND_ASSIGN(TestCustomElementDefinition);
};

class CreateElement {
  STACK_ALLOCATED();

 public:
  CreateElement(const AtomicString& local_name)
      : namespace_uri_(html_names::xhtmlNamespaceURI),
        local_name_(local_name) {}

  CreateElement& InDocument(Document* document) {
    document_ = document;
    return *this;
  }

  CreateElement& InNamespace(const AtomicString& uri) {
    namespace_uri_ = uri;
    return *this;
  }

  CreateElement& WithId(const AtomicString& id) {
    attributes_.push_back(std::make_pair(html_names::kIdAttr, id));
    return *this;
  }

  CreateElement& WithIsValue(const AtomicString& value) {
    is_value_ = value;
    return *this;
  }

  operator Element*() const {
    Document* document = document_.Get();
    if (!document)
      document = MakeGarbageCollected<HTMLDocument>();
    NonThrowableExceptionState no_exceptions;
    Element* element = document->CreateElement(
        QualifiedName(g_null_atom, local_name_, namespace_uri_),
        CreateElementFlags::ByCreateElement(), is_value_);
    for (const auto& attribute : attributes_)
      element->setAttribute(attribute.first, attribute.second);
    return element;
  }

 private:
  Member<Document> document_;
  AtomicString namespace_uri_;
  AtomicString local_name_;
  AtomicString is_value_;
  Vector<std::pair<QualifiedName, AtomicString>> attributes_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CUSTOM_ELEMENT_TEST_HELPERS_H_
