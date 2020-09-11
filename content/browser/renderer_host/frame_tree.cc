// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/frame_tree.h"

#include <stddef.h>

#include <queue>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/unguessable_token.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/navigator_delegate.h"
#include "content/browser/renderer_host/render_frame_host_factory.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/content_switches_internal.h"
#include "content/common/input_messages.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"

namespace content {

namespace {

// Helper function to collect SiteInstances involved in rendering a single
// FrameTree (which is a subset of SiteInstances in main frame's proxy_hosts_
// because of openers).
std::set<SiteInstance*> CollectSiteInstances(FrameTree* tree) {
  std::set<SiteInstance*> instances;
  for (FrameTreeNode* node : tree->Nodes())
    instances.insert(node->current_frame_host()->GetSiteInstance());
  return instances;
}

}  // namespace

FrameTree::NodeIterator::NodeIterator(const NodeIterator& other) = default;

FrameTree::NodeIterator::~NodeIterator() = default;

FrameTree::NodeIterator& FrameTree::NodeIterator::operator++() {
  if (current_node_ != root_of_subtree_to_skip_) {
    for (size_t i = 0; i < current_node_->child_count(); ++i) {
      FrameTreeNode* child = current_node_->child_at(i);
      queue_.push(child);
    }
  }

  if (!queue_.empty()) {
    current_node_ = queue_.front();
    queue_.pop();
  } else {
    current_node_ = nullptr;
  }

  return *this;
}

bool FrameTree::NodeIterator::operator==(const NodeIterator& rhs) const {
  return current_node_ == rhs.current_node_;
}

FrameTree::NodeIterator::NodeIterator(FrameTreeNode* starting_node,
                                      FrameTreeNode* root_of_subtree_to_skip)
    : current_node_(starting_node),
      root_of_subtree_to_skip_(root_of_subtree_to_skip) {}

FrameTree::NodeIterator FrameTree::NodeRange::begin() {
  // We shouldn't be attempting a frame tree traversal while the tree is
  // being constructed.
  DCHECK(root_->current_frame_host());
  return NodeIterator(root_, root_of_subtree_to_skip_);
}

FrameTree::NodeIterator FrameTree::NodeRange::end() {
  return NodeIterator(nullptr, nullptr);
}

FrameTree::NodeRange::NodeRange(FrameTreeNode* root,
                                FrameTreeNode* root_of_subtree_to_skip)
    : root_(root), root_of_subtree_to_skip_(root_of_subtree_to_skip) {}

FrameTree::FrameTree(NavigationControllerImpl* navigation_controller,
                     NavigatorDelegate* navigator_delegate,
                     RenderFrameHostDelegate* render_frame_delegate,
                     RenderViewHostDelegate* render_view_delegate,
                     RenderWidgetHostDelegate* render_widget_delegate,
                     RenderFrameHostManager::Delegate* manager_delegate)
    : render_frame_delegate_(render_frame_delegate),
      render_view_delegate_(render_view_delegate),
      render_widget_delegate_(render_widget_delegate),
      manager_delegate_(manager_delegate),
      navigator_(navigation_controller, navigator_delegate),
      root_(new FrameTreeNode(this,
                              nullptr,
                              // The top-level frame must always be in a
                              // document scope.
                              blink::mojom::TreeScopeType::kDocument,
                              std::string(),
                              std::string(),
                              false,
                              base::UnguessableToken::Create(),
                              blink::mojom::FrameOwnerProperties(),
                              blink::mojom::FrameOwnerElementType::kNone)),
      focused_frame_tree_node_id_(FrameTreeNode::kFrameTreeNodeInvalidId),
      load_progress_(0.0) {}

FrameTree::~FrameTree() {
  delete root_;
  root_ = nullptr;
}

FrameTreeNode* FrameTree::FindByID(int frame_tree_node_id) {
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
    if (this == result->frame_tree())
      return result;
  }

  RenderFrameProxyHost* render_frame_proxy_host =
      RenderFrameProxyHost::FromID(process_id, routing_id);
  if (render_frame_proxy_host) {
    FrameTreeNode* result = render_frame_proxy_host->frame_tree_node();
    if (this == result->frame_tree())
      return result;
  }

  return nullptr;
}

FrameTreeNode* FrameTree::FindByName(const std::string& name) {
  if (name.empty())
    return root_;

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
  return NodeRange(subtree_root, nullptr);
}

FrameTree::NodeRange FrameTree::NodesExceptSubtree(FrameTreeNode* node) {
  return NodeRange(root_, node);
}

FrameTreeNode* FrameTree::AddFrame(
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
    blink::mojom::FrameOwnerElementType owner_type) {
  CHECK_NE(new_routing_id, MSG_ROUTING_NONE);

  // A child frame always starts with an initial empty document, which means
  // it is in the same SiteInstance as the parent frame. Ensure that the process
  // which requested a child frame to be added is the same as the process of the
  // parent node.
  if (parent->GetProcess()->GetID() != process_id)
    return nullptr;

  std::unique_ptr<FrameTreeNode> new_node = base::WrapUnique(new FrameTreeNode(
      this, parent, scope, frame_name, frame_unique_name, is_created_by_script,
      devtools_frame_token, frame_owner_properties, owner_type));

  // Set sandbox flags and container policy and make them effective immediately,
  // since initial sandbox flags and feature policy should apply to the initial
  // empty document in the frame. This needs to happen before the call to
  // AddChild so that the effective policy is sent to any newly-created
  // RenderFrameProxy objects when the RenderFrameHost is created.
  // SetPendingFramePolicy is necessary here because next navigation on this
  // frame will need the value of pending frame policy instead of effective
  // frame policy.
  new_node->SetPendingFramePolicy(frame_policy);
  new_node->CommitFramePolicy(frame_policy);

  if (was_discarded)
    new_node->set_was_discarded();

  // Add the new node to the FrameTree, creating the RenderFrameHost.
  FrameTreeNode* added_node = parent->AddChild(std::move(new_node), process_id,
                                               new_routing_id, frame_token);

  DCHECK(interface_provider_receiver.is_valid());
  added_node->current_frame_host()->BindInterfaceProviderReceiver(
      std::move(interface_provider_receiver));

  DCHECK(browser_interface_broker_receiver.is_valid());
  added_node->current_frame_host()->BindBrowserInterfaceBrokerReceiver(
      std::move(browser_interface_broker_receiver));

  // The last committed NavigationEntry may have a FrameNavigationEntry with the
  // same |frame_unique_name|, since we don't remove FrameNavigationEntries if
  // their frames are deleted.  If there is a stale one, remove it to avoid
  // conflicts on future updates.
  NavigationEntryImpl* last_committed_entry = static_cast<NavigationEntryImpl*>(
      navigator_.GetController()->GetLastCommittedEntry());
  if (last_committed_entry) {
    last_committed_entry->RemoveEntryForFrame(
        added_node, /* only_if_different_position = */ true);
  }

  // Now that the new node is part of the FrameTree and has a RenderFrameHost,
  // we can announce the creation of the initial RenderFrame which already
  // exists in the renderer process.
  if (added_node->frame_owner_element_type() !=
      blink::mojom::FrameOwnerElementType::kPortal) {
    // Portals do not have a live RenderFrame in the renderer process.
    added_node->current_frame_host()->SetRenderFrameCreated(true);
  }
  return added_node;
}

void FrameTree::RemoveFrame(FrameTreeNode* child) {
  RenderFrameHostImpl* parent = child->parent();
  if (!parent) {
    NOTREACHED() << "Unexpected RemoveFrame call for main frame.";
    return;
  }

  parent->RemoveChild(child);
}

void FrameTree::CreateProxiesForSiteInstance(FrameTreeNode* source,
                                             SiteInstance* site_instance) {
  // Create the RenderFrameProxyHost for the new SiteInstance.
  if (!source || !source->IsMainFrame()) {
    RenderViewHostImpl* render_view_host =
        GetRenderViewHost(site_instance).get();
    if (render_view_host) {
      root()->render_manager()->EnsureRenderViewInitialized(render_view_host,
                                                            site_instance);
    } else {
      root()->render_manager()->CreateRenderFrameProxy(site_instance);
    }
  }

  // Check whether we're in an inner delegate and |site_instance| corresponds
  // to the outer delegate.  Subframe proxies aren't needed if this is the
  // case.
  bool is_site_instance_for_outer_delegate = false;
  RenderFrameProxyHost* outer_delegate_proxy =
      root()->render_manager()->GetProxyToOuterDelegate();
  if (outer_delegate_proxy) {
    is_site_instance_for_outer_delegate =
        (site_instance == outer_delegate_proxy->GetSiteInstance());
  }

  // Proxies are created in the FrameTree in response to a node navigating to a
  // new SiteInstance. Since |source|'s navigation will replace the currently
  // loaded document, the entire subtree under |source| will be removed, and
  // thus proxy creation is skipped for all nodes in that subtree.
  //
  // However, a proxy *is* needed for the |source| node itself.  This lets
  // cross-process navigations in |source| start with a proxy and follow a
  // remote-to-local transition, which avoids race conditions in cases where
  // other navigations need to reference |source| before it commits. See
  // https://crbug.com/756790 for more background.  Therefore,
  // NodesExceptSubtree(source) will include |source| in the nodes traversed
  // (see NodeIterator::operator++).
  for (FrameTreeNode* node : NodesExceptSubtree(source)) {
    // If a new frame is created in the current SiteInstance, other frames in
    // that SiteInstance don't need a proxy for the new frame.
    RenderFrameHostImpl* current_host =
        node->render_manager()->current_frame_host();
    SiteInstance* current_instance = current_host->GetSiteInstance();
    if (current_instance != site_instance) {
      if (node == source && !current_host->IsRenderFrameLive()) {
        // There's no need to create a proxy at |source| when the current
        // RenderFrameHost isn't live, as in that case, the pending
        // RenderFrameHost will be committed immediately, and the proxy
        // destroyed right away, in GetFrameHostForNavigation.  This makes the
        // race described above not possible.
        continue;
      }

      // Do not create proxies for subframes in the outer delegate's
      // SiteInstance, since there is no need to expose these subframes to the
      // outer delegate.  See also comments in CreateProxiesForChildFrame() and
      // https://crbug.com/1013553.
      if (!node->IsMainFrame() && is_site_instance_for_outer_delegate)
        continue;

      node->render_manager()->CreateRenderFrameProxy(site_instance);
    }
  }
}

RenderFrameHostImpl* FrameTree::GetMainFrame() const {
  return root_->current_frame_host();
}

FrameTreeNode* FrameTree::GetFocusedFrame() {
  return FindByID(focused_frame_tree_node_id_);
}

void FrameTree::SetFocusedFrame(FrameTreeNode* node, SiteInstance* source) {
  if (node == GetFocusedFrame())
    return;

  std::set<SiteInstance*> frame_tree_site_instances =
      CollectSiteInstances(this);

  SiteInstance* current_instance =
      node->current_frame_host()->GetSiteInstance();

  // Update the focused frame in all other SiteInstances.  If focus changes to
  // a cross-process frame, this allows the old focused frame's renderer
  // process to clear focus from that frame and fire blur events.  It also
  // ensures that the latest focused frame is available in all renderers to
  // compute document.activeElement.
  //
  // We do not notify the |source| SiteInstance because it already knows the
  // new focused frame (since it initiated the focus change), and we notify the
  // new focused frame's SiteInstance (if it differs from |source|) separately
  // below.
  for (auto* instance : frame_tree_site_instances) {
    if (instance != source && instance != current_instance) {
      RenderFrameProxyHost* proxy =
          node->render_manager()->GetRenderFrameProxyHost(instance);
      proxy->SetFocusedFrame();
    }
  }

  // If |node| was focused from a cross-process frame (i.e., via
  // window.focus()), tell its RenderFrame that it should focus.
  if (current_instance != source)
    node->current_frame_host()->SetFocusedFrame();

  focused_frame_tree_node_id_ = node->frame_tree_node_id();
  node->DidFocus();

  // The accessibility tree data for the root of the frame tree keeps
  // track of the focused frame too, so update that every time the
  // focused frame changes.
  root()->current_frame_host()->UpdateAXTreeData();
}

void FrameTree::SetFrameRemoveListener(
    base::RepeatingCallback<void(RenderFrameHost*)> on_frame_removed) {
  on_frame_removed_ = std::move(on_frame_removed);
}

scoped_refptr<RenderViewHostImpl> FrameTree::CreateRenderViewHost(
    SiteInstance* site_instance,
    int32_t main_frame_routing_id,
    bool swapped_out) {
  RenderViewHostImpl* rvh =
      static_cast<RenderViewHostImpl*>(RenderViewHostFactory::Create(
          site_instance, render_view_delegate_, render_widget_delegate_,
          main_frame_routing_id, swapped_out));
  return base::WrapRefCounted(rvh);
}

scoped_refptr<RenderViewHostImpl> FrameTree::GetRenderViewHost(
    SiteInstance* site_instance) {
  auto it = render_view_host_map_.find(site_instance->GetId());
  if (it == render_view_host_map_.end())
    return nullptr;

  return base::WrapRefCounted(it->second);
}

void FrameTree::RegisterRenderViewHost(RenderViewHostImpl* rvh) {
  CHECK(
      !base::Contains(render_view_host_map_, rvh->GetSiteInstance()->GetId()));
  render_view_host_map_[rvh->GetSiteInstance()->GetId()] = rvh;
}

void FrameTree::UnregisterRenderViewHost(RenderViewHostImpl* rvh) {
  auto it = render_view_host_map_.find(rvh->GetSiteInstance()->GetId());
  CHECK(it != render_view_host_map_.end());
  CHECK_EQ(it->second, rvh);
  render_view_host_map_.erase(it);
}

void FrameTree::FrameUnloading(FrameTreeNode* frame) {
  if (frame->frame_tree_node_id() == focused_frame_tree_node_id_)
    focused_frame_tree_node_id_ = FrameTreeNode::kFrameTreeNodeInvalidId;

  // Ensure frames that are about to be deleted aren't visible from the other
  // processes anymore.
  frame->render_manager()->ResetProxyHosts();
}

void FrameTree::FrameRemoved(FrameTreeNode* frame) {
  if (frame->frame_tree_node_id() == focused_frame_tree_node_id_)
    focused_frame_tree_node_id_ = FrameTreeNode::kFrameTreeNodeInvalidId;

  // No notification for the root frame.
  if (!frame->parent()) {
    CHECK_EQ(frame, root_);
    return;
  }

  // Notify observers of the frame removal.
  if (!on_frame_removed_.is_null())
    on_frame_removed_.Run(frame->current_frame_host());
}

void FrameTree::UpdateLoadProgress(double progress) {
  if (progress <= load_progress_)
    return;
  load_progress_ = progress;

  // Notify the WebContents.
  root_->navigator().GetDelegate()->DidChangeLoadProgress();
}

void FrameTree::ResetLoadProgress() {
  load_progress_ = 0.0;
}

bool FrameTree::IsLoading() const {
  for (const FrameTreeNode* node : const_cast<FrameTree*>(this)->Nodes()) {
    if (node->IsLoading())
      return true;
  }
  return false;
}

void FrameTree::ReplicatePageFocus(bool is_focused) {
  std::set<SiteInstance*> frame_tree_site_instances =
      CollectSiteInstances(this);

  // Send the focus update to main frame's proxies in all SiteInstances of
  // other frames in this FrameTree. Note that the main frame might also know
  // about proxies in SiteInstances for frames in a different FrameTree (e.g.,
  // for window.open), so we can't just iterate over its proxy_hosts_ in
  // RenderFrameHostManager.
  for (auto* instance : frame_tree_site_instances)
    SetPageFocus(instance, is_focused);
}

void FrameTree::SetPageFocus(SiteInstance* instance, bool is_focused) {
  RenderFrameHostManager* root_manager = root_->render_manager();

  // This is only used to set page-level focus in cross-process subframes, and
  // requests to set focus in main frame's SiteInstance are ignored.
  if (instance != root_manager->current_frame_host()->GetSiteInstance()) {
    RenderFrameProxyHost* proxy =
        root_manager->GetRenderFrameProxyHost(instance);
    proxy->GetAssociatedRemoteFrame()->SetPageFocus(is_focused);
  }
}

void FrameTree::RegisterExistingOriginToPreventOptInIsolation(
    const url::Origin& previously_visited_origin,
    NavigationRequest* navigation_request_to_exclude) {
  std::unordered_set<SiteInstance*> matching_site_instances;

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
    static_cast<SiteInstanceImpl*>(site_instance)
        ->PreventOptInOriginIsolation(previously_visited_origin);
  }
}

}  // namespace content
