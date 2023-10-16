// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_MOVE_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_MOVE_SCOPE_H_

#include "base/logging.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

enum class NodeMoveScopeType {
  kOther = 0,
  kInsertBeforeAllChildren,
  kAppendAfterAllChildren,
  kClone,
};

class NodeMoveScopeItem : public GarbageCollected<NodeMoveScopeItem> {
 public:
  NodeMoveScopeItem(Node& destination_root, NodeMoveScopeType type)
      : destination_root_(destination_root),
        all_parts_lists_clean_(type != NodeMoveScopeType::kOther &&
                               !destination_root.GetDocument().HasListenerType(
                                   Document::kDOMMutationEventListener)),
        prepending_children_(type ==
                             NodeMoveScopeType::kInsertBeforeAllChildren) {
    DCHECK(RuntimeEnabledFeatures::DOMPartsAPIActivePartTrackingEnabled());
    DCHECK(RuntimeEnabledFeatures::DOMPartsAPIEnabled());
  }
  NodeMoveScopeItem(const NodeMoveScopeItem&) = delete;
  NodeMoveScopeItem& operator=(const NodeMoveScopeItem&) = delete;

  ~NodeMoveScopeItem() { DCHECK(destination_root_); }

  Node* GetDestinationTreeRoot() {
    if (!destination_tree_root_) {
      destination_tree_root_ = &destination_root_->TreeRoot();
    }
    return destination_tree_root_.Get();
  }

  void SetCurrentNodeBeingRemoved(Node& node) {
    current_node_being_removed_ = &node;
  }

  Node* CurrentNodeBeingRemoved() {
    DCHECK(current_node_being_removed_);
    return current_node_being_removed_.Get();
  }

  bool AllMovedPartsWereClean() { return all_parts_lists_clean_; }

  bool IsPrepend() {
    DCHECK(AllMovedPartsWereClean());
    return prepending_children_;
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(destination_root_);
    visitor->Trace(destination_tree_root_);
    visitor->Trace(current_node_being_removed_);
  }

 private:
  Member<Node> destination_root_;
  Member<Node> destination_tree_root_;
  Member<Node> current_node_being_removed_;
  bool all_parts_lists_clean_;
  bool prepending_children_;
};

class NodeMoveScope {
  STACK_ALLOCATED();

 public:
  NodeMoveScope(Node& destination_root, NodeMoveScopeType type) {
    if (!RuntimeEnabledFeatures::DOMPartsAPIActivePartTrackingEnabled()) {
      return;
    }
    DCHECK(RuntimeEnabledFeatures::DOMPartsAPIEnabled());
    DCHECK(IsMainThread());
    auto* document = &destination_root.GetDocument();
    if (!document->DOMPartsInUse() && type != NodeMoveScopeType::kClone) {
      return;
    }
    document_ = document;
    current_item_ =
        MakeGarbageCollected<NodeMoveScopeItem>(destination_root, type);
    document_->NodeMoveScopeItems().push_back(current_item_);
  }
  NodeMoveScope(const NodeMoveScope&) = delete;
  NodeMoveScope& operator=(const NodeMoveScope&) = delete;

  ~NodeMoveScope() {
    if (!RuntimeEnabledFeatures::DOMPartsAPIActivePartTrackingEnabled()) {
      return;
    }
    DCHECK(RuntimeEnabledFeatures::DOMPartsAPIEnabled());
    DCHECK(IsMainThread());
    if (!InScope()) {
      return;
    }
    if (document_) {
      DCHECK(current_item_);
      DCHECK_EQ(document_->NodeMoveScopeItems().back(), current_item_);
      document_->NodeMoveScopeItems().pop_back();
    }
    if (!document_ || document_->NodeMoveScopeItems().empty()) {
      current_item_ = nullptr;
      document_ = nullptr;
    } else {
      current_item_ = document_->NodeMoveScopeItems().back();
    }
  }

  static bool InScope() {
    DCHECK_EQ(!document_, !current_item_);
    return current_item_;
  }

  static Node* GetDestinationTreeRoot() {
    DCHECK(RuntimeEnabledFeatures::DOMPartsAPIActivePartTrackingEnabled());
    DCHECK(RuntimeEnabledFeatures::DOMPartsAPIEnabled());
    DCHECK(IsMainThread());
    if (!InScope()) {
      return nullptr;
    }
    return current_item_->GetDestinationTreeRoot();
  }

  static void SetCurrentNodeBeingRemoved(Node& node) {
    if (!RuntimeEnabledFeatures::DOMPartsAPIActivePartTrackingEnabled()) {
      return;
    }
    DCHECK(RuntimeEnabledFeatures::DOMPartsAPIEnabled());
    DCHECK(IsMainThread());
    if (!InScope()) {
      return;
    }
    current_item_->SetCurrentNodeBeingRemoved(node);
  }

  static Node* CurrentNodeBeingRemoved() {
    DCHECK(RuntimeEnabledFeatures::DOMPartsAPIActivePartTrackingEnabled());
    DCHECK(RuntimeEnabledFeatures::DOMPartsAPIEnabled());
    DCHECK(IsMainThread());
    if (!InScope()) {
      return nullptr;
    }
    return current_item_->CurrentNodeBeingRemoved();
  }

  static bool AllMovedPartsWereClean() {
    DCHECK(RuntimeEnabledFeatures::DOMPartsAPIActivePartTrackingEnabled());
    DCHECK(RuntimeEnabledFeatures::DOMPartsAPIEnabled());
    DCHECK(IsMainThread());
    if (!InScope()) {
      return false;
    }
    return current_item_->AllMovedPartsWereClean();
  }

  static bool IsPrepend() {
    DCHECK(RuntimeEnabledFeatures::DOMPartsAPIActivePartTrackingEnabled());
    DCHECK(RuntimeEnabledFeatures::DOMPartsAPIEnabled());
    DCHECK(IsMainThread());
    if (!InScope()) {
      return false;
    }
    return current_item_->IsPrepend();
  }

 private:
  CORE_EXPORT static NodeMoveScopeItem* current_item_;
  CORE_EXPORT static Document* document_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_MOVE_SCOPE_H_
