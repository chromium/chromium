// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/eye_dropper_chooser_impl.h"

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/eye_dropper.h"
#include "content/public/browser/eye_dropper_listener.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/blink/public/mojom/choosers/color_chooser.mojom.h"

namespace content {

namespace {

// Tracks the single eye dropper that may be open per WebContents at a time.
// Allowing multiple concurrent eye droppers could let a page confuse the user
// or mount a pixel-stealing attack, so only one is permitted at a time.
// See https://crbug.com/40280878.
class ActiveEyeDropperTracker
    : public WebContentsUserData<ActiveEyeDropperTracker> {
 public:
  ~ActiveEyeDropperTracker() override = default;

  EyeDropperChooserImpl* active() const { return active_; }
  void set_active(EyeDropperChooserImpl* active) { active_ = active; }

 private:
  friend WebContentsUserData;
  explicit ActiveEyeDropperTracker(WebContents* web_contents)
      : WebContentsUserData<ActiveEyeDropperTracker>(*web_contents) {}

  raw_ptr<EyeDropperChooserImpl> active_ = nullptr;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(ActiveEyeDropperTracker);

}  // namespace

// static
void EyeDropperChooserImpl::Create(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::EyeDropperChooser> receiver) {
  CHECK(render_frame_host);

  // Renderer process should already check for user activation before sending
  // this request. Double check in case of compromised renderer and consume
  // the activation.
  if (!static_cast<RenderFrameHostImpl*>(render_frame_host)
           ->frame_tree_node()
           ->UpdateUserActivationState(
               blink::mojom::UserActivationUpdateType::
                   kConsumeTransientActivation,
               blink::mojom::UserActivationNotificationType::kNone)) {
    return;
  }

  new EyeDropperChooserImpl(*render_frame_host, std::move(receiver));
}

EyeDropperChooserImpl::EyeDropperChooserImpl(
    RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::EyeDropperChooser> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}

EyeDropperChooserImpl::~EyeDropperChooserImpl() {
  ClearActiveEyeDropper();
  if (callback_) {
    std::move(callback_).Run(/*success=*/false, /*color=*/0);
  }
}

void EyeDropperChooserImpl::Choose(ChooseCallback callback) {
  if (callback_ || eye_dropper_) {
    ReportBadMessageAndDeleteThis(
        "EyeDropperChooser::Choose() called while a selection was already in "
        "progress.");
    return;
  }

  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());
  if (!web_contents) {
    std::move(callback).Run(/*success=*/false, /*color=*/0);
    return;
  }

  // Only one eye dropper may be open per WebContents at a time. Allowing
  // multiple concurrent eye droppers could let a page confuse the user or
  // mount a pixel-stealing attack. See https://crbug.com/40280878.
  ActiveEyeDropperTracker* tracker =
      ActiveEyeDropperTracker::GetOrCreateForWebContents(web_contents);
  if (tracker->active()) {
    std::move(callback).Run(/*success=*/false, /*color=*/0);
    return;
  }

  callback_ = std::move(callback);
  tracker->set_active(this);
  if (WebContentsDelegate* delegate = web_contents->GetDelegate()) {
    eye_dropper_ = delegate->OpenEyeDropper(&render_frame_host(), this);
  }

  if (!eye_dropper_) {
    // Color selection wasn't successful since the eye dropper can't be opened.
    ColorSelectionCanceled();
  }
}

void EyeDropperChooserImpl::ColorSelected(SkColor color) {
  base::WeakPtr<EyeDropperChooserImpl> weak_this =
      weak_ptr_factory_.GetWeakPtr();
  eye_dropper_.reset();
  CHECK(weak_this);
  ClearActiveEyeDropper();
  std::move(callback_).Run(/*success=*/true, color);
}

void EyeDropperChooserImpl::ColorSelectionCanceled() {
  base::WeakPtr<EyeDropperChooserImpl> weak_this =
      weak_ptr_factory_.GetWeakPtr();
  eye_dropper_.reset();
  CHECK(weak_this);
  ClearActiveEyeDropper();
  std::move(callback_).Run(/*success=*/false, /*color=*/0);
}

void EyeDropperChooserImpl::ClearActiveEyeDropper() {
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());
  if (!web_contents) {
    return;
  }
  ActiveEyeDropperTracker* tracker =
      ActiveEyeDropperTracker::FromWebContents(web_contents);
  if (tracker && tracker->active() == this) {
    tracker->set_active(nullptr);
  }
}

}  // namespace content
