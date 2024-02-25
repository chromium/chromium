// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/custom_element_registry.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_custom_element.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_css_style_sheet_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_element_definition_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_shadow_root_init.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/custom/ce_reactions_scope.h"
#include "third_party/blink/renderer/core/html/custom/custom_element.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_definition.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_definition_builder.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_descriptor.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_test_helpers.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class CustomElementRegistryTest : public ::testing::Test {
 public:
  CustomElementRegistry& Registry() {
    return CustomElementTestingScope::GetInstance().Registry();
  }

  ScriptState* GetScriptState() {
    return CustomElementTestingScope::GetInstance().GetScriptState();
  }

  Document& GetDocument() {
    return CustomElementTestingScope::GetInstance().GetDocument();
  }

  CustomElementDefinition* Define(const char* name,
                                  CustomElementDefinitionBuilder& builder,
                                  const ElementDefinitionOptions* options,
                                  ExceptionState& exception_state) {
    return Registry().DefineInternal(GetScriptState(), AtomicString(name),
                                     builder, options, exception_state);
  }

  void CollectCandidates(const CustomElementDescriptor& desc,
                         HeapVector<Member<Element>>* elements) {
    Registry().CollectCandidates(desc, elements);
  }
  test::TaskEnvironment task_environment_;
};

TEST_F(CustomElementRegistryTest,
       collectCandidates_shouldNotIncludeElementsRemovedFromDocument) {
  CustomElementTestingScope testing_scope;
  Element& element =
      *CreateElement(AtomicString("a-a")).InDocument(&GetDocument());
  Registry().AddCandidate(element);

  HeapVector<Member<Element>> elements;
  CollectCandidates(
      CustomElementDescriptor(AtomicString("a-a"), AtomicString("a-a")),
      &elements);

  EXPECT_TRUE(elements.empty())
      << "no candidates should have been found, but we have "
      << elements.size();
  EXPECT_FALSE(elements.Contains(element))
      << "the out-of-document candidate should not have been found";
}

TEST_F(CustomElementRegistryTest,
       collectCandidates_shouldNotIncludeElementsInDifferentDocument) {
  CustomElementTestingScope testing_scope;
  Element* element =
      CreateElement(AtomicString("a-a")).InDocument(&GetDocument());
  Registry().AddCandidate(*element);

  ScopedNullExecutionContext execution_context;
  auto* other_document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  other_document->AppendChild(element);
  EXPECT_EQ(other_document, element->ownerDocument())
      << "sanity: another document should have adopted an element on append";

  HeapVector<Member<Element>> elements;
  CollectCandidates(
      CustomElementDescriptor(AtomicString("a-a"), AtomicString("a-a")),
      &elements);

  EXPECT_TRUE(elements.empty())
      << "no candidates should have been found, but we have "
      << elements.size();
  EXPECT_FALSE(elements.Contains(element))
      << "the adopted-away candidate should not have been found";
}

TEST_F(CustomElementRegistryTest,
       collectCandidates_shouldOnlyIncludeCandidatesMatchingDescriptor) {
  CustomElementTestingScope testing_scope;
  CustomElementDescriptor descriptor(AtomicString("hello-world"),
                                     AtomicString("hello-world"));

  // Does not match: namespace is not HTML
  Element& element_a =
      *CreateElement(AtomicString("hello-world"))
           .InDocument(&GetDocument())
           .InNamespace(AtomicString("data:text/date,1981-03-10"));
  // Matches
  Element& element_b =
      *CreateElement(AtomicString("hello-world")).InDocument(&GetDocument());
  // Does not match: local name is not hello-world
  Element& element_c = *CreateElement(AtomicString("button"))
                            .InDocument(&GetDocument())
                            .WithIsValue(AtomicString("hello-world"));
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
  CustomElementTestingScope testing_scope;
  Element& element =
      *CreateElement(AtomicString("a-a")).InDocument(&GetDocument());
  Registry().AddCandidate(element);
  GetDocument().documentElement()->AppendChild(&element);

  HeapVector<Member<Element>> elements;
  CollectCandidates(
      CustomElementDescriptor(AtomicString("a-a"), AtomicString("a-a")),
      &elements);

  EXPECT_EQ(1u, elements.size())
      << "exactly one candidate should have been found";
  EXPECT_TRUE(elements.Contains(element))
      << "the candidate should be the element that was added";
}

TEST_F(CustomElementRegistryTest, collectCandidates_shouldBeInDocumentOrder) {
  CustomElementTestingScope testing_scope;
  CreateElement factory = CreateElement(AtomicString("a-a"));
  factory.InDocument(&GetDocument());
  Element* element_a = factory.WithId(AtomicString("a"));
  Element* element_b = factory.WithId(AtomicString("b"));
  Element* element_c = factory.WithId(AtomicString("c"));

  Registry().AddCandidate(*element_b);
  Registry().AddCandidate(*element_a);
  Registry().AddCandidate(*element_c);

  GetDocument().documentElement()->AppendChild(element_a);
  element_a->AppendChild(element_b);
  GetDocument().documentElement()->AppendChild(element_c);

  HeapVector<Member<Element>> elements;
  CollectCandidates(
      CustomElementDescriptor(AtomicString("a-a"), AtomicString("a-a")),
      &elements);

  EXPECT_EQ(element_a, elements[0].Get());
  EXPECT_EQ(element_b, elements[1].Get());
  EXPECT_EQ(element_c, elements[2].Get());
}

// Classes which use trace macros cannot be local because of the
// traceImpl template.
class LogUpgradeDefinition : public TestCustomElementDefinition {
 public:
  LogUpgradeDefinition(const CustomElementDescriptor& descriptor,
                       V8CustomElementConstructor* constructor)
      : TestCustomElementDefinition(
            descriptor,
            constructor,
            {
                AtomicString("attr1"),
                AtomicString("attr2"),
                html_names::kContenteditableAttr.LocalName(),
            },
            {}) {}
  LogUpgradeDefinition(const LogUpgradeDefinition&) = delete;
  LogUpgradeDefinition& operator=(const LogUpgradeDefinition&) = delete;

  void Trace(Visitor* visitor) const override {
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

    void Trace(Visitor* visitor) const {
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
};

class LogUpgradeBuilder final : public TestCustomElementDefinitionBuilder {
  STACK_ALLOCATED();

 public:
  LogUpgradeBuilder() = default;
  LogUpgradeBuilder(const LogUpgradeBuilder&) = delete;
  LogUpgradeBuilder& operator=(const LogUpgradeBuilder&) = delete;

  CustomElementDefinition* Build(
      const CustomElementDescriptor& descriptor) override {
    return MakeGarbageCollected<LogUpgradeDefinition>(descriptor,
                                                      Constructor());
  }
};

TEST_F(CustomElementRegistryTest, define_upgradesInDocumentElements) {
  CustomElementTestingScope testing_scope;
  ScriptForbiddenScope do_not_rely_on_script;

  Element* element =
      CreateElement(AtomicString("a-a")).InDocument(&GetDocument());
  element->setAttribute(QualifiedName(g_null_atom, AtomicString("attr1"),
                                      html_names::xhtmlNamespaceURI),
                        AtomicString("v1"));
  element->SetBooleanAttribute(html_names::kContenteditableAttr, true);
  GetDocument().documentElement()->AppendChild(element);

  LogUpgradeBuilder builder;
  NonThrowableExceptionState should_not_throw;
  {
    CEReactionsScope reactions;
    Define("a-a", builder, ElementDefinitionOptions::Create(),
           should_not_throw);
  }
  LogUpgradeDefinition* definition = static_cast<LogUpgradeDefinition*>(
      Registry().DefinitionForName(AtomicString("a-a")));
  EXPECT_EQ(LogUpgradeDefinition::kConstructor, definition->logs_[0])
      << "defining the element should have 'upgraded' the existing element";
  EXPECT_EQ(element, definition->element_)
      << "the existing a-a element should have been upgraded";

  EXPECT_EQ(LogUpgradeDefinition::kAttributeChangedCallback,
            definition->logs_[1])
      << "Upgrade should invoke attributeChangedCallback for all attributes";
  EXPECT_EQ("attr1", definition->attribute_changed_[0].name.LocalName());
  EXPECT_EQ(g_null_atom, definition->attribute_changed_[0].old_value);
  EXPECT_EQ("v1", definition->attribute_changed_[0].new_value);

  EXPECT_EQ(LogUpgradeDefinition::kAttributeChangedCallback,
            definition->logs_[2])
      << "Upgrade should invoke attributeChangedCallback for all attributes";
  EXPECT_EQ("contenteditable",
            definition->attribute_changed_[1].name.LocalName());
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
  CustomElementTestingScope testing_scope;
  ScriptForbiddenScope do_not_rely_on_script;

  Element* element =
      CreateElement(AtomicString("a-a")).InDocument(&GetDocument());
  GetDocument().documentElement()->AppendChild(element);

  LogUpgradeBuilder builder;
  NonThrowableExceptionState should_not_throw;
  {
    CEReactionsScope reactions;
    Define("a-a", builder, ElementDefinitionOptions::Create(),
           should_not_throw);
  }
  LogUpgradeDefinition* definition = static_cast<LogUpgradeDefinition*>(
      Registry().DefinitionForName(AtomicString("a-a")));

  definition->Clear();
  {
    CEReactionsScope reactions;
    element->setAttribute(QualifiedName(g_null_atom, AtomicString("attr2"),
                                        html_names::xhtmlNamespaceURI),
                          AtomicString("v2"));
  }
  EXPECT_EQ(LogUpgradeDefinition::kAttributeChangedCallback,
            definition->logs_[0])
      << "Adding an attribute should invoke attributeChangedCallback";
  EXPECT_EQ(1u, definition->attribute_changed_.size())
      << "Adding an attribute should invoke attributeChangedCallback";
  EXPECT_EQ("attr2", definition->attribute_changed_[0].name.LocalName());
  EXPECT_EQ(g_null_atom, definition->attribute_changed_[0].old_value);
  EXPECT_EQ("v2", definition->attribute_changed_[0].new_value);

  EXPECT_EQ(1u, definition->logs_.size())
      << "upgrade should not invoke other callbacks";
}

TEST_F(CustomElementRegistryTest, disconnectedCallback) {
  CustomElementTestingScope testing_scope;
  ScriptForbiddenScope do_not_rely_on_script;

  Element* element =
      CreateElement(AtomicString("a-a")).InDocument(&GetDocument());
  GetDocument().documentElement()->AppendChild(element);

  LogUpgradeBuilder builder;
  NonThrowableExceptionState should_not_throw;
  {
    CEReactionsScope reactions;
    Define("a-a", builder, ElementDefinitionOptions::Create(),
           should_not_throw);
  }
  LogUpgradeDefinition* definition = static_cast<LogUpgradeDefinition*>(
      Registry().DefinitionForName(AtomicString("a-a")));

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
  CustomElementTestingScope testing_scope;
  ScriptForbiddenScope do_not_rely_on_script;

  Element* element =
      CreateElement(AtomicString("a-a")).InDocument(&GetDocument());
  GetDocument().documentElement()->AppendChild(element);

  LogUpgradeBuilder builder;
  NonThrowableExceptionState should_not_throw;
  {
    CEReactionsScope reactions;
    Define("a-a", builder, ElementDefinitionOptions::Create(),
           should_not_throw);
  }
  LogUpgradeDefinition* definition = static_cast<LogUpgradeDefinition*>(
      Registry().DefinitionForName(AtomicString("a-a")));

  definition->Clear();
  auto* other_document =
      HTMLDocument::CreateForTest(*GetDocument().GetExecutionContext());
  {
    CEReactionsScope reactions;
    other_document->adoptNode(element, ASSERT_NO_EXCEPTION);
  }
  EXPECT_EQ(LogUpgradeDefinition::kDisconnectedCallback, definition->logs_[0])
      << "adoptNode() should invoke disconnectedCallback";

  EXPECT_EQ(LogUpgradeDefinition::kAdoptedCallback, definition->logs_[1])
      << "adoptNode() should invoke adoptedCallback";

  EXPECT_EQ(GetDocument(), definition->adopted_[0]->old_owner_.Get())
      << "adoptedCallback should have been passed the old owner document";
  EXPECT_EQ(other_document, definition->adopted_[0]->new_owner_.Get())
      << "adoptedCallback should have been passed the new owner document";

  EXPECT_EQ(2u, definition->logs_.size())
      << "adoptNode() should not invoke other callbacks";
}

TEST_F(CustomElementRegistryTest, lookupCustomElementDefinition) {
  CustomElementTestingScope testing_scope;
  NonThrowableExceptionState should_not_throw;
  TestCustomElementDefinitionBuilder builder_a;
  CustomElementDefinition* definition_a = Define(
      "a-a", builder_a, ElementDefinitionOptions::Create(), should_not_throw);
  TestCustomElementDefinitionBuilder builder_b;
  ElementDefinitionOptions* options = ElementDefinitionOptions::Create();
  options->setExtends("div");
  CustomElementDefinition* definition_b =
      Define("b-b", builder_b, options, should_not_throw);
  // look up defined autonomous custom element
  CustomElementDefinition* definition =
      Registry().DefinitionFor(CustomElementDescriptor(
          CustomElementDescriptor(AtomicString("a-a"), AtomicString("a-a"))));
  EXPECT_NE(nullptr, definition) << "a-a, a-a should be registered";
  EXPECT_EQ(definition_a, definition);
  // look up undefined autonomous custom element
  definition = Registry().DefinitionFor(
      CustomElementDescriptor(AtomicString("a-a"), AtomicString("div")));
  EXPECT_EQ(nullptr, definition) << "a-a, div should not be registered";
  // look up defined customized built-in element
  definition = Registry().DefinitionFor(
      CustomElementDescriptor(AtomicString("b-b"), AtomicString("div")));
  EXPECT_NE(nullptr, definition) << "b-b, div should be registered";
  EXPECT_EQ(definition_b, definition);
  // look up undefined customized built-in element
  definition = Registry().DefinitionFor(
      CustomElementDescriptor(AtomicString("a-a"), AtomicString("div")));
  EXPECT_EQ(nullptr, definition) << "a-a, div should not be registered";
}

// The embedder may define its own elements via the CustomElementRegistry
// whose names are not valid custom element names. Ensure that such a definition
// may be done.
TEST_F(CustomElementRegistryTest, DefineEmbedderCustomElements) {
  CustomElementTestingScope testing_scope;
  CustomElement::AddEmbedderCustomElementName(
      AtomicString("embeddercustomelement"));

  WebCustomElement::EmbedderNamesAllowedScope embedder_names_scope;

  NonThrowableExceptionState should_not_throw;
  TestCustomElementDefinitionBuilder builder;
  CustomElementDefinition* definition_embedder =
      Define("embeddercustomelement", builder,
             ElementDefinitionOptions::Create(), should_not_throw);
  CustomElementDefinition* definition = Registry().DefinitionFor(
      CustomElementDescriptor(AtomicString("embeddercustomelement"),
                              AtomicString("embeddercustomelement")));
  EXPECT_NE(nullptr, definition)
      << "embeddercustomelement, embeddercustomelement should be registered";
  EXPECT_EQ(definition_embedder, definition);
}

// Ensure that even when the embedder has declared that an invalid name may
// be used for a custom element definition, the caller of |define| may disallow
// the use of the invalid name (so that we don't expose the ability to use such
// a name to the web).
TEST_F(CustomElementRegistryTest, DisallowedEmbedderCustomElements) {
  CustomElementTestingScope testing_scope;
  CustomElement::AddEmbedderCustomElementName(
      AtomicString("embeddercustomelement"));

  // Without a WebCustomElement::EmbedderNamesAllowedScope, this registration
  // is disallowed.

  TestCustomElementDefinitionBuilder builder;
  CustomElementDefinition* definition_embedder =
      Define("embeddercustomelement", builder,
             ElementDefinitionOptions::Create(), IGNORE_EXCEPTION_FOR_TESTING);
  CustomElementDefinition* definition = Registry().DefinitionFor(
      CustomElementDescriptor(AtomicString("embeddercustomelement"),
                              AtomicString("embeddercustomelement")));
  EXPECT_EQ(nullptr, definition) << "embeddercustomelement, "
                                    "embeddercustomelement should not be "
                                    "registered";
  EXPECT_EQ(definition_embedder, definition);
}

// TODO(dominicc): Add tests which adjust the "is" attribute when type
// extensions are implemented.

}  // namespace blink
