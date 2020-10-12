// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_ACCESSIBILITY_BRIDGE_H_
#define FUCHSIA_ENGINE_BROWSER_ACCESSIBILITY_BRIDGE_H_

#include <fuchsia/accessibility/semantics/cpp/fidl.h>
#include <fuchsia/math/cpp/fidl.h>
#include <fuchsia/ui/views/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include "base/callback.h"
#include "base/macros.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "fuchsia/engine/web_engine_export.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_observer.h"

namespace content {
class WebContents;
}  // namespace content

// This class is the intermediate for accessibility between Chrome and Fuchsia.
// It handles registration to the Fuchsia Semantics Manager, translating events
// and data structures between the two services, and forwarding actions and
// events.
// The lifetime of an instance of AccessibilityBridge is the same as that of a
// View created by FrameImpl. This class refers to the View via the
// caller-supplied ViewRef.
// If |semantic_tree_| gets disconnected, it will cause the FrameImpl that owns
// |this| to close, which will also destroy |this|.
class WEB_ENGINE_EXPORT AccessibilityBridge
    : public content::WebContentsObserver,
      public fuchsia::accessibility::semantics::SemanticListener,
      public ui::AXTreeObserver {
 public:
  // |semantics_manager| is used during construction to register the instance.
  // |web_contents| is required to exist for the duration of |this|.
  AccessibilityBridge(
      fuchsia::accessibility::semantics::SemanticsManager* semantics_manager,
      fuchsia::ui::views::ViewRef view_ref,
      content::WebContents* web_contents,
      base::OnceCallback<void(zx_status_t)> on_error_callback);
  ~AccessibilityBridge() final;

  AccessibilityBridge(const AccessibilityBridge&) = delete;
  AccessibilityBridge& operator=(const AccessibilityBridge&) = delete;

  const ui::AXSerializableTree* ax_tree_for_test() { return &ax_tree_; }

  void set_event_received_callback_for_test(base::OnceClosure callback) {
    event_received_callback_for_test_ = std::move(callback);
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(AccessibilityBridgeTest, OnSemanticsModeChanged);
  FRIEND_TEST_ALL_PREFIXES(AccessibilityBridgeTest,
                           TreeModificationsAreForwarded);

  // Processes pending data and commits it to the Semantic Tree.
  void TryCommit();

  // Callback for SemanticTree::CommitUpdates.
  void OnCommitComplete();

  // Interrupts actions that are waiting for a response. This is invoked during
  // destruction time or when semantic updates have been disabled.
  void InterruptPendingActions();

  // content::WebContentsObserver implementation.
  void AccessibilityEventReceived(
      const content::AXEventNotificationDetails& details) override;

  // fuchsia::accessibility::semantics::SemanticListener implementation.
  void OnAccessibilityActionRequested(
      uint32_t node_id,
      fuchsia::accessibility::semantics::Action action,
      OnAccessibilityActionRequestedCallback callback) final;
  void HitTest(fuchsia::math::PointF local_point,
               HitTestCallback callback) final;
  void OnSemanticsModeChanged(bool updates_enabled,
                              OnSemanticsModeChangedCallback callback) final;

  // ui::AXTreeObserver implementation.
  void OnNodeWillBeDeleted(ui::AXTree* tree, ui::AXNode* node) override;
  void OnAtomicUpdateFinished(
      ui::AXTree* tree,
      bool root_changed,
      const std::vector<ui::AXTreeObserver::Change>& changes) override;

  fuchsia::accessibility::semantics::SemanticTreePtr semantic_tree_;
  fidl::Binding<fuchsia::accessibility::semantics::SemanticListener> binding_;
  content::WebContents* web_contents_;
  ui::AXSerializableTree ax_tree_;

  // Whether semantic updates are enabled.
  bool enable_semantic_updates_ = false;

  // Cache for pending data to be sent to the Semantic Tree between commits.
  std::vector<uint32_t> to_delete_;
  std::vector<fuchsia::accessibility::semantics::Node> to_update_;
  bool commit_inflight_ = false;

  // Maintain a map of callbacks as multiple hit test events can happen at
  // once. These are keyed by the request_id field of ui::AXActionData.
  base::flat_map<int, HitTestCallback> pending_hit_test_callbacks_;

  // Run in the case of an internal error that cannot be recovered from. This
  // will cause the frame |this| is owned by to be torn down.
  base::OnceCallback<void(zx_status_t)> on_error_callback_;

  // The root id of |ax_tree_|.
  int32_t root_id_ = 0;

  base::OnceClosure event_received_callback_for_test_;
};

#endif  // FUCHSIA_ENGINE_BROWSER_ACCESSIBILITY_BRIDGE_H_
