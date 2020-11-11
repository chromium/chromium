// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/frame_tree_node.h"

#include <math.h>

#include <queue>
#include <unordered_map>
#include <utility>

#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/navigator_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/navigation_params.h"
#include "content/common/navigation_params_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "content/public/common/navigation_policy.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-shared.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom.h"

namespace content {

namespace {

// This is a global map between frame_tree_node_ids and pointers to
// FrameTreeNodes.
typedef std::unordered_map<int, FrameTreeNode*> FrameTreeNodeIdMap;

base::LazyInstance<FrameTreeNodeIdMap>::DestructorAtExit
    g_frame_tree_node_id_map = LAZY_INSTANCE_INITIALIZER;

// These values indicate the loading progress status. The minimum progress
// value matches what Blink's ProgressTracker has traditionally used for a
// minimum progress value.
const double kLoadingProgressMinimum = 0.1;
const double kLoadingProgressDone = 1.0;

}  // namespace

const int FrameTreeNode::kFrameTreeNodeInvalidId = -1;

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

// static
FrameTreeNode* FrameTreeNode::From(RenderFrameHost* rfh) {
  if (!rfh)
    return nullptr;
  return static_cast<RenderFrameHostImpl*>(rfh)->frame_tree_node();
}

FrameTreeNode::FrameTreeNode(
    FrameTree* frame_tree,
    RenderFrameHostImpl* parent,
    blink::mojom::TreeScopeType scope,
    const std::string& name,
    const std::string& unique_name,
    bool is_created_by_script,
    const base::UnguessableToken& devtools_frame_token,
    const blink::mojom::FrameOwnerProperties& frame_owner_properties,
    blink::mojom::FrameOwnerElementType owner_type)
    : frame_tree_(frame_tree),
      render_manager_(this, frame_tree->manager_delegate()),
      frame_tree_node_id_(next_frame_tree_node_id_++),
      parent_(parent),
      depth_(parent ? parent->frame_tree_node()->depth_ + 1 : 0u),
      opener_(nullptr),
      original_opener_(nullptr),
      has_committed_real_load_(false),
      is_collapsed_(false),
      replication_state_(
          scope,
          name,
          unique_name,
          blink::mojom::InsecureRequestPolicy::
              kLeaveInsecureRequestsAlone /* should enforce strict mixed content
                                             checking */
          ,
          std::vector<uint32_t>()
          /* hashes of hosts for insecure request upgrades */,
          false /* is a potentially trustworthy unique origin */,
          false /* has an active user gesture */,
          false /* has received a user gesture before nav */,
          owner_type),
      is_created_by_script_(is_created_by_script),
      devtools_frame_token_(devtools_frame_token),
      frame_owner_properties_(frame_owner_properties),
      was_discarded_(false),
      blame_context_(frame_tree_node_id_, FrameTreeNode::From(parent)) {
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

  current_frame_host()->ResetLoadingState();

  // If the removed frame was created by a script, then its history entry will
  // never be reused - we can save some memory by removing the history entry.
  // See also https://crbug.com/784356.
  if (is_created_by_script_ && parent_) {
    NavigationEntryImpl* nav_entry = static_cast<NavigationEntryImpl*>(
        navigator().GetController()->GetLastCommittedEntry());
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

  bool did_stop_loading = false;

  if (navigation_request_) {
    navigation_request_.reset();
    // If a frame with a pending navigation is detached, make sure the
    // WebContents (and its observers) update their loading state.
    did_stop_loading = true;
  }

  // ~SiteProcessCountTracker DCHECKs in some tests if the speculative
  // RenderFrameHostImpl is not destroyed last. Ideally this would be closer to
  // (possible before) the ResetLoadingState() call above.
  //
  // There is an inherent race condition causing bugs 838348/915179/et al, where
  // the renderer may have committed the speculative main frame and the browser
  // has not heard about it yet. If this is a main frame, then in that case the
  // speculative RenderFrame was unable to be deleted (it is owned by the
  // renderer) and we should not be able to cancel the navigation at this point.
  // CleanUpNavigation() would normally be called here but it will try to undo
  // the navigation and expose the race condition. When it replaces the main
  // frame with a RenderFrameProxy, that leaks the committed main frame, leaving
  // the frame and its friend group with pointers that will become invalid
  // shortly as we are shutting everything down and deleting the RenderView etc.
  // We avoid this problematic situation by not calling CleanUpNavigation() or
  // DiscardUnusedFrame() here. The speculative RenderFrameHost is simply
  // returned and deleted immediately. This satisfies the requirement that the
  // speculative RenderFrameHost is removed from the RenderFrameHostManager
  // before it is destroyed.
  if (render_manager_.speculative_frame_host()) {
    did_stop_loading |= render_manager_.speculative_frame_host()->is_loading();
    render_manager_.UnsetSpeculativeRenderFrameHost();
  }

  if (did_stop_loading)
    DidStopLoading();

  DCHECK(!IsLoading());
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

FrameTreeNode::ResetForNavigationResult FrameTreeNode::ResetForNavigation(
    bool was_served_from_back_forward_cache) {
  // TODO(altimin,carlscab): Remove this logic after the relevant states are
  // moved to RenderFrameHost or BrowsingInstanceFrameState.
  ResetForNavigationResult result;

  replication_state_.accumulated_csp_headers.clear();
  if (!was_served_from_back_forward_cache) {
    render_manager_.OnDidResetContentSecurityPolicy();
  } else {
    for (auto& policy : current_frame_host()->ContentSecurityPolicies()) {
      replication_state_.accumulated_csp_headers.push_back(*policy->header);
    }
    // Note: there is no need to call OnDidResetContentSecurityPolicy or any
    // other update as the proxies are being restored from bfcache as well and
    // they already have the correct value.
  }

  // Clear any CSP-set sandbox flags, and the declared feature policy for the
  // frame.
  // TODO(https://crbug.com/1145886): Remove this.
  result.changed_frame_policy =
      UpdateFramePolicyHeaders(network::mojom::WebSandboxFlags::kNone, {});

  // This frame has had its user activation bits cleared in the renderer
  // before arriving here. We just need to clear them here and in the other
  // renderer processes that may have a reference to this frame.
  //
  // We do not take user activation into account when calculating
  // |ResetForNavigationResult|, as we are using it to determine bfcache
  // eligibility and the page can get another user gesture after restore.
  UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kClearActivation,
      blink::mojom::UserActivationNotificationType::kNone);

  return result;
}

size_t FrameTreeNode::GetFrameTreeSize() const {
  if (is_collapsed())
    return 0;

  size_t size = 0;
  for (size_t i = 0; i < child_count(); i++) {
    size += child_at(i)->GetFrameTreeSize();
  }

  // Account for this node.
  size++;
  return size;
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

void FrameTreeNode::SetOpenerDevtoolsFrameToken(
    base::UnguessableToken opener_devtools_frame_token) {
  DCHECK(!opener_devtools_frame_token_ ||
         opener_devtools_frame_token_->is_empty());
  opener_devtools_frame_token_ = std::move(opener_devtools_frame_token);
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
    std::vector<network::mojom::ContentSecurityPolicyHeaderPtr> headers) {
  for (auto& header : headers)
    replication_state_.accumulated_csp_headers.push_back(*header);
  render_manager_.OnDidAddContentSecurityPolicies(std::move(headers));
}

void FrameTreeNode::SetInsecureRequestPolicy(
    blink::mojom::InsecureRequestPolicy policy) {
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
  pending_frame_policy_.disallow_document_access =
      frame_policy.disallow_document_access;

  if (parent()) {
    // Subframes should always inherit their parent's sandbox flags.
    pending_frame_policy_.sandbox_flags |=
        parent()->frame_tree_node()->active_sandbox_flags();
    // This is only applied on subframes; container policy and required document
    // policy are not mutable on main frame.
    pending_frame_policy_.container_policy = frame_policy.container_policy;
    pending_frame_policy_.required_document_policy =
        frame_policy.required_document_policy;
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

bool FrameTreeNode::CommitFramePolicy(
    const blink::FramePolicy& new_frame_policy) {
  bool did_change_flags = new_frame_policy.sandbox_flags !=
                          replication_state_.frame_policy.sandbox_flags;
  bool did_change_container_policy =
      new_frame_policy.container_policy !=
      replication_state_.frame_policy.container_policy;
  bool did_change_required_document_policy =
      pending_frame_policy_.required_document_policy !=
      replication_state_.frame_policy.required_document_policy;
  bool did_change_document_access =
      new_frame_policy.disallow_document_access !=
      replication_state_.frame_policy.disallow_document_access;
  if (did_change_flags)
    replication_state_.frame_policy.sandbox_flags =
        new_frame_policy.sandbox_flags;
  if (did_change_container_policy)
    replication_state_.frame_policy.container_policy =
        new_frame_policy.container_policy;
  if (did_change_required_document_policy)
    replication_state_.frame_policy.required_document_policy =
        new_frame_policy.required_document_policy;
  if (did_change_document_access)
    replication_state_.frame_policy.disallow_document_access =
        new_frame_policy.disallow_document_access;

  UpdateFramePolicyHeaders(new_frame_policy.sandbox_flags,
                           replication_state_.feature_policy_header);
  return did_change_flags || did_change_container_policy ||
         did_change_required_document_policy || did_change_document_access;
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
    if (navigation_request_ && navigation_request_->IsNavigationStarted()) {
      // Mark the old request as aborted.
      navigation_request_->set_net_error(net::ERR_ABORTED);
    }
    ResetNavigationRequest(true);
  }

  navigation_request_ = std::move(navigation_request);
  if (was_discarded_) {
    navigation_request_->set_was_discarded();
    was_discarded_ = false;
  }
  render_manager()->DidCreateNavigationRequest(navigation_request_.get());

  bool to_different_document = !NavigationTypeUtils::IsSameDocument(
      navigation_request_->common_params().navigation_type);

  DidStartLoading(to_different_document, was_previously_loading);
}

void FrameTreeNode::ResetNavigationRequest(bool keep_state) {
  if (!navigation_request_)
    return;

  devtools_instrumentation::OnResetNavigationRequest(navigation_request_.get());
  navigation_request_.reset();

  if (keep_state)
    return;

  // The RenderFrameHostManager should clean up any speculative RenderFrameHost
  // it created for the navigation. Also register that the load stopped.
  DidStopLoading();
  render_manager_.CleanUpNavigation();
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
    navigator().GetDelegate()->DidStartLoading(this, to_different_document);

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

  // Notify the RenderFrameHostManager of the event.
  render_manager()->OnDidStopLoading();

  // Notify the WebContents.
  if (!frame_tree_->IsLoading())
    navigator().GetDelegate()->DidStopLoading();
}

void FrameTreeNode::DidChangeLoadProgress(double load_progress) {
  DCHECK_GE(load_progress, kLoadingProgressMinimum);
  DCHECK_LE(load_progress, kLoadingProgressDone);
  if (IsMainFrame())
    frame_tree_->UpdateLoadProgress(load_progress);
}

bool FrameTreeNode::StopLoading() {
  if (navigation_request_ && navigation_request_->IsNavigationStarted())
    navigation_request_->set_net_error(net::ERR_ABORTED);
  ResetNavigationRequest(false);

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
  // BeforeUnloadCompleted callback is invoked.
  if (navigation_request_)
    ResetNavigationRequest(false);
}

bool FrameTreeNode::NotifyUserActivation(
    blink::mojom::UserActivationNotificationType notification_type) {
  for (RenderFrameHostImpl* rfh = current_frame_host(); rfh;
       rfh = rfh->GetParent()) {
    if (!rfh->frame_tree_node()->user_activation_state_.HasBeenActive())
      rfh->DidReceiveFirstUserActivation();
    rfh->frame_tree_node()->user_activation_state_.Activate(notification_type);
  }
  replication_state_.has_active_user_gesture = true;

  // See the "Same-origin Visibility" section in |UserActivationState| class
  // doc.
  if (base::FeatureList::IsEnabled(
          features::kUserActivationSameOriginVisibility)) {
    const url::Origin& current_origin =
        this->current_frame_host()->GetLastCommittedOrigin();
    for (FrameTreeNode* node : frame_tree()->Nodes()) {
      if (node->current_frame_host()->GetLastCommittedOrigin().IsSameOriginWith(
              current_origin)) {
        node->user_activation_state_.Activate(notification_type);
      }
    }
  }

  NavigationControllerImpl* controller =
      static_cast<NavigationControllerImpl*>(navigator().GetController());
  if (controller)
    controller->NotifyUserActivation();

  return true;
}

bool FrameTreeNode::ConsumeTransientUserActivation() {
  bool was_active = user_activation_state_.IsActive();
  for (FrameTreeNode* node : frame_tree()->Nodes())
    node->user_activation_state_.ConsumeIfActive();
  replication_state_.has_active_user_gesture = false;
  return was_active;
}

bool FrameTreeNode::ClearUserActivation() {
  for (FrameTreeNode* node : frame_tree()->SubtreeNodes(this))
    node->user_activation_state_.Clear();
  replication_state_.has_active_user_gesture = false;
  return true;
}

bool FrameTreeNode::VerifyUserActivation() {
  DCHECK(base::FeatureList::IsEnabled(
             features::kBrowserVerifiedUserActivationMouse) ||
         base::FeatureList::IsEnabled(
             features::kBrowserVerifiedUserActivationKeyboard));

  return render_manager_.current_frame_host()
      ->GetRenderWidgetHost()
      ->RemovePendingUserActivationIfAvailable();
}

bool FrameTreeNode::UpdateUserActivationState(
    blink::mojom::UserActivationUpdateType update_type,
    blink::mojom::UserActivationNotificationType notification_type) {
  bool update_result = false;
  switch (update_type) {
    case blink::mojom::UserActivationUpdateType::kConsumeTransientActivation:
      update_result = ConsumeTransientUserActivation();
      break;
    case blink::mojom::UserActivationUpdateType::kNotifyActivation:
      update_result = NotifyUserActivation(notification_type);
      break;
    case blink::mojom::UserActivationUpdateType::
        kNotifyActivationPendingBrowserVerification: {
      const bool user_activation_verified = VerifyUserActivation();
      // Add UMA metric for when browser user activation verification succeeds
      base::UmaHistogramBoolean("Event.BrowserVerifiedUserActivation",
                                user_activation_verified);
      if (user_activation_verified) {
        update_result = NotifyUserActivation(
            blink::mojom::UserActivationNotificationType::kInteraction);
        update_type = blink::mojom::UserActivationUpdateType::kNotifyActivation;
      } else {
        // TODO(crbug.com/848778): We need to decide what to do when user
        // activation verification failed. NOTREACHED here will make all
        // unrelated tests that inject event to renderer fail.
        return false;
      }
    } break;
    case blink::mojom::UserActivationUpdateType::kClearActivation:
      update_result = ClearUserActivation();
      break;
  }
  render_manager_.UpdateUserActivationState(update_type, notification_type);
  return update_result;
}

void FrameTreeNode::OnSetHadStickyUserActivationBeforeNavigation(bool value) {
  render_manager_.OnSetHadStickyUserActivationBeforeNavigation(value);
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

bool FrameTreeNode::UpdateFramePolicyHeaders(
    network::mojom::WebSandboxFlags sandbox_flags,
    const blink::ParsedFeaturePolicy& parsed_header) {
  bool changed = false;
  if (replication_state_.feature_policy_header != parsed_header) {
    replication_state_.feature_policy_header = parsed_header;
    changed = true;
  }
  // TODO(iclelland): Kill the renderer if sandbox flags is not a subset of the
  // currently effective sandbox flags from the frame. https://crbug.com/740556
  network::mojom::WebSandboxFlags updated_flags =
      sandbox_flags | effective_frame_policy().sandbox_flags;
  if (replication_state_.active_sandbox_flags != updated_flags) {
    replication_state_.active_sandbox_flags = updated_flags;
    changed = true;
  }
  // Notify any proxies if the policies have been changed.
  if (changed)
    render_manager()->OnDidSetFramePolicyHeaders();
  return changed;
}

void FrameTreeNode::PruneChildFrameNavigationEntries(
    NavigationEntryImpl* entry) {
  for (size_t i = 0; i < current_frame_host()->child_count(); ++i) {
    FrameTreeNode* child = current_frame_host()->child_at(i);
    if (child->is_created_by_script_) {
      entry->RemoveEntryForFrame(child,
                                 /* only_if_different_position = */ false);
    } else {
      child->PruneChildFrameNavigationEntries(entry);
    }
  }
}

void FrameTreeNode::SetOpenerFeaturePolicyState(
    const blink::FeaturePolicyFeatureState& feature_state) {
  DCHECK(IsMainFrame());
  if (base::FeatureList::IsEnabled(features::kFeaturePolicyForSandbox)) {
    replication_state_.opener_feature_state = feature_state;
  }
}

void FrameTreeNode::SetAdFrameType(blink::mojom::AdFrameType ad_frame_type) {
  DCHECK_NE(ad_frame_type, blink::mojom::AdFrameType::kNonAd);
  if (replication_state_.ad_frame_type == blink::mojom::AdFrameType::kNonAd) {
    replication_state_.ad_frame_type = ad_frame_type;
    render_manager()->OnDidSetAdFrameType(ad_frame_type);
  } else {
    DCHECK_EQ(ad_frame_type, replication_state_.ad_frame_type);
  }
}

void FrameTreeNode::SetInitialPopupURL(const GURL& initial_popup_url) {
  DCHECK(initial_popup_url_.is_empty());
  DCHECK(!has_committed_real_load_);
  initial_popup_url_ = initial_popup_url;
}

void FrameTreeNode::SetPopupCreatorOrigin(
    const url::Origin& popup_creator_origin) {
  DCHECK(!has_committed_real_load_);
  popup_creator_origin_ = popup_creator_origin;
}

}  // namespace content
