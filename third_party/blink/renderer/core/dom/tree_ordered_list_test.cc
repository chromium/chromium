// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/tree_ordered_list.h"

#include "testing/gtest/include/gtest/gtest.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class TreeOrderedListTest : public PageTestBase {
 public:
  TreeOrderedListTest() = default;
};

TEST_F(TreeOrderedListTest, Basic) {
  SetBodyInnerHTML(
      "<div id='a'></div><div id='b'></div><div id='c'></div><div "
      "id='d'></div>");

  Element* body = GetDocument().body();
  Element* a = body->QuerySelector(AtomicString("#a"));

  TreeOrderedList list;

  EXPECT_TRUE(list.IsEmpty());
  list.Add(a);
  EXPECT_EQ(a, *list.begin());
  list.Remove(a);
  EXPECT_TRUE(list.IsEmpty());
}

TEST_F(TreeOrderedListTest, DuplicateKeys) {
  SetBodyInnerHTML(
      "<div id='a'></div><div id='b'></div><div id='c'></div><div "
      "id='d'></div>");

  Element* body = GetDocument().body();
  Element* a = body->QuerySelector(AtomicString("#a"));
  Element* b = body->QuerySelector(AtomicString("#b"));
  Element* c = body->QuerySelector(AtomicString("#c"));

  TreeOrderedList list;

  list.Add(a);
  list.Add(c);
  list.Add(c);
  list.Add(b);
  EXPECT_EQ(list.size(), 3u);
  list.Clear();
  EXPECT_TRUE(list.IsEmpty());
}

TEST_F(TreeOrderedListTest, SortedByDocumentPosition) {
  SetBodyInnerHTML(
      "<div id='a'></div><div id='b'></div><div id='c'></div><div "
      "id='d'></div>");

  Element* body = GetDocument().body();
  Element* a = body->QuerySelector(AtomicString("#a"));
  Element* b = body->QuerySelector(AtomicString("#b"));
  Element* c = body->QuerySelector(AtomicString("#c"));
  Element* d = body->QuerySelector(AtomicString("#d"));

  TreeOrderedList list;

  list.Add(a);
  list.Add(d);
  list.Add(c);
  list.Add(b);
  TreeOrderedList::iterator it = list.begin();
  EXPECT_EQ(a, *it);
  EXPECT_EQ(b, *++it);
  EXPECT_EQ(c, *++it);
  EXPECT_EQ(d, *++it);
  EXPECT_EQ(++it, list.end());
}

}  // namespace blink
