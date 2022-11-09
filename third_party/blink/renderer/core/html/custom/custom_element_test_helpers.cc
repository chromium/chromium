// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/custom_element_test_helpers.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_custom_element_constructor.h"

namespace blink {

TestCustomElementDefinitionBuilder::TestCustomElementDefinitionBuilder(
    ScriptState* script_state) {
  // Create a fake v8 constructor callback that should never be invoked.
  constructor_ = V8CustomElementConstructor::Create(
      v8::Object::New(script_state->GetIsolate()));
}

CustomElementDefinition* TestCustomElementDefinitionBuilder::Build(
    const CustomElementDescriptor& descriptor) {
  return MakeGarbageCollected<TestCustomElementDefinition>(descriptor);
}

}  // namespace blink
