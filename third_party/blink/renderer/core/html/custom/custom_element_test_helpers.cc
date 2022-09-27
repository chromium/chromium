// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/custom_element_test_helpers.h"

namespace blink {

CustomElementDefinition* TestCustomElementDefinitionBuilder::Build(
    const CustomElementDescriptor& descriptor,
    CustomElementDefinition::Id) {
  return MakeGarbageCollected<TestCustomElementDefinition>(descriptor);
}

}  // namespace blink
