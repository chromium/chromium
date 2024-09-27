// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/frame_tree.h"

#include <stddef.h>

#include <queue>
#include <set>
#include <utility>

#include "base/containers/contains.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/safe_ref.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/trace_event/optional_trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "base/unguessable_token.h"
#include "content/browser/renderer_host/batched_proxy_ipc_sender.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/navigator_delegate.h"
#include "content/browser/renderer_host/page_impl.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_factory.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/content_switches_internal.h"
#include "content/common/features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/common/loader/loader_constants.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"

namespace content {

namespace {

using perfetto::protos::pbzero::ChromeTrackEvent;

// Helper function to collect SiteInstanceGroups involved in rendering a single
// FrameTree (which is a subset of SiteInstanceGroups in main frame's
// proxy_hosts_ because of openers).
std::set<SiteInstanceGroup*> CollectSiteInstanceGroups(FrameTree* tree) {
  std::set<SiteInstanceGroup*> groups;
  for (FrameTreeNode* node : tree->Nodes())
    groups.insert(node->current_frame_host()->GetSiteInstance()->group());
  return groups;
}

// If |node| is the placeholder FrameTreeNode for an embedded frame tree,
// returns the inner tree's main frame's FrameTreeNode. Otherwise, returns null.
FrameTreeNode* GetInnerTreeMainFrameNode(FrameTreeNode* node) {
  FrameTreeNode* inner_main_frame_tree_node = FrameTreeNode::GloballyFindByID(
      node->current_frame_host()->inner_tree_main_frame_tree_node_id());

  if (inner_main_frame_tree_node) {
    DCHECK_NE(&node->frame_tree(), &inner_main_frame_tree_node->frame_tree());
  }

  return inner_main_frame_tree_node;
}

}  // namespace

FrameTree::NodeIterator::NodeIterator(const NodeIterator& other) = default;

FrameTree::NodeIterator::~NodeIterator() = default;

FrameTree::NodeIterator& FrameTree::NodeIterator::operator++() {
  if (current_node_ != root_of_subtree_to_skip_) {
    // Reserve enough space in the queue to accommodate the nodes we're
    // going to add, to avoid repeated resize calls.
    queue_.reserve(queue_.size() + current_node_->child_count());

    for (size_t i = 0; i < current_node_->child_count(); ++i) {
      FrameTreeNode* child = current_node_->child_at(i);
      FrameTreeNode* inner_tree_main_ftn = GetInnerTreeMainFrameNode(child);
      if (should_descend_into_inner_trees_ && inner_tree_main_ftn) {
        if (include_delegate_nodes_for_inner_frame_trees_)
          queue_.push_back(child);
        queue_.push_back(inner_tree_main_ftn);
      } else {
        queue_.push_back(child);
      }
    }

    if (should_descend_into_inner_trees_) {
      auto unattached_nodes =
          current_node_->current_frame_host()
              ->delegate()
              ->GetUnattachedOwnedNodes(current_node_->current_frame_host());

      // Reserve enough space in the queue to accommodate the nodes we're
      // going to add.
      queue_.reserve(queue_.size() + unattached_nodes.size());

      for (auto* unattached_node : unattached_nodes) {
        queue_.push_back(unattached_node);
      }
    }
  }

  AdvanceNode();
  return *this;
}

FrameTree::NodeIterator& FrameTree::NodeIterator::AdvanceSkippingChildren() {
  AdvanceNode();
  return *this;
}

bool FrameTree::NodeIterator::operator==(const NodeIterator& rhs) const {
  return current_node_ == rhs.current_node_;
}

void FrameTree::NodeIterator::AdvanceNode() {
  if (!queue_.empty()) {
    current_node_ = queue_.front();
    queue_.pop_front();
  } else {
    current_node_ = nullptr;
  }
}

FrameTree::NodeIterator::NodeIterator(
    const std::vector<raw_ptr<FrameTreeNode, VectorExperimental>>&
        starting_nodes,
    const FrameTreeNode* root_of_subtree_to_skip,
    bool should_descend_into_inner_trees,
    bool include_delegate_nodes_for_inner_frame_trees)
    : current_node_(nullptr),
      root_of_subtree_to_skip_(root_of_subtree_to_skip),
      should_descend_into_inner_trees_(should_descend_into_inner_trees),
      include_delegate_nodes_for_inner_frame_trees_(
          include_delegate_nodes_for_inner_frame_trees),
      queue_(starting_nodes.begin(), starting_nodes.end()) {
  // If `include_delegate_nodes_for_inner_frame_trees_` is true then
  // `should_descend_into_inner_trees_` must be true.
  DCHECK(!include_delegate_nodes_for_inner_frame_trees_ ||
         should_descend_into_inner_trees_);
  AdvanceNode();
}

FrameTree::NodeIterator FrameTree::NodeRange::begin() {
  // We shouldn't be attempting a frame tree traversal while the tree is
  // being constructed or destructed.
  DCHECK(base::ranges::all_of(starting_nodes_, [](FrameTreeNode* ftn) {
    return ftn->current_frame_host();
  }));

  return NodeIterator(starting_nodes_, root_of_subtree_to_skip_,
                      should_descend_into_inner_trees_,
                      include_delegate_nodes_for_inner_frame_trees_);
}

FrameTree::NodeIterator FrameTree::NodeRange::end() {
  return NodeIterator({}, nullptr, should_descend_into_inner_trees_,
                      include_delegate_nodes_for_inner_frame_trees_);
}

FrameTree::NodeRange::NodeRange(
    const std::vector<raw_ptr<FrameTreeNode, VectorExperimental>>&
        starting_nodes,
    const FrameTreeNode* root_of_subtree_to_skip,
    bool should_descend_into_inner_trees,
    bool include_delegate_nodes_for_inner_frame_trees)
    : starting_nodes_(starting_nodes),
      root_of_subtree_to_skip_(root_of_subtree_to_skip),
      should_descend_into_inner_trees_(should_descend_into_inner_trees),
      include_delegate_nodes_for_inner_frame_trees_(
          include_delegate_nodes_for_inner_frame_trees) {}

FrameTree::NodeRange::NodeRange(const NodeRange&) = default;
FrameTree::NodeRange::~NodeRange() = default;

FrameTree::FrameTree(
    BrowserContext* browser_context,
    Delegate* delegate,
    NavigationControllerDelegate* navigation_controller_delegate,
    NavigatorDelegate* navigator_delegate,
    RenderFrameHostDelegate* render_frame_delegate,
    RenderViewHostDelegate* render_view_delegate,
    RenderWidgetHostDelegate* render_widget_delegate,
    RenderFrameHostManager::Delegate* manager_delegate,
    PageDelegate* page_delegate,
    Type type)
    : delegate_(delegate),
      render_frame_delegate_(render_frame_delegate),
      render_view_delegate_(render_view_delegate),
      render_widget_delegate_(render_widget_delegate),
      manager_delegate_(manager_delegate),
      page_delegate_(page_delegate),
      navigator_(browser_context,
                 *this,
                 navigator_delegate,
                 navigation_controller_delegate),
      type_(type),
      load_progress_(0.0),
      root_(*this,
            nullptr,
            // The top-level frame must always be in a
            // document scope.
            blink::mojom::TreeScopeType::kDocument,
            false,
            blink::mojom::FrameOwnerProperties(),
            blink::FrameOwnerElementType::kNone,
            blink::FramePolicy()) {}

FrameTree::~FrameTree() {
  is_being_destroyed_ = true;
#if DCHECK_IS_ON()
  DCHECK(was_shut_down_);
#endif
}

void FrameTree::ForEachRenderViewHost(
    base::FunctionRef<void(RenderViewHostImpl*)> on_host) {
  if (speculative_render_view_host_) {
    on_host(speculative_render_view_host_.get());
  }

  for (auto& rvh : render_view_host_map_) {
    on_host(rvh.second);
  }
}

void FrameTree::MakeSpeculativeRVHCurrent() {
  CHECK(speculative_render_view_host_);

  // The existing RenderViewHost needs to be unregistered first.
  // Speculative RenderViewHosts are only used for same-SiteInstanceGroup
  // navigations, so there should be a RenderViewHost of the same
  // SiteInstanceGroup already in the tree.
  RenderViewHostMapId speculative_id =
      speculative_render_view_host_->rvh_map_id();
  auto it = render_view_host_map_.find(speculative_id);
  CHECK(it != render_view_host_map_.end());
  UnregisterRenderViewHost(speculative_id, it->second);

  speculative_render_view_host_->set_is_speculative(false);
  RegisterRenderViewHost(speculative_id, speculative_render_view_host_.get());
  speculative_render_view_host_.reset();
}

FrameTreeNode* FrameTree::FindByID(FrameTreeNodeId frame_tree_node_id) {
  for (FrameTreeNode* node : Nodes()) {
    if (node->frame_tree_node_id() == frame_tree_node_id)
      return node;
  }
  return nullptr;
}

FrameTreeNode* FrameTree::FindByRoutingID(int process_id, int routing_id) {
  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(process_id, routing_id);
  if (render_frame_host) {
    FrameTreeNode* result = render_frame_host->frame_tree_node();
    if (this == &result->frame_tree())
      return result;
  }

  RenderFrameProxyHost* render_frame_proxy_host =
      RenderFrameProxyHost::FromID(process_id, routing_id);
  if (render_frame_proxy_host) {
    FrameTreeNode* result = render_frame_proxy_host->frame_tree_node();
    if (this == &result->frame_tree())
      return result;
  }

  return nullptr;
}

FrameTreeNode* FrameTree::FindByName(const std::string& name) {
  if (name.empty())
    return &root_;

  for (FrameTreeNode* node : Nodes()) {
    if (node->frame_name() == name)
      return node;
  }

  return nullptr;
}

FrameTree::NodeRange FrameTree::Nodes() {
  return NodesExceptSubtree(nullptr);
}

FrameTree::NodeRange FrameTree::SubtreeNodes(FrameTreeNode* subtree_root) {
  return NodeRange({subtree_root}, nullptr,
                   /*should_descend_into_inner_trees=*/false,
                   /*include_delegate_nodes_for_inner_frame_trees=*/false);
}

FrameTree::NodeRange FrameTree::NodesIncludingInnerTreeNodes() {
  return NodeRange({&root_}, nullptr,
                   /*should_descend_into_inner_trees=*/true,
                   /*include_delegate_nodes_for_inner_frame_trees=*/false);
}

std::vector<FrameTreeNode*> FrameTree::CollectNodesForIsLoading() {
  FrameTree::NodeRange node_range = NodesIncludingInnerTreeNodes();
  FrameTree::NodeIterator node_iter = node_range.begin();
  std::vector<FrameTreeNode*> nodes;

  CHECK(node_iter != node_range.end(), base::NotFatalUntil::M130);
  FrameTree* root_loading_tree = root_.frame_tree().LoadingTree();
  while (node_iter != node_range.end()) {
    // Skip over frame trees and children which belong to inner web contents
    // i.e., when nodes doesn't point to the same loading frame tree.
    if ((*node_iter)->frame_tree().LoadingTree() != root_loading_tree) {
      node_iter.AdvanceSkippingChildren();
    } else {
      nodes.push_back(*node_iter);
      ++node_iter;
    }
  }
  return nodes;
}

FrameTree::NodeRange FrameTree::SubtreeAndInnerTreeNodes(
    RenderFrameHostImpl* parent,
    bool include_delegate_nodes_for_inner_frame_trees) {
  std::vector<raw_ptr<FrameTreeNode, VectorExperimental>> starting_nodes;
  starting_nodes.reserve(parent->child_count());
  for (size_t i = 0; i < parent->child_count(); ++i) {
    FrameTreeNode* child = parent->child_at(i);
    FrameTreeNode* inner_tree_main_ftn = GetInnerTreeMainFrameNode(child);
    if (inner_tree_main_ftn) {
      if (include_delegate_nodes_for_inner_frame_trees)
        starting_nodes.push_back(child);
      starting_nodes.push_back(inner_tree_main_ftn);
    } else {
      starting_nodes.push_back(child);
    }
  }
  const std::vector<FrameTreeNode*> unattached_owned_nodes =
      parent->delegate()->GetUnattachedOwnedNodes(parent);
  starting_nodes.insert(starting_nodes.end(), unattached_owned_nodes.begin(),
                        unattached_owned_nodes.end());
  return NodeRange(starting_nodes, nullptr,
                   /* should_descend_into_inner_trees */ true,
                   include_delegate_nodes_for_inner_frame_trees);
}

FrameTree::NodeRange FrameTree::NodesExceptSubtree(FrameTreeNode* node) {
  return NodeRange({&root_}, node, /*should_descend_into_inner_trees=*/false,
                   /*include_delegate_nodes_for_inner_frame_trees=*/false);
}

FrameTree* FrameTree::LoadingTree() {
  // We return the delegate's loading frame tree to infer loading related
  // states.
  return delegate_->LoadingTree();
}

FrameTreeNode* FrameTree::AddFrame(
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
    bool is_dummy_frame_for_inner_tree) {
  CHECK_NE(new_routing_id, MSG_ROUTING_NONE);
  // Normally this path is for blink adding a child local frame. But fenced
  // frames add a dummy child frame that never gets a corresponding
  // RenderFrameImpl in any renderer process, and therefore its `frame_remote`
  // is invalid. Also its RenderFrameHostImpl is exempt from having
  // `RenderFrameCreated()` called on it (see later in this method, as well as
  // `WebContentsObserverConsistencyChecker::RenderFrameHostChanged()`).
  DCHECK_NE(frame_remote.is_valid(), is_dummy_frame_for_inner_tree);
  DCHECK_NE(browser_interface_broker_receiver.is_valid(),
            is_dummy_frame_for_inner_tree);
  DCHECK_NE(associated_interface_provider_receiver.is_valid(),
            is_dummy_frame_for_inner_tree);

  // A child frame always starts with an initial empty document, which means
  // it is in the same SiteInstance as the parent frame. Ensure that the process
  // which requested a child frame to be added is the same as the process of the
  // parent node.
  CHECK_EQ(parent->GetProcess()->GetID(), process_id);

  std::unique_ptr<FrameTreeNode> new_node = base::WrapUnique(
      new FrameTreeNode(*this, parent, scope, is_created_by_script,
                        frame_owner_properties, owner_type, frame_policy));

  // Set sandbox flags and container policy and make them effective immediately,
  // since initial sandbox flags and permissions policy should apply to the
  // initial empty document in the frame. This needs to happen before the call
  // to AddChild so that the effective policy is sent to any newly-created
  // `blink::RemoteFrame` objects when the RenderFrameHost is created.
  // SetPendingFramePolicy is necessary here because next navigation on this
  // frame will need the value of pending frame policy instead of effective
  // frame policy.
  new_node->SetPendingFramePolicy(frame_policy);

  if (was_discarded)
    new_node->set_was_discarded();

  // Add the new node to the FrameTree, creating the RenderFrameHost.
  FrameTreeNode* added_node = parent->AddChild(
      std::move(new_node), new_routing_id, std::move(frame_remote), frame_token,
      document_token, devtools_frame_token, frame_policy, frame_name,
      frame_unique_name);

  added_node->SetFencedFramePropertiesIfNeeded();

  if (browser_interface_broker_receiver.is_valid()) {
    added_node->current_frame_host()->BindBrowserInterfaceBrokerReceiver(
        std::move(browser_interface_broker_receiver));
  }

  if (policy_container_bind_params) {
    added_node->current_frame_host()->policy_container_host()->Bind(
        std::move(policy_container_bind_params));
  }

  if (associated_interface_provider_receiver.is_valid()) {
    added_node->current_frame_host()->BindAssociatedInterfaceProviderReceiver(
        std::move(associated_interface_provider_receiver));
  }

  // The last committed NavigationEntry may have a FrameNavigationEntry with the
  // same |frame_unique_name|, since we don't remove FrameNavigationEntries if
  // their frames are deleted.  If there is a stale one, remove it to avoid
  // conflicts on future updates.
  NavigationEntryImpl* last_committed_entry = static_cast<NavigationEntryImpl*>(
      navigator_.controller().GetLastCommittedEntry());
  if (last_committed_entry) {
    last_committed_entry->RemoveEntryForFrame(
        added_node, /* only_if_different_position = */ true);
  }

  // Now that the new node is part of the FrameTree and has a RenderFrameHost,
  // we can announce the creation of the initial RenderFrame which already
  // exists in the renderer process.
  // For consistency with navigating to a new RenderFrameHost case, we dispatch
  // RenderFrameCreated before RenderFrameHostChanged.
  if (!is_dummy_frame_for_inner_tree) {
    // The outer dummy FrameTreeNode for fenced frames does not have a live
    // RenderFrame in the renderer process.
    added_node->current_frame_host()->RenderFrameCreated();
  }

  // Notify the delegate of the creation of the current RenderFrameHost.
  // This is only for subframes, as the main frame case is taken care of by
  // WebContentsImpl::Init.
  manager_delegate_->NotifySwappedFromRenderManager(
      nullptr, added_node->current_frame_host());
  return added_node;
}

void FrameTree::RemoveFrame(FrameTreeNode* child) {
  RenderFrameHostImpl* parent = child->parent();
  if (!parent) {
    NOTREACHED_IN_MIGRATION() << "Unexpected RemoveFrame call for main frame.";
    return;
  }

  parent->RemoveChild(child);
}

void FrameTree::CreateProxiesForSiteInstanceGroup(
    FrameTreeNode* source,
    SiteInstanceGroup* site_instance_group,
    const scoped_refptr<BrowsingContextState>&
        source_new_browsing_context_state) {
  // Will be instantiated with the root proxy later and passed to
  // `CreateRenderFrameProxy()` to batch create proxies for child frames.
  std::unique_ptr<BatchedProxyIPCSender> batched_proxy_ipc_sender;

  if (!source || !source->IsMainFrame()) {
    RenderViewHostImpl* render_view_host =
        GetRenderViewHost(site_instance_group).get();
    if (render_view_host) {
      root()->render_manager()->EnsureRenderViewInitialized(
          render_view_host, site_instance_group);
    } else {
      // Due to the check above, we are creating either an opener proxy (when
      // source is null) or a main frame proxy due to a subframe navigation
      // (when source is not a main frame). In the former case, we should use
      // root's current BrowsingContextState, while in the latter case we should
      // use BrowsingContextState from the main RenderFrameHost of the subframe
      // being navigated. We want to ensure that the `blink::WebView` is created
      // in the right SiteInstanceGroup if it doesn't exist, before creating the
      // other proxies; if the `blink::WebView` doesn't exist, the only way to
      // do this is to also create a proxy for the main frame as well.
      const scoped_refptr<BrowsingContextState>& root_browsing_context_state =
          source ? source->parent()->GetMainFrame()->browsing_context_state()
                 : root()->current_frame_host()->browsing_context_state();

      // TODO(crbug.com/40248300): Batch main frame proxy creation and
      // pass an instance of `BatchedProxyIPCSender` here instead of nullptr.
      root()->render_manager()->CreateRenderFrameProxy(
          site_instance_group, root_browsing_context_state,
          /*batched_proxy_ipc_sender=*/nullptr);

      // We only need to use `BatchedProxyIPCSender` when navigating to a new
      // SiteInstanceGroup. Proxies do not need to be created when navigating to
      // a SiteInstanceGroup that has already been encountered, because site
      // isolation would guarantee that all nodes already have either proxies
      // or real frames. Due to the check above, the `render_view_host` does
      // not exist here, which means we have not seen this SiteInstanceGroup
      // before, so we instantiate `batched_proxy_ipc_sender` to consolidate
      // IPCs for proxy creation.
      base::SafeRef<RenderFrameProxyHost> root_proxy =
          root_browsing_context_state
              ->GetRenderFrameProxyHost(site_instance_group)
              ->GetSafeRef();
      batched_proxy_ipc_sender =
          std::make_unique<BatchedProxyIPCSender>(std::move(root_proxy));
    }
  }

  // Check whether we're in an inner delegate and the |site_instance_group|
  // corresponds to the outer delegate. Subframe proxies aren't needed if this
  // is the case.
  bool is_site_instance_group_for_outer_delegate = false;
  RenderFrameProxyHost* outer_delegate_proxy =
      root()->render_manager()->GetProxyToOuterDelegate();
  if (outer_delegate_proxy) {
    is_site_instance_group_for_outer_delegate =
        (site_instance_group == outer_delegate_proxy->site_instance_group());
  }

  // Proxies are created in the FrameTree in response to a node navigating to a
  // new SiteInstanceGroup. Since |source|'s navigation will replace the
  // currently loaded document, the entire subtree under |source| will be
  // removed, and thus proxy creation is skipped for all nodes in that subtree.
  //
  // However, a proxy *is* needed for the |source| node if it is
  // cross-SiteInstanceGroup from the current node. This lets cross-process
  // navigations in |source| start with a proxy and follow a remote-to-local
  // transition, which avoids race conditions in cases where other navigations
  // need to reference |source| before it commits. See https://crbug.com/756790
  // for more background. Therefore, NodesExceptSubtree(source) will include
  // |source| in the nodes traversed (see NodeIterator::operator++).
  for (FrameTreeNode* node : NodesExceptSubtree(source)) {
    // If a new frame is created in the current SiteInstanceGroup, other frames
    // in that SiteInstanceGroup don't need a proxy for the new frame.
    RenderFrameHostImpl* current_host =
        node->render_manager()->current_frame_host();
    SiteInstanceGroup* current_group = current_host->GetSiteInstance()->group();

    // Check that the proxy is for a different SiteInstanceGroup. This ensures
    // that a navigation within a SiteInstanceGroup does not cause proxies to be
    // created. That then allows the Blink side to do a local to local frame
    // transition within the same process.
    if (current_group != site_instance_group) {
      if (node == source && !current_host->IsRenderFrameLive()) {
        // We don't create a proxy at |source| when the current RenderFrameHost
        // isn't live.  This is because either (1) the speculative
        // RenderFrameHost will be committed immediately, and the proxy
        // destroyed right away, in GetFrameHostForNavigation, which makes the
        // races above impossible, or (2) the early commit will be skipped due
        // to ShouldSkipEarlyCommitPendingForCrashedFrame, in which case the
        // proxy for |source| *is* needed, but it will be created later in
        // CreateProxiesForNewRenderFrameHost.
        //
        // TODO(fergal): Consider creating a proxy for |source| here rather than
        // in CreateProxiesForNewRenderFrameHost for case (2) above.
        continue;
      }

      // Do not create proxies for subframes in the outer delegate's
      // SiteInstanceGroup, since there is no need to expose these subframes to
      // the outer delegate.  See also comments in CreateProxiesForChildFrame()
      // and https://crbug.com/1013553.
      if (!node->IsMainFrame() && is_site_instance_group_for_outer_delegate) {
        continue;
      }

      // If |node| is the FrameTreeNode being navigated, we use
      // |browsing_context_state| (as BrowsingContextState might change for
      // cross-BrowsingInstance navigations). Otherwise, we should use the
      // |node|'s current BrowsingContextState.
      node->render_manager()->CreateRenderFrameProxy(
          site_instance_group,
          node == source ? source_new_browsing_context_state
                         : node->current_frame_host()->browsing_context_state(),
          batched_proxy_ipc_sender.get());
    }
  }

  if (batched_proxy_ipc_sender) {
    batched_proxy_ipc_sender->CreateAllProxies();
  }
}

RenderFrameHostImpl* FrameTree::GetMainFrame() const {
  return root_.current_frame_host();
}

FrameTreeNode* FrameTree::GetFocusedFrame() {
  return FindByID(focused_frame_tree_node_id_);
}

void FrameTree::SetFocusedFrame(FrameTreeNode* node,
                                SiteInstanceGroup* source) {
  CHECK(node->current_frame_host()->IsActive());
  if (node == GetFocusedFrame())
    return;

  std::set<SiteInstanceGroup*> frame_tree_groups =
      CollectSiteInstanceGroups(this);

  SiteInstanceGroup* current_group =
      node->current_frame_host()->GetSiteInstance()->group();

  // Update the focused frame in all other SiteInstanceGroups.  If focus changes
  // to a cross-group frame, this allows the old focused frame's renderer
  // process to clear focus from that frame and fire blur events.  It also
  // ensures that the latest focused frame is available in all renderers to
  // compute document.activeElement.
  //
  // We do not notify the |source| SiteInstanceGroup because it already knows
  // the new focused frame (since it initiated the focus change), and we notify
  // the new focused frame's SiteInstanceGroup (if it differs from |source|)
  // separately below.
  for (auto* group : frame_tree_groups) {
    if (group != source && group != current_group) {
      RenderFrameProxyHost* proxy = node->current_frame_host()
                                        ->browsing_context_state()
                                        ->GetRenderFrameProxyHost(group);

      if (proxy) {
        proxy->SetFocusedFrame();
      } else {
        base::debug::DumpWithoutCrashing();
      }
    }
  }

  // If |node| was focused from a cross-group frame (i.e., via
  // window.focus()), tell its RenderFrame that it should focus.
  if (current_group != source)
    node->current_frame_host()->SetFocusedFrame();

  focused_frame_tree_node_id_ = node->frame_tree_node_id();
  node->DidFocus();

  // The accessibility tree data for the root of the frame tree keeps
  // track of the focused frame too, so update that every time the
  // focused frame changes.
  root()
      ->current_frame_host()
      ->GetOutermostMainFrameOrEmbedder()
      ->UpdateAXTreeData();
}

scoped_refptr<RenderViewHostImpl> FrameTree::CreateRenderViewHost(
    SiteInstanceGroup* site_instance_group,
    int32_t main_frame_routing_id,
    bool renderer_initiated_creation,
    scoped_refptr<BrowsingContextState> main_browsing_context_state,
    CreateRenderViewHostCase create_case,
    std::optional<viz::FrameSinkId> frame_sink_id) {
  if (main_browsing_context_state) {
    DCHECK(main_browsing_context_state->is_main_frame());
  }
  RenderViewHostImpl* rvh =
      static_cast<RenderViewHostImpl*>(RenderViewHostFactory::Create(
          this, site_instance_group,
          site_instance_group->GetStoragePartitionConfig(),
          render_view_delegate_, render_widget_delegate_, main_frame_routing_id,
          renderer_initiated_creation, std::move(main_browsing_context_state),
          create_case, frame_sink_id));

  if (create_case == CreateRenderViewHostCase::kSpeculative) {
    set_speculative_render_view_host(rvh->GetWeakPtr());
  } else {
    // Register non-speculative RenderViewHosts. If they are speculative, they
    // will be registered when they become active.
    RegisterRenderViewHost(rvh->rvh_map_id(), rvh);
  }

  return base::WrapRefCounted(rvh);
}

scoped_refptr<RenderViewHostImpl> FrameTree::GetRenderViewHost(
    SiteInstanceGroup* group) {
  // When called from RenderFrameHostManager::CreateRenderFrameHost, it's
  // possible that a RenderProcessHost hasn't yet been created, which means
  // a SiteInstanceGroup won't have been created yet.
  if (!group)
    return nullptr;

  auto it = render_view_host_map_.find(GetRenderViewHostMapId(group));
  if (it == render_view_host_map_.end())
    return nullptr;

  return base::WrapRefCounted(it->second);
}

FrameTree::RenderViewHostMapId FrameTree::GetRenderViewHostMapId(
    SiteInstanceGroup* site_instance_group) const {
  return RenderViewHostMapId::FromUnsafeValue(
      site_instance_group->GetId().value());
}

void FrameTree::RegisterRenderViewHost(RenderViewHostMapId id,
                                       RenderViewHostImpl* rvh) {
  TRACE_EVENT_INSTANT("navigation", "FrameTree::RegisterRenderViewHost",
                      ChromeTrackEvent::kRenderViewHost, *rvh);
  CHECK(!rvh->is_speculative());
  bool rvh_id_already_in_map = base::Contains(render_view_host_map_, id);
  bool rfh_in_bfcache =
      controller()
          .GetBackForwardCache()
          .IsRenderFrameHostWithSIGInBackForwardCacheForDebugging(
              rvh->site_instance_group()->GetId());
  bool rfph_in_bfcache =
      controller()
          .GetBackForwardCache()
          .IsRenderFrameProxyHostWithSIGInBackForwardCacheForDebugging(
              rvh->site_instance_group()->GetId());
  bool rvh_in_bfcache =
      controller()
          .GetBackForwardCache()
          .IsRenderViewHostWithMapIdInBackForwardCacheForDebugging(*rvh);
  // We're seeing cases where an RVH being restored from BFCache has the same
  // ID as an RVH already in the map, where the 2 RVHs are different but one
  // was in BFCache and one isn't.
  // To investigate, detect if any of these cases happen:
  // 1) A RenderViewHost with the same ID as `rvh` is already in the map
  // 2) A RenderFrameHost with the same SIG ID as `rvh` is in BFCache
  // 3) A RenderFrameProxyHost with the same SIG ID as `rvh` is in BFCache
  // 4) A RenderViewHost with the same ID as `rvh` is in BFCache
  // These cases shouldn't be possible. Note that when checking #2-#4 for
  // a RenderViewHost that is getting out of BFCache, we are guaranteed to not
  // accidentally match to the RVH/RFPH/RFH of the page being restored,
  // because we can only get here after the StoredPage is taken out of the
  // BFCache and thus won't be iterated over in the functions above.
  // See the linked bug below for more details.
  if (rvh_id_already_in_map || rfh_in_bfcache || rfph_in_bfcache ||
      rvh_in_bfcache) {
    // TODO(https://crbug.com/354382462): Remove crash keys once investigation
    // is done.
    SCOPED_CRASH_KEY_BOOL("rvh-double", "in_map", rvh_id_already_in_map);
    SCOPED_CRASH_KEY_BOOL("rvh-double", "rfh_in_bfcache", rfh_in_bfcache);
    SCOPED_CRASH_KEY_BOOL("rvh-double", "rfph_in_bfcache", rfph_in_bfcache);
    SCOPED_CRASH_KEY_BOOL("rvh-double", "rvh_in_bfcache", rvh_in_bfcache);
    SCOPED_CRASH_KEY_BOOL("rvh-double", "passed_renderer_created",
                          rvh->renderer_view_created());
    SCOPED_CRASH_KEY_NUMBER("rvh-double", "passed_rvh_main_id",
                            rvh->main_frame_routing_id());
    SCOPED_CRASH_KEY_NUMBER("rvh-double", "root_routing_id",
                            root()->current_frame_host()->GetRoutingID());
    SCOPED_CRASH_KEY_NUMBER("rvh-double", "passed_rvh_ptr",
                            reinterpret_cast<size_t>(rvh));
    SCOPED_CRASH_KEY_BOOL("rvh-double", "passed_rvh_bfcache",
                          rvh->is_in_back_forward_cache());
    SCOPED_CRASH_KEY_BOOL("rvh-double", "frame_tree_primary", is_primary());

    if (rvh_id_already_in_map) {
      SCOPED_CRASH_KEY_BOOL(
          "rvh-double", "mapped_rvh_registered",
          render_view_host_map_[id]->is_registered_with_frame_tree());
      SCOPED_CRASH_KEY_NUMBER(
          "rvh-double", "mapped_rvh_main_id",
          render_view_host_map_[id]->main_frame_routing_id());
      SCOPED_CRASH_KEY_NUMBER(
          "rvh-double", "map_rvh_ptr",
          reinterpret_cast<size_t>(render_view_host_map_[id]));
      SCOPED_CRASH_KEY_BOOL(
          "rvh-double", "map_rvh_bfcache",
          render_view_host_map_[id]->is_in_back_forward_cache());
      SCOPED_CRASH_KEY_BOOL("rvh-double", "mapped_renderer_created",
                            render_view_host_map_[id]->renderer_view_created());
      CHECK_EQ(rvh, render_view_host_map_[id]);
    }
  }
  render_view_host_map_[id] = rvh;
  rvh->set_is_registered_with_frame_tree(true);
}

void FrameTree::UnregisterRenderViewHost(RenderViewHostMapId id,
                                         RenderViewHostImpl* rvh) {
  TRACE_EVENT_INSTANT("navigation", "FrameTree::UnregisterRenderViewHost",
                      ChromeTrackEvent::kRenderViewHost, *rvh);
  CHECK(!rvh->is_speculative());
  auto it = render_view_host_map_.find(id);
  CHECK(it != render_view_host_map_.end());
  CHECK_EQ(it->second, rvh);
  render_view_host_map_.erase(it);
  rvh->set_is_registered_with_frame_tree(false);
}

void FrameTree::FrameUnloading(FrameTreeNode* frame) {
  if (frame->frame_tree_node_id() == focused_frame_tree_node_id_)
    focused_frame_tree_node_id_ = FrameTreeNodeId();

  // Ensure frames that are about to be deleted aren't visible from the other
  // processes anymore.
  frame->GetBrowsingContextStateForSubframe()->ResetProxyHosts();
}

void FrameTree::FrameRemoved(FrameTreeNode* frame) {
  if (frame->frame_tree_node_id() == focused_frame_tree_node_id_)
    focused_frame_tree_node_id_ = FrameTreeNodeId();
}

double FrameTree::GetLoadProgress() {
  if (root_.HasNavigation())
    return blink::kInitialLoadProgress;

  return root_.current_frame_host()->GetPage().load_progress();
}

bool FrameTree::IsLoadingIncludingInnerFrameTrees() const {
  return GetLoadingState() != LoadingState::NONE;
}

LoadingState FrameTree::GetLoadingState() const {
  // The overall loading state for the FrameTree matches the root node's loading
  // state if the root is loading.
  if (root_.GetLoadingState() != LoadingState::NONE) {
    return root_.GetLoadingState();
  }

  // Otherwise, check if a subframe is loading without an associated navigation
  // in the root frame. If so, we are loading, but we don't want to show
  // loading UI.
  for (const FrameTreeNode* node_to_check :
       const_cast<FrameTree*>(this)->CollectNodesForIsLoading()) {
    if (node_to_check->IsLoading()) {
      return LoadingState::LOADING_WITHOUT_UI;
    }
  }
  return LoadingState::NONE;
}

void FrameTree::ReplicatePageFocus(bool is_focused) {
  // Focus loss may occur while this FrameTree is being destroyed.  Don't
  // send the message in this case, as the main frame's RenderFrameHost and
  // other state has already been cleared.
  if (is_being_destroyed_)
    return;
  std::set<SiteInstanceGroup*> frame_tree_site_instance_groups =
      CollectSiteInstanceGroups(this);

  // Send the focus update to main frame's proxies in all SiteInstanceGroups of
  // other frames in this FrameTree. Note that the main frame might also know
  // about proxies in SiteInstanceGroups for frames in a different FrameTree
  // (e.g., for window.open), so we can't just iterate over its proxy_hosts_ in
  // RenderFrameHostManager.
  for (auto* group : frame_tree_site_instance_groups)
    SetPageFocus(group, is_focused);
}

void FrameTree::SetPageFocus(SiteInstanceGroup* group, bool is_focused) {
  RenderFrameHostManager* root_manager = root_.render_manager();

  // This is only used to set page-level focus in cross-process subframes, and
  // requests to set focus in main frame's SiteInstanceGroup are ignored.
  if (group != root_manager->current_frame_host()->GetSiteInstance()->group()) {
    RenderFrameProxyHost* proxy = root_manager->current_frame_host()
                                      ->browsing_context_state()
                                      ->GetRenderFrameProxyHost(group);
    if (proxy->is_render_frame_proxy_live())
      proxy->GetAssociatedRemoteFrame()->SetPageFocus(is_focused);
  }
}

void FrameTree::RegisterExistingOriginAsHavingDefaultIsolation(
    const url::Origin& previously_visited_origin,
    NavigationRequest* navigation_request_to_exclude) {
  controller().RegisterExistingOriginAsHavingDefaultIsolation(
      previously_visited_origin);

  std::unordered_set<SiteInstanceImpl*> matching_site_instances;

  // Be sure to visit all RenderFrameHosts associated with this frame that might
  // have an origin that could script other frames. We skip RenderFrameHosts
  // that are in the bfcache, assuming there's no way for a frame to join the
  // BrowsingInstance of a bfcache RFH while it's in the cache.
  for (auto* frame_tree_node : SubtreeNodes(root())) {
    auto* frame_host = frame_tree_node->current_frame_host();
    if (previously_visited_origin == frame_host->GetLastCommittedOrigin())
      matching_site_instances.insert(frame_host->GetSiteInstance());

    if (frame_host->HasCommittingNavigationRequestForOrigin(
            previously_visited_origin, navigation_request_to_exclude)) {
      matching_site_instances.insert(frame_host->GetSiteInstance());
    }

    auto* spec_frame_host =
        frame_tree_node->render_manager()->speculative_frame_host();
    if (spec_frame_host &&
        spec_frame_host->HasCommittingNavigationRequestForOrigin(
            previously_visited_origin, navigation_request_to_exclude)) {
      matching_site_instances.insert(spec_frame_host->GetSiteInstance());
    }

    auto* navigation_request = frame_tree_node->navigation_request();
    if (navigation_request &&
        navigation_request != navigation_request_to_exclude &&
        navigation_request->HasCommittingOrigin(previously_visited_origin)) {
      matching_site_instances.insert(frame_host->GetSiteInstance());
    }
  }

  // Update any SiteInstances found to contain |origin|.
  for (auto* site_instance : matching_site_instances) {
    site_instance->RegisterAsDefaultOriginIsolation(previously_visited_origin);
  }
}

void FrameTree::Init(SiteInstanceImpl* main_frame_site_instance,
                     bool renderer_initiated_creation,
                     const std::string& main_frame_name,
                     RenderFrameHostImpl* opener_for_origin,
                     const blink::FramePolicy& frame_policy,
                     const base::UnguessableToken& devtools_frame_token) {
  // blink::FrameTree::SetName always keeps |unique_name| empty in case of a
  // main frame - let's do the same thing here.
  std::string unique_name;
  root_.render_manager()->InitRoot(main_frame_site_instance,
                                   renderer_initiated_creation, frame_policy,
                                   main_frame_name, devtools_frame_token);
  root_.SetFencedFramePropertiesIfNeeded();

  // The initial empty document should inherit the origin (the origin may
  // change after the first commit) and other state (such as the
  // RuntimeFeatureStateReadContext) from its opener, except when they are in
  // different browsing context groups (`renderer_initiated_creation` will be
  // false), where it should use a new opaque origin and default values for the
  // other state, respectively.
  // See also https://crbug.com/932067.
  //
  // Note that the origin of the new frame might depend on sandbox flags.
  // Checking sandbox flags of the new frame should be safe at this point,
  // because the flags should be already inherited when creating the root node.
  DCHECK(!renderer_initiated_creation || opener_for_origin);
  root_.current_frame_host()->SetOriginDependentStateOfNewFrame(
      renderer_initiated_creation ? opener_for_origin : nullptr);

  controller().CreateInitialEntry();
}

void FrameTree::DidAccessInitialMainDocument() {
  OPTIONAL_TRACE_EVENT0("content", "FrameTree::DidAccessInitialDocument");
  has_accessed_initial_main_document_ = true;
  controller().DidAccessInitialMainDocument();
}

void FrameTree::NodeLoadingStateChanged(
    FrameTreeNode& node,
    LoadingState previous_frame_tree_loading_state) {
  LoadingState new_frame_tree_loading_state = GetLoadingState();
  if (previous_frame_tree_loading_state == new_frame_tree_loading_state) {
    return;
  }

  root()->render_manager()->SetIsLoading(new_frame_tree_loading_state !=
                                         LoadingState::NONE);
  delegate_->LoadingStateChanged(new_frame_tree_loading_state);
  if (previous_frame_tree_loading_state == LoadingState::NONE) {
    delegate_->DidStartLoading(&node);
  } else if (new_frame_tree_loading_state == LoadingState::NONE) {
    delegate_->DidStopLoading();
  }
}

void FrameTree::DidCancelLoading() {
  OPTIONAL_TRACE_EVENT0("content", "FrameTree::DidCancelLoading");
  navigator_.controller().DiscardNonCommittedEntries();
}

void FrameTree::StopLoading() {
  for (FrameTreeNode* node : Nodes())
    node->StopLoading();
}

void FrameTree::Shutdown() {
  is_being_destroyed_ = true;
#if DCHECK_IS_ON()
  DCHECK(!was_shut_down_);
  was_shut_down_ = true;
#endif

  RenderFrameHostManager* root_manager = root_.render_manager();

  if (!root_manager->current_frame_host()) {
    // The page has been transferred out during an activation. There is little
    // left to do.
    // TODO(crbug.com/40177949): If we decide that pending delete RFHs
    // need to be moved along during activation replace this line with a DCHECK
    // that there are no pending delete instances.
    root_manager->ClearRFHsPendingShutdown();
    DCHECK(!root_.navigation_request());
    DCHECK(!root_manager->speculative_frame_host());
    manager_delegate_->OnFrameTreeNodeDestroyed(&root_);
    return;
  }

  for (FrameTreeNode* node : Nodes()) {
    // Delete all RFHs pending shutdown, which will lead the corresponding RVHs
    // to be shutdown and be deleted as well.
    node->render_manager()->ClearRFHsPendingShutdown();
    // TODO(crbug.com/40177939): Ban WebUI instance in Prerender pages.
    node->render_manager()->ClearWebUIInstances();
  }

  // Destroy all subframes now. This notifies observers.
  root_manager->current_frame_host()->ResetChildren();
  root_manager->current_frame_host()
      ->browsing_context_state()
      ->ResetProxyHosts();

  // Manually call the observer methods for the root FrameTreeNode. It is
  // necessary to manually delete all objects tracking navigations
  // (NavigationHandle, NavigationRequest) for observers to be properly
  // notified of these navigations stopping before the WebContents is
  // destroyed.

  root_manager->current_frame_host()->RenderFrameDeleted();
  root_manager->current_frame_host()->ResetOwnedNavigationRequests(
      NavigationDiscardReason::kWillRemoveFrame);

  // Do not update state as the FrameTree::Delegate (possibly a WebContents) is
  // being destroyed.
  root_.ResetNavigationRequestButKeepState(
      NavigationDiscardReason::kWillRemoveFrame);
  if (root_manager->speculative_frame_host()) {
    root_manager->DiscardSpeculativeRenderFrameHostForShutdown();
  }

  // NavigationRequests restoring the page from bfcache have a reference to the
  // RFHs stored in the cache, so the cache should be cleared after the
  // navigation request is reset.
  controller().GetBackForwardCache().Shutdown();

  manager_delegate_->OnFrameTreeNodeDestroyed(&root_);
  render_view_delegate_->RenderViewDeleted(
      root_manager->current_frame_host()->render_view_host());
}

base::SafeRef<FrameTree> FrameTree::GetSafeRef() {
  return weak_ptr_factory_.GetSafeRef();
}

void FrameTree::FocusOuterFrameTrees() {
  OPTIONAL_TRACE_EVENT0("content", "FrameTree::FocusOuterFrameTrees");

  FrameTree* frame_tree_to_focus = this;
  while (true) {
    FrameTreeNode* outer_node = FrameTreeNode::GloballyFindByID(
        frame_tree_to_focus->delegate()->GetOuterDelegateFrameTreeNodeId());
    if (!outer_node || !outer_node->current_frame_host()->IsActive()) {
      // Don't set focus on an inactive FrameTreeNode.
      return;
    }
    outer_node->frame_tree().SetFocusedFrame(outer_node, nullptr);

    // For a browser initiated focus change, let embedding renderer know of the
    // change. Otherwise, if the currently focused element is just across a
    // process boundary in focus order, it will not be possible to move across
    // that boundary. This is because the target element will already be focused
    // (that renderer was not notified) and drop the event.
    if (auto* proxy_to_outer_delegate = frame_tree_to_focus->root()
                                            ->render_manager()
                                            ->GetProxyToOuterDelegate()) {
      proxy_to_outer_delegate->SetFocusedFrame();
    }
    frame_tree_to_focus = &outer_node->frame_tree();
  }
}

void FrameTree::Discard() {
  // A speculative pending-commit rfh should not be cancelled or deleted. In
  // this case ignore the discard request and allow the navigation to complete
  // as normal.
  if (const auto* speculative_rfh =
          root()->render_manager()->speculative_frame_host();
      speculative_rfh && speculative_rfh->HasPendingCommitNavigation()) {
    return;
  }

  root()->set_was_discarded();
  root()->current_frame_host()->DiscardFrame();
  NavigationControllerImpl& navigation_controller = controller();
  navigation_controller.SetNeedsReload();
  navigation_controller.GetBackForwardCache().Flush();
}

}  // namespace content
