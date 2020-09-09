// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_FRAME_TREE_H_
#define CONTENT_BROWSER_RENDERER_HOST_FRAME_TREE_H_

#include <stdint.h>

#include <iterator>
#include <memory>
#include <string>
#include <unordered_map>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/navigator_delegate.h"
#include "content/browser/renderer_host/render_frame_host_manager.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_owner_element_type.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom-forward.h"

namespace blink {
struct FramePolicy;
}  // namespace blink

namespace content {

class NavigationControllerImpl;
class RenderFrameHostDelegate;
class RenderViewHostDelegate;
class RenderViewHostImpl;
class RenderFrameHostManager;
class RenderWidgetHostDelegate;

// Represents the frame tree for a page. With the exception of the main frame,
// all FrameTreeNodes will be created/deleted in response to frame attach and
// detach events in the DOM.
//
// The main frame's FrameTreeNode is special in that it is reused. This allows
// it to serve as an anchor for state that needs to persist across top-level
// page navigations.
//
// TODO(ajwong): Move NavigationController ownership to the main frame
// FrameTreeNode. Possibly expose access to it from here.
//
// This object is only used on the UI thread.
class CONTENT_EXPORT FrameTree {
 public:
  class NodeRange;

  class CONTENT_EXPORT NodeIterator
      : public std::iterator<std::forward_iterator_tag, FrameTreeNode> {
   public:
    NodeIterator(const NodeIterator& other);
    ~NodeIterator();

    NodeIterator& operator++();

    bool operator==(const NodeIterator& rhs) const;
    bool operator!=(const NodeIterator& rhs) const { return !(*this == rhs); }

    FrameTreeNode* operator*() { return current_node_; }

   private:
    friend class NodeRange;

    NodeIterator(FrameTreeNode* starting_node,
                 FrameTreeNode* root_of_subtree_to_skip);

    FrameTreeNode* current_node_;
    FrameTreeNode* const root_of_subtree_to_skip_;
    base::queue<FrameTreeNode*> queue_;
  };

  class CONTENT_EXPORT NodeRange {
   public:
    NodeIterator begin();
    NodeIterator end();

   private:
    friend class FrameTree;

    NodeRange(FrameTreeNode* root, FrameTreeNode* root_of_subtree_to_skip);

    FrameTreeNode* const root_;
    FrameTreeNode* const root_of_subtree_to_skip_;
  };

  // Each FrameTreeNode will default to using the given |navigator| for
  // navigation tasks in the frame.
  // A set of delegates are remembered here so that we can create
  // RenderFrameHostManagers.
  // TODO(creis): This set of delegates will change as we move things to
  // Navigator.
  FrameTree(NavigationControllerImpl* navigation_controller,
            NavigatorDelegate* navigator_delegate,
            RenderFrameHostDelegate* render_frame_delegate,
            RenderViewHostDelegate* render_view_delegate,
            RenderWidgetHostDelegate* render_widget_delegate,
            RenderFrameHostManager::Delegate* manager_delegate);
  ~FrameTree();

  FrameTreeNode* root() const { return root_; }

  // Delegates for RenderFrameHosts, RenderViewHosts, RenderWidgetHosts and
  // RenderFrameHostManagers. These can be kept centrally on the FrameTree
  // because they are expected to be the same for all frames on a given
  // FrameTree.
  RenderFrameHostDelegate* render_frame_delegate() {
    return render_frame_delegate_;
  }
  RenderViewHostDelegate* render_view_delegate() {
    return render_view_delegate_;
  }
  RenderWidgetHostDelegate* render_widget_delegate() {
    return render_widget_delegate_;
  }
  RenderFrameHostManager::Delegate* manager_delegate() {
    return manager_delegate_;
  }
  const std::unordered_map<int /* SiteInstance ID */, RenderViewHostImpl*>&
  render_view_hosts() const {
    return render_view_host_map_;
  }

  // Returns the FrameTreeNode with the given |frame_tree_node_id| if it is part
  // of this FrameTree.
  FrameTreeNode* FindByID(int frame_tree_node_id);

  // Returns the FrameTreeNode with the given renderer-specific |routing_id|.
  FrameTreeNode* FindByRoutingID(int process_id, int routing_id);

  // Returns the first frame in this tree with the given |name|, or the main
  // frame if |name| is empty.
  // Note that this does NOT support pseudo-names like _self, _top, and _blank,
  // nor searching other FrameTrees (unlike blink::WebView::findFrameByName).
  FrameTreeNode* FindByName(const std::string& name);

  // Returns a range to iterate over all FrameTreeNodes in the frame tree in
  // breadth-first traversal order.
  NodeRange Nodes();

  // Returns a range to iterate over all FrameTreeNodes in a subtree of the
  // frame tree, starting from |subtree_root|.
  NodeRange SubtreeNodes(FrameTreeNode* subtree_root);

  // Adds a new child frame to the frame tree. |process_id| is required to
  // disambiguate |new_routing_id|, and it must match the process of the
  // |parent| node. Otherwise no child is added and this method returns false.
  // |interface_provider_receiver| is the receiver end of the InterfaceProvider
  // interface through which the child RenderFrame can access Mojo services
  // exposed by the corresponding RenderFrameHost. The caller takes care of
  // sending the client end of the interface down to the RenderFrame.
  FrameTreeNode* AddFrame(
      RenderFrameHostImpl* parent,
      int process_id,
      int new_routing_id,
      mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>
          interface_provider_receiver,
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
          browser_interface_broker_receiver,
      blink::mojom::TreeScopeType scope,
      const std::string& frame_name,
      const std::string& frame_unique_name,
      bool is_created_by_script,
      const base::UnguessableToken& frame_token,
      const base::UnguessableToken& devtools_frame_token,
      const blink::FramePolicy& frame_policy,
      const blink::mojom::FrameOwnerProperties& frame_owner_properties,
      bool was_discarded,
      blink::mojom::FrameOwnerElementType owner_type);

  // Removes a frame from the frame tree. |child|, its children, and objects
  // owned by their RenderFrameHostManagers are immediately deleted. The root
  // node cannot be removed this way.
  void RemoveFrame(FrameTreeNode* child);

  // This method walks the entire frame tree and creates a RenderFrameProxyHost
  // for the given |site_instance| in each node except the |source| one --
  // the source will have a RenderFrameHost.  |source| may be null if there is
  // no node navigating in this frame tree (such as when this is called
  // for an opener's frame tree), in which case no nodes are skipped for
  // RenderFrameProxyHost creation.
  void CreateProxiesForSiteInstance(FrameTreeNode* source,
                                    SiteInstance* site_instance);

  // Convenience accessor for the main frame's RenderFrameHostImpl.
  RenderFrameHostImpl* GetMainFrame() const;

  // Returns the focused frame.
  FrameTreeNode* GetFocusedFrame();

  // Sets the focused frame to |node|.  |source| identifies the SiteInstance
  // that initiated this focus change.  If this FrameTree has SiteInstances
  // other than |source|, those SiteInstances will be notified about the new
  // focused frame.   Note that |source| may differ from |node|'s current
  // SiteInstance (e.g., this happens for cross-process window.focus() calls).
  void SetFocusedFrame(FrameTreeNode* node, SiteInstance* source);

  // Allows a client to listen for frame removal.  The listener should expect
  // to receive the RenderViewHostImpl containing the frame and the renderer-
  // specific frame routing ID of the removed frame.
  void SetFrameRemoveListener(
      base::RepeatingCallback<void(RenderFrameHost*)> on_frame_removed);

  // Creates a RenderViewHostImpl for a given |site_instance| in the tree.
  //
  // The RenderFrameHostImpls and the RenderFrameProxyHosts will share ownership
  // of this object.
  scoped_refptr<RenderViewHostImpl> CreateRenderViewHost(
      SiteInstance* site_instance,
      int32_t main_frame_routing_id,
      bool swapped_out);

  // Returns the existing RenderViewHost for a new RenderFrameHost.
  // There should always be such a RenderViewHost, because the main frame
  // RenderFrameHost for each SiteInstance should be created before subframes.
  scoped_refptr<RenderViewHostImpl> GetRenderViewHost(
      SiteInstance* site_instance);

  // Registers a RenderViewHost so that it can be reused by other frames
  // belonging to the same SiteInstance.
  //
  // This method does not take ownership of|rvh|.
  //
  // NOTE: This method CHECK fails if a RenderViewHost is already registered for
  // |rvh|'s SiteInstance.
  //
  // ALSO NOTE: After calling RegisterRenderViewHost, UnregisterRenderViewHost
  // *must* be called for |rvh| when it is destroyed or put into the
  // BackForwardCache, to prevent FrameTree::CreateRenderViewHost from trying to
  // reuse it.
  void RegisterRenderViewHost(RenderViewHostImpl* rvh);

  // Unregisters the RenderViewHostImpl that's available for reuse for a
  // particular SiteInstance. NOTE: This method CHECK fails if it is called for
  // a |render_view_host| that is not currently set for reuse.
  void UnregisterRenderViewHost(RenderViewHostImpl* render_view_host);

  // This is called when the frame is about to be removed and started to run
  // unload handlers.
  void FrameUnloading(FrameTreeNode* frame);

  // This is only meant to be called by FrameTreeNode. Triggers calling
  // the listener installed by SetFrameRemoveListener.
  void FrameRemoved(FrameTreeNode* frame);

  // Updates the overall load progress and notifies the WebContents.
  // Set based on the main frame's progress only.
  void UpdateLoadProgress(double progress);

  // Returns this FrameTree's total load progress.
  double load_progress() const { return load_progress_; }

  // Resets the load progress on all nodes in this FrameTree.
  void ResetLoadProgress();

  // Returns true if at least one of the nodes in this FrameTree is loading.
  bool IsLoading() const;

  // Set page-level focus in all SiteInstances involved in rendering
  // this FrameTree, not including the current main frame's
  // SiteInstance. The focus update will be sent via the main frame's proxies
  // in those SiteInstances.
  void ReplicatePageFocus(bool is_focused);

  // Updates page-level focus for this FrameTree in the subframe renderer
  // identified by |instance|.
  void SetPageFocus(SiteInstance* instance, bool is_focused);

  // Walks the current frame tree and registers any origins matching |origin|,
  // either the last committed origin of a RenderFrameHost or the origin
  // associated with a NavigationRequest that has been assigned to a
  // SiteInstance, as not-opted-in for origin isolation.
  void RegisterExistingOriginToPreventOptInIsolation(
      const url::Origin& previously_visited_origin,
      NavigationRequest* navigation_request_to_exclude);

  NavigationControllerImpl* controller() { return navigator_.controller(); }
  Navigator& navigator() { return navigator_; }

 private:
  friend class FrameTreeTest;
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBrowserTest, RemoveFocusedFrame);

  // Returns a range to iterate over all FrameTreeNodes in the frame tree in
  // breadth-first traversal order, skipping the subtree rooted at
  // |node|, but including |node| itself.
  NodeRange NodesExceptSubtree(FrameTreeNode* node);

  // These delegates are installed into all the RenderViewHosts and
  // RenderFrameHosts that we create.
  RenderFrameHostDelegate* render_frame_delegate_;
  RenderViewHostDelegate* render_view_delegate_;
  RenderWidgetHostDelegate* render_widget_delegate_;
  RenderFrameHostManager::Delegate* manager_delegate_;

  // The Navigator object responsible for managing navigations on this frame
  // tree.
  Navigator navigator_;

  // Map of SiteInstance ID to RenderViewHost. This allows us to look up the
  // RenderViewHost for a given SiteInstance when creating RenderFrameHosts.
  // Each RenderViewHost maintains a refcount and is deleted when there are no
  // more RenderFrameHosts or RenderFrameProxyHosts using it.
  std::unordered_map<int /* SiteInstance ID */, RenderViewHostImpl*>
      render_view_host_map_;

  // This is an owned ptr to the root FrameTreeNode, which never changes over
  // the lifetime of the FrameTree. It is not a scoped_ptr because we need the
  // pointer to remain valid even while the FrameTreeNode is being destroyed,
  // since it's common for a node to test whether it's the root node.
  FrameTreeNode* root_;

  int focused_frame_tree_node_id_;

  base::RepeatingCallback<void(RenderFrameHost*)> on_frame_removed_;

  // Overall load progress.
  double load_progress_;

  DISALLOW_COPY_AND_ASSIGN(FrameTree);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_FRAME_TREE_H_
