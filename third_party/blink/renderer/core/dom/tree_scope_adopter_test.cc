// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/tree_scope_adopter.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

// TODO(hayato): It's hard to see what's happening in these tests.
// It would be better to refactor these tests.
TEST(TreeScopeAdopterTest, SimpleMove) {
  auto* doc1 = Document::CreateForTest();
  auto* doc2 = Document::CreateForTest();

  Element* html1 = doc1->CreateRawElement(html_names::kHTMLTag);
  doc1->AppendChild(html1);
  Element* div1 = doc1->CreateRawElement(html_names::kDivTag);
  html1->AppendChild(div1);

  Element* html2 = doc2->CreateRawElement(html_names::kHTMLTag);
  doc2->AppendChild(html2);
  Element* div2 = doc1->CreateRawElement(html_names::kDivTag);
  html2->AppendChild(div2);

  EXPECT_EQ(div1->ownerDocument(), doc1);
  EXPECT_EQ(div2->ownerDocument(), doc2);

  TreeScopeAdopter adopter1(*div1, *doc1);
  EXPECT_FALSE(adopter1.NeedsScopeChange());

  TreeScopeAdopter adopter2(*div2, *doc1);
  ASSERT_TRUE(adopter2.NeedsScopeChange());

  adopter2.Execute();
  EXPECT_EQ(div1->ownerDocument(), doc1);
  EXPECT_EQ(div2->ownerDocument(), doc1);
}


}  // namespace blink
