// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FRAGMENTED_OOF_CONTAINER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FRAGMENTED_OOF_CONTAINER_H_

#include "base/containers/span.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/oof_positioned_node.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// A container of out-of-flow positioned elements, or a container in the
// containing block chain of such a container.
//
// This forms a tree, starting from the outermost fragmentation context root,
// with paths down to each OOF that participates in any of the fragmentation
// contexts inside.
//
// Elements are discovered where they occur naturally in the DOM tree. This also
// applies to out-of-flow positioned elements, which means that they may be
// discovered inside a later fragmentainer than where they actually start
// (dictated by block insets). This means that we have to go back and relayout
// from the outermost fragmentation context when we have found all the OOFs.
//
// A FragmentedOofContainer tree is built after layout of the outermost
// fragmentation context root, and is passed as input to relayout.
// FragmentedOofContainer manages discovery of any OOFs that participate in
// fragmentation, bypassing the natural discovery mechanism that normally takes
// place during child layout and propagation.
class FragmentedOofContainer : public GarbageCollected<FragmentedOofContainer> {
 public:
  explicit FragmentedOofContainer(LayoutUnit block_size)
      : block_size_(block_size) {}
  void Trace(Visitor*) const;

  // The block-size of the containing block established by this container in the
  // previous layout pass.
  LayoutUnit BlockSize() const { return block_size_; }

  base::span<const LogicalOofPositionedNode> OofPositionedNodes() const {
    return base::span(oof_nodes_);
  }
  bool HasOofPositionedNodes() const { return !oof_nodes_.empty(); }

  const FragmentedOofContainer* GetChild(const LayoutBox& box) const {
    auto iterator = children_.find(&box);
    if (iterator == children_.end()) {
      return nullptr;
    }
    return iterator->value;
  }

  FragmentedOofContainer& EnsureChild(const LayoutBox& box,
                                      LayoutUnit stitched_block_size) {
    auto iterator = children_.find(&box);
    if (iterator != children_.end()) {
      return *iterator->value;
    }
    auto* new_child =
        MakeGarbageCollected<FragmentedOofContainer>(stitched_block_size);
    children_.insert(&box, new_child);
    return *new_child;
  }

  HeapHashMap<Member<const LayoutBox>, Member<FragmentedOofContainer>>
  Children() {
    return children_;
  }

  void AddOofPositionedNode(const LogicalOofPositionedNode& oof_node) {
#if EXPENSIVE_DCHECKS_ARE_ON()
    // Duplicate entries not allowed.
    for (const LogicalOofPositionedNode& walker : oof_nodes_) {
      DCHECK(walker.Node() != oof_node.Node());
    }
#endif
    oof_nodes_.push_back(oof_node);
  }

#ifndef NDEBUG
  String DumpTree() const;
  void ShowTree() const;
#endif

 private:
#ifndef NDEBUG
  void DumpRecursive(const LayoutBox*, int indentation, StringBuilder*) const;
#endif

  // Child containers that have OOFs inside (either directly, or inside a
  // descendant).
  HeapHashMap<Member<const LayoutBox>, Member<FragmentedOofContainer>>
      children_;

  // All OOFs for with this container (or non-atomic inlines inside) is the
  // containing block.
  HeapVector<LogicalOofPositionedNode> oof_nodes_;

  // The block-size of the containing block established by this container in the
  // previous layout pass.
  LayoutUnit block_size_;
};

}  // namespace blink

#ifndef NDEBUG
void ShowFragmentedOofContainerTree(const blink::FragmentedOofContainer*);
#endif

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FRAGMENTED_OOF_CONTAINER_H_
