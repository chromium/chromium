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
class WEB_ENGINE_EXPORT AccessibilityBridge
    : public content::WebContentsObserver,
      public fuchsia::accessibility::semantics::SemanticListener,
      public ui::AXTreeObserver {
 public:
  // |web_contents| is required to exist for the duration of |this|.
  AccessibilityBridge(
      fuchsia::accessibility::semantics::SemanticsManagerPtr semantics_manager,
      fuchsia::ui::views::ViewRef view_ref,
      content::WebContents* web_contents);
  ~AccessibilityBridge() final;

  void set_semantic_tree_for_test(
      fidl::InterfaceRequest<fuchsia::accessibility::semantics::SemanticTree>
          tree_request);

 private:
  FRIEND_TEST_ALL_PREFIXES(AccessibilityBridgeTest, OnSemanticsModeChanged);

  // Handles batching of semantic nodes and committing them to the SemanticTree.
  void TryCommit();

  // Callback for SemanticTree::CommitUpdates.
  void OnCommitComplete();

  // Converts AXNode ids to Semantic Node ids, and handles special casing of the
  // root.
  uint32_t ConvertToFuchsiaNodeId(int32_t ax_node_id);

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

  fuchsia::accessibility::semantics::SemanticTreePtr tree_ptr_;
  fidl::Binding<fuchsia::accessibility::semantics::SemanticListener> binding_;
  content::WebContents* web_contents_;
  ui::AXSerializableTree tree_;

  std::vector<fuchsia::accessibility::semantics::Node> to_send_;
  std::vector<uint32_t> to_delete_;
  bool commit_inflight_ = false;

  // The root id of |tree_|.
  int32_t root_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityBridge);
};

#endif  // FUCHSIA_ENGINE_BROWSER_ACCESSIBILITY_BRIDGE_H_
