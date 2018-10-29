// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_TRAVERSAL_ROOT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_TRAVERSAL_ROOT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/container_node.h"

namespace blink {

// Class used to represent a common ancestor for all dirty nodes in a DOM tree.
// Subclasses implement the various types of dirtiness for style recalc, style
// invalidation, and layout tree rebuild. The common ancestor is used as a
// starting point for traversal to avoid unnecessary DOM tree traversal.
//
// The first dirty node is stored as a single root. When a second node is
// added with a common child-dirty ancestor which is not dirty, we store that
// as a common root. Any subsequent dirty nodes added whose closest child-dirty
// ancestor is not itself dirty, or is the current root, will cause us to fall
// back to use the document as the root node. In order to find a lowest common
// ancestor we would have had to traverse up the ancestor chain to see if we are
// below the current common root or not.
//
// Note that when the common ancestor candidate passed into Update is itself
// dirty, we know that we are currently below the current root node and don't
// have to modify it.

class CORE_EXPORT StyleTraversalRoot {
  DISALLOW_NEW();

 public:
  // Update the common ancestor root when dirty_node is marked dirty. The
  // common_ancestor is the closest ancestor of dirty_node which was already
  // marked as having dirty children.
  void Update(ContainerNode* common_ancestor, Node* dirty_node);

  // Clear the root if the removal caused the current root_node_ to be
  // disconnected.
  void ChildrenRemoved(ContainerNode& parent);

  Node* GetRootNode() const { return root_node_; }
  void Clear() {
    root_node_ = nullptr;
    root_type_ = RootType::kSingleRoot;
  }

  void Trace(blink::Visitor* visitor) { visitor->Trace(root_node_); }

 protected:
  virtual ~StyleTraversalRoot() = default;

#if DCHECK_IS_ON()
  // Return the parent node for type of traversal for which the implementation
  // is a root.
  virtual ContainerNode* Parent(const Node&) const = 0;

  // Return true if the given node has dirty descendants.
  virtual bool IsChildDirty(const ContainerNode&) const = 0;
#endif  // DCHECK_IS_ON()

  // Return true if the given node is dirty.
  virtual bool IsDirty(const Node&) const = 0;

  // Clear the child-dirty flag on the ancestors of the given node.
  virtual void ClearChildDirtyForAncestors(ContainerNode& parent) const = 0;

  bool IsSingleRoot() const { return root_type_ == RootType::kSingleRoot; }

 private:
  friend class StyleTraversalRootTestImpl;

  // The current root for dirty nodes.
  Member<Node> root_node_;

  // Is the current root a common ancestor or a single dirty node.
  enum class RootType { kSingleRoot, kCommonRoot };
  RootType root_type_ = RootType::kSingleRoot;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_TRAVERSAL_ROOT_H_
