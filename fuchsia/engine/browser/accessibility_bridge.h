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
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/optional.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "fuchsia/engine/browser/ax_tree_converter.h"
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

  ui::AXSerializableTree* ax_tree_for_test();

  void set_event_received_callback_for_test(base::OnceClosure callback) {
    event_received_callback_for_test_ = std::move(callback);
  }

  void set_device_scale_factor_for_test(float device_scale_factor) {
    device_scale_factor_override_for_test_ = device_scale_factor;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(AccessibilityBridgeTest, OnSemanticsModeChanged);
  FRIEND_TEST_ALL_PREFIXES(AccessibilityBridgeTest,
                           TreeModificationsAreForwarded);
  FRIEND_TEST_ALL_PREFIXES(AccessibilityBridgeTest,
                           TransformAccountsForOffsetContainerBounds);
  FRIEND_TEST_ALL_PREFIXES(AccessibilityBridgeTest,
                           UpdateTransformWhenContainerBoundsChange);
  FRIEND_TEST_ALL_PREFIXES(AccessibilityBridgeTest,
                           OffsetContainerBookkeepingIsUpdated);

  using AXNodeID = std::pair<ui::AXTreeID, int32_t>;

  // Represents a connection between two AXTrees that are in different frames.
  struct TreeConnection {
    // ID of the node in the parent tree that points to this tree.
    int32_t parent_node_id = 0;
    // ID of the parent tree.
    ui::AXTreeID parent_tree_id = ui::AXTreeIDUnknown();
    // Whether the trees are connected.
    bool is_connected = false;
  };

  // Processes pending data and commits it to the Semantic Tree.
  void TryCommit();

  // Connects trees if they are present or deletes the connection if both are
  // gone.
  void UpdateTreeConnections();

  // Returns true if the main frame AXTree is not present or if trees are not
  // connected.
  bool ShouldHoldCommit();

  // The AXTreeID of a tree can change. Updates all internal references of an
  // AXTreeID by fetching the RenderFrameHost associated with |tree_id| and
  // updates the value if it is different from the previously used AXTreeID.
  // Returns false if the frame does not exist anymore, true otherwise.
  bool UpdateAXTreeID(const ui::AXTreeID& tree_id);

  // If |tree| is connected to another tree as its child, mark them as
  // disconnected.
  void MaybeDisconnectTreeFromParentTree(ui::AXTree* tree);

  // Callback for SemanticTree::CommitUpdates.
  void OnCommitComplete();

  // Interrupts actions that are waiting for a response. This is invoked during
  // destruction time or when semantic updates have been disabled.
  void InterruptPendingActions();

  // Accessor for the device scale factor that allows for overriding the value
  // in tests.
  float GetDeviceScaleFactor();

  // Helper method to remove a node id from its offset container's offset
  // children mapping.
  void RemoveNodeFromOffsetMapping(ui::AXTree* tree,
                                   const ui::AXNodeData& node_data);

  // content::WebContentsObserver implementation.
  void AccessibilityEventReceived(
      const content::AXEventNotificationDetails& details) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

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
  void OnNodeDeleted(ui::AXTree* tree, int32_t node_id) override;
  void OnAtomicUpdateFinished(
      ui::AXTree* tree,
      bool root_changed,
      const std::vector<ui::AXTreeObserver::Change>& changes) override;
  void OnNodeDataChanged(ui::AXTree* tree,
                         const ui::AXNodeData& old_node_data,
                         const ui::AXNodeData& new_node_data) override;

  fuchsia::accessibility::semantics::SemanticTreePtr semantic_tree_;
  fidl::Binding<fuchsia::accessibility::semantics::SemanticListener> binding_;
  content::WebContents* web_contents_;

  // Holds one semantic tree per iframe.
  base::flat_map<ui::AXTreeID, std::unique_ptr<ui::AXSerializableTree>>
      ax_trees_;

  // Maps frames to AXTrees.
  base::flat_map<content::GlobalFrameRoutingId, ui::AXTreeID>
      frame_id_to_tree_id_;

  // Keeps track of semantic trees connections.
  // The key is the AXTreeID of the semantic tree that is connected to another
  // tree.
  base::flat_map<ui::AXTreeID, TreeConnection> tree_connections_;

  // Maintain a map of callbacks as multiple hit test events can happen at
  // once. These are keyed by the request_id field of ui::AXActionData.
  base::flat_map<int, HitTestCallback> pending_hit_test_callbacks_;

  // Whether semantic updates are enabled.
  bool enable_semantic_updates_ = false;

  // Cache for pending data to be sent to the Semantic Tree between commits.
  std::vector<uint32_t> to_delete_;
  std::vector<fuchsia::accessibility::semantics::Node> to_update_;
  bool commit_inflight_ = false;

  // Maintain a map from AXNode IDs to a list of the AXNode IDs of descendant
  // nodes that have the key node ID as their offset containers.
  std::map<AXNodeID, base::flat_set<AXNodeID>> offset_container_children_;

  // Run in the case of an internal error that cannot be recovered from. This
  // will cause the frame |this| is owned by to be torn down.
  base::OnceCallback<void(zx_status_t)> on_error_callback_;

  // The root id of the AXTree of the main frame.
  int32_t root_id_ = 0;

  // Maps node IDs from one platform to another.
  std::unique_ptr<NodeIDMapper> id_mapper_;

  base::OnceClosure event_received_callback_for_test_;

  // If set, the scale factor for this device for use in tests.
  base::Optional<float> device_scale_factor_override_for_test_;
};

#endif  // FUCHSIA_ENGINE_BROWSER_ACCESSIBILITY_BRIDGE_H_
