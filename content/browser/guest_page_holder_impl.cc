// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/guest_page_holder_impl.h"

#include "base/notimplemented.h"
#include "content/browser/renderer_host/cross_process_frame_connector.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/site_instance.h"

namespace content {

std::unique_ptr<GuestPageHolder> GuestPageHolder::Create(
    WebContents* owner_web_contents,
    scoped_refptr<SiteInstance> site_instance,
    base::WeakPtr<GuestPageHolder::Delegate> delegate) {
  CHECK(owner_web_contents);
  // Note that `site_instance->IsGuest()` would only be true for <webview>, not
  // other guest types.
  CHECK(site_instance);
  CHECK(delegate);

  std::unique_ptr<GuestPageHolderImpl> guest_page =
      std::make_unique<GuestPageHolderImpl>(
          static_cast<WebContentsImpl&>(*owner_web_contents), std::string(),
          /*opener=*/nullptr, std::move(site_instance), delegate);
  return guest_page;
}

std::unique_ptr<GuestPageHolder> GuestPageHolder::CreateWithOpener(
    WebContents* owner_web_contents,
    const std::string& frame_name,
    RenderFrameHost* opener,
    scoped_refptr<SiteInstance> site_instance,
    base::WeakPtr<GuestPageHolder::Delegate> delegate) {
  CHECK(owner_web_contents);
  // Note that `site_instance->IsGuest()` would only be true for <webview>, not
  // other guest types.
  CHECK(site_instance);
  CHECK(delegate);

  std::unique_ptr<GuestPageHolderImpl> guest_page =
      std::make_unique<GuestPageHolderImpl>(
          static_cast<WebContentsImpl&>(*owner_web_contents), frame_name,
          static_cast<RenderFrameHostImpl*>(opener), std::move(site_instance),
          delegate);
  return guest_page;
}

GuestPageHolderImpl::GuestPageHolderImpl(
    WebContentsImpl& owner_web_contents,
    const std::string& frame_name,
    RenderFrameHostImpl* opener,
    scoped_refptr<SiteInstance> site_instance,
    base::WeakPtr<GuestPageHolder::Delegate> delegate)
    : owner_web_contents_(owner_web_contents),
      delegate_(delegate),
      frame_tree_(owner_web_contents.GetBrowserContext(),
                  /*delegate=*/this,
                  /*navigation_controller_delegate=*/this,
                  /*navigator_delegate=*/&owner_web_contents,
                  /*render_frame_delegate=*/&owner_web_contents,
                  /*render_view_delegate=*/&owner_web_contents,
                  /*render_widget_delegate=*/&owner_web_contents,
                  /*manager_delegate=*/&owner_web_contents,
                  /*page_delegate=*/&owner_web_contents,
                  FrameTree::Type::kGuest) {
  frame_tree_.Init(static_cast<SiteInstanceImpl*>(site_instance.get()),
                   /*renderer_initiated_creation=*/false,
                   /*main_frame_name=*/frame_name,
                   /*opener_for_origin=*/opener, blink::FramePolicy{},
                   /*devtools_frame_token=*/base::UnguessableToken::Create());
  // Notify WebContentsObservers of the new guest frame via
  // RenderFrameHostChanged.
  // TODO(crbug.com/40177940): This should be moved to FrameTree::Init.
  owner_web_contents.NotifySwappedFromRenderManager(
      /*old_frame=*/nullptr,
      frame_tree_.root()->render_manager()->current_frame_host());

  // Ensure our muted state is correct on creation.
  if (owner_web_contents.IsAudioMuted()) {
    GetAudioStreamFactory()->SetMuted(true);
  }

  if (opener) {
    FrameTreeNode* new_root = frame_tree_.root();

    // For the "original opener", track the opener's main frame instead, because
    // if the opener is a subframe, the opener tracking could be easily bypassed
    // by spawning from a subframe and deleting the subframe.
    // https://crbug.com/705316
    new_root->SetOriginalOpener(opener->frame_tree()->root());
    new_root->SetOpenerDevtoolsFrameToken(opener->devtools_frame_token());
    new_root->SetOpener(opener->frame_tree_node());
  }
}

GuestPageHolderImpl::~GuestPageHolderImpl() {
  frame_tree_.Shutdown();
}

void GuestPageHolderImpl::set_outer_frame_tree_node_id(
    FrameTreeNodeId outer_frame_tree_node_id) {
  CHECK(!outer_frame_tree_node_id_);
  CHECK(outer_frame_tree_node_id);
  outer_frame_tree_node_id_ = outer_frame_tree_node_id;
}

NavigationController& GuestPageHolderImpl::GetController() {
  return frame_tree_.controller();
}

RenderFrameHostImpl* GuestPageHolderImpl::GetGuestMainFrame() {
  return frame_tree_.root()->current_frame_host();
}

bool GuestPageHolderImpl::IsAudioMuted() {
  return audio_stream_factory_ && audio_stream_factory_->IsMuted();
}

void GuestPageHolderImpl::SetAudioMuted(bool mute) {
  audio_muted_ = mute;
  // The AudioStreamFactory's mute state is an OR of our state and our
  // owning WebContents state.
  GetAudioStreamFactory()->SetMuted(mute ||
                                    owner_web_contents_->IsAudioMuted());
}

RenderFrameHost* GuestPageHolderImpl::GetOpener() {
  if (auto* opener = frame_tree_.root()->GetOpener()) {
    return opener->current_frame_host();
  }
  return nullptr;
}

void GuestPageHolderImpl::SetAudioMutedFromWebContents(
    bool web_contents_muted) {
  GetAudioStreamFactory()->SetMuted(audio_muted_ || web_contents_muted);
}

void GuestPageHolderImpl::LoadingStateChanged(LoadingState new_state) {
  NOTIMPLEMENTED();
}

void GuestPageHolderImpl::DidStartLoading(FrameTreeNode* frame_tree_node) {
  NOTIMPLEMENTED();
}

void GuestPageHolderImpl::DidStopLoading() {
  if (delegate_) {
    delegate_->GuestDidStopLoading();
  }
  load_stop_callbacks_for_testing_.Notify();
}

bool GuestPageHolderImpl::IsHidden() {
  return owner_web_contents_->IsHidden();
}

FrameTreeNodeId GuestPageHolderImpl::GetOuterDelegateFrameTreeNodeId() {
  return outer_frame_tree_node_id_;
}

RenderFrameHostImpl* GuestPageHolderImpl::GetProspectiveOuterDocument() {
  if (!delegate_) {
    return nullptr;
  }
  return static_cast<RenderFrameHostImpl*>(
      delegate_->GetProspectiveOuterDocument());
}

FrameTree* GuestPageHolderImpl::LoadingTree() {
  return &frame_tree_;
}

void GuestPageHolderImpl::SetFocusedFrame(FrameTreeNode* node,
                                          SiteInstanceGroup* source) {
  owner_web_contents_->SetFocusedFrame(node, source);
}

FrameTree* GuestPageHolderImpl::GetOwnedPictureInPictureFrameTree() {
  return nullptr;
}

FrameTree* GuestPageHolderImpl::GetPictureInPictureOpenerFrameTree() {
  return nullptr;
}

void GuestPageHolderImpl::NotifyNavigationStateChangedFromController(
    InvalidateTypes changed_flags) {}

void GuestPageHolderImpl::NotifyBeforeFormRepostWarningShow() {}

void GuestPageHolderImpl::NotifyNavigationEntryCommitted(
    const LoadCommittedDetails& load_details) {}

void GuestPageHolderImpl::NotifyNavigationEntryChanged(
    const EntryChangedDetails& change_details) {}

void GuestPageHolderImpl::NotifyNavigationListPruned(
    const PrunedDetails& pruned_details) {}

void GuestPageHolderImpl::NotifyNavigationEntriesDeleted() {}

void GuestPageHolderImpl::ActivateAndShowRepostFormWarningDialog() {
  NOTIMPLEMENTED();
}

bool GuestPageHolderImpl::ShouldPreserveAbortedURLs() {
  return false;
}

void GuestPageHolderImpl::UpdateOverridingUserAgent() {
  owner_web_contents_->UpdateOverridingUserAgent();
}

ForwardingAudioStreamFactory* GuestPageHolderImpl::GetAudioStreamFactory() {
  if (!audio_stream_factory_) {
    audio_stream_factory_ = owner_web_contents_->CreateAudioStreamFactory(
        base::PassKey<GuestPageHolderImpl>());
  }
  return audio_stream_factory_.get();
}

const blink::RendererPreferences& GuestPageHolderImpl::GetRendererPrefs() {
  // Copy the renderer preferences of the primary main frame, then apply guest
  // specific changes.
  renderer_preferences_ = owner_web_contents_->GetRendererPrefs(
      owner_web_contents_->GetRenderViewHost());

  // Navigation is disabled in Chrome Apps. We want to make sure guest-initiated
  // navigations still continue to function inside the app.
  renderer_preferences_.browser_handles_all_top_level_requests = false;
  // Also disable drag/drop navigations.
  renderer_preferences_.can_accept_load_drops = false;

  if (delegate_) {
    delegate_->GuestOverrideRendererPreferences(renderer_preferences_);
  }

  return renderer_preferences_;
}

const blink::web_pref::WebPreferences&
GuestPageHolderImpl::GetWebPreferences() {
  if (!web_preferences_) {
    web_preferences_ = std::make_unique<blink::web_pref::WebPreferences>(
        owner_web_contents_->ComputeWebPreferences(GetGuestMainFrame()));
  }
  return *web_preferences_;
}

GuestPageHolderImpl* GuestPageHolderImpl::FromRenderFrameHost(
    RenderFrameHostImpl& render_frame_host) {
  // Escape fenced frames, looking at the outermost main frame (not escaping
  // guests).
  FrameTree* frame_tree =
      render_frame_host.GetOutermostMainFrame()->frame_tree();
  if (!frame_tree->is_guest()) {
    return nullptr;
  }

  // Guest FrameTrees are only created with a GuestPageHolderImpl as the
  // FrameTree delegate.
  GuestPageHolderImpl* holder =
      static_cast<GuestPageHolderImpl*>(frame_tree->delegate());
  CHECK(holder);

  // If the guest is attached, we can lookup the GuestPageHolderImpl via the
  // embedder to validate the correctness of the above static_cast.
  if (FrameTreeNode* frame_in_embedder =
          frame_tree->root()->render_manager()->GetOuterDelegateNode()) {
    GuestPageHolderImpl* holder_via_embedder =
        frame_in_embedder->parent()->FindGuestPageHolder(frame_in_embedder);
    CHECK_EQ(holder, holder_via_embedder);
  }

  return holder;
}

base::CallbackListSubscription
GuestPageHolderImpl::RegisterLoadStopCallbackForTesting(
    base::RepeatingClosure callback) {
  return load_stop_callbacks_for_testing_.Add(callback);
}

FrameTree* GuestPageHolderImpl::CreateNewWindow(
    WindowOpenDisposition disposition,
    const GURL& url,
    const std::string& main_frame_name,
    scoped_refptr<SiteInstance> site_instance,
    RenderFrameHostImpl* opener) {
  auto* guest_page =
      static_cast<GuestPageHolderImpl*>(delegate_->GuestCreateNewWindow(
          disposition, url, main_frame_name, opener, std::move(site_instance)));
  if (!guest_page) {
    return nullptr;
  }
  return &guest_page->frame_tree();
}

bool GuestPageHolderImpl::OnRenderFrameProxyVisibilityChanged(
    RenderFrameProxyHost* render_frame_proxy_host,
    blink::mojom::FrameVisibility visibility) {
  CHECK(base::FeatureList::IsEnabled(features::kGuestViewMPArch));

  if (render_frame_proxy_host->frame_tree_node() != frame_tree_.root()) {
    return false;
  }
  const bool hidden_with_parent_state =
      render_frame_proxy_host->cross_process_frame_connector()->IsHidden() ||
      render_frame_proxy_host->cross_process_frame_connector()
              ->EmbedderVisibility() != Visibility::VISIBLE;
  frame_tree_.ForEachRenderViewHost([hidden_with_parent_state](
                                        RenderViewHostImpl* rvh) {
    rvh->SetFrameTreeVisibility(
        hidden_with_parent_state ? blink::mojom::PageVisibilityState::kHidden
                                 : blink::mojom::PageVisibilityState::kVisible);
  });
  return false;
}

}  // namespace content
