// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/accessibility_bridge.h"

#include <lib/sys/cpp/component_context.h>
#include <lib/ui/scenic/cpp/view_ref_pair.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "fuchsia/engine/browser/ax_tree_converter.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace {

constexpr uint32_t kSemanticNodeRootId = 0;

// TODO(https://crbug.com/973095): Update this value based on average and
// maximum sizes of serialized Semantic Nodes.
constexpr size_t kMaxNodesPerUpdate = 16;

// Error allowed for each edge when converting from gfx::RectF to gfx::Rect.
constexpr float kRectConversionError = 0.5;

}  // namespace

AccessibilityBridge::AccessibilityBridge(
    fuchsia::accessibility::semantics::SemanticsManager* semantics_manager,
    fuchsia::ui::views::ViewRef view_ref,
    content::WebContents* web_contents,
    base::OnceCallback<void(zx_status_t)> on_error_callback)
    : binding_(this),
      web_contents_(web_contents),
      on_error_callback_(std::move(on_error_callback)) {
  DCHECK(web_contents_);
  Observe(web_contents_);
  ax_tree_.AddObserver(this);

  semantics_manager->RegisterViewForSemantics(
      std::move(view_ref), binding_.NewBinding(), semantic_tree_.NewRequest());
  semantic_tree_.set_error_handler([this](zx_status_t status) {
    ZX_LOG(ERROR, status) << "SemanticTree disconnected";
    std::move(on_error_callback_).Run(ZX_ERR_INTERNAL);
  });
}

AccessibilityBridge::~AccessibilityBridge() {
  // Acknowledge to the SemanticsManager if any actions have not been handled
  // upon destruction time.
  for (auto& callback : pending_hit_test_callbacks_) {
    fuchsia::accessibility::semantics::Hit hit;
    hit.set_node_id(kSemanticNodeRootId);
    callback.second(std::move(hit));
  }
}

void AccessibilityBridge::TryCommit() {
  if (commit_inflight_ || to_send_.empty())
    return;

  SemanticUpdateOrDelete::Type current = to_send_.at(0).type;
  int range_start = 0;
  for (size_t i = 1; i < to_send_.size(); i++) {
    if (to_send_.at(i).type == current &&
        (i - range_start < kMaxNodesPerUpdate)) {
      continue;
    } else {
      DispatchSemanticsMessages(range_start, i - range_start);
      current = to_send_.at(i).type;
      range_start = i;
    }
  }
  DispatchSemanticsMessages(range_start, to_send_.size() - range_start);

  semantic_tree_->CommitUpdates(
      fit::bind_member(this, &AccessibilityBridge::OnCommitComplete));
  commit_inflight_ = true;
  to_send_.clear();
}

void AccessibilityBridge::DispatchSemanticsMessages(size_t start, size_t size) {
  if (to_send_.at(start).type == SemanticUpdateOrDelete::Type::UPDATE) {
    std::vector<fuchsia::accessibility::semantics::Node> updates;
    for (size_t i = start; i < start + size; i++) {
      DCHECK(to_send_.at(i).type == SemanticUpdateOrDelete::Type::UPDATE);
      updates.push_back(std::move(to_send_.at(i).update_node));
    }
    semantic_tree_->UpdateSemanticNodes(std::move(updates));
  } else if (to_send_.at(start).type == SemanticUpdateOrDelete::Type::DELETE) {
    std::vector<uint32_t> deletes;
    for (size_t i = start; i < start + size; i++) {
      DCHECK(to_send_.at(i).type == SemanticUpdateOrDelete::Type::DELETE);
      deletes.push_back(to_send_.at(i).id_to_delete);
    }
    semantic_tree_->DeleteSemanticNodes(deletes);
  }
}

AccessibilityBridge::SemanticUpdateOrDelete::SemanticUpdateOrDelete(
    AccessibilityBridge::SemanticUpdateOrDelete&& m)
    : type(m.type),
      update_node(std::move(m.update_node)),
      id_to_delete(m.id_to_delete) {}

AccessibilityBridge::SemanticUpdateOrDelete::SemanticUpdateOrDelete(
    Type type,
    fuchsia::accessibility::semantics::Node node,
    uint32_t id_to_delete)
    : type(type), update_node(std::move(node)), id_to_delete(id_to_delete) {}

void AccessibilityBridge::OnCommitComplete() {
  commit_inflight_ = false;
  TryCommit();
}

uint32_t AccessibilityBridge::ConvertToFuchsiaNodeId(int32_t ax_node_id) {
  if (ax_node_id == root_id_) {
    // On the Fuchsia side, the root node is indicated by id
    // |kSemanticNodeRootId|, which is 0.
    return kSemanticNodeRootId;
  } else {
    // AXNode ids are signed, Semantic Node ids are unsigned.
    return bit_cast<uint32_t>(ax_node_id);
  }
}

void AccessibilityBridge::AccessibilityEventReceived(
    const content::AXEventNotificationDetails& details) {
  // Updates to AXTree must be applied first.
  for (const ui::AXTreeUpdate& update : details.updates) {
    if (!ax_tree_.Unserialize(update)) {
      // If this fails, it is a fatal error that will cause an early exit.
      std::move(on_error_callback_).Run(ZX_ERR_INTERNAL);
      return;
    }
  }

  // Events to fire after tree has been updated.
  for (const ui::AXEvent& event : details.events) {
    if (event.event_type == ax::mojom::Event::kHitTestResult &&
        pending_hit_test_callbacks_.find(event.action_request_id) !=
            pending_hit_test_callbacks_.end()) {
      fuchsia::accessibility::semantics::Hit hit;
      hit.set_node_id(ConvertToFuchsiaNodeId(event.id));

      // Run the pending callback with the hit.
      pending_hit_test_callbacks_[event.action_request_id](std::move(hit));
      pending_hit_test_callbacks_.erase(event.action_request_id);
    } else if (event_received_callback_for_test_) {
      std::move(event_received_callback_for_test_).Run();
    }
  }
}

void AccessibilityBridge::OnAccessibilityActionRequested(
    uint32_t node_id,
    fuchsia::accessibility::semantics::Action action,
    OnAccessibilityActionRequestedCallback callback) {
  ui::AXActionData action_data = ui::AXActionData();

  if (!ConvertAction(action, &action_data.action)) {
    callback(false);
    return;
  }

  action_data.target_node_id = node_id;

  if (action == fuchsia::accessibility::semantics::Action::SHOW_ON_SCREEN) {
    ui::AXNode* node = ax_tree_.GetFromId(node_id);
    if (!node) {
      callback(false);
      return;
    }

    action_data.target_rect = gfx::ToEnclosedRectIgnoringError(
        node->data().relative_bounds.bounds, kRectConversionError);
    action_data.horizontal_scroll_alignment =
        ax::mojom::ScrollAlignment::kScrollAlignmentCenter;
    action_data.vertical_scroll_alignment =
        ax::mojom::ScrollAlignment::kScrollAlignmentCenter;
    action_data.scroll_behavior = ax::mojom::ScrollBehavior::kScrollIfVisible;
  }

  web_contents_->GetMainFrame()->AccessibilityPerformAction(action_data);
  callback(true);
}

void AccessibilityBridge::HitTest(fuchsia::math::PointF local_point,
                                  HitTestCallback callback) {
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kHitTest;
  gfx::Point point;
  point.set_x(local_point.x);
  point.set_y(local_point.y);
  action_data.target_point = point;
  action_data.hit_test_event_to_fire = ax::mojom::Event::kHitTestResult;
  pending_hit_test_callbacks_[action_data.request_id] = std::move(callback);

  web_contents_->GetMainFrame()->AccessibilityPerformAction(action_data);
}

void AccessibilityBridge::OnSemanticsModeChanged(
    bool updates_enabled,
    OnSemanticsModeChangedCallback callback) {
  if (updates_enabled) {
    // The first call to AccessibilityEventReceived after this call will be the
    // entire semantic tree.
    web_contents_->EnableWebContentsOnlyAccessibilityMode();
  } else {
    // The SemanticsManager will clear all state in this case, which is mirrored
    // here.
    to_send_.clear();
    commit_inflight_ = false;
  }

  // Notify the SemanticsManager that semantics mode has been updated.
  callback();
}

void AccessibilityBridge::DeleteSubtree(ui::AXNode* node) {
  DCHECK(node);

  // When navigating, page 1, including the root, is deleted after page 2 has
  // loaded. Since the root id is the same for page 1 and 2, page 2's root id
  // ends up getting deleted. To handle this, the root will only be updated.
  if (node->id() != root_id_) {
    to_send_.push_back(
        SemanticUpdateOrDelete(SemanticUpdateOrDelete::Type::DELETE, {},
                               ConvertToFuchsiaNodeId(node->id())));
  }
  for (ui::AXNode* child : node->children())
    DeleteSubtree(child);
}

void AccessibilityBridge::OnNodeWillBeDeleted(ui::AXTree* tree,
                                              ui::AXNode* node) {
  DeleteSubtree(node);
}

void AccessibilityBridge::OnSubtreeWillBeDeleted(ui::AXTree* tree,
                                                 ui::AXNode* node) {
  DeleteSubtree(node);
}

void AccessibilityBridge::OnAtomicUpdateFinished(
    ui::AXTree* tree,
    bool root_changed,
    const std::vector<ui::AXTreeObserver::Change>& changes) {
  DCHECK_EQ(tree, &ax_tree_);
  root_id_ = ax_tree_.root()->id();
  for (const ui::AXTreeObserver::Change& change : changes) {
    ui::AXNodeData ax_data;
    switch (change.type) {
      case ui::AXTreeObserver::NODE_CREATED:
      case ui::AXTreeObserver::SUBTREE_CREATED:
      case ui::AXTreeObserver::NODE_CHANGED:
        ax_data = change.node->data();
        if (change.node->id() == root_id_) {
          ax_data.id = kSemanticNodeRootId;
        }
        to_send_.push_back(
            SemanticUpdateOrDelete(SemanticUpdateOrDelete::Type::UPDATE,
                                   AXNodeDataToSemanticNode(ax_data), 0));
        break;
      case ui::AXTreeObserver::NODE_REPARENTED:
      case ui::AXTreeObserver::SUBTREE_REPARENTED:
        DeleteSubtree(change.node);
        break;
    }
  }
  TryCommit();
}
