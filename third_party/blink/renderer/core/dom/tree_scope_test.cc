// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/tree_scope.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class TreeScopeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    document_ =
        Document::CreateForTest(execution_context_.GetExecutionContext());
    Element* html = document_->CreateRawElement(html_names::kHTMLTag);
    document_->AppendChild(html);
    body_ = document_->CreateRawElement(html_names::kBodyTag);
    html->AppendChild(body_);
  }
  Document* GetDocument() { return document_; }
  Element* GetBody() { return body_; }
  ExecutionContext& GetExecutionContext() {
    return execution_context_.GetExecutionContext();
  }

 private:
  test::TaskEnvironment task_environment_;
  ScopedNullExecutionContext execution_context_;
  Persistent<Document> document_;
  Persistent<Element> body_;
};

TEST_F(TreeScopeTest, CommonAncestorOfSameTrees) {
  EXPECT_EQ(GetDocument(),
            GetDocument()->CommonAncestorTreeScope(*GetDocument()));
  ShadowRoot& shadow_root =
      GetBody()->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  EXPECT_EQ(shadow_root, shadow_root.CommonAncestorTreeScope(shadow_root));
}

TEST_F(TreeScopeTest, CommonAncestorOfInclusiveTrees) {
  //  document
  //     |      : Common ancestor is document.
  // shadowRoot

  ShadowRoot& shadow_root =
      GetBody()->AttachShadowRootForTesting(ShadowRootMode::kOpen);

  EXPECT_EQ(GetDocument(), GetDocument()->CommonAncestorTreeScope(shadow_root));
  EXPECT_EQ(GetDocument(), shadow_root.CommonAncestorTreeScope(*GetDocument()));
}

TEST_F(TreeScopeTest, CommonAncestorOfSiblingTrees) {
  //  document
  //   /    \  : Common ancestor is document.
  //  A      B

  Element* div_a = GetDocument()->CreateRawElement(html_names::kDivTag);
  GetBody()->AppendChild(div_a);
  Element* div_b = GetDocument()->CreateRawElement(html_names::kDivTag);
  GetBody()->AppendChild(div_b);

  ShadowRoot& shadow_root_a =
      div_a->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  ShadowRoot& shadow_root_b =
      div_b->AttachShadowRootForTesting(ShadowRootMode::kOpen);

  EXPECT_EQ(GetDocument(),
            shadow_root_a.CommonAncestorTreeScope(shadow_root_b));
  EXPECT_EQ(GetDocument(),
            shadow_root_b.CommonAncestorTreeScope(shadow_root_a));
}

TEST_F(TreeScopeTest, CommonAncestorOfTreesAtDifferentDepths) {
  //  document
  //    / \    : Common ancestor is document.
  //   Y   B
  //  /
  // A

  Element* div_y = GetDocument()->CreateRawElement(html_names::kDivTag);
  GetBody()->AppendChild(div_y);
  Element* div_b = GetDocument()->CreateRawElement(html_names::kDivTag);
  GetBody()->AppendChild(div_b);

  ShadowRoot& shadow_root_y =
      div_y->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  ShadowRoot& shadow_root_b =
      div_b->AttachShadowRootForTesting(ShadowRootMode::kOpen);

  Element* div_in_y = GetDocument()->CreateRawElement(html_names::kDivTag);
  shadow_root_y.AppendChild(div_in_y);
  ShadowRoot& shadow_root_a =
      div_in_y->AttachShadowRootForTesting(ShadowRootMode::kOpen);

  EXPECT_EQ(GetDocument(),
            shadow_root_a.CommonAncestorTreeScope(shadow_root_b));
  EXPECT_EQ(GetDocument(),
            shadow_root_b.CommonAncestorTreeScope(shadow_root_a));
}

TEST_F(TreeScopeTest, CommonAncestorOfTreesInDifferentDocuments) {
  auto* document2 = Document::CreateForTest(GetExecutionContext());
  EXPECT_EQ(nullptr, GetDocument()->CommonAncestorTreeScope(*document2));
  EXPECT_EQ(nullptr, document2->CommonAncestorTreeScope(*GetDocument()));
}

}  // namespace blink
