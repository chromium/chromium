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
  auto* doc1 = MakeGarbageCollected<Document>();
  auto* doc2 = MakeGarbageCollected<Document>();

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

TEST(TreeScopeAdopterTest, AdoptV1ShadowRootToV0Document) {
  auto* doc1 = MakeGarbageCollected<Document>();
  auto* doc2 = MakeGarbageCollected<Document>();

  Element* html1 = doc1->CreateRawElement(html_names::kHTMLTag);
  doc1->AppendChild(html1);
  Element* div1 = doc1->CreateRawElement(html_names::kDivTag);
  html1->AppendChild(div1);
  EXPECT_EQ(doc1->GetShadowCascadeOrder(),
            ShadowCascadeOrder::kShadowCascadeNone);
  div1->CreateV0ShadowRootForTesting();
  EXPECT_EQ(doc1->GetShadowCascadeOrder(),
            ShadowCascadeOrder::kShadowCascadeV0);
  EXPECT_TRUE(doc1->MayContainV0Shadow());

  Element* html2 = doc2->CreateRawElement(html_names::kHTMLTag);
  doc2->AppendChild(html2);
  Element* div2 = doc1->CreateRawElement(html_names::kDivTag);
  html2->AppendChild(div2);
  div2->AttachShadowRootInternal(ShadowRootType::kOpen);

  EXPECT_EQ(div1->ownerDocument(), doc1);
  EXPECT_EQ(div2->ownerDocument(), doc2);

  TreeScopeAdopter adopter(*div2, *doc1);
  ASSERT_TRUE(adopter.NeedsScopeChange());

  adopter.Execute();
  EXPECT_EQ(div1->ownerDocument(), doc1);
  EXPECT_EQ(div2->ownerDocument(), doc1);
  EXPECT_EQ(doc1->GetShadowCascadeOrder(),
            ShadowCascadeOrder::kShadowCascadeV1);
  EXPECT_TRUE(doc1->MayContainV0Shadow());
  EXPECT_FALSE(doc2->MayContainV0Shadow());
}

TEST(TreeScopeAdopterTest, AdoptV0ShadowRootToV1Document) {
  auto* doc1 = MakeGarbageCollected<Document>();
  auto* doc2 = MakeGarbageCollected<Document>();

  Element* html1 = doc1->CreateRawElement(html_names::kHTMLTag);
  doc1->AppendChild(html1);
  Element* div1 = doc1->CreateRawElement(html_names::kDivTag);
  html1->AppendChild(div1);
  EXPECT_EQ(doc1->GetShadowCascadeOrder(),
            ShadowCascadeOrder::kShadowCascadeNone);
  div1->AttachShadowRootInternal(ShadowRootType::kOpen);
  EXPECT_EQ(doc1->GetShadowCascadeOrder(),
            ShadowCascadeOrder::kShadowCascadeV1);
  EXPECT_FALSE(doc1->MayContainV0Shadow());

  Element* html2 = doc2->CreateRawElement(html_names::kHTMLTag);
  doc2->AppendChild(html2);
  Element* div2 = doc1->CreateRawElement(html_names::kDivTag);
  html2->AppendChild(div2);
  div2->CreateV0ShadowRootForTesting();

  EXPECT_EQ(div1->ownerDocument(), doc1);
  EXPECT_EQ(div2->ownerDocument(), doc2);

  TreeScopeAdopter adopter(*div2, *doc1);
  ASSERT_TRUE(adopter.NeedsScopeChange());

  adopter.Execute();
  EXPECT_EQ(div1->ownerDocument(), doc1);
  EXPECT_EQ(div2->ownerDocument(), doc1);
  EXPECT_EQ(doc1->GetShadowCascadeOrder(),
            ShadowCascadeOrder::kShadowCascadeV1);
  EXPECT_TRUE(doc1->MayContainV0Shadow());
  EXPECT_TRUE(doc2->MayContainV0Shadow());
}

TEST(TreeScopeAdopterTest, AdoptV0InV1ToNewDocument) {
  auto* old_doc = MakeGarbageCollected<Document>();
  Element* html = old_doc->CreateRawElement(html_names::kHTMLTag);
  old_doc->AppendChild(html);
  Element* host1 = old_doc->CreateRawElement(html_names::kDivTag);
  html->AppendChild(host1);
  ShadowRoot& shadow_root_v1 =
      host1->AttachShadowRootInternal(ShadowRootType::kOpen);
  Element* host2 = old_doc->CreateRawElement(html_names::kDivTag);
  shadow_root_v1.AppendChild(host2);
  host2->CreateV0ShadowRootForTesting();

  // old_doc
  // └── html
  //     └── host1
  //         └──/shadow-root-v1
  //             └── host2
  //                 └──/shadow-root-v0
  EXPECT_TRUE(old_doc->MayContainV0Shadow());

  auto* new_doc = MakeGarbageCollected<Document>();
  EXPECT_FALSE(new_doc->MayContainV0Shadow());

  TreeScopeAdopter adopter(*host1, *new_doc);
  ASSERT_TRUE(adopter.NeedsScopeChange());
  adopter.Execute();

  EXPECT_TRUE(old_doc->MayContainV0Shadow());
  EXPECT_TRUE(new_doc->MayContainV0Shadow());
}

}  // namespace blink
