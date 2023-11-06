// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/custom/document_layout_definition.h"

namespace blink {

DocumentLayoutDefinition::DocumentLayoutDefinition(
    CSSLayoutDefinition* definition)
    : layout_definition_(definition), registered_definitions_count_(1u) {
  DCHECK(definition);
}

DocumentLayoutDefinition::~DocumentLayoutDefinition() = default;

bool DocumentLayoutDefinition::RegisterAdditionalLayoutDefinition(
    const CSSLayoutDefinition& other) {
  if (!IsEqual(other))
    return false;
  registered_definitions_count_++;
  return true;
}

bool DocumentLayoutDefinition::IsEqual(const CSSLayoutDefinition& other) {
  return NativeInvalidationProperties() ==
             other.NativeInvalidationProperties() &&
         CustomInvalidationProperties() ==
             other.CustomInvalidationProperties() &&
         ChildNativeInvalidationProperties() ==
             other.ChildNativeInvalidationProperties() &&
         ChildCustomInvalidationProperties() ==
             other.ChildCustomInvalidationProperties();
}

void DocumentLayoutDefinition::Trace(Visitor* visitor) const {
  visitor->Trace(layout_definition_);
}

}  // namespace blink
