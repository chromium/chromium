// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_table_row_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_paragraph_element.h"
#include "third_party/blink/renderer/core/html/html_table_element.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

// rowIndex
// https://html.spec.whatwg.org/C/#dom-tr-rowindex

TEST(HTMLTableRowElementTest, rowIndex_notInTable) {
  auto* document = MakeGarbageCollected<Document>();
  auto* row = MakeGarbageCollected<HTMLTableRowElement>(*document);
  EXPECT_EQ(-1, row->rowIndex())
      << "rows not in tables should have row index -1";
}

TEST(HTMLTableRowElementTest, rowIndex_directChildOfTable) {
  auto* document = MakeGarbageCollected<Document>();
  auto* table = MakeGarbageCollected<HTMLTableElement>(*document);
  auto* row = MakeGarbageCollected<HTMLTableRowElement>(*document);
  table->AppendChild(row);
  EXPECT_EQ(0, row->rowIndex())
      << "rows that are direct children of a table should have a row index";
}

TEST(HTMLTableRowElementTest, rowIndex_inUnrelatedElementInTable) {
  auto* document = MakeGarbageCollected<Document>();
  auto* table = MakeGarbageCollected<HTMLTableElement>(*document);
  // Almost any element will do; what's pertinent is that this is not
  // THEAD, TBODY or TFOOT.
  auto* paragraph = MakeGarbageCollected<HTMLParagraphElement>(*document);
  auto* row = MakeGarbageCollected<HTMLTableRowElement>(*document);
  table->AppendChild(paragraph);
  paragraph->AppendChild(row);
  EXPECT_EQ(-1, row->rowIndex())
      << "rows in a table, but within an unrelated element, should have "
      << "row index -1";
}

}  // namespace blink
