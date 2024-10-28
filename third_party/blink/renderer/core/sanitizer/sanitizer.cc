// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sanitizer.h"

namespace blink {

Sanitizer* Sanitizer::Create(ExecutionContext* execution_context,
                             const SanitizerConfig* sanitizer_config,
                             ExceptionState& exception_state) {
  Sanitizer* sanitizer = MakeGarbageCollected<Sanitizer>();
  return sanitizer;
}

void Sanitizer::allowElement(
    const V8UnionSanitizerElementNamespaceWithAttributesOrString* element) {}

void Sanitizer::removeElement(
    const V8UnionSanitizerElementNamespaceOrString* element) {}

void Sanitizer::replaceWithChildrenElement(
    const V8UnionSanitizerElementNamespaceOrString* element) {}

void Sanitizer::allowAttribute(
    const V8UnionSanitizerAttributeNamespaceOrString* attribute) {}

void Sanitizer::removeAttribute(
    const V8UnionSanitizerAttributeNamespaceOrString* attribute) {}

void Sanitizer::setComments(bool comments) {}

void Sanitizer::setDataAttributes(bool data_attributes) {}

void Sanitizer::removeUnsafe() {}

SanitizerConfig* Sanitizer::get() const {
  return nullptr;
}

}  // namespace blink
