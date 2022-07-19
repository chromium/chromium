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
#include "base/observer_list.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/timer/elapsed_timer.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/fenced_frame/fenced_frame.h"
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

FencedFrame* FindFencedFrame(const FrameTreeNode* frame_tree_node) {
  // TODO(crbug.com/1123606): Consider having a pointer to `FencedFrame` in
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

RenderFrameHostImpl::FencedFrameStatus ComputeFencedFrameStatus(
    FrameTree* frame_tree,
    RenderFrameHostImpl* parent,
    const blink::FramePolicy& frame_policy) {
  if (blink::features::IsFencedFramesEnabled()) {
    switch (blink::features::kFencedFramesImplementationTypeParam.Get()) {
      case blink::features::FencedFramesImplementationType::kMPArch: {
        if (frame_tree->type() == FrameTree::Type::kFencedFrame) {
          if (!parent)
            return RenderFrameHostImpl::FencedFrameStatus::kFencedFrameRoot;
          return RenderFrameHostImpl::FencedFrameStatus::
              kIframeNestedWithinFencedFrame;
        } else {
          return RenderFrameHostImpl::FencedFrameStatus::
              kNotNestedInFencedFrame;
        }
      }
      case blink::features::FencedFramesImplementationType::kShadowDOM: {
        // Different from the MPArch case, the ShadowDOM implementation of
        // fenced frame lives in the same FrameTree as its parent, so we need to
        // check its effective frame policy instead.
        if (frame_policy.is_fenced) {
          return RenderFrameHostImpl::FencedFrameStatus::kFencedFrameRoot;
        } else if (parent && parent->frame_tree_node()->IsInFencedFrameTree()) {
          return RenderFrameHostImpl::FencedFrameStatus::
              kIframeNestedWithinFencedFrame;
        }
        return RenderFrameHostImpl::FencedFrameStatus::kNotNestedInFencedFrame;
      }
      default: {
        return RenderFrameHostImpl::FencedFrameStatus::kNotNestedInFencedFrame;
      }
    }
  }

  return RenderFrameHostImpl::FencedFrameStatus::kNotNestedInFencedFrame;
}

FrameTreeNode::FrameTreeNode(
    FrameTree* frame_tree,
    RenderFrameHostImpl* parent,
    blink::mojom::TreeScopeType tree_scope_type,
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
      pending_frame_policy_(frame_policy),
      is_created_by_script_(is_created_by_script),
      devtools_frame_token_(devtools_frame_token),
      frame_owner_properties_(frame_owner_properties),
      fenced_frame_status_(
          ComputeFencedFrameStatus(frame_tree_, parent_, frame_policy)),
      render_manager_(this, frame_tree->manager_delegate()) {
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
  //   - `Portal`
  //   - `FencedFrame`
  //   - `GuestView`
  // If we are representing a `FencedFrame` object, we need to destroy it
  // alongside ourself. `Portals` and `GuestView` however, *currently* have a
  // more complex lifetime and are dealt with separately.
  bool is_outer_dummy_node = false;
  if (current_frame_host() &&
      current_frame_host()->inner_tree_main_frame_tree_node_id() !=
          FrameTreeNode::kFrameTreeNodeInvalidId) {
    is_outer_dummy_node = true;
  }

  if (is_outer_dummy_node) {
    FencedFrame* doomed_fenced_frame = FindFencedFrame(this);
    // `doomed_fenced_frame` might not actually exist, because some outer dummy
    // `FrameTreeNode`s might correspond to `Portal`s, which do not have their
    // lifetime managed in the same way as `FencedFrames`.
    if (doomed_fenced_frame) {
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

  DestroyInnerFrameTreeIfExists();

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

bool FrameTreeNode::IsOutermostMainFrame() {
  return !GetParentOrOuterDocument();
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

FrameType FrameTreeNode::GetFrameType() const {
  if (!IsMainFrame())
    return FrameType::kSubframe;

  switch (frame_tree()->type()) {
    case FrameTree::Type::kPrimary:
      return FrameType::kPrimaryMainFrame;
    case FrameTree::Type::kPrerender:
      return FrameType::kPrerenderMainFrame;
    case FrameTree::Type::kFencedFrame:
      // We also have FencedFramesImplementationType::kShadowDOM for a
      // fenced frame implementation based on <iframe> + shadowDOM,
      // which will return kSubframe as it's a modified <iframe> rather
      // than a dedicated FrameTree. This returns kSubframe for the
      // shadow dom implementation in order to keep consistency (i.e.
      // NavigationHandle::GetParentFrame returning non-null value for
      // shadow-dom based FFs).
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

void FrameTreeNode::SetCurrentURL(const GURL& url) {
  current_frame_host()->SetLastCommittedUrl(url);
}

void FrameTreeNode::SetCollapsed(bool collapsed) {
  DCHECK(!IsMainFrame() || IsFencedFrameRoot());
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

void FrameTreeNode::SetPendingFramePolicy(blink::FramePolicy frame_policy) {
  // The `is_fenced` and `fenced_frame_mode` bits should never be able to
  // transition from their initial values. Since we never expect to be in a
  // position where it can even be updated to new value, if we catch this
  // happening we have to kill the renderer and refuse to accept any other frame
  // policy changes here.
  if (pending_frame_policy_.is_fenced != frame_policy.is_fenced ||
      pending_frame_policy_.fenced_frame_mode !=
          frame_policy.fenced_frame_mode) {
    mojo::ReportBadMessage(
        "FramePolicy properties dealing with fenced frames are considered "
        "immutable, and therefore should never be changed by the renderer.");
    return;
  }

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
}

void FrameTreeNode::SetAnonymous(bool anonymous) {
  if (anonymous) {
    if (!parent_) {
      bad_message::ReceivedBadMessage(current_frame_host()->GetProcess(),
                                      bad_message::FTN_ANONYMOUS);
      return;
    }

    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        parent_, blink::mojom::WebFeature::kAnonymousIframe);
  }

  anonymous_ = anonymous;
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

  bool was_previously_loading =
      frame_tree()->LoadingTree()->IsLoadingIncludingInnerFrameTrees();

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

  frame_tree()->LoadingTree()->DidStartLoadingNode(
      *this, should_show_loading_ui, was_previously_loading);

  // Set initial load progress and update overall progress. This will notify
  // the WebContents of the load progress change.
  //
  // Only notify when the load is triggered from primary/prerender main frame as
  // we only update load progress for these nodes which happens when the frame
  // tree matches the loading tree.
  if (frame_tree() == frame_tree()->LoadingTree())
    DidChangeLoadProgress(blink::kInitialLoadProgress);

  // Notify the proxies of the event.
  current_frame_host()->browsing_context_state()->OnDidStartLoading();
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
  //
  // Only notify when the load is triggered from primary/prerender main frame as
  // we only update load progress for these nodes which happens when the frame
  // tree matches the loading tree.
  if (frame_tree() == frame_tree()->LoadingTree())
    DidChangeLoadProgress(blink::kFinalLoadProgress);

  // Notify the proxies of the event.
  current_frame_host()->browsing_context_state()->OnDidStopLoading();

  FrameTree* loading_tree = frame_tree()->LoadingTree();
  // When loading tree is null, ignore invoking DidStopLoadingNode as the frame
  // tree is already deleted. This can happen when prerendering gets cancelled
  // and DidStopLoading is called during FrameTree destruction.
  if (loading_tree)
    loading_tree->DidStopLoadingNode(*this);
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
  // TODO(clamy): Support BeforeUnload in subframes. Fenced Frames don't run
  // BeforeUnload. Maybe need to check whether other MPArch inner pages cases
  // need beforeunload(e.g., portals, GuestView if it gets ported to MPArch).
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
  if (navigation_request_ && navigation_request_->IsWaitingForBeforeUnload())
    ResetNavigationRequest(false);
}

bool FrameTreeNode::NotifyUserActivation(
    blink::mojom::UserActivationNotificationType notification_type) {
  // User activation notifications shouldn't propagate into/out of fenced
  // frames.
  // For ShadowDOM, fenced frames are in the same frame tree as their embedder,
  // so we need to perform additional checks to enforce the boundary.
  // For MPArch, fenced frames have a separate frame tree, so this boundary is
  // enforced by default.
  // https://docs.google.com/document/d/1WnIhXOFycoje_sEoZR3Mo0YNSR2Ki7LABIC_HEWFaog
  bool shadow_dom_fenced_frame_enabled =
      frame_tree()->IsFencedFramesShadowDOMBased();

  // User Activation V2 requires activating all ancestor frames in addition to
  // the current frame. See
  // https://html.spec.whatwg.org/multipage/interaction.html#tracking-user-activation.
  for (RenderFrameHostImpl* rfh = current_frame_host(); rfh;
       rfh = rfh->GetParent()) {
    rfh->DidReceiveUserActivation();
    rfh->frame_tree_node()->user_activation_state_.Activate(notification_type);

    if (shadow_dom_fenced_frame_enabled &&
        rfh->frame_tree_node()->IsFencedFrameRoot()) {
      break;
    }
  }

  current_frame_host()->browsing_context_state()->set_has_active_user_gesture(
      true);

  absl::optional<base::UnguessableToken> originator_nonce =
      fenced_frame_nonce();

  // See the "Same-origin Visibility" section in |UserActivationState| class
  // doc.
  if (base::FeatureList::IsEnabled(
          features::kUserActivationSameOriginVisibility)) {
    const url::Origin& current_origin =
        this->current_frame_host()->GetLastCommittedOrigin();
    for (FrameTreeNode* node : frame_tree()->Nodes()) {
      if (shadow_dom_fenced_frame_enabled &&
          node->fenced_frame_nonce() != originator_nonce) {
        continue;
      }

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
  // User activation consumptions shouldn't propagate into/out of fenced
  // frames.
  // For ShadowDOM, fenced frames are in the same frame tree as their embedder,
  // so we need to perform additional checks to enforce the boundary.
  // For MPArch, fenced frames have a separate frame tree, so this boundary is
  // enforced by default.
  // https://docs.google.com/document/d/1WnIhXOFycoje_sEoZR3Mo0YNSR2Ki7LABIC_HEWFaog
  bool shadow_dom_fenced_frame_enabled =
      frame_tree()->IsFencedFramesShadowDOMBased();
  absl::optional<base::UnguessableToken> originator_nonce =
      fenced_frame_nonce();

  bool was_active = user_activation_state_.IsActive();
  for (FrameTreeNode* node : frame_tree()->Nodes()) {
    if (shadow_dom_fenced_frame_enabled &&
        node->fenced_frame_nonce() != originator_nonce) {
      continue;
    }

    node->user_activation_state_.ConsumeIfActive();
  }
  current_frame_host()->browsing_context_state()->set_has_active_user_gesture(
      false);
  return was_active;
}

bool FrameTreeNode::ClearUserActivation() {
  for (FrameTreeNode* node : frame_tree()->SubtreeNodes(this))
    node->user_activation_state_.Clear();
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
  DCHECK(is_on_initial_empty_document_);
  initial_popup_url_ = initial_popup_url;
}

void FrameTreeNode::SetPopupCreatorOrigin(
    const url::Origin& popup_creator_origin) {
  DCHECK(is_on_initial_empty_document_);
  popup_creator_origin_ = popup_creator_origin;
}

void FrameTreeNode::WriteIntoTrace(
    perfetto::TracedProto<TraceProto> proto) const {
  proto->set_frame_tree_node_id(frame_tree_node_id());
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

bool FrameTreeNode::IsFencedFrameRoot() const {
  return fenced_frame_status_ ==
         RenderFrameHostImpl::FencedFrameStatus::kFencedFrameRoot;
}

bool FrameTreeNode::IsInFencedFrameTree() const {
  return fenced_frame_status_ !=
         RenderFrameHostImpl::FencedFrameStatus::kNotNestedInFencedFrame;
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

absl::optional<blink::mojom::FencedFrameMode>
FrameTreeNode::GetFencedFrameMode() {
  if (!IsInFencedFrameTree()) {
    return absl::nullopt;
  }

  switch (blink::features::kFencedFramesImplementationTypeParam.Get()) {
    case blink::features::FencedFramesImplementationType::kMPArch: {
      FrameTreeNode* outer_delegate_node =
          render_manager()->GetOuterDelegateNode();
      DCHECK(outer_delegate_node);

      FencedFrame* fenced_frame = FindFencedFrame(outer_delegate_node);
      DCHECK(fenced_frame);

      return fenced_frame->mode();
    }
    case blink::features::FencedFramesImplementationType::kShadowDOM: {
      FrameTreeNode* node = this;
      while (!node->IsFencedFrameRoot()) {
        FrameTreeNode* next_node = parent()->frame_tree_node();
        node = next_node;
      }
      return node->pending_frame_policy_.fenced_frame_mode;
    }
  }
}

bool FrameTreeNode::IsErrorPageIsolationEnabled() const {
  // Error page isolation is enabled for main frames only (crbug.com/1092524).
  // Note that this will also enable error page isolation for fenced frames in
  // MPArch mode, but not ShadowDOM mode.
  // See the issue in crbug.com/1264224#c7 for why it can't be enabled for
  // ShadowDOM mode.
  return SiteIsolationPolicy::IsErrorPageIsolationEnabled(IsMainFrame());
}

void FrameTreeNode::SetSrcdocValue(const std::string& srcdoc_value) {
  srcdoc_value_ = srcdoc_value;
}

FencedFrameURLMapping::SharedStorageBudgetMetadata*
FrameTreeNode::FindSharedStorageBudgetMetadata() {
  FrameTreeNode* node = this;

  while (true) {
    if (node->shared_storage_budget_metadata()) {
      DCHECK(node->IsFencedFrameRoot());
      return node->shared_storage_budget_metadata();
    }

    if (node->GetParentOrOuterDocument()) {
      node = node->GetParentOrOuterDocument()->frame_tree_node();
    } else {
      break;
    }
  }

  return nullptr;
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
  return csp_attribute_ || (parent() && parent()->required_csp());
}

}  // namespace content
