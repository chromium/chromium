// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/eye_dropper_chooser_impl.h"

#include "base/functional/callback.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/eye_dropper.h"
#include "content/public/browser/eye_dropper_listener.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/blink/public/mojom/choosers/color_chooser.mojom.h"

namespace content {

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
  if (callback_)
    std::move(callback_).Run(/*success=*/false, /*color=*/0);
}

void EyeDropperChooserImpl::Choose(ChooseCallback callback) {
  if (callback_ || eye_dropper_) {
    std::move(callback).Run(/*success=*/false, /*color=*/0);
    return;
  }

  callback_ = std::move(callback);
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());
  if (WebContentsDelegate* delegate = web_contents->GetDelegate())
    eye_dropper_ = delegate->OpenEyeDropper(&render_frame_host(), this);

  if (!eye_dropper_) {
    // Color selection wasn't successful since the eye dropper can't be opened.
    ColorSelectionCanceled();
  }
}

void EyeDropperChooserImpl::ColorSelected(SkColor color) {
  eye_dropper_.reset();
  std::move(callback_).Run(/*success=*/true, color);
}

void EyeDropperChooserImpl::ColorSelectionCanceled() {
  eye_dropper_.reset();
  std::move(callback_).Run(/*success=*/false, /*color=*/0);
}

}  // namespace content
