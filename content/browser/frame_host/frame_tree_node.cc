// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/frame_tree_node.h"

#include <math.h>

#include <queue>
#include <utility>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "third_party/blink/public/common/frame/sandbox_flags.h"
#include "third_party/blink/public/common/frame/user_activation_update_type.h"

namespace content {

namespace {

// This is a global map between frame_tree_node_ids and pointers to
// FrameTreeNodes.
typedef base::hash_map<int, FrameTreeNode*> FrameTreeNodeIdMap;

base::LazyInstance<FrameTreeNodeIdMap>::DestructorAtExit
    g_frame_tree_node_id_map = LAZY_INSTANCE_INITIALIZER;

// These values indicate the loading progress status. The minimum progress
// value matches what Blink's ProgressTracker has traditionally used for a
// minimum progress value.
const double kLoadingProgressMinimum = 0.1;
const double kLoadingProgressDone = 1.0;

}  // namespace

// This observer watches the opener of its owner FrameTreeNode and clears the
// owner's opener if the opener is destroyed.
class FrameTreeNode::OpenerDestroyedObserver : public FrameTreeNode::Observer {
 public:
  OpenerDestroyedObserver(FrameTreeNode* owner, bool observing_original_opener)
      : owner_(owner), observing_original_opener_(observing_original_opener) {}

  // FrameTreeNode::Observer
  void OnFrameTreeNodeDestroyed(FrameTreeNode* node) override {
    if (observing_original_opener_) {
      // The "original owner" is special. It's used for attribution, and clients
      // walk down the original owner chain. Therefore, if a link in the chain
      // is being destroyed, reconnect the observation to the parent of the link
      // being destroyed.
      CHECK_EQ(owner_->original_opener(), node);
      owner_->SetOriginalOpener(node->original_opener());
      // |this| is deleted at this point.
    } else {
      CHECK_EQ(owner_->opener(), node);
      owner_->SetOpener(nullptr);
      // |this| is deleted at this point.
    }
  }

 private:
  FrameTreeNode* owner_;
  bool observing_original_opener_;

  DISALLOW_COPY_AND_ASSIGN(OpenerDestroyedObserver);
};

int FrameTreeNode::next_frame_tree_node_id_ = 1;

// static
FrameTreeNode* FrameTreeNode::GloballyFindByID(int frame_tree_node_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  FrameTreeNodeIdMap* nodes = g_frame_tree_node_id_map.Pointer();
  auto it = nodes->find(frame_tree_node_id);
  return it == nodes->end() ? nullptr : it->second;
}

FrameTreeNode::FrameTreeNode(FrameTree* frame_tree,
                             Navigator* navigator,
                             FrameTreeNode* parent,
                             blink::WebTreeScopeType scope,
                             const std::string& name,
                             const std::string& unique_name,
                             bool is_created_by_script,
                             const base::UnguessableToken& devtools_frame_token,
                             const FrameOwnerProperties& frame_owner_properties,
                             blink::FrameOwnerElementType owner_type)
    : frame_tree_(frame_tree),
      navigator_(navigator),
      render_manager_(this, frame_tree->manager_delegate()),
      frame_tree_node_id_(next_frame_tree_node_id_++),
      parent_(parent),
      depth_(parent ? parent->depth_ + 1 : 0u),
      opener_(nullptr),
      original_opener_(nullptr),
      has_committed_real_load_(false),
      is_collapsed_(false),
      replication_state_(
          scope,
          name,
          unique_name,
          false /* should enforce strict mixed content checking */,
          std::vector<uint32_t>()
          /* hashes of hosts for insecure request upgrades */,
          false /* is a potentially trustworthy unique origin */,
          false /* has received a user gesture */,
          false /* has received a user gesture before nav */,
          owner_type),
      is_created_by_script_(is_created_by_script),
      devtools_frame_token_(devtools_frame_token),
      frame_owner_properties_(frame_owner_properties),
      was_discarded_(false),
      blame_context_(frame_tree_node_id_, parent) {
  std::pair<FrameTreeNodeIdMap::iterator, bool> result =
      g_frame_tree_node_id_map.Get().insert(
          std::make_pair(frame_tree_node_id_, this));
  CHECK(result.second);

  // Note: this should always be done last in the constructor.
  blame_context_.Initialize();
}

FrameTreeNode::~FrameTreeNode() {
  // Remove the children.
  current_frame_host()->ResetChildren();

  // If the removed frame was created by a script, then its history entry will
  // never be reused - we can save some memory by removing the history entry.
  // See also https://crbug.com/784356.
  if (is_created_by_script_ && parent_) {
    NavigationEntryImpl* nav_entry = static_cast<NavigationEntryImpl*>(
        navigator()->GetController()->GetLastCommittedEntry());
    if (nav_entry) {
      nav_entry->RemoveEntryForFrame(this,
                                     /* only_if_different_position = */ false);
    }
  }

  frame_tree_->FrameRemoved(this);
  for (auto& observer : observers_)
    observer.OnFrameTreeNodeDestroyed(this);

  if (opener_)
    opener_->RemoveObserver(opener_observer_.get());
  if (original_opener_)
    original_opener_->RemoveObserver(original_opener_observer_.get());

  g_frame_tree_node_id_map.Get().erase(frame_tree_node_id_);

  if (navigation_request_) {
    // PlzNavigate: if a frame with a pending navigation is detached, make sure
    // the WebContents (and its observers) update their loading state.
    navigation_request_.reset();
    DidStopLoading();
  }
}

void FrameTreeNode::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FrameTreeNode::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool FrameTreeNode::IsMainFrame() const {
  return frame_tree_->root() == this;
}

void FrameTreeNode::ResetForNavigation() {
  // Discard any CSP headers from the previous document.
  replication_state_.accumulated_csp_headers.clear();
  render_manager_.OnDidResetContentSecurityPolicy();

  // Clear any CSP-set sandbox flags, and the declared feature policy for the
  // frame.
  UpdateFramePolicyHeaders(blink::WebSandboxFlags::kNone, {});
}

void FrameTreeNode::SetOpener(FrameTreeNode* opener) {
  if (opener_) {
    opener_->RemoveObserver(opener_observer_.get());
    opener_observer_.reset();
  }

  opener_ = opener;

  if (opener_) {
    opener_observer_ = std::make_unique<OpenerDestroyedObserver>(this, false);
    opener_->AddObserver(opener_observer_.get());
  }
}

void FrameTreeNode::SetOriginalOpener(FrameTreeNode* opener) {
  // The original opener tracks main frames only.
  DCHECK(opener == nullptr || !opener->parent());

  if (original_opener_) {
    original_opener_->RemoveObserver(original_opener_observer_.get());
    original_opener_observer_.reset();
  }

  original_opener_ = opener;

  if (original_opener_) {
    original_opener_observer_ =
        std::make_unique<OpenerDestroyedObserver>(this, true);
    original_opener_->AddObserver(original_opener_observer_.get());
  }
}

void FrameTreeNode::SetCurrentURL(const GURL& url) {
  if (!has_committed_real_load_ && !url.IsAboutBlank())
    has_committed_real_load_ = true;
  current_frame_host()->SetLastCommittedUrl(url);
  blame_context_.TakeSnapshot();
}

void FrameTreeNode::SetCurrentOrigin(
    const url::Origin& origin,
    bool is_potentially_trustworthy_unique_origin) {
  if (!origin.IsSameOriginWith(replication_state_.origin) ||
      replication_state_.has_potentially_trustworthy_unique_origin !=
          is_potentially_trustworthy_unique_origin) {
    render_manager_.OnDidUpdateOrigin(origin,
                                      is_potentially_trustworthy_unique_origin);
  }
  replication_state_.origin = origin;
  replication_state_.has_potentially_trustworthy_unique_origin =
      is_potentially_trustworthy_unique_origin;
}

void FrameTreeNode::SetCollapsed(bool collapsed) {
  DCHECK(!IsMainFrame());
  if (is_collapsed_ == collapsed)
    return;

  is_collapsed_ = collapsed;
  render_manager_.OnDidChangeCollapsedState(collapsed);
}

void FrameTreeNode::SetFrameName(const std::string& name,
                                 const std::string& unique_name) {
  if (name == replication_state_.name) {
    // |unique_name| shouldn't change unless |name| changes.
    DCHECK_EQ(unique_name, replication_state_.unique_name);
    return;
  }

  if (parent()) {
    // Non-main frames should have a non-empty unique name.
    DCHECK(!unique_name.empty());
  } else {
    // Unique name of main frames should always stay empty.
    DCHECK(unique_name.empty());
  }

  // Note the unique name should only be able to change before the first real
  // load is committed, but that's not strongly enforced here.
  render_manager_.OnDidUpdateName(name, unique_name);
  replication_state_.name = name;
  replication_state_.unique_name = unique_name;
}

void FrameTreeNode::AddContentSecurityPolicies(
    const std::vector<ContentSecurityPolicyHeader>& headers) {
  replication_state_.accumulated_csp_headers.insert(
      replication_state_.accumulated_csp_headers.end(), headers.begin(),
      headers.end());
  render_manager_.OnDidAddContentSecurityPolicies(headers);
}

void FrameTreeNode::SetInsecureRequestPolicy(
    blink::WebInsecureRequestPolicy policy) {
  if (policy == replication_state_.insecure_request_policy)
    return;
  render_manager_.OnEnforceInsecureRequestPolicy(policy);
  replication_state_.insecure_request_policy = policy;
}

void FrameTreeNode::SetInsecureNavigationsSet(
    const std::vector<uint32_t>& insecure_navigations_set) {
  DCHECK(std::is_sorted(insecure_navigations_set.begin(),
                        insecure_navigations_set.end()));
  if (insecure_navigations_set == replication_state_.insecure_navigations_set)
    return;
  render_manager_.OnEnforceInsecureNavigationsSet(insecure_navigations_set);
  replication_state_.insecure_navigations_set = insecure_navigations_set;
}

void FrameTreeNode::SetPendingFramePolicy(blink::FramePolicy frame_policy) {
  pending_frame_policy_.sandbox_flags = frame_policy.sandbox_flags;

  if (parent()) {
    // Subframes should always inherit their parent's sandbox flags.
    pending_frame_policy_.sandbox_flags |= parent()->active_sandbox_flags();
    // This is only applied on subframes; container policy is not mutable on
    // main frame.
    pending_frame_policy_.container_policy = frame_policy.container_policy;
  }
}

FrameTreeNode* FrameTreeNode::PreviousSibling() const {
  return GetSibling(-1);
}

FrameTreeNode* FrameTreeNode::NextSibling() const {
  return GetSibling(1);
}

bool FrameTreeNode::IsLoading() const {
  RenderFrameHostImpl* current_frame_host =
      render_manager_.current_frame_host();

  DCHECK(current_frame_host);

  if (navigation_request_)
    return true;

  RenderFrameHostImpl* speculative_frame_host =
      render_manager_.speculative_frame_host();
  if (speculative_frame_host && speculative_frame_host->is_loading())
    return true;
  return current_frame_host->is_loading();
}

bool FrameTreeNode::CommitPendingFramePolicy() {
  bool did_change_flags = pending_frame_policy_.sandbox_flags !=
                          replication_state_.frame_policy.sandbox_flags;
  bool did_change_container_policy =
      pending_frame_policy_.container_policy !=
      replication_state_.frame_policy.container_policy;
  if (did_change_flags)
    replication_state_.frame_policy.sandbox_flags =
        pending_frame_policy_.sandbox_flags;
  if (did_change_container_policy)
    replication_state_.frame_policy.container_policy =
        pending_frame_policy_.container_policy;
  UpdateFramePolicyHeaders(pending_frame_policy_.sandbox_flags,
                           replication_state_.feature_policy_header);
  return did_change_flags || did_change_container_policy;
}

void FrameTreeNode::TransferNavigationRequestOwnership(
    RenderFrameHostImpl* render_frame_host) {
  devtools_instrumentation::OnResetNavigationRequest(navigation_request_.get());
  render_frame_host->SetNavigationRequest(std::move(navigation_request_));
}

void FrameTreeNode::CreatedNavigationRequest(
    std::unique_ptr<NavigationRequest> navigation_request) {
  // This is never called when navigating to a Javascript URL. For the loading
  // state, this matches what Blink is doing: Blink doesn't send throbber
  // notifications for Javascript URLS.
  DCHECK(!navigation_request->common_params().url.SchemeIs(
      url::kJavaScriptScheme));

  bool was_previously_loading = frame_tree()->IsLoading();

  // There's no need to reset the state: there's still an ongoing load, and the
  // RenderFrameHostManager will take care of updates to the speculative
  // RenderFrameHost in DidCreateNavigationRequest below.
  if (was_previously_loading) {
    if (navigation_request_ && navigation_request_->navigation_handle()) {
      // Mark the old request as aborted.
      navigation_request_->navigation_handle()->set_net_error_code(
          net::ERR_ABORTED);
    }
    ResetNavigationRequest(true, true);
  }

  navigation_request_ = std::move(navigation_request);
  if (was_discarded_) {
    navigation_request_->set_was_discarded();
    was_discarded_ = false;
  }
  render_manager()->DidCreateNavigationRequest(navigation_request_.get());

  bool to_different_document = !FrameMsg_Navigate_Type::IsSameDocument(
      navigation_request_->common_params().navigation_type);

  DidStartLoading(to_different_document, was_previously_loading);
}

void FrameTreeNode::ResetNavigationRequest(bool keep_state,
                                           bool inform_renderer) {
  if (!navigation_request_)
    return;

  devtools_instrumentation::OnResetNavigationRequest(navigation_request_.get());

  // The renderer should be informed if the caller allows to do so and the
  // navigation came from a BeginNavigation IPC.
  bool need_to_inform_renderer =
      !IsPerNavigationMojoInterfaceEnabled() & inform_renderer &&
      navigation_request_->from_begin_navigation();

  NavigationRequest::AssociatedSiteInstanceType site_instance_type =
      navigation_request_->associated_site_instance_type();
  navigation_request_.reset();

  if (keep_state)
    return;

  // The RenderFrameHostManager should clean up any speculative RenderFrameHost
  // it created for the navigation. Also register that the load stopped.
  DidStopLoading();
  render_manager_.CleanUpNavigation();

  // When reusing the same SiteInstance, a pending WebUI may have been created
  // on behalf of the navigation in the current RenderFrameHost. Clear it.
  if (site_instance_type ==
      NavigationRequest::AssociatedSiteInstanceType::CURRENT) {
    current_frame_host()->ClearPendingWebUI();
  }

  // If the navigation is renderer-initiated, the renderer should also be
  // informed that the navigation stopped if needed. In the case the renderer
  // process asked for the navigation to be aborted, e.g. following a
  // document.open, do not send an IPC to the renderer process as it already
  // expects the navigation to stop.
  if (need_to_inform_renderer) {
    current_frame_host()->Send(
        new FrameMsg_DroppedNavigation(current_frame_host()->GetRoutingID()));
  }
}

void FrameTreeNode::DidStartLoading(bool to_different_document,
                                    bool was_previously_loading) {
  TRACE_EVENT2("navigation", "FrameTreeNode::DidStartLoading",
               "frame_tree_node", frame_tree_node_id(), "to different document",
               to_different_document);
  // Any main frame load to a new document should reset the load progress since
  // it will replace the current page and any frames. The WebContents will
  // be notified when DidChangeLoadProgress is called.
  if (to_different_document && IsMainFrame())
    frame_tree_->ResetLoadProgress();

  // Notify the WebContents.
  if (!was_previously_loading)
    navigator()->GetDelegate()->DidStartLoading(this, to_different_document);

  // Set initial load progress and update overall progress. This will notify
  // the WebContents of the load progress change.
  DidChangeLoadProgress(kLoadingProgressMinimum);

  // Notify the RenderFrameHostManager of the event.
  render_manager()->OnDidStartLoading();
}

void FrameTreeNode::DidStopLoading() {
  TRACE_EVENT1("navigation", "FrameTreeNode::DidStopLoading", "frame_tree_node",
               frame_tree_node_id());
  // Set final load progress and update overall progress. This will notify
  // the WebContents of the load progress change.
  DidChangeLoadProgress(kLoadingProgressDone);

  // Notify the WebContents.
  if (!frame_tree_->IsLoading())
    navigator()->GetDelegate()->DidStopLoading();

  // Notify the RenderFrameHostManager of the event.
  render_manager()->OnDidStopLoading();

  // Notify accessibility that the user is no longer trying to load or
  // reload a page.
  BrowserAccessibilityManager* manager =
      current_frame_host()->browser_accessibility_manager();
  if (manager)
    manager->DidStopLoading();
}

void FrameTreeNode::DidChangeLoadProgress(double load_progress) {
  DCHECK_GE(load_progress, kLoadingProgressMinimum);
  DCHECK_LE(load_progress, kLoadingProgressDone);
  if (IsMainFrame())
    frame_tree_->UpdateLoadProgress(load_progress);
}

bool FrameTreeNode::StopLoading() {
  if (navigation_request_) {
    int expected_pending_nav_entry_id = navigation_request_->nav_entry_id();
    if (navigation_request_->navigation_handle()) {
      navigation_request_->navigation_handle()->set_net_error_code(
          net::ERR_ABORTED);
      expected_pending_nav_entry_id =
          navigation_request_->navigation_handle()->pending_nav_entry_id();
    }
    navigator_->DiscardPendingEntryIfNeeded(expected_pending_nav_entry_id);
  }
  ResetNavigationRequest(false, true);

  // TODO(nasko): see if child frames should send IPCs in site-per-process
  // mode.
  if (!IsMainFrame())
    return true;

  render_manager_.Stop();
  return true;
}

void FrameTreeNode::DidFocus() {
  last_focus_time_ = base::TimeTicks::Now();
  for (auto& observer : observers_)
    observer.OnFrameTreeNodeFocused(this);
}

void FrameTreeNode::BeforeUnloadCanceled() {
  // TODO(clamy): Support BeforeUnload in subframes.
  if (!IsMainFrame())
    return;

  RenderFrameHostImpl* current_frame_host =
      render_manager_.current_frame_host();
  DCHECK(current_frame_host);
  current_frame_host->ResetLoadingState();

  RenderFrameHostImpl* speculative_frame_host =
      render_manager_.speculative_frame_host();
  if (speculative_frame_host)
    speculative_frame_host->ResetLoadingState();
  // Note: there is no need to set an error code on the NavigationHandle here
  // as it has not been created yet. It is only created when the
  // BeforeUnloadACK is received.
  if (navigation_request_)
    ResetNavigationRequest(false, true);
}

bool FrameTreeNode::NotifyUserActivation() {
  for (FrameTreeNode* node = this; node; node = node->parent())
    node->user_activation_state_.Activate();
  replication_state_.has_received_user_gesture = true;
  return true;
}

bool FrameTreeNode::ConsumeTransientUserActivation() {
  bool was_active = user_activation_state_.IsActive();
  for (FrameTreeNode* node : frame_tree()->Nodes())
    node->user_activation_state_.ConsumeIfActive();
  return was_active;
}

bool FrameTreeNode::UpdateUserActivationState(
    blink::UserActivationUpdateType update_type) {
  render_manager_.UpdateUserActivationState(update_type);
  switch (update_type) {
    case blink::UserActivationUpdateType::kConsumeTransientActivation:
      return ConsumeTransientUserActivation();

    case blink::UserActivationUpdateType::kNotifyActivation:
      return NotifyUserActivation();
  }
  NOTREACHED() << "Invalid update_type.";
}

void FrameTreeNode::OnSetHasReceivedUserGestureBeforeNavigation(bool value) {
  render_manager_.OnSetHasReceivedUserGestureBeforeNavigation(value);
  replication_state_.has_received_user_gesture_before_nav = value;
}

FrameTreeNode* FrameTreeNode::GetSibling(int relative_offset) const {
  if (!parent_ || !parent_->child_count())
    return nullptr;

  for (size_t i = 0; i < parent_->child_count(); ++i) {
    if (parent_->child_at(i) == this) {
      if ((relative_offset < 0 && static_cast<size_t>(-relative_offset) > i) ||
          i + relative_offset >= parent_->child_count()) {
        return nullptr;
      }
      return parent_->child_at(i + relative_offset);
    }
  }

  NOTREACHED() << "FrameTreeNode not found in its parent's children.";
  return nullptr;
}

void FrameTreeNode::UpdateFramePolicyHeaders(
    blink::WebSandboxFlags sandbox_flags,
    const blink::ParsedFeaturePolicy& parsed_header) {
  bool changed = false;
  if (replication_state_.feature_policy_header != parsed_header) {
    replication_state_.feature_policy_header = parsed_header;
    changed = true;
  }
  // TODO(iclelland): Kill the renderer if sandbox flags is not a subset of the
  // currently effective sandbox flags from the frame. https://crbug.com/740556
  blink::WebSandboxFlags updated_flags =
      sandbox_flags | effective_frame_policy().sandbox_flags;
  if (replication_state_.active_sandbox_flags != updated_flags) {
    replication_state_.active_sandbox_flags = updated_flags;
    changed = true;
  }
  // Notify any proxies if the policies have been changed.
  if (changed)
    render_manager()->OnDidSetFramePolicyHeaders();
}

}  // namespace content
