// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/xml/dom_parser.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_supported_type.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/parser/html_construction_site.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/core/xml/dom_parser.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace {

TEST(DOMParserTest, DomParserDocumentUsesQuirksMode) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* parser = DOMParser::Create(scope.GetScriptState());
  base::HistogramTester histogram_tester;
  Document* document = parser->parseFromString(
      "<div></div>", V8SupportedType(V8SupportedType::Enum::kTextHtml));
  EXPECT_TRUE(document->InQuirksMode());
}

TEST(DOMParserTest, DomParserDocumentUsesNoQuirksMode) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* parser = DOMParser::Create(scope.GetScriptState());
  base::HistogramTester histogram_tester;
  Document* document = parser->parseFromString(
      "<!doctype html>", V8SupportedType(V8SupportedType::Enum::kTextHtml));
  EXPECT_TRUE(document->InNoQuirksMode());
}

}  // namespace
}  // namespace blink
