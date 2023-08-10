// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/part_root.h"

#include "base/containers/contains.h"
#include "third_party/blink/renderer/core/dom/child_node_part.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_part_root.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node_move_scope.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/part.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

void PartRoot::Trace(Visitor* visitor) const {
  visitor->Trace(cached_ordered_parts_);
}

void PartRoot::AddPart(Part& new_part) {
  if (cached_parts_list_dirty_) {
    return;
  }
  if (NodeMoveScope::AllMovedPartsWereClean()) {
    DCHECK(!base::Contains(cached_ordered_parts_, &new_part));
    if (NodeMoveScope::IsPrepend()) {
      cached_ordered_parts_.push_front(&new_part);
    } else {
      cached_ordered_parts_.push_back(&new_part);
    }
  } else {
    cached_ordered_parts_.clear();
    cached_parts_list_dirty_ = true;
  }
}

// If we're removing the first Part in the cached part list, then just remove
// that Part and keep the parts list clean. Otherwise mark it dirty and clear
// the cached list.
// TODO(crbug.com/1453291) The above case happens when we're moving the entire
// tree that contains Parts, or the *first* part of the tree that contains
// Parts. If we're moving the *last* part of the tree, it would be possible
// to detect that situation and remove parts from the end of the parts list.
// The tricky bit there is that we need to know that we're
// doing that, and we only know it's true when we get to the last removal
// and we've removed the entire end of the list of parts.
void PartRoot::RemovePart(Part& part) {
  if (cached_parts_list_dirty_) {
    return;
  }
  DCHECK(!cached_ordered_parts_.empty());
  if (NodeMoveScope::InScope() && cached_ordered_parts_.front() == &part) {
    cached_ordered_parts_.pop_front();
  } else {
    cached_ordered_parts_.clear();
    cached_parts_list_dirty_ = true;
  }
}

// |getParts| must always return the contained parts list subject to these
// rules:
//  1. parts are returned in DOM tree order. If more than one part refers to the
//     same Node, parts are returned in the order they were constructed.
//  2. parts referring to nodes that aren't in a document, not in the same
//     document as the owning DocumentPartRoot, or not contained by the root
//     Element of the DocumentPartRoot are not returned.
//  3. parts referring to invalid parts are not returned. For example, a
//     ChildNodePart whose previous_node comes after its next_node.
// To rebuild the parts list, we simply traverse the entire tree under the
// PartRoot (from FirstIncludedChildNode to LastIncludedChildNode), and collect
// any Parts we find. If we find a ChildNodePart (or other PartRoot), we ignore
// Parts until we exit the Partroot.
// TODO(crbug.com/1453291) Future optimization: just skip the Node
// traversal directly to the LastIncludedChildNode and avoid looping through
// the descendants.)
HeapDeque<Member<Part>>& PartRoot::RebuildPartsList() {
  DCHECK(cached_parts_list_dirty_);

  auto& ordered_parts = *MakeGarbageCollected<HeapDeque<Member<Part>>>();

  // Then traverse the tree under the root container and add parts in the order
  // they're found in the tree, and for the same Node, in the order they were
  // constructed.
  Node* child = FirstIncludedChildNode();
  Node* last_child = LastIncludedChildNode();
  if (!child || !last_child) {
    return ordered_parts;
  }
  bool done = false;
  Part* inside_sub_root = nullptr;
  while (!done) {
    for (auto& descendant : NodeTraversal::InclusiveDescendantsOf(*child)) {
      if (auto* parts = descendant.GetDOMParts()) {
        for (Part* part : *parts) {
          PartRoot* part_root = part->GetAsPartRoot();
          if (part_root == this) {
            // Skip the PartRoot itself.
            continue;
          }
          if (inside_sub_root) {
            if (inside_sub_root == part) {
              // We just exited the other side of the ChildNodePart.
              DCHECK_EQ(part_root->LastIncludedChildNode(), &descendant);
              inside_sub_root = nullptr;
            }
            continue;
          }
          if (part->NodeToSortBy() != descendant) {
            continue;
          }
          if (!part->IsValid()) {
            continue;
          }
          DCHECK(!base::Contains(ordered_parts, part));
          ordered_parts.push_back(part);
          if (!inside_sub_root && part_root) {
            // We just entered a PartRoot - ignore further parts until we
            // traverse the end node of this PartRoot.
            inside_sub_root = part;
          }
        }
      }
    }
    done = child == last_child;
    child = child->nextSibling();
  }
  return ordered_parts;
}

HeapVector<Member<Part>>& PartRoot::getParts() {
  if (cached_parts_list_dirty_) {
    cached_ordered_parts_ = RebuildPartsList();
    cached_parts_list_dirty_ = false;
  }
  // TODO(crbug.com/1453291) It would help to be able to directly return
  // HeapDeque for sequences-valued IDL return values. Add an overload of
  // bindings::ToV8HelperSequence(). In profiles, this call takes significant
  // time.
  auto& returned_vector = *MakeGarbageCollected<HeapVector<Member<Part>>>();
  for (auto part : cached_ordered_parts_) {
    returned_vector.push_back(part);
  }
  return returned_vector;
}

// static
PartRoot* PartRoot::GetPartRootFromUnion(PartRootUnion* root_union) {
  if (root_union->IsChildNodePart()) {
    return root_union->GetAsChildNodePart();
  }
  CHECK(root_union->IsDocumentPartRoot());
  return root_union->GetAsDocumentPartRoot();
}

// static
PartRootUnion* PartRoot::GetUnionFromPartRoot(PartRoot* root) {
  if (!root) {
    return nullptr;
  }
  if (root->IsDocumentPartRoot()) {
    return MakeGarbageCollected<PartRootUnion>(
        static_cast<DocumentPartRoot*>(root));
  }
  return MakeGarbageCollected<PartRootUnion>(static_cast<ChildNodePart*>(root));
}

}  // namespace blink
