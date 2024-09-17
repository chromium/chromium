// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_FRAME_TREE_H_
#define CONTENT_BROWSER_RENDERER_HOST_FRAME_TREE_H_

#include <stdint.h>

#include <iterator>
#include <memory>
#include <string>
#include <unordered_map>

#include "base/containers/queue.h"
#include "base/dcheck_is_on.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/navigator_delegate.h"
#include "content/browser/renderer_host/render_frame_host_manager.h"
#include "content/browser/renderer_host/render_view_host_enums.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom-forward.h"
#include "url/origin.h"

namespace blink {
namespace mojom {
class BrowserInterfaceBroker;
enum class TreeScopeType;
}  // namespace mojom

struct FramePolicy;
}  // namespace blink

namespace content {

class BrowserContext;
class PageDelegate;
class RenderFrameHostDelegate;
class RenderViewHostDelegate;
class RenderViewHostImpl;
class RenderFrameHostManager;
class RenderWidgetHostDelegate;
class SiteInstance;
class SiteInstanceGroup;

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

  class CONTENT_EXPORT NodeIterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = FrameTreeNode*;
    using difference_type = std::ptrdiff_t;
    using pointer = FrameTreeNode**;
    using reference = FrameTreeNode*&;

    NodeIterator(const NodeIterator& other);
    ~NodeIterator();

    NodeIterator& operator++();
    // Advances the iterator and excludes the children of the current node
    NodeIterator& AdvanceSkippingChildren();

    bool operator==(const NodeIterator& rhs) const;
    bool operator!=(const NodeIterator& rhs) const { return !(*this == rhs); }

    FrameTreeNode* operator*() { return current_node_; }

   private:
    friend class FrameTreeTest;
    friend class NodeRange;

    NodeIterator(const std::vector<raw_ptr<FrameTreeNode, VectorExperimental>>&
                     starting_nodes,
                 const FrameTreeNode* root_of_subtree_to_skip,
                 bool should_descend_into_inner_trees,
                 bool include_delegate_nodes_for_inner_frame_trees);

    void AdvanceNode();

    // `current_node_` and `root_of_subtree_to_skip_` are not a raw_ptr<...> for
    // performance reasons (based on analysis of sampling profiler data and
    // tab_search:top100:2020).
    RAW_PTR_EXCLUSION FrameTreeNode* current_node_;
    RAW_PTR_EXCLUSION const FrameTreeNode* const root_of_subtree_to_skip_;

    const bool should_descend_into_inner_trees_;
    const bool include_delegate_nodes_for_inner_frame_trees_;
    base::circular_deque<FrameTreeNode*> queue_;
  };

  class CONTENT_EXPORT NodeRange {
   public:
    NodeRange(const NodeRange&);
    ~NodeRange();

    NodeIterator begin();
    NodeIterator end();

   private:
    friend class FrameTree;

    NodeRange(const std::vector<raw_ptr<FrameTreeNode, VectorExperimental>>&
                  starting_nodes,
              const FrameTreeNode* root_of_subtree_to_skip,
              bool should_descend_into_inner_trees,
              bool include_delegate_nodes_for_inner_frame_trees);

    const std::vector<raw_ptr<FrameTreeNode, VectorExperimental>>
        starting_nodes_;
    const raw_ptr<const FrameTreeNode> root_of_subtree_to_skip_;
    const bool should_descend_into_inner_trees_;
    const bool include_delegate_nodes_for_inner_frame_trees_;
  };

  class CONTENT_EXPORT Delegate {
   public:
    // The FrameTree changed its LoadingState. This can be a transition between
    // not-loading and loading (in which case it will be accompanied by either a
    // DidStartLoading or DidStopLoading), or a transition between not showing
    // loading UI and showing loading UI while a navigation is in progress (in
    // which case it will be called without either DidStartLoading or
    // DidStopLoading).
    virtual void LoadingStateChanged(LoadingState new_state) = 0;

    // The FrameTree has started loading in `frame_tree_node`. Note that this
    // is only called when the FrameTree as a whole goes from not-loading to
    // loading. If a second FrameTreeNode begins loading, a new DidStartLoading
    // message will not be sent.
    virtual void DidStartLoading(FrameTreeNode* frame_tree_node) = 0;

    // The FrameTree has stopped loading. Sent only when all FrameTreeNodes have
    // stopped loading.
    virtual void DidStopLoading() = 0;

    // Returns the delegate's top loading tree, which should be used to infer
    // the values of loading-related states. The state of
    // IsLoadingIncludingInnerFrameTrees() is a WebContents level concept and
    // LoadingTree would return the frame tree to which loading events should be
    // directed.
    //
    // TODO(crbug.com/40202416): Remove this method and directly rely on
    // GetOutermostMainFrame() once guest views are migrated to MPArch.
    virtual FrameTree* LoadingTree() = 0;

    // Returns true when the active RenderWidgetHostView should be hidden.
    virtual bool IsHidden() = 0;

    // If the FrameTree using this delegate is an inner/nested FrameTree, then
    // there may be a FrameTreeNode in the outer FrameTree that is considered
    // our outer delegate FrameTreeNode. This method returns the outer delegate
    // FrameTreeNode ID if one exists. If we don't have a an outer delegate
    // FrameTreeNode, this method returns an invalid value.
    virtual FrameTreeNodeId GetOuterDelegateFrameTreeNodeId() = 0;

    // If the FrameTree using this delegate is an inner/nested FrameTree that
    // has not yet been attached to an outer FrameTreeNode, returns the parent
    // RenderFrameHost of the intended outer FrameTreeNode to which the inner
    // frame tree will be attached. This is usually the RenderFrameHost that is
    // the outer document once attachment occurs, however in the case of some
    // kinds of GuestView, the outer document may end up being a same-origin
    // subframe of the RenderFrameHost returned by this method (see the
    // `testNewWindowAttachInSubFrame` webview test for an example of this).
    // Otherwise, returns null.
    virtual RenderFrameHostImpl* GetProspectiveOuterDocument() = 0;

    // Set the `node` frame as focused in its own FrameTree as well as possibly
    // changing the focused frame tree in the case of inner/outer FrameTrees.
    virtual void SetFocusedFrame(FrameTreeNode* node,
                                 SiteInstanceGroup* source) = 0;

    // Returns this FrameTree's picture-in-picture FrameTree if it has one.
    virtual FrameTree* GetOwnedPictureInPictureFrameTree() = 0;

    // Returns this FrameTree's opener if this FrameTree represents a
    // picture-in-picture window.
    virtual FrameTree* GetPictureInPictureOpenerFrameTree() = 0;
  };

  // Type of FrameTree instance.
  enum class Type {
    // This FrameTree is the primary frame tree for the WebContents, whose main
    // document URL is shown in the Omnibox.
    kPrimary,

    // This FrameTree is used to prerender a page in the background which is
    // invisible to the user.
    kPrerender,

    // This FrameTree is used to host the contents of a <fencedframe> element.
    //
    // Note that the FrameTree's Type should not be confused for
    // `RenderFrameHost::LifecycleState`. For example, when a <fencedframe> is
    // nested in a page in the bfcache, the FrameTree associated with the fenced
    // frame will be kFencedFrame, but the RenderFrameHosts inside of it will
    // have their lifecycle state indicate that they are bfcached.
    kFencedFrame,
  };

  // A set of delegates are remembered here so that we can create
  // RenderFrameHostManagers.
  FrameTree(BrowserContext* browser_context,
            Delegate* delegate,
            NavigationControllerDelegate* navigation_controller_delegate,
            NavigatorDelegate* navigator_delegate,
            RenderFrameHostDelegate* render_frame_delegate,
            RenderViewHostDelegate* render_view_delegate,
            RenderWidgetHostDelegate* render_widget_delegate,
            RenderFrameHostManager::Delegate* manager_delegate,
            PageDelegate* page_delegate,
            Type type);

  FrameTree(const FrameTree&) = delete;
  FrameTree& operator=(const FrameTree&) = delete;

  ~FrameTree();

  // Initializes the main frame for this FrameTree. That is it creates the
  // initial RenderFrameHost in the root node's RenderFrameHostManager, and also
  // creates an initial NavigationEntry that potentially inherits
  // `opener_for_origin`'s origin in its NavigationController. This method will
  // call back into the delegates so it should only be called once they have
  // completed their initialization. Pass in frame_policy so that it can be set
  // in the root node's replication_state.
  // TODO(carlscab): It would be great if initialization could happened in the
  // constructor so we do not leave objects in a half initialized state.
  void Init(SiteInstanceImpl* main_frame_site_instance,
            bool renderer_initiated_creation,
            const std::string& main_frame_name,
            RenderFrameHostImpl* opener_for_origin,
            const blink::FramePolicy& frame_policy,
            const base::UnguessableToken& devtools_frame_token);

  Type type() const { return type_; }

  FrameTreeNode* root() { return &root_; }
  const FrameTreeNode* root() const { return &root_; }

  bool is_primary() const { return type_ == Type::kPrimary; }
  bool is_prerendering() const { return type_ == Type::kPrerender; }
  bool is_fenced_frame() const { return type_ == Type::kFencedFrame; }

  Delegate* delegate() { return delegate_; }

  // Delegates for various objects.  These can be kept centrally on the
  // FrameTree because they are expected to be the same for all frames on a
  // given FrameTree.
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
  PageDelegate* page_delegate() { return page_delegate_; }

  // Iterate over all RenderViewHosts, including speculative RenderViewHosts.
  // See `speculative_render_view_host_` for more details.
  void ForEachRenderViewHost(
      base::FunctionRef<void(RenderViewHostImpl*)> on_host);

  // Speculative RenderViewHost accessors.
  RenderViewHostImpl* speculative_render_view_host() const {
    return speculative_render_view_host_.get();
  }
  void set_speculative_render_view_host(
      base::WeakPtr<RenderViewHostImpl> render_view_host) {
    speculative_render_view_host_ = render_view_host;
  }

  // Moves `speculative_render_view_host_` to `render_view_host_map_`. This
  // should be called every time a main-frame same-SiteInstanceGroup speculative
  // RenderFrameHost gets swapped in and becomes the active RenderFrameHost.
  // This overwrites the previous RenderViewHost for the SiteInstanceGroup in
  // `render_view_host_map_`, if one exists.
  void MakeSpeculativeRVHCurrent();

  // Returns the FrameTreeNode with the given |frame_tree_node_id| if it is part
  // of this FrameTree.
  FrameTreeNode* FindByID(FrameTreeNodeId frame_tree_node_id);

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

  // Returns a range to iterate over all FrameTreeNodes in this frame tree, as
  // well as any FrameTreeNodes of inner frame trees. Note that this includes
  // inner frame trees of inner WebContents as well.
  NodeRange NodesIncludingInnerTreeNodes();

  // Returns a range to iterate over all FrameTreeNodes in a subtree, starting
  // from, but not including |parent|, as well as any FrameTreeNodes of inner
  // frame trees. Note that this includes inner frame trees of inner WebContents
  // as well. If `include_delegate_nodes_for_inner_frame_trees` is true the
  // delegate RenderFrameHost owning the inner frame tree will also be returned.
  static NodeRange SubtreeAndInnerTreeNodes(
      RenderFrameHostImpl* parent,
      bool include_delegate_nodes_for_inner_frame_trees = false);

  // Adds a new child frame to the frame tree. |process_id| is required to
  // disambiguate |new_routing_id|, and it must match the process of the
  // |parent| node. Otherwise no child is added and this method returns nullptr.
  // |interface_provider_receiver| is the receiver end of the InterfaceProvider
  // interface through which the child RenderFrame can access Mojo services
  // exposed by the corresponding RenderFrameHost. The caller takes care of
  // sending the client end of the interface down to the
  // RenderFrame. |policy_container_bind_params|, if not null, is used for
  // binding Blink's policy container to the new RenderFrameHost's
  // PolicyContainerHost. This is only needed if this frame is the result of the
  // CreateChildFrame mojo call, which also delivers the
  // |policy_container_bind_params|. |is_dummy_frame_for_inner_tree| is true if
  // the added frame is only to serve as a placeholder for an inner frame tree
  // (e.g. fenced frames) and will not have a live RenderFrame of its
  // own.
  FrameTreeNode* AddFrame(
      RenderFrameHostImpl* parent,
      int process_id,
      int new_routing_id,
      mojo::PendingAssociatedRemote<mojom::Frame> frame_remote,
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
          browser_interface_broker_receiver,
      blink::mojom::PolicyContainerBindParamsPtr policy_container_bind_params,
      mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterfaceProvider>
          associated_interface_provider_receiver,
      blink::mojom::TreeScopeType scope,
      const std::string& frame_name,
      const std::string& frame_unique_name,
      bool is_created_by_script,
      const blink::LocalFrameToken& frame_token,
      const base::UnguessableToken& devtools_frame_token,
      const blink::DocumentToken& document_token,
      const blink::FramePolicy& frame_policy,
      const blink::mojom::FrameOwnerProperties& frame_owner_properties,
      bool was_discarded,
      blink::FrameOwnerElementType owner_type,
      bool is_dummy_frame_for_inner_tree);

  // Removes a frame from the frame tree. |child|, its children, and objects
  // owned by their RenderFrameHostManagers are immediately deleted. The root
  // node cannot be removed this way.
  void RemoveFrame(FrameTreeNode* child);

  // This method walks the entire frame tree and creates RenderFrameProxyHosts
  // as needed. Proxies are not created if suitable proxies already exist. See
  // below for special handling of |source|. Otherwise proxies are created for
  // the given |site_instance_group| in each node.
  //
  // |source| may be null if there is no node navigating in this frame tree
  // (such as when this is called for an opener's frame tree), in which case no
  // nodes are skipped for RenderFrameProxyHost creation. Otherwise, a proxy is
  // temporarily created for |source| in cross-SiteInstanceGroup cases (to allow
  // a remote-to-local swap to the new RenderFrameHost in |source|), but the
  // subtree rooted at source is skipped.
  // |source_new_browsing_context_state| is the BrowsingContextState used by the
  // speculative frame host, which may differ from the BrowsingContextState in
  // |source| during cross-origin cross- browsing-instance navigations.
  void CreateProxiesForSiteInstanceGroup(
      FrameTreeNode* source,
      SiteInstanceGroup* site_instance_group,
      const scoped_refptr<BrowsingContextState>&
          source_new_browsing_context_state);

  // Convenience accessor for the main frame's RenderFrameHostImpl.
  RenderFrameHostImpl* GetMainFrame() const;

  // Returns the focused frame.
  FrameTreeNode* GetFocusedFrame();

  // Sets the focused frame to |node|.  |source| identifies the
  // SiteInstanceGroup that initiated this focus change.  If this FrameTree has
  // SiteInstanceGroups other than |source|, those SiteInstanceGroups will be
  // notified about the new focused frame.   Note that |source| may differ from
  // |node|'s current SiteInstanceGroup (e.g., this happens for cross-process
  // window.focus() calls).
  void SetFocusedFrame(FrameTreeNode* node, SiteInstanceGroup* source);

  // Creates a RenderViewHostImpl for a given |site_instance| in the tree.
  //
  // The RenderFrameHostImpls and the RenderFrameProxyHosts will share ownership
  // of this object.
  // `create_case` indicates whether or not the RenderViewHost being created is
  // speculative or not. It should only be registered with the FrameTree if it
  // is not speculative.
  // `frame_sink_id` is optionally set only if we're creating a speculative
  // RenderViewHost. If set, it implies we're reusing the compositor from the
  // previous RenderViewHost.
  scoped_refptr<RenderViewHostImpl> CreateRenderViewHost(
      SiteInstanceGroup* site_instance_group,
      int32_t main_frame_routing_id,
      bool renderer_initiated_creation,
      scoped_refptr<BrowsingContextState> main_browsing_context_state,
      CreateRenderViewHostCase create_case,
      std::optional<viz::FrameSinkId> frame_sink_id);

  // Returns the existing RenderViewHost for a new RenderFrameHost.
  // There should always be such a RenderViewHost, because the main frame
  // RenderFrameHost for each SiteInstance should be created before subframes.
  // Note that this will never return `speculative_render_view_host_`. If that
  // is needed, call `speculative_render_view_host()` instead.
  scoped_refptr<RenderViewHostImpl> GetRenderViewHost(SiteInstanceGroup* group);

  using RenderViewHostMapId = base::IdType32<class RenderViewHostMap>;

  // Returns the ID used for the RenderViewHost associated with
  // |site_instance_group|.
  RenderViewHostMapId GetRenderViewHostMapId(
      SiteInstanceGroup* site_instance_group) const;

  // Registers a RenderViewHost so that it can be reused by other frames
  // whose SiteInstance maps to the same RenderViewHostMapId.
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
  void RegisterRenderViewHost(RenderViewHostMapId id, RenderViewHostImpl* rvh);

  // Unregisters the RenderViewHostImpl that's available for reuse for a
  // particular RenderViewHostMapId. NOTE: This method CHECK fails if it is
  // called for a |render_view_host| that is not currently set for reuse.
  void UnregisterRenderViewHost(RenderViewHostMapId id,
                                RenderViewHostImpl* render_view_host);

  // This is called when the frame is about to be removed and started to run
  // unload handlers.
  void FrameUnloading(FrameTreeNode* frame);

  // This is only meant to be called by FrameTreeNode. Triggers calling
  // the listener installed by SetFrameRemoveListener.
  void FrameRemoved(FrameTreeNode* frame);

  void NodeLoadingStateChanged(FrameTreeNode& node,
                               LoadingState previous_frame_tree_loading_state);
  void DidCancelLoading();

  // Returns this FrameTree's total load progress. If the `root_` FrameTreeNode
  // is navigating returns `blink::kInitialLoadProgress`.
  double GetLoadProgress();

  // Returns true if at least one of the nodes in this frame tree or nodes in
  // any inner frame tree of the same WebContents is loading.
  bool IsLoadingIncludingInnerFrameTrees() const;

  // Returns the LoadingState for the FrameTree as a whole, indicating whether
  // a load is in progress, as well as whether loading UI should be shown.
  LoadingState GetLoadingState() const;

  // Set page-level focus in all SiteInstances involved in rendering
  // this FrameTree, not including the current main frame's
  // SiteInstance. The focus update will be sent via the main frame's proxies
  // in those SiteInstances.
  void ReplicatePageFocus(bool is_focused);

  // Updates page-level focus for this FrameTree in the subframe renderer
  // identified by |group|.
  void SetPageFocus(SiteInstanceGroup* group, bool is_focused);

  // Walks the current frame tree and registers any origins matching
  // `previously_visited_origin`, either the last committed origin of a
  // RenderFrameHost or the origin associated with a NavigationRequest that has
  // been assigned to a SiteInstance, as having the default origin isolation
  // state. This is only necessary when `previously_visited_origin` is seen with
  // an OriginAgentCluster header explicitly requesting something other than the
  // default.
  void RegisterExistingOriginAsHavingDefaultIsolation(
      const url::Origin& previously_visited_origin,
      NavigationRequest* navigation_request_to_exclude);

  NavigationControllerImpl& controller() { return navigator_.controller(); }
  Navigator& navigator() { return navigator_; }

  // Another page accessed the initial empty main document, which means it
  // is no longer safe to display a pending URL without risking a URL spoof.
  void DidAccessInitialMainDocument();

  bool has_accessed_initial_main_document() const {
    return has_accessed_initial_main_document_;
  }

  void ResetHasAccessedInitialMainDocument() {
    has_accessed_initial_main_document_ = false;
  }

  bool IsHidden() const { return delegate_->IsHidden(); }

  // LoadingTree returns the following for different frame trees to direct
  // loading related events. Please see FrameTree::Delegate::LoadingTree for
  // more comments.
  // - For prerender frame tree -> returns the frame tree itself.
  // - For fenced frame and primary frame tree -> returns
  // the delegate's primary frame tree.
  FrameTree* LoadingTree();

  // Stops all ongoing navigations in each of the nodes of this FrameTree.
  void StopLoading();

  // Prepares this frame tree for destruction, cleaning up the internal state
  // and firing the appropriate events like FrameDeleted.
  // Must be called before FrameTree is destroyed.
  void Shutdown();

  bool IsBeingDestroyed() const { return is_being_destroyed_; }

  base::SafeRef<FrameTree> GetSafeRef();

  // Walks up the FrameTree chain and focuses the FrameTreeNode where
  // each inner FrameTree is attached.
  void FocusOuterFrameTrees();

  // Discards the frame tree. The root frame is transitioned to an empty
  // document in blink and BFCache entries are cleared. The tree is configured
  // to reload when activated.
  void Discard();

 private:
  friend class FrameTreeTest;
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBrowserTest, RemoveFocusedFrame);
  FRIEND_TEST_ALL_PREFIXES(FencedFrameMPArchBrowserTest, NodesForIsLoading);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           CreateRenderViewAfterProcessKillAndClosedProxy);

  // Returns a range to iterate over all FrameTreeNodes in the frame tree in
  // breadth-first traversal order, skipping the subtree rooted at
  // |node|, but including |node| itself.
  NodeRange NodesExceptSubtree(FrameTreeNode* node);

  // Returns all FrameTreeNodes in this frame tree, as well as any
  // FrameTreeNodes of inner frame trees. Note that this doesn't include inner
  // frame trees of inner delegates. This is used to find the aggregate
  // IsLoading value for a frame tree.
  //
  // TODO(crbug.com/1261928, crbug.com/1261928): Remove this method and directly
  // rely on GetOutermostMainFrame() and NodesIncludingInnerTreeNodes() once
  // guest views are migrated to MPArch.
  std::vector<FrameTreeNode*> CollectNodesForIsLoading();

  const raw_ptr<Delegate> delegate_;

  // These delegates are installed into all the RenderViewHosts and
  // RenderFrameHosts that we create.
  raw_ptr<RenderFrameHostDelegate> render_frame_delegate_;
  raw_ptr<RenderViewHostDelegate> render_view_delegate_;
  raw_ptr<RenderWidgetHostDelegate> render_widget_delegate_;
  raw_ptr<RenderFrameHostManager::Delegate> manager_delegate_;
  raw_ptr<PageDelegate> page_delegate_;

  // The Navigator object responsible for managing navigations on this frame
  // tree. Each FrameTreeNode will default to using it for navigation tasks in
  // the frame.
  Navigator navigator_;

  // A map to store RenderViewHosts, keyed by SiteInstanceGroup ID.
  // This map does not cover all RenderViewHosts in a FrameTree. See
  // `speculative_render_view_host_`.
  using RenderViewHostMap = std::unordered_map<RenderViewHostMapId,
                                               RenderViewHostImpl*,
                                               RenderViewHostMapId::Hasher>;
  // Map of RenderViewHostMapId to RenderViewHost. This allows us to look up the
  // RenderViewHost for a given SiteInstance when creating RenderFrameHosts.
  // Each RenderViewHost maintains a refcount and is deleted when there are no
  // more RenderFrameHosts or RenderFrameProxyHosts using it.
  RenderViewHostMap render_view_host_map_;

  // A speculative RenderViewHost is created for all speculative cross-page
  // same-SiteInstanceGroup RenderFrameHosts. When the corresponding
  // RenderFrameHost gets committed and becomes the current RenderFrameHost,
  // `speculative_render_view_host_` will be moved to `render_view_host_map_`,
  // overwriting the previous RenderViewHost of the same SiteInstanceGroup, if
  // applicable. This field will also be reset at that time, or if the
  // speculative RenderFrameHost gets deleted.
  //
  // For any given FrameTree, there will be at most one
  // `speculative_render_view_host_`, because only main-frame speculative
  // RenderFrameHosts have speculative RenderViewHosts, and there is at most one
  // such RenderFrameHost per FrameTree at a time.
  // This is a WeakPtr, since the RenderViewHost is owned by the
  // RenderFrameHostImpl, not the FrameTree. This implies that if the owning
  // RenderFrameHostImpl gets deleted, this will too.
  //
  // This supports but is independent of RenderDocument, which introduces cases
  // where there may be more than one RenderViewHost per SiteInstanceGroup, such
  // as cross-page same-SiteInstanceGroup navigations. The speculative
  // RenderFrameHost has an associated RenderViewHost, but it cannot be put in
  // `render_view_host_map_` when it is created, as the existing RenderViewHost
  // will be incorrectly overwritten.
  // TODO(yangsharon, crbug.com/1336305): Expand support to include
  // cross-SiteInstanceGroup main-frame navigations, so all main-frame
  // navigations use speculative RenderViewHost.
  base::WeakPtr<RenderViewHostImpl> speculative_render_view_host_;

  // Indicates type of frame tree.
  const Type type_;

  FrameTreeNodeId focused_frame_tree_node_id_;

  // Overall load progress.
  double load_progress_;

  // Whether the initial empty page has been accessed by another page, making it
  // unsafe to show the pending URL. Usually false unless another window tries
  // to modify the blank page.  Always false after the first commit.
  bool has_accessed_initial_main_document_ = false;

  bool is_being_destroyed_ = false;

#if DCHECK_IS_ON()
  // Whether Shutdown() was called.
  bool was_shut_down_ = false;
#endif

  // The root FrameTreeNode.
  //
  // Note: It is common for a node to test whether it's the root node, via the
  // `root()` method, even while `root_` is running its destructor.
  // For that reason, we want to destroy |root_| before any other fields.
  FrameTreeNode root_;

  base::WeakPtrFactory<FrameTree> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_FRAME_TREE_H_
