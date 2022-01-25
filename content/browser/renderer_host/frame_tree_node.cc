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
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/timer/elapsed_timer.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/navigator_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/navigation_params_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/common/content_features.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-shared.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/loader_constants.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom.h"

namespace content {

namespace {

// This is a global map between frame_tree_node_ids and pointers to
// FrameTreeNodes.
typedef std::unordered_map<int, FrameTreeNode*> FrameTreeNodeIdMap;

base::LazyInstance<FrameTreeNodeIdMap>::DestructorAtExit
    g_frame_tree_node_id_map = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// This observer watches the opener of its owner FrameTreeNode and clears the
// owner's opener if the opener is destroyed.
class FrameTreeNode::OpenerDestroyedObserver : public FrameTreeNode::Observer {
 public:
  OpenerDestroyedObserver(FrameTreeNode* owner, bool observing_original_opener)
      : owner_(owner), observing_original_opener_(observing_original_opener) {}

  OpenerDestroyedObserver(const OpenerDestroyedObserver&) = delete;
  OpenerDestroyedObserver& operator=(const OpenerDestroyedObserver&) = delete;

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
  raw_ptr<FrameTreeNode> owner_;
  bool observing_original_opener_;
};

const int FrameTreeNode::kFrameTreeNodeInvalidId = -1;

static_assert(FrameTreeNode::kFrameTreeNodeInvalidId ==
                  RenderFrameHost::kNoFrameTreeNodeId,
              "Have consistent sentinel values for an invalid FTN id.");

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
    blink::mojom::TreeScopeType tree_scope_type,
    const std::string& name,
    const std::string& unique_name,
    bool is_created_by_script,
    const base::UnguessableToken& devtools_frame_token,
    const blink::mojom::FrameOwnerProperties& frame_owner_properties,
    blink::FrameOwnerElementType owner_type,
    const blink::FramePolicy& frame_policy)
    : frame_tree_(frame_tree),
      frame_tree_node_id_(next_frame_tree_node_id_++),
      parent_(parent),
      frame_owner_element_type_(owner_type),
      tree_scope_type_(tree_scope_type),
      replication_state_(blink::mojom::FrameReplicationState::New(
          url::Origin(),
          name,
          unique_name,
          blink::ParsedPermissionsPolicy(),
          network::mojom::WebSandboxFlags::kNone,
          frame_policy,
          // should enforce strict mixed content checking
          blink::mojom::InsecureRequestPolicy::kLeaveInsecureRequestsAlone,
          // hashes of hosts for insecure request upgrades
          std::vector<uint32_t>(),
          false /* is a potentially trustworthy unique origin */,
          false /* has an active user gesture */,
          false /* has received a user gesture before nav */,
          false /* is_ad_subframe */)),
      pending_frame_policy_(frame_policy),
      is_created_by_script_(is_created_by_script),
      devtools_frame_token_(devtools_frame_token),
      frame_owner_properties_(frame_owner_properties),
      blame_context_(frame_tree_node_id_, FrameTreeNode::From(parent)),
      render_manager_(this, frame_tree->manager_delegate()) {
  std::pair<FrameTreeNodeIdMap::iterator, bool> result =
      g_frame_tree_node_id_map.Get().insert(
          std::make_pair(frame_tree_node_id_, this));
  CHECK(result.second);

  // Note: this should always be done last in the constructor.
  blame_context_.Initialize();
}

FrameTreeNode::~FrameTreeNode() {
  // There should always be a current RenderFrameHost except during prerender
  // activation. Prerender activation moves the current RenderFrameHost from
  // the old FrameTree's FrameTreeNode to the new FrameTree's FrameTreeNode and
  // then destroys the old FrameTree. See
  // `RenderFrameHostManager::TakePrerenderedPage()`.
  if (current_frame_host()) {
    // Remove the children.
    current_frame_host()->ResetChildren();

    current_frame_host()->ResetLoadingState();
  } else {
    DCHECK(blink::features::IsPrerender2Enabled());
    DCHECK(!parent());  // Only main documents can be activated.
    DCHECK(!opener());  // Prerendered frame trees can't have openers.

    // Activation is not allowed during ongoing navigations.
    DCHECK(!navigation_request_);

    // TODO(https://crbug.com/1199693): Need to determine how to handle pending
    // deletions, as observers will be notified.
    DCHECK(!render_manager()->speculative_frame_host());
  }

  // If the removed frame was created by a script, then its history entry will
  // never be reused - we can save some memory by removing the history entry.
  // See also https://crbug.com/784356.
  if (is_created_by_script_ && parent_) {
    NavigationEntryImpl* nav_entry =
        navigator().controller().GetLastCommittedEntry();
    if (nav_entry) {
      nav_entry->RemoveEntryForFrame(this,
                                     /* only_if_different_position = */ false);
    }
  }

  frame_tree_->FrameRemoved(this);

  // Do not dispatch notification for the root frame as ~WebContentsImpl already
  // dispatches it for now.
  // TODO(https://crbug.com/1170277): This is only needed because the FrameTree
  // is a member of WebContentsImpl and we would call back into it during
  // destruction. We should clean up the FrameTree destruction code and call the
  // delegate unconditionally.
  if (parent())
    render_manager_.delegate()->OnFrameTreeNodeDestroyed(this);

  for (auto& observer : observers_)
    observer.OnFrameTreeNodeDestroyed(this);
  observers_.Clear();

  if (opener_)
    opener_->RemoveObserver(opener_observer_.get());
  if (original_opener_)
    original_opener_->RemoveObserver(original_opener_observer_.get());

  g_frame_tree_node_id_map.Get().erase(frame_tree_node_id_);

  // If a frame with a pending navigation is detached, make sure the
  // WebContents (and its observers) update their loading state.
  // TODO(dcheng): This should just check `IsLoading()`, but `IsLoading()`
  // assumes that `current_frame_host_` is not null. This is incompatible with
  // prerender activation when destroying the old frame tree (see above).
  bool did_stop_loading = false;

  if (navigation_request_) {
    navigation_request_.reset();
    did_stop_loading = true;
  }

  // ~SiteProcessCountTracker DCHECKs in some tests if the speculative
  // RenderFrameHostImpl is not destroyed last. Ideally this would be closer to
  // (possible before) the ResetLoadingState() call above.
  if (render_manager_.speculative_frame_host()) {
    // TODO(dcheng): Shouldn't a FrameTreeNode with a speculative
    // RenderFrameHost always be considered loading?
    did_stop_loading |= render_manager_.speculative_frame_host()->is_loading();
    // `FrameTree::Shutdown()` has special handling for the main frame's
    // speculative RenderFrameHost, and the speculative RenderFrameHost should
    // already be reset for main frames.
    DCHECK(!IsMainFrame());

    // This does not use `UnsetSpeculativeRenderFrameHost()`: if the speculative
    // RenderFrameHost has already reached kPendingCommit, it would needlessly
    // re-create a proxy for a frame that's going away.
    render_manager_.DiscardSpeculativeRenderFrameHostForShutdown();
  }

  if (did_stop_loading)
    DidStopLoading();

  // IsLoading() requires that current_frame_host() is non-null.
  DCHECK(!current_frame_host() || !IsLoading());
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
  // This frame has had its user activation bits cleared in the renderer before
  // arriving here. We just need to clear them here and in the other renderer
  // processes that may have a reference to this frame.
  //
  // We do not take user activation into account when calculating
  // |ResetForNavigationResult|, as we are using it to determine bfcache
  // eligibility and the page can get another user gesture after restore.
  UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kClearActivation,
      blink::mojom::UserActivationNotificationType::kNone);
}

RenderFrameHostImpl* FrameTreeNode::GetParentOrOuterDocument() {
  return GetParentOrOuterDocumentHelper(/*escape_guest_view=*/false);
}

RenderFrameHostImpl* FrameTreeNode::GetParentOrOuterDocumentOrEmbedder() {
  return GetParentOrOuterDocumentHelper(/*escape_guest_view=*/true);
}

RenderFrameHostImpl* FrameTreeNode::GetParentOrOuterDocumentHelper(
    bool escape_guest_view) {
  // Find the parent in the FrameTree (iframe).
  if (parent_)
    return parent_;

  if (!escape_guest_view) {
    // If we are not a fenced frame root nor inside a portal then return early.
    // This code does not escape GuestViews.
    if (!IsFencedFrameRoot() && !frame_tree_->delegate()->IsPortal())
      return nullptr;
  }

  // Find the parent in the outer embedder (GuestView, Portal, or Fenced Frame).
  FrameTreeNode* frame_in_embedder = render_manager()->GetOuterDelegateNode();
  if (frame_in_embedder)
    return frame_in_embedder->current_frame_host()->GetParent();

  // No parent found.
  return nullptr;
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
  current_frame_host()->SetLastCommittedUrl(url);
  blame_context_.TakeSnapshot();
}

void FrameTreeNode::SetCurrentOrigin(
    const url::Origin& origin,
    bool is_potentially_trustworthy_unique_origin) {
  if (!origin.IsSameOriginWith(replication_state_->origin) ||
      replication_state_->has_potentially_trustworthy_unique_origin !=
          is_potentially_trustworthy_unique_origin) {
    render_manager_.OnDidUpdateOrigin(origin,
                                      is_potentially_trustworthy_unique_origin);
  }
  replication_state_->origin = origin;
  replication_state_->has_potentially_trustworthy_unique_origin =
      is_potentially_trustworthy_unique_origin;
}

void FrameTreeNode::SetCollapsed(bool collapsed) {
  DCHECK(!IsMainFrame());
  if (is_collapsed_ == collapsed)
    return;

  is_collapsed_ = collapsed;
  render_manager_.OnDidChangeCollapsedState(collapsed);
}

void FrameTreeNode::SetFrameTree(FrameTree& frame_tree) {
  DCHECK(blink::features::IsPrerender2Enabled());
  frame_tree_ = &frame_tree;
  DCHECK(current_frame_host());
  current_frame_host()->SetFrameTree(frame_tree);
  RenderFrameHostImpl* speculative_frame_host =
      render_manager_.speculative_frame_host();
  if (speculative_frame_host)
    speculative_frame_host->SetFrameTree(frame_tree);
}

void FrameTreeNode::SetFrameName(const std::string& name,
                                 const std::string& unique_name) {
  if (name == replication_state_->name) {
    // |unique_name| shouldn't change unless |name| changes.
    DCHECK_EQ(unique_name, replication_state_->unique_name);
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
  replication_state_->name = name;
  replication_state_->unique_name = unique_name;
}

void FrameTreeNode::SetInsecureRequestPolicy(
    blink::mojom::InsecureRequestPolicy policy) {
  if (policy == replication_state_->insecure_request_policy)
    return;
  render_manager_.OnEnforceInsecureRequestPolicy(policy);
  replication_state_->insecure_request_policy = policy;
}

void FrameTreeNode::SetInsecureNavigationsSet(
    const std::vector<uint32_t>& insecure_navigations_set) {
  DCHECK(std::is_sorted(insecure_navigations_set.begin(),
                        insecure_navigations_set.end()));
  if (insecure_navigations_set == replication_state_->insecure_navigations_set)
    return;
  render_manager_.OnEnforceInsecureNavigationsSet(insecure_navigations_set);
  replication_state_->insecure_navigations_set = insecure_navigations_set;
}

void FrameTreeNode::SetPendingFramePolicy(blink::FramePolicy frame_policy) {
  // The |is_fenced| bit should never be able to transition from what its
  // initial value was. Since we never expect to be in a position where it can
  // even be updated to new value, if we catch this happening we have to kill
  // the renderer and refuse to accept any other frame policy changes here.
  if (pending_frame_policy_.is_fenced != frame_policy.is_fenced) {
    mojo::ReportBadMessage(
        "The `is_fenced` FramePolicy bit is const and should never be changed");
    return;
  }

  pending_frame_policy_.sandbox_flags = frame_policy.sandbox_flags;

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

bool FrameTreeNode::IsLoading() const {
  RenderFrameHostImpl* current_frame_host =
      render_manager_.current_frame_host();

  DCHECK(current_frame_host);

  if (navigation_request_)
    return true;

  RenderFrameHostImpl* speculative_frame_host =
      render_manager_.speculative_frame_host();
  // TODO(dcheng): Shouldn't a FrameTreeNode with a speculative RenderFrameHost
  // always be considered loading?
  if (speculative_frame_host && speculative_frame_host->is_loading())
    return true;
  return current_frame_host->is_loading();
}

bool FrameTreeNode::HasPendingCrossDocumentNavigation() const {
  // Having a |navigation_request_| on FrameTreeNode implies that there's an
  // ongoing navigation that hasn't reached the ReadyToCommit state.  If the
  // navigation is between ReadyToCommit and DidCommitNavigation, the
  // NavigationRequest will be held by RenderFrameHost, which is checked below.
  if (navigation_request_ && !navigation_request_->IsSameDocument())
    return true;

  // Having a speculative RenderFrameHost should imply a cross-document
  // navigation.
  if (render_manager_.speculative_frame_host())
    return true;

  return render_manager_.current_frame_host()
      ->HasPendingCommitForCrossDocumentNavigation();
}

bool FrameTreeNode::CommitFramePolicy(
    const blink::FramePolicy& new_frame_policy) {
  // Documents create iframes, iframes host new documents. Both are associated
  // with sandbox flags. They are required to be stricter or equal to their
  // owner when they change, as we go down.
  // TODO(https://crbug.com/1262061). Enforce the invariant mentioned above,
  // once the interactions with FencedIframe has been tested and clarified.

  bool did_change_flags = new_frame_policy.sandbox_flags !=
                          replication_state_->frame_policy.sandbox_flags;
  bool did_change_container_policy =
      new_frame_policy.container_policy !=
      replication_state_->frame_policy.container_policy;
  bool did_change_required_document_policy =
      pending_frame_policy_.required_document_policy !=
      replication_state_->frame_policy.required_document_policy;
  DCHECK_EQ(new_frame_policy.is_fenced,
            replication_state_->frame_policy.is_fenced);
  if (did_change_flags) {
    replication_state_->frame_policy.sandbox_flags =
        new_frame_policy.sandbox_flags;
  }
  if (did_change_container_policy) {
    replication_state_->frame_policy.container_policy =
        new_frame_policy.container_policy;
  }
  if (did_change_required_document_policy) {
    replication_state_->frame_policy.required_document_policy =
        new_frame_policy.required_document_policy;
  }

  UpdateFramePolicyHeaders(new_frame_policy.sandbox_flags,
                           replication_state_->permissions_policy_header);
  return did_change_flags || did_change_container_policy ||
         did_change_required_document_policy;
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

void FrameTreeNode::DidStartLoading(bool should_show_loading_ui,
                                    bool was_previously_loading) {
  TRACE_EVENT2("navigation", "FrameTreeNode::DidStartLoading",
               "frame_tree_node", frame_tree_node_id(),
               "should_show_loading_ui ", should_show_loading_ui);
  base::ElapsedTimer timer;

  frame_tree_->DidStartLoadingNode(*this, should_show_loading_ui,
                                   was_previously_loading);

  // Set initial load progress and update overall progress. This will notify
  // the WebContents of the load progress change.
  DidChangeLoadProgress(blink::kInitialLoadProgress);

  // Notify the RenderFrameHostManager of the event.
  render_manager()->OnDidStartLoading();
  base::UmaHistogramTimes(
      base::StrCat({"Navigation.DidStartLoading.",
                    IsMainFrame() ? "MainFrame" : "Subframe"}),
      timer.Elapsed());
}

void FrameTreeNode::DidStopLoading() {
  TRACE_EVENT1("navigation", "FrameTreeNode::DidStopLoading", "frame_tree_node",
               frame_tree_node_id());
  // Set final load progress and update overall progress. This will notify
  // the WebContents of the load progress change.
  DidChangeLoadProgress(blink::kFinalLoadProgress);

  // Notify the RenderFrameHostManager of the event.
  render_manager()->OnDidStopLoading();

  frame_tree_->DidStopLoadingNode(*this);
}

void FrameTreeNode::DidChangeLoadProgress(double load_progress) {
  DCHECK_GE(load_progress, blink::kInitialLoadProgress);
  DCHECK_LE(load_progress, blink::kFinalLoadProgress);
  current_frame_host()->DidChangeLoadProgress(load_progress);
}

bool FrameTreeNode::StopLoading() {
  if (navigation_request_ && navigation_request_->IsNavigationStarted())
    navigation_request_->set_net_error(net::ERR_ABORTED);
  ResetNavigationRequest(false);

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
  // Note: there is no need to set an error code on the NavigationHandle as
  // the observers have not been notified about its creation.
  // We also reset navigation request only when this navigation request was
  // responsible for this dialog, as a new navigation request might cancel
  // existing unrelated dialog.
  if (navigation_request_ && navigation_request_->IsWaitingForBeforeUnload())
    ResetNavigationRequest(false);
}

bool FrameTreeNode::NotifyUserActivation(
    blink::mojom::UserActivationNotificationType notification_type) {
  // User Activation V2 requires activating all ancestor frames in addition to
  // the current frame. See
  // https://html.spec.whatwg.org/multipage/interaction.html#tracking-user-activation.
  for (RenderFrameHostImpl* rfh = current_frame_host(); rfh;
       rfh = rfh->GetParent()) {
    rfh->DidReceiveUserActivation();
    rfh->frame_tree_node()->user_activation_state_.Activate(notification_type);
  }

  replication_state_->has_active_user_gesture = true;

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

  navigator().controller().NotifyUserActivation();
  current_frame_host()->MaybeIsolateForUserActivation();

  return true;
}

bool FrameTreeNode::ConsumeTransientUserActivation() {
  bool was_active = user_activation_state_.IsActive();
  for (FrameTreeNode* node : frame_tree()->Nodes())
    node->user_activation_state_.ConsumeIfActive();
  replication_state_->has_active_user_gesture = false;
  return was_active;
}

bool FrameTreeNode::ClearUserActivation() {
  for (FrameTreeNode* node : frame_tree()->SubtreeNodes(this))
    node->user_activation_state_.Clear();
  replication_state_->has_active_user_gesture = false;
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
        // TODO(https://crbug.com/848778): We need to decide what to do when
        // user activation verification failed. NOTREACHED here will make all
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
  replication_state_->has_received_user_gesture_before_nav = value;
}

bool FrameTreeNode::UpdateFramePolicyHeaders(
    network::mojom::WebSandboxFlags sandbox_flags,
    const blink::ParsedPermissionsPolicy& parsed_header) {
  bool changed = false;
  if (replication_state_->permissions_policy_header != parsed_header) {
    replication_state_->permissions_policy_header = parsed_header;
    changed = true;
  }
  // TODO(iclelland): Kill the renderer if sandbox flags is not a subset of the
  // currently effective sandbox flags from the frame. https://crbug.com/740556
  network::mojom::WebSandboxFlags updated_flags =
      sandbox_flags | effective_frame_policy().sandbox_flags;
  if (replication_state_->active_sandbox_flags != updated_flags) {
    replication_state_->active_sandbox_flags = updated_flags;
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

void FrameTreeNode::SetIsAdSubframe(bool is_ad_subframe) {
  if (is_ad_subframe == replication_state_->is_ad_subframe)
    return;

  replication_state_->is_ad_subframe = is_ad_subframe;
  render_manager()->OnDidSetIsAdSubframe(is_ad_subframe);
}

void FrameTreeNode::SetInitialPopupURL(const GURL& initial_popup_url) {
  DCHECK(initial_popup_url_.is_empty());
  DCHECK(is_on_initial_empty_document_);
  initial_popup_url_ = initial_popup_url;
}

void FrameTreeNode::SetPopupCreatorOrigin(
    const url::Origin& popup_creator_origin) {
  DCHECK(is_on_initial_empty_document_);
  popup_creator_origin_ = popup_creator_origin;
}

void FrameTreeNode::WriteIntoTrace(perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("id", frame_tree_node_id());
  dict.Add("is_main_frame", IsMainFrame());
}

void FrameTreeNode::WriteIntoTrace(
    perfetto::TracedProto<perfetto::protos::pbzero::FrameTreeNodeInfo> proto) {
  proto->set_is_main_frame(IsMainFrame());
  proto->set_frame_tree_node_id(frame_tree_node_id());
  proto->set_has_speculative_render_frame_host(
      !!render_manager()->speculative_frame_host());
}

bool FrameTreeNode::HasNavigation() {
  if (navigation_request())
    return true;

  // Same-RenderFrameHost navigation is committing:
  if (current_frame_host()->HasPendingCommitNavigation())
    return true;

  // Cross-RenderFrameHost navigation is committing:
  if (render_manager()->speculative_frame_host())
    return true;

  return false;
}

bool FrameTreeNode::IsFencedFrameRoot() const {
  if (!blink::features::IsFencedFramesEnabled())
    return false;

  switch (blink::features::kFencedFramesImplementationTypeParam.Get()) {
    case blink::features::FencedFramesImplementationType::kMPArch: {
      return IsMainFrame() &&
             frame_tree()->type() == FrameTree::Type::kFencedFrame;
    }
    case blink::features::FencedFramesImplementationType::kShadowDOM: {
      return effective_frame_policy().is_fenced;
    }
    default:
      return false;
  }
}

bool FrameTreeNode::IsInFencedFrameTree() const {
  if (!blink::features::IsFencedFramesEnabled())
    return false;

  switch (blink::features::kFencedFramesImplementationTypeParam.Get()) {
    case blink::features::FencedFramesImplementationType::kMPArch:
      return frame_tree()->type() == FrameTree::Type::kFencedFrame;
    case blink::features::FencedFramesImplementationType::kShadowDOM: {
      auto* node = this;
      while (node) {
        if (node->effective_frame_policy().is_fenced) {
          return true;
        }
        node = node->parent() ? node->parent()->frame_tree_node() : nullptr;
      }
      return false;
    }
    default:
      return false;
  }
}

void FrameTreeNode::SetFencedFrameNonceIfNeeded() {
  if (!IsInFencedFrameTree()) {
    return;
  }

  if (IsFencedFrameRoot()) {
    fenced_frame_nonce_ = base::UnguessableToken::Create();
    return;
  }

  // For nested iframes in a fenced frame tree, propagate the same nonce as was
  // set in the fenced frame root.
  DCHECK(parent_);
  absl::optional<base::UnguessableToken> nonce =
      parent_->frame_tree_node()->fenced_frame_nonce();
  DCHECK(nonce.has_value());
  fenced_frame_nonce_ = nonce;
}

bool FrameTreeNode::IsErrorPageIsolationEnabled() const {
  // Enable error page isolation for fenced frames in both MPArch and ShadowDOM
  // modes to address the issue with invalid urn:uuid (crbug.com/1264224).
  //
  // Note that `IsMainFrame()` only covers MPArch, therefore we add explicit
  // `IsFencedFrameRoot()` check for ShadowDOM, at least until error page
  // isolation is supported for subframes in crbug.com/1092524.
  return SiteIsolationPolicy::IsErrorPageIsolationEnabled(IsMainFrame() ||
                                                          IsFencedFrameRoot());
}

}  // namespace content
