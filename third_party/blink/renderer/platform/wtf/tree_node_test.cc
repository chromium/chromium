/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/wtf/tree_node.h"

#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace WTF {

class TestTree : public RefCounted<TestTree>, public TreeNode<TestTree> {
 public:
  static scoped_refptr<TestTree> Create() {
    return base::AdoptRef(new TestTree());
  }
};

TEST(TreeNodeTest, AppendChild) {
  scoped_refptr<TestTree> root = TestTree::Create();
  scoped_refptr<TestTree> first_child = TestTree::Create();
  scoped_refptr<TestTree> last_child = TestTree::Create();

  root->AppendChild(first_child.get());
  EXPECT_EQ(root->FirstChild(), first_child.get());
  EXPECT_EQ(root->LastChild(), first_child.get());
  EXPECT_EQ(first_child->Parent(), root.get());

  root->AppendChild(last_child.get());
  EXPECT_EQ(root->FirstChild(), first_child.get());
  EXPECT_EQ(root->LastChild(), last_child.get());
  EXPECT_EQ(last_child->Previous(), first_child.get());
  EXPECT_EQ(first_child->Next(), last_child.get());
  EXPECT_EQ(last_child->Parent(), root.get());
}

TEST(TreeNodeTest, InsertBefore) {
  scoped_refptr<TestTree> root = TestTree::Create();
  scoped_refptr<TestTree> first_child = TestTree::Create();
  scoped_refptr<TestTree> middle_child = TestTree::Create();
  scoped_refptr<TestTree> last_child = TestTree::Create();

  // Inserting single node
  root->InsertBefore(last_child.get(), nullptr);
  EXPECT_EQ(last_child->Parent(), root.get());
  EXPECT_EQ(root->FirstChild(), last_child.get());
  EXPECT_EQ(root->LastChild(), last_child.get());

  // Then prepend
  root->InsertBefore(first_child.get(), last_child.get());
  EXPECT_EQ(first_child->Parent(), root.get());
  EXPECT_EQ(root->FirstChild(), first_child.get());
  EXPECT_EQ(root->LastChild(), last_child.get());
  EXPECT_EQ(first_child->Next(), last_child.get());
  EXPECT_EQ(first_child.get(), last_child->Previous());

  // Inserting in the middle
  root->InsertBefore(middle_child.get(), last_child.get());
  EXPECT_EQ(middle_child->Parent(), root.get());
  EXPECT_EQ(root->FirstChild(), first_child.get());
  EXPECT_EQ(root->LastChild(), last_child.get());
  EXPECT_EQ(middle_child->Previous(), first_child.get());
  EXPECT_EQ(middle_child->Next(), last_child.get());
  EXPECT_EQ(first_child->Next(), middle_child.get());
  EXPECT_EQ(last_child->Previous(), middle_child.get());
}

TEST(TreeNodeTest, RemoveSingle) {
  scoped_refptr<TestTree> root = TestTree::Create();
  scoped_refptr<TestTree> child = TestTree::Create();
  scoped_refptr<TestTree> null_node;

  root->AppendChild(child.get());
  root->RemoveChild(child.get());
  EXPECT_EQ(child->Next(), null_node.get());
  EXPECT_EQ(child->Previous(), null_node.get());
  EXPECT_EQ(child->Parent(), null_node.get());
  EXPECT_EQ(root->FirstChild(), null_node.get());
  EXPECT_EQ(root->LastChild(), null_node.get());
}

class Trio {
  STACK_ALLOCATED();

 public:
  Trio()
      : root(TestTree::Create()),
        first_child(TestTree::Create()),
        middle_child(TestTree::Create()),
        last_child(TestTree::Create()) {}

  void AppendChildren() {
    root->AppendChild(first_child.get());
    root->AppendChild(middle_child.get());
    root->AppendChild(last_child.get());
  }

  scoped_refptr<TestTree> root;
  scoped_refptr<TestTree> first_child;
  scoped_refptr<TestTree> middle_child;
  scoped_refptr<TestTree> last_child;
};

TEST(TreeNodeTest, RemoveMiddle) {
  Trio trio;
  trio.AppendChildren();

  trio.root->RemoveChild(trio.middle_child.get());
  EXPECT_TRUE(trio.middle_child->Orphan());
  EXPECT_EQ(trio.first_child->Next(), trio.last_child.get());
  EXPECT_EQ(trio.last_child->Previous(), trio.first_child.get());
  EXPECT_EQ(trio.root->FirstChild(), trio.first_child.get());
  EXPECT_EQ(trio.root->LastChild(), trio.last_child.get());
}

TEST(TreeNodeTest, RemoveLast) {
  scoped_refptr<TestTree> null_node;
  Trio trio;
  trio.AppendChildren();

  trio.root->RemoveChild(trio.last_child.get());
  EXPECT_TRUE(trio.last_child->Orphan());
  EXPECT_EQ(trio.middle_child->Next(), null_node.get());
  EXPECT_EQ(trio.root->FirstChild(), trio.first_child.get());
  EXPECT_EQ(trio.root->LastChild(), trio.middle_child.get());
}

TEST(TreeNodeTest, RemoveFirst) {
  scoped_refptr<TestTree> null_node;
  Trio trio;
  trio.AppendChildren();

  trio.root->RemoveChild(trio.first_child.get());
  EXPECT_TRUE(trio.first_child->Orphan());
  EXPECT_EQ(trio.middle_child->Previous(), null_node.get());
  EXPECT_EQ(trio.root->FirstChild(), trio.middle_child.get());
  EXPECT_EQ(trio.root->LastChild(), trio.last_child.get());
}

TEST(TreeNodeTest, TakeChildrenFrom) {
  scoped_refptr<TestTree> new_parent = TestTree::Create();
  Trio trio;
  trio.AppendChildren();

  new_parent->TakeChildrenFrom(trio.root.get());

  EXPECT_FALSE(trio.root->HasChildren());
  EXPECT_TRUE(new_parent->HasChildren());
  EXPECT_EQ(trio.first_child.get(), new_parent->FirstChild());
  EXPECT_EQ(trio.middle_child.get(), new_parent->FirstChild()->Next());
  EXPECT_EQ(trio.last_child.get(), new_parent->LastChild());
}

class TrioWithGrandChild : public Trio {
 public:
  TrioWithGrandChild() : grand_child(TestTree::Create()) {}

  void AppendChildren() {
    Trio::AppendChildren();
    middle_child->AppendChild(grand_child.get());
  }

  scoped_refptr<TestTree> grand_child;
};

TEST(TreeNodeTest, TraverseNext) {
  TrioWithGrandChild trio;
  trio.AppendChildren();

  TestTree* order[] = {trio.root.get(), trio.first_child.get(),
                       trio.middle_child.get(), trio.grand_child.get(),
                       trio.last_child.get()};

  unsigned order_index = 0;
  for (TestTree *node = trio.root.get(); node;
       node = TraverseNext(node), order_index++)
    EXPECT_EQ(node, order[order_index]);
  EXPECT_EQ(order_index, sizeof(order) / sizeof(TestTree*));
}

TEST(TreeNodeTest, TraverseNextPostORder) {
  TrioWithGrandChild trio;
  trio.AppendChildren();

  TestTree* order[] = {trio.first_child.get(), trio.grand_child.get(),
                       trio.middle_child.get(), trio.last_child.get(),
                       trio.root.get()};

  unsigned order_index = 0;
  for (TestTree *node = TraverseFirstPostOrder(trio.root.get()); node;
       node = TraverseNextPostOrder(node), order_index++)
    EXPECT_EQ(node, order[order_index]);
  EXPECT_EQ(order_index, sizeof(order) / sizeof(TestTree*));
}

}  // namespace WTF
