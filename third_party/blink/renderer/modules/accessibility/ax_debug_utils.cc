// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_debug_utils.h"

#include <memory>
#include <numeric>
#include <string>
#include <utility>

namespace blink {

namespace {

std::string NewlineToSpaceReplacer(std::string str) {
  std::replace(str.begin(), str.end(), '\n', ' ');
  return str;
}

size_t RecursiveIncludedNodeCount(AXObject* subtree) {
  size_t count = 1;  // For |subtree| itself.
  for (const auto& child : subtree->ChildrenIncludingIgnored()) {
    count += RecursiveIncludedNodeCount(child);
  }
  return count;
}

}  // namespace

std::string TreeToStringHelper(const AXObject* obj, bool verbose) {
  return TreeToStringWithMarkedObjectHelper(obj, nullptr, verbose);
}

std::string TreeToStringWithMarkedObjectHelperRecursive(
    const AXObject* obj,
    const AXObject* marked_object,
    int indent,
    bool verbose,
    int* marked_object_found_count) {
  if (!obj) {
    return "";
  }

  if (marked_object_found_count && marked_object && obj == marked_object) {
    ++*marked_object_found_count;
  }
  std::string extra = obj == marked_object ? "*" : " ";
  return std::accumulate(
      obj->CachedChildrenIncludingIgnored().begin(),
      obj->CachedChildrenIncludingIgnored().end(),
      extra + std::string(std::max(2 * indent - 1, 0), ' ') +
          NewlineToSpaceReplacer(obj->ToString(verbose).Utf8()) + "\n",
      [indent, verbose, marked_object, marked_object_found_count](
          const std::string& str, const AXObject* child) {
        return str + TreeToStringWithMarkedObjectHelperRecursive(
                         child, marked_object, indent + 1, verbose,
                         marked_object_found_count);
      });
}

std::string TreeToStringWithMarkedObjectHelper(const AXObject* obj,
                                               const AXObject* marked_object,
                                               bool verbose) {
  int marked_object_found_count = 0;

  std::string tree_str = TreeToStringWithMarkedObjectHelperRecursive(
      obj, marked_object, 0, verbose, &marked_object_found_count);
  if (marked_object_found_count == 1) {
    return tree_str;
  }

  if (!marked_object) {
    return tree_str;
  }
  return std::string("**** ERROR: Found marked objects was found ") +
         String::Number(marked_object_found_count).Utf8() +
         " times, should have been found exactly once.\n* Marked object: " +
         marked_object->ToString().Utf8() + "\n\n" + tree_str;
}

std::string ParentChainToStringHelper(const AXObject* obj) {
  AXObject::AXObjectVector ancestors;
  while (obj) {
    ancestors.push_back(const_cast<AXObject*>(obj));
    obj = obj->ParentObject();
  }

  size_t indent = 0;
  std::string builder;
  for (auto iter = ancestors.rbegin(); iter != ancestors.rend(); iter++) {
    builder = builder + std::string(2 * indent, ' ') +
              (*iter)->ToString().Utf8() + '\n';
    ++indent;
  }
  return builder;
}

void CheckTreeConsistency(
    AXObjectCacheImpl& cache,
    ui::AXTreeSerializer<const AXObject*,
                         HeapVector<Member<const AXObject>>,
                         ui::AXTreeUpdate*,
                         ui::AXTreeData*,
                         ui::AXNodeData>& serializer,
    ui::AXTreeSerializer<const ui::AXNode*,
                         std::vector<const ui::AXNode*>,
                         ui::AXTreeUpdate*,
                         ui::AXTreeData*,
                         ui::AXNodeData>* plugin_serializer) {
  // If all serializations are complete, check that the number of included nodes
  // being serialized is the same as the number of included nodes according to
  // the AXObjectCache.
  size_t included_node_count_from_cache = cache.GetIncludedNodeCount();
  size_t plugin_included_node_count_from_cache =
      cache.GetPluginIncludedNodeCount();
  size_t serializer_client_node_count = serializer.ClientTreeNodeCount();
  size_t plugin_serializer_client_node_count =
      plugin_serializer ? plugin_serializer->ClientTreeNodeCount() : 0;
  if (included_node_count_from_cache != serializer_client_node_count ||
      plugin_included_node_count_from_cache !=
          plugin_serializer_client_node_count) {
    // There was an inconsistency in the node count: provide a helpful message
    // to facilitate debugging.
    std::ostringstream msg;
    msg << "AXTreeSerializer should have the expected number of included nodes:"
        << "\n* AXObjectCache: " << included_node_count_from_cache
        << "\n* AXObjectCache plugin: " << plugin_included_node_count_from_cache
        << "\n* Depth first cache count: "
        << RecursiveIncludedNodeCount(cache.Root())
        << "\n* Serializer: " << serializer.ClientTreeNodeCount()
        << "\n* plugin Serializer: " << plugin_serializer_client_node_count;
    HeapHashMap<AXID, Member<AXObject>>& all_objects = cache.GetObjects();
    for (const auto& id_to_object_entry : all_objects) {
      AXObject* obj = id_to_object_entry.value;
      if (obj->IsIncludedInTree()) {
        if (!serializer.IsInClientTree(obj)) {
          if (obj->IsMissingParent()) {
            msg << "\n* Included node not serialized, is missing parent: "
                << obj;
          } else if (!obj->GetDocument()->GetFrame()) {
            msg << "\n* Included node not serialized, in closed document: "
                << obj;
          } else {
            bool included_state_stale = !obj->IsIncludedInTree();
            msg << "\n* Included node not serialized: " << obj;
            if (included_state_stale) {
              msg << "\n  Included state was stale.";
            }
            msg << "\n  Parent: " << obj->ParentObject();
          }
        }
      }
    }
    for (AXID id : serializer.ClientTreeNodeIds()) {
      AXObject* obj = cache.ObjectFromAXID(id);
      if (!obj) {
        msg << "\n* Serialized node does not exist: " << id;
        if (const AXObject* parent = serializer.ParentOf(id)) {
          msg << "\n* Parent = " << parent;
        }
      } else if (!obj->IsIncludedInTree()) {
        msg << "\n* Serialized an unincluded node: " << obj;
      }
    }
    DCHECK(false) << msg.str();
  }

#if EXPENSIVE_DCHECKS_ARE_ON()
  constexpr size_t kMaxNodesForDeepSlowConsistencyCheck = 100;
  if (included_node_count_from_cache > kMaxNodesForDeepSlowConsistencyCheck) {
    return;
  }

  DCHECK_EQ(included_node_count_from_cache,
            RecursiveIncludedNodeCount(cache.Root()))
      << "\n* AXObjectCacheImpl's tree:\n"
      << TreeToStringHelper(cache.Root(), /* verbose */ true);
#endif  // EXPENSIVE_DCHECKS_ARE_ON()
}

}  // namespace blink
