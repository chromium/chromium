// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/display_cutout/safe_area_insets_host.h"

#include "content/browser/display_cutout/display_cutout_host_impl.h"
#include "content/browser/display_cutout/safe_area_insets_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/page/display_cutout.mojom.h"
#include "ui/gfx/geometry/insets.h"

namespace content {

// static
std::unique_ptr<SafeAreaInsetsHost> SafeAreaInsetsHost::Create(
    WebContentsImpl* web_contents_impl) {
  if (base::FeatureList::IsEnabled(features::kDrawCutoutEdgeToEdge)) {
    return absl::make_unique<SafeAreaInsetsHostImpl>(web_contents_impl);
  } else {
    return absl::make_unique<DisplayCutoutHostImpl>(web_contents_impl);
  }
}

SafeAreaInsetsHost::SafeAreaInsetsHost(WebContentsImpl* web_contents)
    : web_contents_impl_(web_contents), receivers_(web_contents, this) {}

SafeAreaInsetsHost::~SafeAreaInsetsHost() = default;

void SafeAreaInsetsHost::BindReceiver(
    mojo::PendingAssociatedReceiver<blink::mojom::DisplayCutoutHost> receiver,
    RenderFrameHost* rfh) {
  receivers_.Bind(rfh, std::move(receiver));
}

void SafeAreaInsetsHost::NotifyViewportFitChanged(
    blink::mojom::ViewportFit value) {
  ViewportFitChangedForFrame(receivers_.GetCurrentTargetFrame(), value);
}

void SafeAreaInsetsHost::SendSafeAreaToFrame(RenderFrameHost* rfh,
                                             gfx::Insets insets) {
  blink::AssociatedInterfaceProvider* provider =
      rfh->GetRemoteAssociatedInterfaces();
  if (!provider) {
    return;
  }

  mojo::AssociatedRemote<blink::mojom::DisplayCutoutClient> client;
  provider->GetInterface(client.BindNewEndpointAndPassReceiver());
  client->SetSafeArea(insets);
}

}  // namespace content
