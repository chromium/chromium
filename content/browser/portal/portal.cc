// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/portal/portal.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "third_party/blink/public/common/features.h"

namespace content {

Portal::Portal(RenderFrameHostImpl* owner_render_frame_host)
    : WebContentsObserver(
          WebContents::FromRenderFrameHost(owner_render_frame_host)),
      owner_render_frame_host_(owner_render_frame_host),
      portal_token_(base::UnguessableToken::Create()) {}

Portal::~Portal() {}

// static
bool Portal::IsEnabled() {
  return base::FeatureList::IsEnabled(blink::features::kPortals) ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kEnableExperimentalWebPlatformFeatures);
}

// static
Portal* Portal::Create(RenderFrameHostImpl* owner_render_frame_host,
                       blink::mojom::PortalRequest request) {
  auto portal_ptr = base::WrapUnique(new Portal(owner_render_frame_host));
  Portal* portal = portal_ptr.get();
  portal->binding_ =
      mojo::MakeStrongBinding(std::move(portal_ptr), std::move(request));
  return portal;
}

// static
std::unique_ptr<Portal> Portal::CreateForTesting(
    RenderFrameHostImpl* owner_render_frame_host) {
  return base::WrapUnique(new Portal(owner_render_frame_host));
}

void Portal::Init(
    base::OnceCallback<void(const base::UnguessableToken&)> callback) {
  std::move(callback).Run(portal_token_);
  WebContents::CreateParams params(
      WebContents::FromRenderFrameHost(owner_render_frame_host_)
          ->GetBrowserContext());
  portal_contents_ = WebContents::Create(params);
  WebContents::FromRenderFrameHost(owner_render_frame_host_)
      ->GetDelegate()
      ->PortalWebContentsCreated(portal_contents_.get());
}

void Portal::Navigate(const GURL& url) {
  NavigationController::LoadURLParams load_url_params(url);
  portal_contents_->GetController().LoadURLWithParams(load_url_params);
}

void Portal::Activate(
    base::OnceCallback<void(blink::mojom::PortalActivationStatus)> callback) {
  WebContents* outer_contents =
      WebContents::FromRenderFrameHost(owner_render_frame_host_);
  WebContentsDelegate* delegate = outer_contents->GetDelegate();
  if (delegate) {
    bool is_loading = portal_contents_->IsLoading();
    WebContents* portal_contents = portal_contents_.get();
    std::unique_ptr<WebContents> contents = delegate->SwapWebContents(
        outer_contents, std::move(portal_contents_), true, is_loading);

    if (contents.get() == outer_contents) {
      // TODO(lfg): The old WebContents is currently discarded, but should be
      // kept and passed to the new page.
      std::move(callback).Run(blink::mojom::PortalActivationStatus::kSuccess);
    } else {
      DCHECK_EQ(portal_contents, contents.get());
      portal_contents_ = std::move(contents);
      std::move(callback).Run(
          blink::mojom::PortalActivationStatus::kNotSupported);
    }

    return;
  }

  std::move(callback).Run(blink::mojom::PortalActivationStatus::kNotSupported);
}

void Portal::RenderFrameDeleted(RenderFrameHost* render_frame_host) {
  if (render_frame_host == owner_render_frame_host_)
    binding_->Close();  // Also deletes |this|.
}

WebContents* Portal::GetPortalContents() {
  return portal_contents_.get();
}

void Portal::SetBindingForTesting(
    mojo::StrongBindingPtr<blink::mojom::Portal> binding) {
  binding_ = binding;
}

}  // namespace content
