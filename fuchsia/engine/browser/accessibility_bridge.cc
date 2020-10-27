// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/accessibility_bridge.h"

#include <algorithm>

#include <lib/sys/cpp/component_context.h>
#include <lib/ui/scenic/cpp/view_ref_pair.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "fuchsia/engine/browser/ax_tree_converter.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace {

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
  InterruptPendingActions();
}

void AccessibilityBridge::TryCommit() {
  if (commit_inflight_ || (to_delete_.empty() && to_update_.empty()))
    return;

  // Deletions come before updates because first the nodes are deleted, and
  // then we update the parents to no longer point at them.
  if (!to_delete_.empty())
    semantic_tree_->DeleteSemanticNodes(std::move(to_delete_));

  size_t start = 0;
  while (start < to_update_.size()) {
    // TODO(https://crbug.com/1134727): AccessibilityBridge must respect FIDL
    // size limits.
    size_t end =
        start + std::min(kMaxNodesPerUpdate, to_update_.size() - start);
    decltype(to_update_) batch;
    std::move(to_update_.begin() + start, to_update_.begin() + end,
              std::back_inserter(batch));
    semantic_tree_->UpdateSemanticNodes(std::move(batch));
    start = end;
  }
  semantic_tree_->CommitUpdates(
      fit::bind_member(this, &AccessibilityBridge::OnCommitComplete));
  commit_inflight_ = true;
  to_delete_.clear();
  to_update_.clear();
}

void AccessibilityBridge::OnCommitComplete() {
  // TODO(https://crbug.com/1134737): Separate updates of atomic updates and
  // don't allow all of them to be in the same commit.
  commit_inflight_ = false;
  TryCommit();
}

void AccessibilityBridge::AccessibilityEventReceived(
    const content::AXEventNotificationDetails& details) {
  // No need to process events if Fuchsia is not receiving them.
  if (!enable_semantic_updates_)
    return;

  // Updates to AXTree must be applied first.
  for (const ui::AXTreeUpdate& update : details.updates) {
    if (!update.has_tree_data &&
        ax_tree_.GetAXTreeID() != ui::AXTreeIDUnknown() &&
        ax_tree_.GetAXTreeID() != details.ax_tree_id) {
      // TODO(https://crbug.com/1128954): Add support for combining AXTrees.
      continue;
    }

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
      hit.set_node_id(ConvertToFuchsiaNodeId(event.id, root_id_));

      // Run the pending callback with the hit.
      pending_hit_test_callbacks_[event.action_request_id](std::move(hit));
      pending_hit_test_callbacks_.erase(event.action_request_id);
    } else if (event_received_callback_for_test_ &&
               event.event_type == ax::mojom::Event::kEndOfTest) {
      std::move(event_received_callback_for_test_).Run();
    }
  }
}

void AccessibilityBridge::OnAccessibilityActionRequested(
    uint32_t node_id,
    fuchsia::accessibility::semantics::Action action,
    OnAccessibilityActionRequestedCallback callback) {
  ui::AXActionData action_data = ui::AXActionData();

  // The requested action is not supported.
  if (!ConvertAction(action, &action_data.action)) {
    callback(false);
    return;
  }

  action_data.target_node_id = ConvertToAxNodeId(node_id, root_id_);

  if (action == fuchsia::accessibility::semantics::Action::SHOW_ON_SCREEN) {
    ui::AXNode* node = ax_tree_.GetFromId(action_data.target_node_id);
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

  if (event_received_callback_for_test_) {
    // Perform an action with a corresponding event to signal the action has
    // been pumped through.
    action_data.action = ax::mojom::Action::kSignalEndOfTest;
    web_contents_->GetMainFrame()->AccessibilityPerformAction(action_data);
  }
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
  // TODO(https://crbug.com/1134591): Fix the case when enabling / disabling
  // semantics can lead to race conditions.
  if (enable_semantic_updates_ == updates_enabled)
    return callback();

  enable_semantic_updates_ = updates_enabled;
  if (updates_enabled) {
    // The first call to AccessibilityEventReceived after this call will be
    // the entire semantic tree.
    web_contents_->EnableWebContentsOnlyAccessibilityMode();
  } else {
    // The SemanticsManager will clear all state in this case, which is
    // mirrored here.
    ui::AXMode mode = web_contents_->GetAccessibilityMode();
    mode.set_mode(ui::AXMode::kWebContents, false);
    web_contents_->SetAccessibilityMode(mode);
    to_delete_.clear();
    to_update_.clear();
    commit_inflight_ = false;
    ax_tree_.Destroy();
    InterruptPendingActions();
  }

  // Notify the SemanticsManager that this request was handled.
  callback();
}

void AccessibilityBridge::OnNodeWillBeDeleted(ui::AXTree* tree,
                                              ui::AXNode* node) {
  to_delete_.push_back(ConvertToFuchsiaNodeId(node->id(), root_id_));
}

void AccessibilityBridge::OnAtomicUpdateFinished(
    ui::AXTree* tree,
    bool root_changed,
    const std::vector<ui::AXTreeObserver::Change>& changes) {
  DCHECK_EQ(tree, &ax_tree_);
  root_id_ = ax_tree_.root()->id();
  // Changes included here are only nodes that are still on the tree. Since this
  // indicates the end of an atomic update, it is safe to assume that these
  // nodes will not change until the next change arrives. Nodes that would be
  // deleted are already gone, which means that all updates collected here in
  // |to_update_| are going to be executed after |to_delete_|.
  for (const ui::AXTreeObserver::Change& change : changes) {
    ui::AXNodeData ax_data = change.node->data();
    ax_data.id = ConvertToFuchsiaNodeId(change.node->id(), root_id_);
    to_update_.push_back(AXNodeDataToSemanticNode(ax_data));
  }
  // TODO(https://crbug.com/1134737): Separate updates of atomic updates and
  // don't allow all of them to be in the same commit.
  TryCommit();
}

void AccessibilityBridge::InterruptPendingActions() {
  // Acknowledge to the SemanticsManager if any actions have not been handled
  // upon destruction time or when semantic updates have been disabled.
  for (auto& callback : pending_hit_test_callbacks_) {
    fuchsia::accessibility::semantics::Hit hit;
    callback.second(std::move(hit));
  }
  pending_hit_test_callbacks_.clear();
}
