// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_traversal_root.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class StyleTraversalRootTestImpl : public StyleTraversalRoot {
  STACK_ALLOCATED();

 public:
  StyleTraversalRootTestImpl() = default;
  void MarkDirty(const Node* node) {
    DCHECK(node);
    dirty_nodes_.insert(node);
#if DCHECK_IS_ON()
    for (const Element* element = node->parentElement(); element;
         element = element->parentElement()) {
      child_dirty_nodes_.insert(element);
    }
#endif
  }
  bool IsSingleRoot() const { return root_type_ == RootType::kSingleRoot; }
  bool IsCommonRoot() const { return root_type_ == RootType::kCommonRoot; }

  void SubtreeModified(ContainerNode& parent) override {
    if (!GetRootNode() || GetRootNode()->isConnected()) {
      return;
    }
    Clear();
  }

 private:
  virtual ContainerNode* ParentInternal(const Node& node) const {
    return node.parentNode();
  }
#if DCHECK_IS_ON()
  ContainerNode* Parent(const Node& node) const override {
    return ParentInternal(node);
  }
  bool IsChildDirty(const Node& node) const override {
    return child_dirty_nodes_.Contains(&node);
  }
#endif  // DCHECK_IS_ON()
  bool IsDirty(const Node& node) const final {
    return dirty_nodes_.Contains(&node);
  }

  HeapHashSet<Member<const Node>> dirty_nodes_;
#if DCHECK_IS_ON()
  HeapHashSet<Member<const Node>> child_dirty_nodes_;
#endif
};

class StyleTraversalRootTest : public testing::Test {
 protected:
  enum ElementIndex { kA, kB, kC, kD, kE, kF, kG, kElementCount };
  void SetUp() final {
    document_ =
        Document::CreateForTest(execution_context_.GetExecutionContext());
    elements_ = MakeGarbageCollected<HeapVector<Member<Element>, 7>>();
    for (size_t i = 0; i < kElementCount; i++) {
      elements_->push_back(GetDocument().CreateRawElement(html_names::kDivTag));
    }
    GetDocument().appendChild(DivElement(kA));
    DivElement(kA)->appendChild(DivElement(kB));
    DivElement(kA)->appendChild(DivElement(kC));
    DivElement(kB)->appendChild(DivElement(kD));
    DivElement(kB)->appendChild(DivElement(kE));
    DivElement(kC)->appendChild(DivElement(kF));
    DivElement(kC)->appendChild(DivElement(kG));

    // Tree Looks like this:
    // div#a
    // |-- div#b
    // |   |-- div#d
    // |   `-- div#e
    // `-- div#c
    //     |-- div#f
    //     `-- div#g
  }
  Document& GetDocument() { return *document_; }
  Element* DivElement(ElementIndex index) { return elements_->at(index).Get(); }

 private:
  test::TaskEnvironment task_environment_;
  ScopedNullExecutionContext execution_context_;
  Persistent<Document> document_;
  Persistent<HeapVector<Member<Element>, 7>> elements_;
};

TEST_F(StyleTraversalRootTest, Update_SingleRoot) {
  StyleTraversalRootTestImpl root;
  root.MarkDirty(DivElement(kA));

  // A single dirty node becomes a single root.
  root.Update(nullptr, DivElement(kA));
  EXPECT_EQ(DivElement(kA), root.GetRootNode());
  EXPECT_TRUE(root.IsSingleRoot());
}

TEST_F(StyleTraversalRootTest, Update_CommonRoot) {
  StyleTraversalRootTestImpl root;
  root.MarkDirty(DivElement(kB));

  // Initially make B a single root.
  root.Update(nullptr, DivElement(kB));
  EXPECT_EQ(DivElement(kB), root.GetRootNode());
  EXPECT_TRUE(root.IsSingleRoot());

  // Adding C makes A a common root.
  root.MarkDirty(DivElement(kC));
  root.Update(DivElement(kA), DivElement(kC));
  EXPECT_EQ(DivElement(kA), root.GetRootNode());
  EXPECT_FALSE(root.IsSingleRoot());
  EXPECT_TRUE(root.IsCommonRoot());
}

TEST_F(StyleTraversalRootTest, Update_CommonRootDirtySubtree) {
  StyleTraversalRootTestImpl root;
  root.MarkDirty(DivElement(kA));
  root.Update(nullptr, DivElement(kA));

  // Marking descendants of a single dirty root makes the single root a common
  // root as long as the new common ancestor is the current root.
  root.MarkDirty(DivElement(kD));
  root.Update(DivElement(kA), DivElement(kD));
  EXPECT_EQ(DivElement(kA), root.GetRootNode());
  EXPECT_FALSE(root.IsSingleRoot());
  EXPECT_TRUE(root.IsCommonRoot());
}

TEST_F(StyleTraversalRootTest, Update_CommonRootDocumentFallback) {
  StyleTraversalRootTestImpl root;

  // Initially make B a common root for D and E.
  root.MarkDirty(DivElement(kD));
  root.Update(nullptr, DivElement(kD));
  root.MarkDirty(DivElement(kE));
  root.Update(DivElement(kB), DivElement(kE));
  EXPECT_EQ(DivElement(kB), root.GetRootNode());
  EXPECT_FALSE(root.IsSingleRoot());
  EXPECT_TRUE(root.IsCommonRoot());

  // Adding C falls back to using the document as the root because we don't know
  // if A is above or below the current common root B.
  root.MarkDirty(DivElement(kC));
  root.Update(DivElement(kA), DivElement(kC));
  EXPECT_EQ(&GetDocument(), root.GetRootNode());
  EXPECT_FALSE(root.IsSingleRoot());
  EXPECT_TRUE(root.IsCommonRoot());
}

TEST_F(StyleTraversalRootTest, SubtreeModified) {
  StyleTraversalRootTestImpl root;
  // Initially make E a single root.
  root.MarkDirty(DivElement(kE));
  root.Update(nullptr, DivElement(kE));
  EXPECT_EQ(DivElement(kE), root.GetRootNode());
  EXPECT_TRUE(root.IsSingleRoot());

  // Removing D not affecting E.
  DivElement(kD)->remove();
  root.SubtreeModified(*DivElement(kB));
  EXPECT_EQ(DivElement(kE), root.GetRootNode());
  EXPECT_TRUE(root.IsSingleRoot());

  // Removing B
  DivElement(kB)->remove();
  root.SubtreeModified(*DivElement(kA));
  EXPECT_FALSE(root.GetRootNode());
  EXPECT_TRUE(root.IsSingleRoot());
}

class StyleTraversalRootFlatTreeTestImpl : public StyleTraversalRootTestImpl {
 private:
  ContainerNode* ParentInternal(const Node& node) const final {
    // Flat tree does not include Document or ShadowRoot.
    return FlatTreeTraversal::ParentElement(node);
  }
};

TEST_F(StyleTraversalRootTest, Update_CommonRoot_FlatTree) {
  StyleTraversalRootFlatTreeTestImpl root;

  // The single dirty node D becomes a single root.
  root.MarkDirty(DivElement(kD));
  root.Update(nullptr, DivElement(kD));

  EXPECT_EQ(DivElement(kD), root.GetRootNode());
  EXPECT_TRUE(root.IsSingleRoot());

  // A becomes a common root.
  root.MarkDirty(DivElement(kA));
  root.Update(nullptr, DivElement(kA));

  EXPECT_EQ(DivElement(kA), root.GetRootNode());
  EXPECT_TRUE(root.IsCommonRoot());

  // Making E dirty and the document becomes the common root.
  root.MarkDirty(DivElement(kE));
  root.Update(DivElement(kB), DivElement(kE));

  EXPECT_EQ(&GetDocument(), root.GetRootNode());
  EXPECT_TRUE(root.IsCommonRoot());
}

}  // namespace blink
