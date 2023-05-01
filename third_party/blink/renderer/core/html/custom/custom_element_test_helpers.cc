// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/custom_element_test_helpers.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_custom_element_constructor.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_construction_stack.h"

namespace blink {

namespace {

// Creates a mock custom element constructor that has a callback object to be
// hashed, but should never be be invoked.
V8CustomElementConstructor* CreateMockConstructor() {
  ScriptState* script_state =
      CustomElementTestingScope::GetInstance().GetScriptState();
  return V8CustomElementConstructor::Create(
      v8::Object::New(script_state->GetIsolate()));
}

}  // namespace

CustomElementTestingScope* CustomElementTestingScope::instance_ = nullptr;

CustomElementRegistry& CustomElementTestingScope::Registry() {
  return *GetFrame().DomWindow()->customElements();
}

TestCustomElementDefinitionBuilder::TestCustomElementDefinitionBuilder()
    : constructor_(CreateMockConstructor()) {}

CustomElementDefinition* TestCustomElementDefinitionBuilder::Build(
    const CustomElementDescriptor& descriptor) {
  DCHECK(constructor_);
  return MakeGarbageCollected<TestCustomElementDefinition>(descriptor,
                                                           constructor_);
}

TestCustomElementDefinition::TestCustomElementDefinition(
    const CustomElementDescriptor& descriptor)
    : TestCustomElementDefinition(descriptor, CreateMockConstructor()) {}

TestCustomElementDefinition::TestCustomElementDefinition(
    const CustomElementDescriptor& descriptor,
    V8CustomElementConstructor* constructor)
    : CustomElementDefinition(
          CustomElementTestingScope::GetInstance().Registry(),
          descriptor),
      constructor_(constructor) {}

TestCustomElementDefinition::TestCustomElementDefinition(
    const CustomElementDescriptor& descriptor,
    V8CustomElementConstructor* constructor,
    HashSet<AtomicString>&& observed_attributes,
    const Vector<String>& disabled_features)
    : CustomElementDefinition(
          CustomElementTestingScope::GetInstance().Registry(),
          descriptor,
          std::move(observed_attributes),
          disabled_features,
          FormAssociationFlag::kNo),
      constructor_(constructor) {}

bool TestCustomElementDefinition::RunConstructor(Element& element) {
  CustomElementConstructionStack* construction_stack =
      GetCustomElementConstructionStack(GetRegistry().GetOwnerWindow(),
                                        constructor_->CallbackObject());
  if (!construction_stack || construction_stack->empty() ||
      construction_stack->back().element != &element) {
    return false;
  }
  construction_stack->back() = CustomElementConstructionStackEntry();
  return true;
}

}  // namespace blink
