// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/document_paint_definition.h"

namespace blink {

DocumentPaintDefinition::DocumentPaintDefinition(
    const Vector<CSSPropertyID>& native_invalidation_properties,
    const Vector<AtomicString>& custom_invalidation_properties,
    const Vector<CSSSyntaxDefinition>& input_argument_types,
    bool alpha)
    : native_invalidation_properties_(native_invalidation_properties),
      custom_invalidation_properties_(custom_invalidation_properties),
      input_argument_types_(input_argument_types),
      alpha_(alpha),
      registered_definitions_count_(1u) {}

DocumentPaintDefinition::~DocumentPaintDefinition() = default;

bool DocumentPaintDefinition::RegisterAdditionalPaintDefinition(
    const CSSPaintDefinition& other) {
  if (other.NativeInvalidationProperties() != NativeInvalidationProperties() ||
      other.CustomInvalidationProperties() != CustomInvalidationProperties() ||
      other.InputArgumentTypes() != InputArgumentTypes() ||
      other.GetPaintRenderingContext2DSettings()->alpha() != alpha())
    return false;
  registered_definitions_count_++;
  return true;
}

bool DocumentPaintDefinition::RegisterAdditionalPaintDefinition(
    const Vector<CSSPropertyID>& native_properties,
    const Vector<String>& custom_properties,
    const Vector<CSSSyntaxDefinition>& input_argument_types,
    bool alpha) {
  if (native_properties != NativeInvalidationProperties() ||
      !std::equal(custom_properties.begin(), custom_properties.end(),
                  CustomInvalidationProperties().begin(),
                  CustomInvalidationProperties().end()) ||
      input_argument_types != InputArgumentTypes() || alpha != this->alpha())
    return false;
  registered_definitions_count_++;
  return true;
}

}  // namespace blink
