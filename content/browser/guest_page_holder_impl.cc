// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/guest_page_holder_impl.h"

#include "base/notimplemented.h"
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
          static_cast<WebContentsImpl&>(*owner_web_contents),
          std::move(site_instance), delegate);
  return guest_page;
}

GuestPageHolderImpl::GuestPageHolderImpl(
    WebContentsImpl& owner_web_contents,
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
  // TODO(crbug.com/40202416): Implement support for devtools and set the
  // `devtools_frame_token`.
  frame_tree_.Init(static_cast<SiteInstanceImpl*>(site_instance.get()),
                   /*renderer_initiated_creation=*/false,
                   /*main_frame_name=*/"", /*opener_for_origin=*/nullptr,
                   blink::FramePolicy{},
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

RenderFrameHost* GuestPageHolderImpl::GetGuestMainFrame() {
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
}

bool GuestPageHolderImpl::IsHidden() {
  return owner_web_contents_->IsHidden();
}

FrameTreeNodeId GuestPageHolderImpl::GetOuterDelegateFrameTreeNodeId() {
  return outer_frame_tree_node_id_;
}

RenderFrameHostImpl* GuestPageHolderImpl::GetProspectiveOuterDocument() {
  NOTIMPLEMENTED();
  return nullptr;
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
  NOTIMPLEMENTED();
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

  // TODO(crbug.com/40202416): Let the delegate make additional modifications.
  // TODO(crbug.com/376085326): Apply user agent override.

  return renderer_preferences_;
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

}  // namespace content
