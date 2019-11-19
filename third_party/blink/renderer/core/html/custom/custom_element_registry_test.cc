// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/custom_element_registry.h"

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_custom_element.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/css/css_style_sheet_init.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_definition_options.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/shadow_root_init.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/custom/ce_reactions_scope.h"
#include "third_party/blink/renderer/core/html/custom/custom_element.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_definition.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_definition_builder.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_descriptor.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_test_helpers.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class CustomElementRegistryTest : public PageTestBase {
 protected:
  void SetUp() override { PageTestBase::SetUp(IntSize(1, 1)); }

  CustomElementRegistry& Registry() {
    return *GetFrame().DomWindow()->customElements();
  }

  CustomElementDefinition* Define(const AtomicString& name,
                                  CustomElementDefinitionBuilder& builder,
                                  const ElementDefinitionOptions* options,
                                  ExceptionState& exception_state) {
    return Registry().DefineInternal(GetScriptState(), name, builder, options,
                                     exception_state);
  }

  ScriptState* GetScriptState() {
    return ToScriptStateForMainWorld(&GetFrame());
  }

  void CollectCandidates(const CustomElementDescriptor& desc,
                         HeapVector<Member<Element>>* elements) {
    Registry().CollectCandidates(desc, elements);
  }
};

TEST_F(CustomElementRegistryTest,
       collectCandidates_shouldNotIncludeElementsRemovedFromDocument) {
  Element& element = *CreateElement("a-a").InDocument(&GetDocument());
  Registry().AddCandidate(element);

  HeapVector<Member<Element>> elements;
  CollectCandidates(CustomElementDescriptor("a-a", "a-a"), &elements);

  EXPECT_TRUE(elements.IsEmpty())
      << "no candidates should have been found, but we have "
      << elements.size();
  EXPECT_FALSE(elements.Contains(element))
      << "the out-of-document candidate should not have been found";
}

TEST_F(CustomElementRegistryTest,
       collectCandidates_shouldNotIncludeElementsInDifferentDocument) {
  Element* element = CreateElement("a-a").InDocument(&GetDocument());
  Registry().AddCandidate(*element);

  auto* other_document = MakeGarbageCollected<HTMLDocument>();
  other_document->AppendChild(element);
  EXPECT_EQ(other_document, element->ownerDocument())
      << "sanity: another document should have adopted an element on append";

  HeapVector<Member<Element>> elements;
  CollectCandidates(CustomElementDescriptor("a-a", "a-a"), &elements);

  EXPECT_TRUE(elements.IsEmpty())
      << "no candidates should have been found, but we have "
      << elements.size();
  EXPECT_FALSE(elements.Contains(element))
      << "the adopted-away candidate should not have been found";
}

TEST_F(CustomElementRegistryTest,
       collectCandidates_shouldOnlyIncludeCandidatesMatchingDescriptor) {
  CustomElementDescriptor descriptor("hello-world", "hello-world");

  // Does not match: namespace is not HTML
  Element& element_a = *CreateElement("hello-world")
                            .InDocument(&GetDocument())
                            .InNamespace("data:text/date,1981-03-10");
  // Matches
  Element& element_b = *CreateElement("hello-world").InDocument(&GetDocument());
  // Does not match: local name is not hello-world
  Element& element_c = *CreateElement("button")
                            .InDocument(&GetDocument())
                            .WithIsValue("hello-world");
  GetDocument().documentElement()->AppendChild(&element_a);
  element_a.AppendChild(&element_b);
  element_a.AppendChild(&element_c);

  Registry().AddCandidate(element_a);
  Registry().AddCandidate(element_b);
  Registry().AddCandidate(element_c);

  HeapVector<Member<Element>> elements;
  CollectCandidates(descriptor, &elements);

  EXPECT_EQ(1u, elements.size())
      << "only one candidates should have been found";
  EXPECT_EQ(element_b, elements[0])
      << "the matching element should have been found";
}

TEST_F(CustomElementRegistryTest, collectCandidates_oneCandidate) {
  Element& element = *CreateElement("a-a").InDocument(&GetDocument());
  Registry().AddCandidate(element);
  GetDocument().documentElement()->AppendChild(&element);

  HeapVector<Member<Element>> elements;
  CollectCandidates(CustomElementDescriptor("a-a", "a-a"), &elements);

  EXPECT_EQ(1u, elements.size())
      << "exactly one candidate should have been found";
  EXPECT_TRUE(elements.Contains(element))
      << "the candidate should be the element that was added";
}

TEST_F(CustomElementRegistryTest, collectCandidates_shouldBeInDocumentOrder) {
  CreateElement factory = CreateElement("a-a");
  factory.InDocument(&GetDocument());
  Element* element_a = factory.WithId("a");
  Element* element_b = factory.WithId("b");
  Element* element_c = factory.WithId("c");

  Registry().AddCandidate(*element_b);
  Registry().AddCandidate(*element_a);
  Registry().AddCandidate(*element_c);

  GetDocument().documentElement()->AppendChild(element_a);
  element_a->AppendChild(element_b);
  GetDocument().documentElement()->AppendChild(element_c);

  HeapVector<Member<Element>> elements;
  CollectCandidates(CustomElementDescriptor("a-a", "a-a"), &elements);

  EXPECT_EQ(element_a, elements[0].Get());
  EXPECT_EQ(element_b, elements[1].Get());
  EXPECT_EQ(element_c, elements[2].Get());
}

// Classes which use trace macros cannot be local because of the
// traceImpl template.
class LogUpgradeDefinition : public TestCustomElementDefinition {
 public:
  LogUpgradeDefinition(const CustomElementDescriptor& descriptor)
      : TestCustomElementDefinition(
            descriptor,
            {
                "attr1", "attr2", html_names::kContenteditableAttr.LocalName(),
            },
            {}) {}

  void Trace(Visitor* visitor) override {
    TestCustomElementDefinition::Trace(visitor);
    visitor->Trace(element_);
    visitor->Trace(adopted_);
  }

  // TODO(dominicc): Make this class collect a vector of what's
  // upgraded; it will be useful in more tests.
  Member<Element> element_;
  enum MethodType {
    kConstructor,
    kConnectedCallback,
    kDisconnectedCallback,
    kAdoptedCallback,
    kAttributeChangedCallback,
  };
  Vector<MethodType> logs_;

  struct AttributeChanged {
    QualifiedName name;
    AtomicString old_value;
    AtomicString new_value;
  };
  Vector<AttributeChanged> attribute_changed_;

  struct Adopted : public GarbageCollected<Adopted> {
    Adopted(Document& old_owner, Document& new_owner)
        : old_owner_(old_owner), new_owner_(new_owner) {}

    Member<Document> old_owner_;
    Member<Document> new_owner_;

    void Trace(Visitor* visitor) {
      visitor->Trace(old_owner_);
      visitor->Trace(new_owner_);
    }
  };
  HeapVector<Member<Adopted>> adopted_;

  void Clear() {
    logs_.clear();
    attribute_changed_.clear();
  }

  bool RunConstructor(Element& element) override {
    logs_.push_back(kConstructor);
    element_ = element;
    return TestCustomElementDefinition::RunConstructor(element);
  }

  bool HasConnectedCallback() const override { return true; }
  bool HasDisconnectedCallback() const override { return true; }
  bool HasAdoptedCallback() const override { return true; }

  void RunConnectedCallback(Element& element) override {
    logs_.push_back(kConnectedCallback);
    EXPECT_EQ(&element, element_);
  }

  void RunDisconnectedCallback(Element& element) override {
    logs_.push_back(kDisconnectedCallback);
    EXPECT_EQ(&element, element_);
  }

  void RunAdoptedCallback(Element& element,
                          Document& old_owner,
                          Document& new_owner) override {
    logs_.push_back(kAdoptedCallback);
    EXPECT_EQ(&element, element_);
    adopted_.push_back(MakeGarbageCollected<Adopted>(old_owner, new_owner));
  }

  void RunAttributeChangedCallback(Element& element,
                                   const QualifiedName& name,
                                   const AtomicString& old_value,
                                   const AtomicString& new_value) override {
    logs_.push_back(kAttributeChangedCallback);
    EXPECT_EQ(&element, element_);
    attribute_changed_.push_back(AttributeChanged{name, old_value, new_value});
  }

  DISALLOW_COPY_AND_ASSIGN(LogUpgradeDefinition);
};

class LogUpgradeBuilder final : public TestCustomElementDefinitionBuilder {
  STACK_ALLOCATED();

 public:
  LogUpgradeBuilder() = default;

  CustomElementDefinition* Build(const CustomElementDescriptor& descriptor,
                                 CustomElementDefinition::Id) override {
    return MakeGarbageCollected<LogUpgradeDefinition>(descriptor);
  }

  DISALLOW_COPY_AND_ASSIGN(LogUpgradeBuilder);
};

TEST_F(CustomElementRegistryTest, define_upgradesInDocumentElements) {
  ScriptForbiddenScope do_not_rely_on_script;

  Element* element = CreateElement("a-a").InDocument(&GetDocument());
  element->setAttribute(
      QualifiedName(g_null_atom, "attr1", html_names::xhtmlNamespaceURI), "v1");
  element->SetBooleanAttribute(html_names::kContenteditableAttr, true);
  GetDocument().documentElement()->AppendChild(element);

  LogUpgradeBuilder builder;
  NonThrowableExceptionState should_not_throw;
  {
    CEReactionsScope reactions;
    Define("a-a", builder, ElementDefinitionOptions::Create(),
           should_not_throw);
  }
  LogUpgradeDefinition* definition =
      static_cast<LogUpgradeDefinition*>(Registry().DefinitionForName("a-a"));
  EXPECT_EQ(LogUpgradeDefinition::kConstructor, definition->logs_[0])
      << "defining the element should have 'upgraded' the existing element";
  EXPECT_EQ(element, definition->element_)
      << "the existing a-a element should have been upgraded";

  EXPECT_EQ(LogUpgradeDefinition::kAttributeChangedCallback,
            definition->logs_[1])
      << "Upgrade should invoke attributeChangedCallback for all attributes";
  EXPECT_EQ("attr1", definition->attribute_changed_[0].name);
  EXPECT_EQ(g_null_atom, definition->attribute_changed_[0].old_value);
  EXPECT_EQ("v1", definition->attribute_changed_[0].new_value);

  EXPECT_EQ(LogUpgradeDefinition::kAttributeChangedCallback,
            definition->logs_[2])
      << "Upgrade should invoke attributeChangedCallback for all attributes";
  EXPECT_EQ("contenteditable", definition->attribute_changed_[1].name);
  EXPECT_EQ(g_null_atom, definition->attribute_changed_[1].old_value);
  EXPECT_EQ(g_empty_atom, definition->attribute_changed_[1].new_value);
  EXPECT_EQ(2u, definition->attribute_changed_.size())
      << "Upgrade should invoke attributeChangedCallback for all attributes";

  EXPECT_EQ(LogUpgradeDefinition::kConnectedCallback, definition->logs_[3])
      << "upgrade should invoke connectedCallback";

  EXPECT_EQ(4u, definition->logs_.size())
      << "upgrade should not invoke other callbacks";
}

TEST_F(CustomElementRegistryTest, attributeChangedCallback) {
  ScriptForbiddenScope do_not_rely_on_script;

  Element* element = CreateElement("a-a").InDocument(&GetDocument());
  GetDocument().documentElement()->AppendChild(element);

  LogUpgradeBuilder builder;
  NonThrowableExceptionState should_not_throw;
  {
    CEReactionsScope reactions;
    Define("a-a", builder, ElementDefinitionOptions::Create(),
           should_not_throw);
  }
  LogUpgradeDefinition* definition =
      static_cast<LogUpgradeDefinition*>(Registry().DefinitionForName("a-a"));

  definition->Clear();
  {
    CEReactionsScope reactions;
    element->setAttribute(
        QualifiedName(g_null_atom, "attr2", html_names::xhtmlNamespaceURI),
        "v2");
  }
  EXPECT_EQ(LogUpgradeDefinition::kAttributeChangedCallback,
            definition->logs_[0])
      << "Adding an attribute should invoke attributeChangedCallback";
  EXPECT_EQ(1u, definition->attribute_changed_.size())
      << "Adding an attribute should invoke attributeChangedCallback";
  EXPECT_EQ("attr2", definition->attribute_changed_[0].name);
  EXPECT_EQ(g_null_atom, definition->attribute_changed_[0].old_value);
  EXPECT_EQ("v2", definition->attribute_changed_[0].new_value);

  EXPECT_EQ(1u, definition->logs_.size())
      << "upgrade should not invoke other callbacks";
}

TEST_F(CustomElementRegistryTest, disconnectedCallback) {
  ScriptForbiddenScope do_not_rely_on_script;

  Element* element = CreateElement("a-a").InDocument(&GetDocument());
  GetDocument().documentElement()->AppendChild(element);

  LogUpgradeBuilder builder;
  NonThrowableExceptionState should_not_throw;
  {
    CEReactionsScope reactions;
    Define("a-a", builder, ElementDefinitionOptions::Create(),
           should_not_throw);
  }
  LogUpgradeDefinition* definition =
      static_cast<LogUpgradeDefinition*>(Registry().DefinitionForName("a-a"));

  definition->Clear();
  {
    CEReactionsScope reactions;
    element->remove(should_not_throw);
  }
  EXPECT_EQ(LogUpgradeDefinition::kDisconnectedCallback, definition->logs_[0])
      << "remove() should invoke disconnectedCallback";

  EXPECT_EQ(1u, definition->logs_.size())
      << "remove() should not invoke other callbacks";
}

TEST_F(CustomElementRegistryTest, adoptedCallback) {
  ScriptForbiddenScope do_not_rely_on_script;

  Element* element = CreateElement("a-a").InDocument(&GetDocument());
  GetDocument().documentElement()->AppendChild(element);

  LogUpgradeBuilder builder;
  NonThrowableExceptionState should_not_throw;
  {
    CEReactionsScope reactions;
    Define("a-a", builder, ElementDefinitionOptions::Create(),
           should_not_throw);
  }
  LogUpgradeDefinition* definition =
      static_cast<LogUpgradeDefinition*>(Registry().DefinitionForName("a-a"));

  definition->Clear();
  auto* other_document = MakeGarbageCollected<HTMLDocument>();
  {
    CEReactionsScope reactions;
    other_document->adoptNode(element, ASSERT_NO_EXCEPTION);
  }
  EXPECT_EQ(LogUpgradeDefinition::kDisconnectedCallback, definition->logs_[0])
      << "adoptNode() should invoke disconnectedCallback";

  EXPECT_EQ(LogUpgradeDefinition::kAdoptedCallback, definition->logs_[1])
      << "adoptNode() should invoke adoptedCallback";

  EXPECT_EQ(&GetDocument(), definition->adopted_[0]->old_owner_.Get())
      << "adoptedCallback should have been passed the old owner document";
  EXPECT_EQ(other_document, definition->adopted_[0]->new_owner_.Get())
      << "adoptedCallback should have been passed the new owner document";

  EXPECT_EQ(2u, definition->logs_.size())
      << "adoptNode() should not invoke other callbacks";
}

TEST_F(CustomElementRegistryTest, lookupCustomElementDefinition) {
  NonThrowableExceptionState should_not_throw;
  TestCustomElementDefinitionBuilder builder;
  CustomElementDefinition* definition_a = Define(
      "a-a", builder, ElementDefinitionOptions::Create(), should_not_throw);
  ElementDefinitionOptions* options = ElementDefinitionOptions::Create();
  options->setExtends("div");
  CustomElementDefinition* definition_b =
      Define("b-b", builder, options, should_not_throw);
  // look up defined autonomous custom element
  CustomElementDefinition* definition = Registry().DefinitionFor(
      CustomElementDescriptor(CustomElementDescriptor("a-a", "a-a")));
  EXPECT_NE(nullptr, definition) << "a-a, a-a should be registered";
  EXPECT_EQ(definition_a, definition);
  // look up undefined autonomous custom element
  definition = Registry().DefinitionFor(CustomElementDescriptor("a-a", "div"));
  EXPECT_EQ(nullptr, definition) << "a-a, div should not be registered";
  // look up defined customized built-in element
  definition = Registry().DefinitionFor(CustomElementDescriptor("b-b", "div"));
  EXPECT_NE(nullptr, definition) << "b-b, div should be registered";
  EXPECT_EQ(definition_b, definition);
  // look up undefined customized built-in element
  definition = Registry().DefinitionFor(CustomElementDescriptor("a-a", "div"));
  EXPECT_EQ(nullptr, definition) << "a-a, div should not be registered";
}

// The embedder may define its own elements via the CustomElementRegistry
// whose names are not valid custom element names. Ensure that such a
// definition may be done.
TEST_F(CustomElementRegistryTest, DefineEmbedderCustomElements) {
  CustomElement::AddEmbedderCustomElementName("embeddercustomelement");

  WebCustomElement::EmbedderNamesAllowedScope embedder_names_scope;

  NonThrowableExceptionState should_not_throw;
  TestCustomElementDefinitionBuilder builder;
  CustomElementDefinition* definition_embedder =
      Define("embeddercustomelement", builder,
             ElementDefinitionOptions::Create(), should_not_throw);
  CustomElementDefinition* definition =
      Registry().DefinitionFor(CustomElementDescriptor(
          "embeddercustomelement", "embeddercustomelement"));
  EXPECT_NE(nullptr, definition)
      << "embeddercustomelement, embeddercustomelement should be registered";
  EXPECT_EQ(definition_embedder, definition);
}

// Ensure that even when the embedder has declared that an invalid name may
// be used for a custom element definition, the caller of |define| may disallow
// the use of the invalid name (so that we don't expose the ability to use such
// a name to the web).
TEST_F(CustomElementRegistryTest, DisallowedEmbedderCustomElements) {
  CustomElement::AddEmbedderCustomElementName("embeddercustomelement");

  // Without a WebCustomElement::EmbedderNamesAllowedScope, this registration
  // is disallowed.

  TestCustomElementDefinitionBuilder builder;
  CustomElementDefinition* definition_embedder =
      Define("embeddercustomelement", builder,
             ElementDefinitionOptions::Create(), IGNORE_EXCEPTION_FOR_TESTING);
  CustomElementDefinition* definition =
      Registry().DefinitionFor(CustomElementDescriptor(
          "embeddercustomelement", "embeddercustomelement"));
  EXPECT_EQ(nullptr, definition) << "embeddercustomelement, "
                                    "embeddercustomelement should not be "
                                    "registered";
  EXPECT_EQ(definition_embedder, definition);
}

// TODO(dominicc): Add tests which adjust the "is" attribute when type
// extensions are implemented.

}  // namespace blink
