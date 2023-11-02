// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/plain_text_range.h"

#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"

namespace blink {

class PlainTextRangeTest : public EditingTestBase {};

TEST_F(PlainTextRangeTest, RangeContainingTableCellBoundary) {
  SetBodyInnerHTML(
      "<table id='sample' contenteditable><tr><td>a</td><td "
      "id='td2'>b</td></tr></table>");
  Element* table = GetElementById("sample");

  PlainTextRange plain_text_range(2, 2);
  const EphemeralRange& range = plain_text_range.CreateRange(*table);
  EXPECT_EQ(
      "<table contenteditable id=\"sample\"><tbody><tr><td>a</td><td "
      "id=\"td2\">|b</td></tr></tbody></table>",
      GetCaretTextFromBody(range.StartPosition()));
  EXPECT_TRUE(range.IsCollapsed());
}

}  // namespace blink
