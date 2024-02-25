// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mathml/mathml_table_cell_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

// Test parsing of the columnspan attribute as described in (8) of
// https://html.spec.whatwg.org/#algorithm-for-processing-rows
// TODO(crbug.com/1371806: Convert this to a WPT test when MathML has an IDL
// for that. See https://github.com/w3c/mathml-core/issues/166
TEST(MathMLTableCellElementTest, colSpan_parsing) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      Document::CreateForTest(execution_context.GetExecutionContext());
  auto* cell = MakeGarbageCollected<MathMLTableCellElement>(*document);

  for (unsigned colSpan : {1, 2, 16, 256, 999, 1000}) {
    StringBuilder attributeValue;
    attributeValue.AppendNumber(colSpan);
    cell->setAttribute(mathml_names::kColumnspanAttr, attributeValue.ToString(),
                       ASSERT_NO_EXCEPTION);
    EXPECT_EQ(colSpan, cell->colSpan())
        << "valid columnspan value '" << colSpan << "' is properly parsed";
  }

  cell->setAttribute(mathml_names::kColumnspanAttr, AtomicString("-1"));
  EXPECT_EQ(1u, cell->colSpan()) << "columnspan is 1 if parsing failed";

  cell->setAttribute(mathml_names::kColumnspanAttr, AtomicString("0"));
  EXPECT_EQ(1u, cell->colSpan()) << "columnspan is 1 if parsing returned 0";

  cell->setAttribute(mathml_names::kColumnspanAttr, AtomicString("1001"));
  EXPECT_EQ(1000u, cell->colSpan())
      << "columnspan is clamped to max value 1000";

  cell->removeAttribute(mathml_names::kColumnspanAttr);
  EXPECT_EQ(1u, cell->colSpan()) << "columnspan is 1 if attribute is absent";
}

// Test parsing of the rowspan attribute as described in (9) of
// https://html.spec.whatwg.org/#algorithm-for-processing-rows
// TODO(crbug.com/1371806: Convert this to a WPT test when MathML has an IDL
// for that. See https://github.com/w3c/mathml-core/issues/166
TEST(MathMLTableCellElementTest, rowspan_parsing) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      Document::CreateForTest(execution_context.GetExecutionContext());
  auto* cell = MakeGarbageCollected<MathMLTableCellElement>(*document);

  for (unsigned rowspan : {0, 1, 16, 256, 4096, 65533, 65534}) {
    StringBuilder attributeValue;
    attributeValue.AppendNumber(rowspan);
    cell->setAttribute(mathml_names::kRowspanAttr, attributeValue.ToString(),
                       ASSERT_NO_EXCEPTION);
    EXPECT_EQ(rowspan, cell->rowSpan())
        << "valid rowspan value '" << rowspan << "' is properly parsed";
  }

  cell->setAttribute(mathml_names::kRowspanAttr, AtomicString("-1"));
  EXPECT_EQ(1u, cell->rowSpan()) << "rowspan is 1 if parsing failed";

  cell->setAttribute(mathml_names::kRowspanAttr, AtomicString("65534"));
  EXPECT_EQ(65534u, cell->rowSpan()) << "rowspan is clamped to max value 65534";

  cell->removeAttribute(mathml_names::kRowspanAttr);
  EXPECT_EQ(1u, cell->rowSpan()) << "rowspan is 1 if attribute is absent";
}

}  // namespace blink
