// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/part_root.h"

#include "base/containers/contains.h"
#include "third_party/blink/renderer/core/dom/child_node_list.h"
#include "third_party/blink/renderer/core/dom/child_node_part.h"
#include "third_party/blink/renderer/core/dom/comment.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_part_root.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node_cloning_data.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/part.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

void PartRoot::Trace(Visitor* visitor) const {
  visitor->Trace(cached_ordered_parts_);
}

void PartRoot::AddPart(Part& new_part) {
  DCHECK(!RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled());
  if (cached_parts_list_dirty_) {
    return;
  }
  DCHECK(!base::Contains(cached_ordered_parts_, &new_part));
  cached_ordered_parts_.push_back(&new_part);
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
// TODO(crbug.com/1453291) The comment for this function should get updated
// if we get rid of part tracking.
void PartRoot::RemovePart(Part& part) {
  DCHECK(!RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled());
  if (cached_parts_list_dirty_) {
    return;
  }
  // TODO(crbug.com/1453291) If we go back to tracking parts, we can pop_front
  // this part if it's in the front.
  cached_parts_list_dirty_ = true;
}

// static
void PartRoot::CloneParts(const Node& source_node,
                          Node& destination_node,
                          NodeCloningData& data) {
  DCHECK(RuntimeEnabledFeatures::DOMPartsAPIEnabled());
  DCHECK(!RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled());
  DCHECK(data.Has(CloneOption::kPreserveDOMParts));
  DCHECK(!data.Has(CloneOption::kPreserveDOMPartsMinimalAPI));
  if (auto* parts = source_node.GetDOMParts()) {
    for (Part* part : *parts) {
      if (!part->IsValid()) {
        // Only valid parts get cloned. This avoids issues with nesting
        // of invalid parts affecting the part root stack.
        continue;
      }
      if (part->NodeToSortBy() == source_node) {
        // This can be a NodePart or the previousSibling of a ChildNodePart.
        // If this is a ChildNodePart, this will push the new part onto the
        // part root stack.
        part->ClonePart(data, destination_node);
        continue;
      }
      // This should *only* be the nextSibling of a ChildNodePart.
      CHECK(part->GetAsPartRoot()) << "Should be a ChildNodePart";
      DCHECK_EQ(static_cast<ChildNodePart*>(part)->nextSibling(), source_node)
          << "This should be the next sibling node";
      if (data.PartRootStackHasOnlyDocumentRoot()) {
        // If there have been mis-nested parts, abort.
        continue;
      }
      // The top of the part root stack should be the appropriate part.
      ChildNodePart& child_node_part =
          static_cast<ChildNodePart&>(data.CurrentPartRoot());
      child_node_part.setNextSibling(destination_node);
      data.PopPartRoot(child_node_part);
    }
  }
}

void PartRoot::SwapPartsList(PartRoot& other) {
  DCHECK(!RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled());
  cached_ordered_parts_.swap(other.cached_ordered_parts_);
  std::swap(cached_parts_list_dirty_, other.cached_parts_list_dirty_);
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
void PartRoot::RebuildPartsList() {
  DCHECK(!RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled());
  DCHECK(cached_parts_list_dirty_);
  cached_ordered_parts_.clear();
  // Then traverse the tree under the root container and add parts in the order
  // they're found in the tree, and for the same Node, in the order they were
  // constructed.
  Node* node = FirstIncludedChildNode();
  if (!node || !LastIncludedChildNode()) {
    return;  // Empty list
  }
  Node* end_node = LastIncludedChildNode()->nextSibling();
  enum class NestedPartRoot {
    kNone,
    kAtStart,
    kAtEnd
  } nested_part_root = NestedPartRoot::kNone;
  while (node != end_node) {
    Node* next_node = NodeTraversal::Next(*node);
    if (auto* parts = node->GetDOMParts()) {
      // If we were previously at the start of a nested root, we're now at the
      // end.
      nested_part_root = nested_part_root == NestedPartRoot::kAtStart
                             ? NestedPartRoot::kAtEnd
                             : NestedPartRoot::kNone;
      for (Part* part : *parts) {
        if (!part->IsValid()) {
          continue;
        }
        if (PartRoot* part_root = part->GetAsPartRoot()) {
          // Skip the PartRoot itself.
          if (part_root == this) {
            continue;
          }
          // TODO(crbug.com/1453291) It's still possible to construct two
          // overlapping ChildNodeParts, e.g. both with the same endpoints,
          // overlapping endpoints, or adjoining endpoings (previous==next).
          // Eventually that should not be legal. Until then, ignore the second
          // and subsequent nested part roots we find. When such parts are no
          // longer legal, |nested_part_root| can be removed.
          if (nested_part_root != NestedPartRoot::kNone) {
            continue;
          }
          // We just entered a contained PartRoot; we should be at the
          // FirstIncludedChildNode. Skip all descendants of this PartRoot and
          // move to the last included child. Make sure to process any other
          // Parts that are on the endpoint Nodes.
          DCHECK_EQ(part_root->FirstIncludedChildNode(), node);
          DCHECK_EQ(part_root->LastIncludedChildNode()->parentNode(),
                    node->parentNode());
          next_node = part_root->LastIncludedChildNode();
          nested_part_root = NestedPartRoot::kAtStart;
        }
        if (part->NodeToSortBy() != node) {
          continue;
        }
        DCHECK(!base::Contains(cached_ordered_parts_, part));
        cached_ordered_parts_.push_back(part);
      }
    }
    node = next_node;
  }
}

namespace {

// This is used only in the case of DOMPartsAPIMinimal enabled, and it just
// fresh-builds the parts list, and/or just the node lists, every time with no
// caching.
void BuildPartsList(PartRoot& part_root,
                    PartRoot::PartList* part_list,
                    PartRoot::PartNodeList* node_part_nodes,
                    PartRoot::PartNodeList* child_node_part_nodes) {
  DCHECK(RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled());
  Node* node = part_root.FirstIncludedChildNode();
  Node* end_node = part_root.LastIncludedChildNode();
  if (!node || !end_node) {
    return;  // Empty lists
  }
  if (!part_root.IsDocumentPartRoot()) {
    // This is a ChildNodePart, so we need to skip the first start node, or
    // we'll just re-detect this ChildNodePart. If `node` doesn't have a
    // nextSibling (i.e. this ChildNodePart is mal-formed), then `node` will be
    // set to nullptr, and the entire while loop below will be properly skipped.
    node = node->nextSibling();
  } else {
    end_node = end_node->nextSibling();
  }
  while (node != end_node) {
    if (node->HasNodePart()) {
      if (Comment* start_comment = DynamicTo<Comment>(node);
          start_comment &&
          start_comment->data() == kChildNodePartStartCommentData) {
        // We've found the starting node of a child node range - scan to find
        // the ending node, skipping contents and nested ChildNodeParts.
        unsigned nested_child_node_part_count = 0;
        while (node->HasNextSibling() &&
               ((node = node->nextSibling()) != end_node)) {
          if (!IsA<Comment>(node)) [[likely]] {
            continue;
          }
          Comment& end_comment = *To<Comment>(node);
          if (!end_comment.HasNodePart()) {
            continue;  // Plain comment, not ChildNodePart marker.
          }
          if (end_comment.data() == kChildNodePartEndCommentData) [[likely]] {
            if (!nested_child_node_part_count) [[likely]] {
              // Found the end of the child node part.
              if (part_list) {
                part_list->push_back(MakeGarbageCollected<ChildNodePart>(
                    part_root, *start_comment, end_comment, Vector<String>()));
              }
              if (child_node_part_nodes) {
                child_node_part_nodes->push_back(start_comment);
                child_node_part_nodes->push_back(&end_comment);
              }
              break;
            }
            --nested_child_node_part_count;
          } else if (end_comment.data() == kChildNodePartStartCommentData) {
            ++nested_child_node_part_count;
          }
        }
      } else {
        // This is just a NodePart.
        if (part_list) {
          part_list->push_back(MakeGarbageCollected<NodePart>(
              part_root, *node, Vector<String>()));
        }
        if (node_part_nodes) {
          node_part_nodes->push_back(node);
        }
      }
    }
    node = NodeTraversal::Next(*node);
  }
}

}  // namespace

const PartRoot::PartNodeList& PartRoot::getNodePartNodes() {
  DCHECK(RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled());
  auto* nodes = MakeGarbageCollected<PartRoot::PartNodeList>();
  BuildPartsList(*this, nullptr, nodes, nullptr);
  return *nodes;
}

const PartRoot::PartNodeList& PartRoot::getChildNodePartNodes() {
  DCHECK(RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled());
  auto* nodes = MakeGarbageCollected<PartRoot::PartNodeList>();
  BuildPartsList(*this, nullptr, nullptr, nodes);
  return *nodes;
}

const PartRoot::PartList& PartRoot::getParts() {
  if (RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled()) {
    DCHECK(cached_ordered_parts_.empty());
    DCHECK(!cached_parts_list_dirty_);
    auto* parts = MakeGarbageCollected<PartRoot::PartList>();
    BuildPartsList(*this, parts, nullptr, nullptr);
    return *parts;
  } else if (cached_parts_list_dirty_) {
    RebuildPartsList();
    cached_parts_list_dirty_ = false;
  } else {
    // Remove invalid cached parts.
    bool remove_invalid = false;
    for (auto& part : cached_ordered_parts_) {
      if (!part->IsValid()) {
        remove_invalid = true;
        break;
      }
    }
    if (remove_invalid) {
      PartRoot::PartList new_list;
      for (auto& part : cached_ordered_parts_) {
        if (part->IsValid()) {
          new_list.push_back(part);
        }
      }
      cached_ordered_parts_.swap(new_list);
    }
  }
  return cached_ordered_parts_;
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
