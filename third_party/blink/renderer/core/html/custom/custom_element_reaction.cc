// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/custom_element_reaction.h"

#include "third_party/blink/renderer/core/html/custom/custom_element_definition.h"

namespace blink {

CustomElementReaction::CustomElementReaction(
    CustomElementDefinition& definition)
    : definition_(definition) {}

void CustomElementReaction::Trace(Visitor* visitor) const {
  visitor->Trace(definition_);
}

}  // namespace blink
