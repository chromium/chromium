// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/document_paint_definition.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_syntax_component.h"
#include "third_party/blink/renderer/core/css/css_syntax_definition.h"
#include "third_party/blink/renderer/core/css/css_syntax_string_parser.h"
#include "third_party/blink/renderer/modules/csspaint/css_paint_definition.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

TEST(DocumentPaintDefinitionTest, NativeInvalidationProperties) {
  test::TaskEnvironment task_environment;
  Vector<CSSPropertyID> native_invalidation_properties = {
      CSSPropertyID::kColor,
      CSSPropertyID::kZoom,
      CSSPropertyID::kTop,
  };
  Vector<AtomicString> custom_invalidation_properties;
  Vector<CSSSyntaxDefinition> input_argument_types;

  DocumentPaintDefinition document_definition(native_invalidation_properties,
                                              custom_invalidation_properties,
                                              input_argument_types, true);
  EXPECT_EQ(document_definition.NativeInvalidationProperties().size(), 3u);
  for (wtf_size_t i = 0; i < 3; i++) {
    EXPECT_EQ(native_invalidation_properties[i],
              document_definition.NativeInvalidationProperties()[i]);
  }
}

TEST(DocumentPaintDefinitionTest, CustomInvalidationProperties) {
  test::TaskEnvironment task_environment;
  Vector<CSSPropertyID> native_invalidation_properties;
  Vector<AtomicString> custom_invalidation_properties = {
      AtomicString("--my-property"),
      AtomicString("--another-property"),
  };
  Vector<CSSSyntaxDefinition> input_argument_types;

  DocumentPaintDefinition document_definition(native_invalidation_properties,
                                              custom_invalidation_properties,
                                              input_argument_types, true);
  EXPECT_EQ(document_definition.CustomInvalidationProperties().size(), 2u);
  for (wtf_size_t i = 0; i < 2; i++) {
    EXPECT_EQ(custom_invalidation_properties[i],
              document_definition.CustomInvalidationProperties()[i]);
  }
}

TEST(DocumentPaintDefinitionTest, Alpha) {
  test::TaskEnvironment task_environment;
  Vector<CSSPropertyID> native_invalidation_properties;
  Vector<AtomicString> custom_invalidation_properties;
  Vector<CSSSyntaxDefinition> input_argument_types;

  DocumentPaintDefinition document_definition_with_alpha(
      native_invalidation_properties, custom_invalidation_properties,
      input_argument_types, true);
  DocumentPaintDefinition document_definition_without_alpha(
      native_invalidation_properties, custom_invalidation_properties,
      input_argument_types, false);

  EXPECT_TRUE(document_definition_with_alpha.alpha());
  EXPECT_FALSE(document_definition_without_alpha.alpha());
}

TEST(DocumentPaintDefinitionTest, InputArgumentTypes) {
  test::TaskEnvironment task_environment;
  Vector<CSSPropertyID> native_invalidation_properties;
  Vector<AtomicString> custom_invalidation_properties;
  Vector<CSSSyntaxDefinition> input_argument_types = {
      CSSSyntaxStringParser("<length> | <color>").Parse().value(),
      CSSSyntaxStringParser("<integer> | foo | <color>").Parse().value()};

  DocumentPaintDefinition document_definition(native_invalidation_properties,
                                              custom_invalidation_properties,
                                              input_argument_types, true);

  EXPECT_EQ(document_definition.InputArgumentTypes().size(), 2u);
  for (wtf_size_t i = 0; i < 2; i++) {
    EXPECT_EQ(input_argument_types[i],
              document_definition.InputArgumentTypes()[i]);
  }
}

}  // namespace blink
