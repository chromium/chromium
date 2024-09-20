// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/frame_tree_node.h"

#include <math.h>
#include <queue>
#include <unordered_map>
#include <utility>

#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/timer/elapsed_timer.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/fenced_frame/fenced_frame.h"
#include "content/browser/network/cross_origin_embedder_policy_reporter.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/navigator_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/webauth/authenticator_environment.h"
#include "content/common/navigation_params_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/common/content_features.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-shared.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/frame/fenced_frame_sandbox_flags.h"
#include "third_party/blink/public/common/loader/loader_constants.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom.h"

namespace content {

namespace {

// This is a global map between frame_tree_node_ids and pointers to
// FrameTreeNodes.
using FrameTreeNodeIdMap = std::
    unordered_map<FrameTreeNodeId, FrameTreeNode*, FrameTreeNodeId::Hasher>;

base::LazyInstance<FrameTreeNodeIdMap>::DestructorAtExit
    g_frame_tree_node_id_map = LAZY_INSTANCE_INITIALIZER;

FencedFrame* FindFencedFrame(const FrameTreeNode* frame_tree_node) {
  // TODO(crbug.com/40053214): Consider having a pointer to `FencedFrame` in
  // `FrameTreeNode` or having a map between them.

  // Try and find the `FencedFrame` that `frame_tree_node` represents.
  DCHECK(frame_tree_node->parent());
  std::vector<FencedFrame*> fenced_frames =
      frame_tree_node->parent()->GetFencedFrames();
  for (FencedFrame* fenced_frame : fenced_frames) {
    if (frame_tree_node->frame_tree_node_id() ==
        fenced_frame->GetOuterDelegateFrameTreeNodeId()) {
      return fenced_frame;
    }
  }
  return nullptr;
}

}  // namespace

// This observer watches the opener of its owner FrameTreeNode and clears the
// owner's opener if the opener is destroyed or swaps BrowsingInstance.
class FrameTreeNode::OpenerDestroyedObserver : public FrameTreeNode::Observer {
 public:
  OpenerDestroyedObserver(FrameTreeNode* owner, bool observing_original_opener)
      : owner_(owner), observing_original_opener_(observing_original_opener) {}

  OpenerDestroyedObserver(const OpenerDestroyedObserver&) = delete;
  OpenerDestroyedObserver& operator=(const OpenerDestroyedObserver&) = delete;

  // FrameTreeNode::Observer
  void OnFrameTreeNodeDestroyed(FrameTreeNode* node) override {
    NullifyOpener(node);
  }

  // FrameTreeNode::Observer
  void OnFrameTreeNodeDisownedOpenee(FrameTreeNode* node) override {
    NullifyOpener(node);
  }

  void NullifyOpener(FrameTreeNode* node) {
    if (observing_original_opener_) {
      // The "original opener" is special. It's used for attribution, and
      // clients walk down the original opener chain. Therefore, if a link in
      // the chain is being destroyed, reconnect the observation to the parent
      // of the link being destroyed.
      CHECK_EQ(owner_->first_live_main_frame_in_original_opener_chain(), node);
      owner_->SetOriginalOpener(
          node->first_live_main_frame_in_original_opener_chain());
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

// static
FrameTreeNodeId::Generator FrameTreeNode::frame_tree_node_id_generator_;

// static
FrameTreeNode* FrameTreeNode::GloballyFindByID(
    FrameTreeNodeId frame_tree_node_id) {
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

FrameTreeNode::FencedFrameStatus ComputeFencedFrameStatus(
    const FrameTree& frame_tree,
    RenderFrameHostImpl* parent,
    const blink::FramePolicy& frame_policy) {
  using FencedFrameStatus = FrameTreeNode::FencedFrameStatus;
  if (blink::features::IsFencedFramesEnabled() &&
      frame_tree.is_fenced_frame()) {
    if (!parent) {
      return FencedFrameStatus::kFencedFrameRoot;
    }
    return FencedFrameStatus::kIframeNestedWithinFencedFrame;
  }

  return FencedFrameStatus::kNotNestedInFencedFrame;
}

FrameTreeNode::FrameTreeNode(
    FrameTree& frame_tree,
    RenderFrameHostImpl* parent,
    blink::mojom::TreeScopeType tree_scope_type,
    bool is_created_by_script,
    const blink::mojom::FrameOwnerProperties& frame_owner_properties,
    blink::FrameOwnerElementType owner_type,
    const blink::FramePolicy& frame_policy)
    : frame_tree_(frame_tree),
      frame_tree_node_id_(frame_tree_node_id_generator_.GenerateNextId()),
      parent_(parent),
      frame_owner_element_type_(owner_type),
      tree_scope_type_(tree_scope_type),
      pending_frame_policy_(frame_policy),
      is_created_by_script_(is_created_by_script),
      frame_owner_properties_(frame_owner_properties),
      attributes_(blink::mojom::IframeAttributes::New()),
      fenced_frame_status_(
          ComputeFencedFrameStatus(frame_tree, parent_, frame_policy)),
      render_manager_(this, frame_tree.manager_delegate()) {
  TRACE_EVENT_BEGIN("navigation", "FrameTreeNode",
                    perfetto::Track::FromPointer(this),
                    "frame_tree_node_when_created", this);
  std::pair<FrameTreeNodeIdMap::iterator, bool> result =
      g_frame_tree_node_id_map.Get().insert(
          std::make_pair(frame_tree_node_id_, this));
  CHECK(result.second);
}

void FrameTreeNode::DestroyInnerFrameTreeIfExists() {
  // If `this` is an dummy outer delegate node, then we really are representing
  // an inner FrameTree for one of the following consumers:
  //   - `FencedFrame`
  //   - `GuestView`
  // If we are representing a `FencedFrame` object, we need to destroy it
  // alongside ourself. `GuestView` however, *currently* has a more complex
  // lifetime and is dealt with separately.
  bool is_outer_dummy_node = false;
  if (current_frame_host() &&
      current_frame_host()->inner_tree_main_frame_tree_node_id()) {
    is_outer_dummy_node = true;
  }

  if (is_outer_dummy_node) {
    if (FencedFrame* doomed_fenced_frame = FindFencedFrame(this)) {
      parent()->DestroyFencedFrame(*doomed_fenced_frame);
    }
  }
}

FrameTreeNode::~FrameTreeNode() {
  TRACE_EVENT("navigation", "FrameTreeNode::~FrameTreeNode");
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
    DCHECK(!parent());  // Only main documents can be activated.
    DCHECK(!opener());  // Prerendered frame trees can't have openers.

    // Activation is not allowed during ongoing navigations.
    CHECK(!navigation_request_);

    // TODO(crbug.com/40177949): Need to determine how to handle pending
    // deletions, as observers will be notified.
    CHECK(!render_manager()->speculative_frame_host());
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

  DestroyInnerFrameTreeIfExists();

  devtools_instrumentation::OnFrameTreeNodeDestroyed(*this);
  // Do not dispatch notification for the root frame as ~WebContentsImpl already
  // dispatches it for now.
  // TODO(crbug.com/40165695): This is only needed because the FrameTree
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
  if (first_live_main_frame_in_original_opener_chain_)
    first_live_main_frame_in_original_opener_chain_->RemoveObserver(
        original_opener_observer_.get());

  g_frame_tree_node_id_map.Get().erase(frame_tree_node_id_);

  // If a frame with a pending navigation is detached, make sure the
  // WebContents (and its observers) update their loading state.
  // TODO(dcheng): This should just check `IsLoading()`, but `IsLoading()`
  // assumes that `current_frame_host_` is not null. This is incompatible with
  // prerender activation when destroying the old frame tree (see above).
  bool did_stop_loading = false;

  if (navigation_request_) {
    navigation_request_->set_navigation_discard_reason(
        NavigationDiscardReason::kWillRemoveFrame);
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

  // Matches the TRACE_EVENT_BEGIN in the constructor.
  TRACE_EVENT_END("navigation", perfetto::Track::FromPointer(this));
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

Navigator& FrameTreeNode::navigator() {
  return frame_tree().navigator();
}

bool FrameTreeNode::IsOutermostMainFrame() const {
  return !GetParentOrOuterDocument();
}

RenderFrameHostImpl* FrameTreeNode::GetParentOrOuterDocument() const {
  return GetParentOrOuterDocumentHelper(/*escape_guest_view=*/false,
                                        /*include_prospective=*/true);
}

RenderFrameHostImpl* FrameTreeNode::GetParentOrOuterDocumentOrEmbedder() {
  return GetParentOrOuterDocumentHelper(/*escape_guest_view=*/true,
                                        /*include_prospective=*/true);
}

RenderFrameHostImpl* FrameTreeNode::GetParentOrOuterDocumentHelper(
    bool escape_guest_view,
    bool include_prospective) const {
  // Find the parent in the FrameTree (iframe).
  if (parent_) {
    return parent_;
  }

  if (!escape_guest_view) {
    // If we are not a fenced frame root then return early.
    // This code does not escape GuestViews.
    if (!IsFencedFrameRoot()) {
      return nullptr;
    }
  }

  // Find the parent in the outer embedder (GuestView or Fenced Frame).
  FrameTreeNode* frame_in_embedder = render_manager()->GetOuterDelegateNode();
  if (frame_in_embedder) {
    return frame_in_embedder->current_frame_host()->GetParent();
  }

  // Consider embedders which own our frame tree, but have not yet attached it
  // to the outer frame tree.
  if (include_prospective) {
    RenderFrameHostImpl* prospective_outer_document =
        frame_tree_->delegate()->GetProspectiveOuterDocument();
    if (prospective_outer_document) {
      return prospective_outer_document;
    }
  }

  // No parent found.
  return nullptr;
}

FrameType FrameTreeNode::GetFrameType() const {
  if (!IsMainFrame())
    return FrameType::kSubframe;

  switch (frame_tree().type()) {
    case FrameTree::Type::kPrimary:
      return FrameType::kPrimaryMainFrame;
    case FrameTree::Type::kPrerender:
      return FrameType::kPrerenderMainFrame;
    case FrameTree::Type::kFencedFrame:
      return FrameType::kFencedFrameRoot;
  }
}

void FrameTreeNode::SetOpener(FrameTreeNode* opener) {
  TRACE_EVENT("navigation", "FrameTreeNode::SetOpener",
              ChromeTrackEvent::kFrameTreeNodeInfo, opener);
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

  if (first_live_main_frame_in_original_opener_chain_) {
    first_live_main_frame_in_original_opener_chain_->RemoveObserver(
        original_opener_observer_.get());
    original_opener_observer_.reset();
  }

  first_live_main_frame_in_original_opener_chain_ = opener;

  if (first_live_main_frame_in_original_opener_chain_) {
    original_opener_observer_ = std::make_unique<OpenerDestroyedObserver>(
        this, true /* observing_original_opener */);
    first_live_main_frame_in_original_opener_chain_->AddObserver(
        original_opener_observer_.get());
  }
}

void FrameTreeNode::SetCollapsed(bool collapsed) {
  DCHECK(!IsMainFrame() || IsFencedFrameRoot());
  if (is_collapsed_ == collapsed)
    return;

  is_collapsed_ = collapsed;
  render_manager_.OnDidChangeCollapsedState(collapsed);
}

void FrameTreeNode::SetFrameTree(FrameTree& frame_tree) {
  frame_tree_ = frame_tree;
  DCHECK(current_frame_host());
  current_frame_host()->SetFrameTree(frame_tree);
  RenderFrameHostImpl* speculative_frame_host =
      render_manager_.speculative_frame_host();
  if (speculative_frame_host)
    speculative_frame_host->SetFrameTree(frame_tree);
}

void FrameTreeNode::SetPendingFramePolicy(blink::FramePolicy frame_policy) {
  // Inside of a fenced frame, the sandbox flags should not be able to change
  // from its initial value. If the flags change, we have to assume the change
  // came from a compromised renderer and terminate it.
  // We will only do the check if the sandbox flags are already set to
  // kFencedFrameForcedSandboxFlags. This is to allow the sandbox flags to
  // be set initially (go from kNone -> kFencedFrameForcedSandboxFlags). Once
  // it has been set, it cannot change to another value.
  // If the flags do change via a compromised fenced frame, then
  // `RenderFrameHostImpl::DidChangeFramePolicy()` will detect that the change
  // wasn't initiated by the parent, and will terminate the renderer before we
  // reach this point, so we can CHECK() here.
  bool fenced_frame_sandbox_flags_changed =
      (IsFencedFrameRoot() &&
       pending_frame_policy_.sandbox_flags ==
           blink::kFencedFrameForcedSandboxFlags &&
       frame_policy.sandbox_flags != blink::kFencedFrameForcedSandboxFlags);
  CHECK(!fenced_frame_sandbox_flags_changed);

  pending_frame_policy_.sandbox_flags = frame_policy.sandbox_flags;

  if (parent()) {
    // Subframes should always inherit their parent's sandbox flags.
    pending_frame_policy_.sandbox_flags |=
        parent()->browsing_context_state()->active_sandbox_flags();
    // This is only applied on subframes; container policy and required document
    // policy are not mutable on main frame.
    pending_frame_policy_.container_policy = frame_policy.container_policy;
    pending_frame_policy_.required_document_policy =
        frame_policy.required_document_policy;
  }

  // Fenced frame roots do not have a parent, so add an extra check here to
  // still allow a fenced frame to properly set its container policy. The
  // required document policy and sandbox flags should stay unmodified.
  if (IsFencedFrameRoot()) {
    DCHECK(pending_frame_policy_.required_document_policy.empty());
    DCHECK_EQ(pending_frame_policy_.sandbox_flags, frame_policy.sandbox_flags);
    pending_frame_policy_.container_policy = frame_policy.container_policy;
  }
}

void FrameTreeNode::SetAttributes(
    blink::mojom::IframeAttributesPtr attributes) {
  if (!Credentialless() && attributes->credentialless) {
    // Log this only when credentialless is changed to true.
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        parent_, blink::mojom::WebFeature::kAnonymousIframe);
  }
  attributes_ = std::move(attributes);
}

bool FrameTreeNode::IsLoading() const {
  return GetLoadingState() != LoadingState::NONE;
}

LoadingState FrameTreeNode::GetLoadingState() const {
  RenderFrameHostImpl* current_frame_host =
      render_manager_.current_frame_host();
  DCHECK(current_frame_host);

  if (navigation_request_) {
    // If navigation_request_ is non-null, the navigation has not been moved to
    // the RenderFrameHostImpl or sent to the renderer to be committed. This
    // loading UI policy is provisional, as the navigation API might "intercept"
    // a same-document commit and change it from LOADING_WITHOUT_UI to
    // LOADING_UI_REQUESTED.
    return navigation_request_->IsSameDocument()
               ? LoadingState::LOADING_WITHOUT_UI
               : LoadingState::LOADING_UI_REQUESTED;
  }

  RenderFrameHostImpl* speculative_frame_host =
      render_manager_.speculative_frame_host();
  // TODO(dcheng): Shouldn't a FrameTreeNode with a speculative RenderFrameHost
  // always be considered loading?
  if (speculative_frame_host && speculative_frame_host->is_loading()) {
    return LoadingState::LOADING_UI_REQUESTED;
  }
  return current_frame_host->loading_state();
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

void FrameTreeNode::TransferNavigationRequestOwnership(
    RenderFrameHostImpl* render_frame_host) {
  devtools_instrumentation::OnResetNavigationRequest(navigation_request_.get());
  render_frame_host->SetNavigationRequest(std::move(navigation_request_));
}

void FrameTreeNode::TakeNavigationRequest(
    std::unique_ptr<NavigationRequest> navigation_request) {
  // This is never called when navigating to a Javascript URL. For the loading
  // state, this matches what Blink is doing: Blink doesn't send throbber
  // notifications for Javascript URLS.
  DCHECK(!navigation_request->common_params().url.SchemeIs(
      url::kJavaScriptScheme));

  LoadingState previous_frame_tree_loading_state =
      frame_tree().LoadingTree()->GetLoadingState();

  // Reset the previous NavigationRequest owned by `this`. However, there's no
  // need to reset the state: there's still an ongoing load, and the
  // RenderFrameHostManager will take care of updates to the speculative
  // RenderFrameHost in DidCreateNavigationRequest below.
  if (previous_frame_tree_loading_state != LoadingState::NONE) {
    if (navigation_request_ && navigation_request_->IsNavigationStarted()) {
      // Mark the old request as aborted.
      navigation_request_->set_net_error(net::ERR_ABORTED);
    }
    ResetNavigationRequestButKeepState(
        navigation_request->GetTypeForNavigationDiscardReason());
  } else if (navigation_request_ &&
             !navigation_request_->GetNavigationDiscardReason().has_value()) {
    navigation_request_->set_navigation_discard_reason(
        navigation_request->GetTypeForNavigationDiscardReason());
  }

  // Cancel any task that will restart BackForwardCache navigation that was
  // initiated previously.
  CancelRestartingBackForwardCacheNavigation();

  // If `navigation_request` is a BFCache navigation, the RFH for BFCache
  // restore should not be evicted before.
  // This CHECK is added with the fix of https://crbug.com/1468984. See
  // `BackForwardCacheBrowserTest.TwoBackNavigationsToTheSameEntry` for how
  // BFCache entry could be evicted before the BFCache `NavigationRequest`
  // is moved to the FrameTreeNode without the fix.
  if (navigation_request->IsServedFromBackForwardCache()) {
    CHECK(!navigation_request->GetRenderFrameHostRestoredFromBackForwardCache()
               ->is_evicted_from_back_forward_cache());
  }

  navigation_request_ = std::move(navigation_request);
  if (was_discarded_) {
    navigation_request_->set_was_discarded();
    was_discarded_ = false;
  }
  render_manager()->DidCreateNavigationRequest(navigation_request_.get());
  DidStartLoading(previous_frame_tree_loading_state);
}

void FrameTreeNode::ResetNavigationRequest(NavigationDiscardReason reason) {
  if (!navigation_request_)
    return;

  ResetNavigationRequestButKeepState(reason);

  // The RenderFrameHostManager should clean up any speculative RenderFrameHost
  // it created for the navigation. Also register that the load stopped.
  DidStopLoading();
  render_manager_.DiscardSpeculativeRFHIfUnused(reason);

  // An ancestor's network revocation status could've changed as a result of
  // the NavigationRequest getting reset. When fenced frames revoke network
  // access by calling `window.fence.disableUntrustedNetwork`, the returned
  // promise cannot be resolved until ongoing navigations in descendant frames
  // complete.
  current_frame_host()
      ->GetOutermostMainFrame()
      ->CalculateUntrustedNetworkStatus();
}

void FrameTreeNode::ResetNavigationRequestButKeepState(
    NavigationDiscardReason reason) {
  if (!navigation_request_)
    return;

  // When resetting the NavigationRequest, any BFCache navigation restarting
  // task should be cancelled. This is to ensure that the FrameTreeNode won't
  // accidentally complete a navigation that should be reset.
  CancelRestartingBackForwardCacheNavigation();
  devtools_instrumentation::OnResetNavigationRequest(navigation_request_.get());
  if (!navigation_request_->GetNavigationDiscardReason().has_value()) {
    navigation_request_->set_navigation_discard_reason(reason);
  }
  navigation_request_.reset();
}

void FrameTreeNode::DidStartLoading(
    LoadingState previous_frame_tree_loading_state) {
  TRACE_EVENT2("navigation", "FrameTreeNode::DidStartLoading",
               "frame_tree_node", frame_tree_node_id(), "loading_state",
               GetLoadingState());
  base::ElapsedTimer timer;

  frame_tree().LoadingTree()->NodeLoadingStateChanged(
      *this, previous_frame_tree_loading_state);

  // Set initial load progress and update overall progress. This will notify
  // the WebContents of the load progress change.
  //
  // Only notify when the load is triggered from primary/prerender main frame as
  // we only update load progress for these nodes which happens when the frame
  // tree matches the loading tree.
  if (&frame_tree() == frame_tree().LoadingTree())
    DidChangeLoadProgress(blink::kInitialLoadProgress);

  // Notify the proxies of the event.
  current_frame_host()->browsing_context_state()->OnDidStartLoading();
  devtools_instrumentation::DidChangeFrameLoadingState(*this);
  base::UmaHistogramTimes(
      base::StrCat({"Navigation.DidStartLoading.",
                    IsOutermostMainFrame() ? "MainFrame" : "Subframe"}),
      timer.Elapsed());
}

void FrameTreeNode::DidStopLoading() {
  TRACE_EVENT1("navigation", "FrameTreeNode::DidStopLoading", "frame_tree_node",
               frame_tree_node_id());
  // Set final load progress and update overall progress. This will notify
  // the WebContents of the load progress change.
  //
  // Only notify when the load is triggered from primary/prerender main frame as
  // we only update load progress for these nodes which happens when the frame
  // tree matches the loading tree.
  if (&frame_tree() == frame_tree().LoadingTree())
    DidChangeLoadProgress(blink::kFinalLoadProgress);

  // Notify the proxies of the event.
  current_frame_host()->browsing_context_state()->OnDidStopLoading();

  FrameTree* loading_tree = frame_tree().LoadingTree();
  // When loading tree is null, ignore invoking NodeLoadingStateChanged as the
  // frame tree is already deleted. This can happen when prerendering gets
  // cancelled and DidStopLoading is called during FrameTree destruction.
  if (loading_tree && !loading_tree->IsLoadingIncludingInnerFrameTrees()) {
    // If `loading_tree->IsLoadingIncludingInnerFrameTrees()` is now false, this
    // was the last FrameTreeNode to be loading, and the FrameTree as a whole
    // has now stopped loading. Notify the FrameTree.
    // It doesn't matter whether we pass LOADING_UI_REQUESTED or
    // LOADING_WITHOUT_UI as the previous_frame_tree_loading_state param,
    // because the previous value is only used to detect when the FrameTree's
    // overall loading state hasn't changed, and we know that the new state will
    // be LoadingState::NONE.
    loading_tree->NodeLoadingStateChanged(*this,
                                          LoadingState::LOADING_UI_REQUESTED);
  }
  devtools_instrumentation::DidChangeFrameLoadingState(*this);
}

void FrameTreeNode::DidChangeLoadProgress(double load_progress) {
  DCHECK_GE(load_progress, blink::kInitialLoadProgress);
  DCHECK_LE(load_progress, blink::kFinalLoadProgress);
  current_frame_host()->DidChangeLoadProgress(load_progress);
}

bool FrameTreeNode::StopLoading() {
  if (navigation_request_ && navigation_request_->IsNavigationStarted())
    navigation_request_->set_net_error(net::ERR_ABORTED);
  ResetNavigationRequest(NavigationDiscardReason::kExplicitCancellation);

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
  // TODO(clamy): Support BeforeUnload in subframes. Fenced Frames don't run
  // BeforeUnload. Maybe need to check whether other MPArch inner pages cases
  // need beforeunload(e.g., GuestView if it gets ported to MPArch).
  if (!IsOutermostMainFrame())
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
  if (navigation_request_ && navigation_request_->IsWaitingForBeforeUnload()) {
    ResetNavigationRequest(NavigationDiscardReason::kExplicitCancellation);
  }
}

bool FrameTreeNode::NotifyUserActivationStickyOnly() {
  return NotifyUserActivation(
      blink::mojom::UserActivationNotificationType::kNone,
      /*sticky_only=*/true);
}

bool FrameTreeNode::NotifyUserActivation(
    blink::mojom::UserActivationNotificationType notification_type,
    bool sticky_only) {
  // User Activation V2 requires activating all ancestor frames in addition to
  // the current frame. See
  // https://html.spec.whatwg.org/multipage/interaction.html#tracking-user-activation.
  for (RenderFrameHostImpl* rfh = current_frame_host(); rfh;
       rfh = rfh->GetParent()) {
    rfh->DidReceiveUserActivation();
    rfh->ActivateUserActivation(notification_type, sticky_only);
  }

  // If we're in a picture-in-picture frame tree, then also activate the opener
  // frame of the picture-in-picture root.
  FrameTree* pip_opener =
      frame_tree().delegate()->GetPictureInPictureOpenerFrameTree();
  if (base::FeatureList::IsEnabled(
          blink::features::kDocumentPictureInPictureUserActivation) &&
      pip_opener) {
    RenderFrameHostImpl* opener_frame_host =
        pip_opener->root()->current_frame_host();

    opener_frame_host->DidReceiveUserActivation();
    opener_frame_host->ActivateUserActivation(notification_type, sticky_only);
  }

  current_frame_host()->browsing_context_state()->set_has_active_user_gesture(
      true);

  // See the "Same-origin Visibility" section in |UserActivationState| class
  // doc.
  if (base::FeatureList::IsEnabled(
          features::kUserActivationSameOriginVisibility)) {
    const url::Origin& current_origin =
        this->current_frame_host()->GetLastCommittedOrigin();
    for (FrameTreeNode* node : frame_tree().Nodes()) {
      if (node->current_frame_host()->GetLastCommittedOrigin().IsSameOriginWith(
              current_origin)) {
        node->current_frame_host()->ActivateUserActivation(notification_type,
                                                           sticky_only);
      }
    }

    if (base::FeatureList::IsEnabled(
            blink::features::kDocumentPictureInPictureUserActivation)) {
      // If we own a picture-in-picture window, then also activate same-origin
      // frames within the picture-in-picture window.
      FrameTree* picture_in_picture_frame_tree =
          frame_tree().delegate()->GetOwnedPictureInPictureFrameTree();
      if (picture_in_picture_frame_tree) {
        for (FrameTreeNode* node : picture_in_picture_frame_tree->Nodes()) {
          if (node->current_frame_host()
                  ->GetLastCommittedOrigin()
                  .IsSameOriginWith(current_origin)) {
            node->current_frame_host()->ActivateUserActivation(
                notification_type, sticky_only);
          }
        }
      }
    }
  }

  navigator().controller().NotifyUserActivation();
  current_frame_host()->MaybeIsolateForUserActivation();

  return true;
}

bool FrameTreeNode::ConsumeTransientUserActivation() {
  bool was_active = current_frame_host()->IsActiveUserActivation();
  for (FrameTreeNode* node : frame_tree().Nodes()) {
    node->current_frame_host()->ConsumeTransientUserActivation();
  }

  if (base::FeatureList::IsEnabled(
          blink::features::kDocumentPictureInPictureUserActivation)) {
    // If we're consuming user activation in a picture-in-picture window, ensure
    // that its opener's frames also consume activation.
    FrameTree* pip_opener =
        frame_tree().delegate()->GetPictureInPictureOpenerFrameTree();
    if (pip_opener) {
      for (FrameTreeNode* node : pip_opener->Nodes()) {
        node->current_frame_host()->ConsumeTransientUserActivation();
      }
    }

    // If we own a picture-in-picture window, ensure that its frames also
    // consume activation.
    FrameTree* picture_in_picture_frame_tree =
        frame_tree().delegate()->GetOwnedPictureInPictureFrameTree();
    if (picture_in_picture_frame_tree) {
      for (FrameTreeNode* node : picture_in_picture_frame_tree->Nodes()) {
        node->current_frame_host()->ConsumeTransientUserActivation();
      }
    }
  }

  current_frame_host()->browsing_context_state()->set_has_active_user_gesture(
      false);
  return was_active;
}

bool FrameTreeNode::ClearUserActivation() {
  // Note that we don't need to clear user activation for the picture-in-picture
  // subtree here since this is only called for a navigation, which closes the
  // picture-in-picture window.
  for (FrameTreeNode* node : frame_tree().SubtreeNodes(this))
    node->current_frame_host()->ClearUserActivation();
  current_frame_host()->browsing_context_state()->set_has_active_user_gesture(
      false);
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
      if (VerifyUserActivation()) {
        update_result = NotifyUserActivation(
            blink::mojom::UserActivationNotificationType::kInteraction);
        update_type = blink::mojom::UserActivationUpdateType::kNotifyActivation;
      } else {
        // TODO(crbug.com/40091540): We need to decide what to do when
        // user activation verification failed. NOTREACHED here will make all
        // unrelated tests that inject event to renderer fail.
        return false;
      }
    } break;
    case blink::mojom::UserActivationUpdateType::kNotifyActivationStickyOnly:
      update_result = NotifyUserActivationStickyOnly();
      break;
    case blink::mojom::UserActivationUpdateType::kClearActivation:
      update_result = ClearUserActivation();
      break;
  }
  render_manager_.UpdateUserActivationState(update_type, notification_type);
  return update_result;
}

void FrameTreeNode::DidConsumeHistoryUserActivation() {
  for (FrameTreeNode* node : frame_tree().Nodes()) {
    node->current_frame_host()->ConsumeHistoryUserActivation();
  }
}

void FrameTreeNode::DidOpenDocumentInputStream() {
  // document.open causes the document to lose its "initial empty document"
  // status.
  set_not_on_initial_empty_document();
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

void FrameTreeNode::SetInitialPopupURL(const GURL& initial_popup_url) {
  DCHECK(initial_popup_url_.is_empty());
  DCHECK(is_on_initial_empty_document());
  initial_popup_url_ = initial_popup_url;
}

void FrameTreeNode::SetPopupCreatorOrigin(
    const url::Origin& popup_creator_origin) {
  DCHECK(is_on_initial_empty_document());
  popup_creator_origin_ = popup_creator_origin;
}

void FrameTreeNode::WriteIntoTrace(
    perfetto::TracedProto<TraceProto> proto) const {
  proto->set_frame_tree_node_id(frame_tree_node_id().value());
  proto->set_is_main_frame(IsMainFrame());
  proto.Set(TraceProto::kCurrentFrameHost, current_frame_host());
  proto.Set(TraceProto::kSpeculativeFrameHost,
            render_manager()->speculative_frame_host());
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

bool FrameTreeNode::HasPendingCommitNavigation() {
  // Same-RenderFrameHost navigation is committing:
  if (current_frame_host()->HasPendingCommitNavigation()) {
    return true;
  }

  // Cross-RenderFrameHost navigation is committing:
  RenderFrameHostImpl* speculative_frame_host =
      render_manager()->speculative_frame_host();
  if (speculative_frame_host &&
      speculative_frame_host->HasPendingCommitNavigation()) {
    return true;
  }

  return false;
}

bool FrameTreeNode::IsFencedFrameRoot() const {
  return fenced_frame_status_ == FencedFrameStatus::kFencedFrameRoot;
}

bool FrameTreeNode::IsInFencedFrameTree() const {
  return fenced_frame_status_ != FencedFrameStatus::kNotNestedInFencedFrame;
}

FrameTreeNode* FrameTreeNode::GetClosestAncestorWithFencedFrameProperties() {
  FrameTreeNode* node = this;
  while (node) {
    if (node->fenced_frame_properties_.has_value()) {
      return node;
    }
    node = node->parent() ? node->parent()->frame_tree_node() : nullptr;
  }

  return nullptr;
}

std::optional<FencedFrameProperties>& FrameTreeNode::GetFencedFrameProperties(
    FencedFramePropertiesNodeSource node_source) {
  if (node_source == FencedFramePropertiesNodeSource::kFrameTreeRoot) {
    return frame_tree().root()->fenced_frame_properties_;
  }

  // The only other option is `kClosestAncestor`. In this case the fenced frame
  // properties are obtained by a bottom-up traversal.
  CHECK_EQ(node_source, FencedFramePropertiesNodeSource::kClosestAncestor);

  FrameTreeNode* node = GetClosestAncestorWithFencedFrameProperties();

  return node ? node->fenced_frame_properties_ : fenced_frame_properties_;
}

size_t FrameTreeNode::GetFencedFrameDepth(
    size_t& shared_storage_fenced_frame_root_count) {
  DCHECK_EQ(shared_storage_fenced_frame_root_count, 0u);

  size_t depth = 0;
  FrameTreeNode* node = this;

  while (node->fenced_frame_status() !=
         FencedFrameStatus::kNotNestedInFencedFrame) {
    if (node->fenced_frame_status() == FencedFrameStatus::kFencedFrameRoot) {
      depth += 1;

      // This implies the fenced frame is from shared storage.
      if (node->fenced_frame_properties_ &&
          node->fenced_frame_properties_->shared_storage_budget_metadata()) {
        shared_storage_fenced_frame_root_count += 1;
      }
    } else {
      DCHECK_EQ(node->fenced_frame_status(),
                FencedFrameStatus::kIframeNestedWithinFencedFrame);
    }

    DCHECK(node->GetParentOrOuterDocument());
    node = node->GetParentOrOuterDocument()->frame_tree_node();
  }

  return depth;
}

std::optional<base::UnguessableToken> FrameTreeNode::GetFencedFrameNonce() {
  // For partition nonce, all nested frame inside a fenced frame tree should
  // operate on the partition nonce of the frame tree root.
  auto& root_fenced_frame_properties = GetFencedFrameProperties(
      /*node_source=*/FencedFramePropertiesNodeSource::kFrameTreeRoot);
  if (!root_fenced_frame_properties.has_value()) {
    return std::nullopt;
  }
  if (root_fenced_frame_properties->partition_nonce().has_value()) {
    return root_fenced_frame_properties->partition_nonce()
        ->GetValueIgnoringVisibility();
  }
  // It is only possible for there to be `FencedFrameProperties` but no
  // partition nonce in urn iframes (when not nested inside a fenced frame).
  CHECK(blink::features::IsAllowURNsInIframeEnabled());
  CHECK(!IsInFencedFrameTree());
  return std::nullopt;
}

void FrameTreeNode::SetFencedFramePropertiesIfNeeded() {
  if (!IsFencedFrameRoot()) {
    return;
  }

  // The fenced frame properties are set only on the fenced frame root.
  // In the future, they will be set on the FrameTree instead.
  fenced_frame_properties_ = FencedFrameProperties();
}

blink::FencedFrame::DeprecatedFencedFrameMode
FrameTreeNode::GetDeprecatedFencedFrameMode() {
  if (!IsInFencedFrameTree()) {
    return blink::FencedFrame::DeprecatedFencedFrameMode::kDefault;
  }

  // See test "NestedUrnIframeUnderFencedFrameUnfencedTopNavigation" in
  // "FencedFrameParameterizedBrowserTest" for why tree traversal is
  // needed here to obtain the correct fenced frame properties.
  // TODO(crbug.com/40279729): Now the fenced frame properties here are obtained
  // via tree traversal, we should make sure it does not break things at
  // renderers, for example, `_unfencedTop` navigation. Note these issues are
  // pre-existing.
  // TODO(crbug.com/40060657): Once navigation support for urn::uuid in iframes
  // is deprecated, the issue above will no longer be relevant.
  auto& root_fenced_frame_properties = GetFencedFrameProperties();
  if (!root_fenced_frame_properties.has_value()) {
    return blink::FencedFrame::DeprecatedFencedFrameMode::kDefault;
  }

  return root_fenced_frame_properties->mode();
}

bool FrameTreeNode::IsErrorPageIsolationEnabled() const {
  // Error page isolation is enabled for main frames only (crbug.com/1092524).
  return SiteIsolationPolicy::IsErrorPageIsolationEnabled(IsMainFrame());
}

void FrameTreeNode::SetSrcdocValue(const std::string& srcdoc_value) {
  srcdoc_value_ = srcdoc_value;
}

std::vector<const SharedStorageBudgetMetadata*>
FrameTreeNode::FindSharedStorageBudgetMetadata() {
  std::vector<const SharedStorageBudgetMetadata*> result;
  FrameTreeNode* node = this;

  while (true) {
    if (node->fenced_frame_properties_ &&
        node->fenced_frame_properties_->shared_storage_budget_metadata()) {
      result.emplace_back(
          node->fenced_frame_properties_->shared_storage_budget_metadata()
              ->GetValueIgnoringVisibility());
    }

    if (!node->GetParentOrOuterDocument()) {
      break;
    }

    node = node->GetParentOrOuterDocument()->frame_tree_node();
  }

  return result;
}

std::optional<std::u16string>
FrameTreeNode::GetEmbedderSharedStorageContextIfAllowed() {
  std::optional<FencedFrameProperties>& properties = GetFencedFrameProperties();
  // We only return embedder context for frames that are same origin with the
  // fenced frame root or ancestor URN iframe.
  if (!properties || !properties->mapped_url().has_value() ||
      !current_origin().IsSameOriginWith(url::Origin::Create(
          properties->mapped_url()->GetValueIgnoringVisibility()))) {
    return std::nullopt;
  }
  return properties->embedder_shared_storage_context();
}

const scoped_refptr<BrowsingContextState>&
FrameTreeNode::GetBrowsingContextStateForSubframe() const {
  DCHECK(!IsMainFrame());
  return current_frame_host()->browsing_context_state();
}

void FrameTreeNode::ClearOpenerReferences() {
  // Simulate the FrameTreeNode being dead to opener observers. They will
  // nullify their opener.
  // Note: observers remove themselves from observers_, no need to take care of
  // that manually.
  for (auto& observer : observers_)
    observer.OnFrameTreeNodeDisownedOpenee(this);
}

bool FrameTreeNode::AncestorOrSelfHasCSPEE() const {
  // Check if CSPEE is set in this frame or any ancestor frames.
  return csp_attribute() || (parent() && parent()->required_csp());
}

void FrameTreeNode::ResetAllNavigationsForFrameDetach() {
  NavigationDiscardReason reason = NavigationDiscardReason::kWillRemoveFrame;
  for (FrameTreeNode* frame : frame_tree().SubtreeNodes(this)) {
    frame->ResetNavigationRequest(reason);
    frame->current_frame_host()->ResetOwnedNavigationRequests(reason);
    frame->GetRenderFrameHostManager().DiscardSpeculativeRFH(reason);
  }
}

void FrameTreeNode::RestartNavigationAsCrossDocument(
    std::unique_ptr<NavigationRequest> navigation_request) {
  navigator().RestartNavigationAsCrossDocument(std::move(navigation_request));
}

bool FrameTreeNode::Reload() {
  return navigator().controller().ReloadFrame(this);
}

Navigator& FrameTreeNode::GetCurrentNavigator() {
  return navigator();
}

RenderFrameHostManager& FrameTreeNode::GetRenderFrameHostManager() {
  return render_manager_;
}

FrameTreeNode* FrameTreeNode::GetOpener() const {
  return opener_;
}

void FrameTreeNode::SetFocusedFrame(SiteInstanceGroup* source) {
  frame_tree_->delegate()->SetFocusedFrame(this, source);
}

void FrameTreeNode::DidChangeReferrerPolicy(
    network::mojom::ReferrerPolicy referrer_policy) {
  navigator().controller().DidChangeReferrerPolicy(this, referrer_policy);
}

std::unique_ptr<NavigationRequest>
FrameTreeNode::CreateNavigationRequestForSynchronousRendererCommit(
    RenderFrameHostImpl* render_frame_host,
    bool is_same_document,
    const GURL& url,
    const url::Origin& origin,
    const std::optional<GURL>& initiator_base_url,
    const net::IsolationInfo& isolation_info_for_subresources,
    blink::mojom::ReferrerPtr referrer,
    const ui::PageTransition& transition,
    bool should_replace_current_entry,
    const std::string& method,
    bool has_transient_activation,
    bool is_overriding_user_agent,
    const std::vector<GURL>& redirects,
    const GURL& original_url,
    std::unique_ptr<CrossOriginEmbedderPolicyReporter> coep_reporter,
    int http_response_code) {
  return NavigationRequest::CreateForSynchronousRendererCommit(
      this, render_frame_host, is_same_document, url, origin,
      initiator_base_url, isolation_info_for_subresources, std::move(referrer),
      transition, should_replace_current_entry, method,
      has_transient_activation, is_overriding_user_agent, redirects,
      original_url, std::move(coep_reporter), http_response_code);
}

void FrameTreeNode::CancelNavigation(NavigationDiscardReason reason) {
  if (navigation_request() && navigation_request()->IsNavigationStarted()) {
    navigation_request()->set_net_error(net::ERR_ABORTED);
  }
  ResetNavigationRequest(reason);
}

void FrameTreeNode::ResetNavigationsForDiscard() {
  for (FrameTreeNode* frame : frame_tree().SubtreeNodes(this)) {
    // TODO(crbug.com/365481515): Consider adding a separate discard reason for
    // frame tree discarding.
    frame->ResetNavigationRequest(NavigationDiscardReason::kWillRemoveFrame);
    frame->current_frame_host()->ResetOwnedNavigationRequests(
        NavigationDiscardReason::kWillRemoveFrame);
  }
}

bool FrameTreeNode::Credentialless() const {
  return attributes_->credentialless;
}

#if !BUILDFLAG(IS_ANDROID)
void FrameTreeNode::GetVirtualAuthenticatorManager(
    mojo::PendingReceiver<blink::test::mojom::VirtualAuthenticatorManager>
        receiver) {
  auto* environment_singleton = AuthenticatorEnvironment::GetInstance();
  environment_singleton->EnableVirtualAuthenticatorFor(this,
                                                       /*enable_ui=*/false);
  environment_singleton->AddVirtualAuthenticatorReceiver(this,
                                                         std::move(receiver));
}
#endif  // !BUILDFLAG(IS_ANDROID)

FrameType FrameTreeNode::GetCurrentFrameType() const {
  return GetFrameType();
}

void FrameTreeNode::RestartBackForwardCachedNavigationAsync(int nav_entry_id) {
  TRACE_EVENT0("navigation",
               "FrameTreeNode::RestartBackForwardCachedNavigationAsync");
  // The `navigation_request_` must be the BFCache navigation to the same entry
  // as the restarted navigation.
  CHECK(navigation_request_->IsServedFromBackForwardCache());
  CHECK_EQ(navigation_request_->nav_entry_id(), nav_entry_id);
  // Reset the `NavigationRequest` since the BFCache navigation will be
  // restarted.
  ResetNavigationRequest(NavigationDiscardReason::kInternalCancellation);

  // Post a task to restart the navigation asynchronously.
  restart_back_forward_cached_navigation_tracker_.PostTask(
      GetUIThreadTaskRunner({}).get(), FROM_HERE,
      base::BindOnce(&FrameTreeNode::RestartBackForwardCachedNavigationImpl,
                     weak_factory_.GetWeakPtr(), nav_entry_id));
}

void FrameTreeNode::RestartBackForwardCachedNavigationImpl(int nav_entry_id) {
  TRACE_EVENT0("navigation",
               "FrameTreeNode::RestartBackForwardCachedNavigationImpl");
  NavigationControllerImpl& controller = frame_tree_->controller();
  int nav_index = controller.GetEntryIndexWithUniqueID(nav_entry_id);
  // If the NavigationEntry was deleted, do not do anything.
  if (nav_index != -1) {
    controller.GoToIndex(nav_index);
  }
}

void FrameTreeNode::CancelRestartingBackForwardCacheNavigation() {
  TRACE_EVENT0("navigation",
               "FrameTreeNode::CancelRestartingBackForwardCacheNavigation");
  restart_back_forward_cached_navigation_tracker_.TryCancelAll();
}

}  // namespace content
