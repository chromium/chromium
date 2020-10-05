// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/eye_dropper_chooser_impl.h"

#include "base/callback.h"
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
  DCHECK(render_frame_host);
  new EyeDropperChooserImpl(render_frame_host, std::move(receiver));
}

EyeDropperChooserImpl::EyeDropperChooserImpl(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::EyeDropperChooser> receiver)
    : FrameServiceBase(render_frame_host, std::move(receiver)) {}

EyeDropperChooserImpl::~EyeDropperChooserImpl() {
  if (callback_)
    std::move(callback_).Run(/*success=*/false, /*color=*/0);
}

void EyeDropperChooserImpl::Choose(ChooseCallback callback) {
  if (!render_frame_host() || callback_ || eye_dropper_) {
    std::move(callback).Run(/*success=*/false, /*color=*/0);
    return;
  }

  callback_ = std::move(callback);
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host());
  if (WebContentsDelegate* delegate = web_contents->GetDelegate())
    eye_dropper_ = delegate->OpenEyeDropper(render_frame_host(), this);

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
