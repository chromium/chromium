// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/manifest/manifest_manager_host.h"

#include <stdint.h>

#include "base/bind.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/manifest/manifest.h"

namespace content {

ManifestManagerHost::ManifestManagerHost(WebContents* web_contents)
    : WebContentsObserver(web_contents),
      manifest_url_change_observer_bindings_(web_contents, this) {}

ManifestManagerHost::~ManifestManagerHost() {
  OnConnectionError();
}

void ManifestManagerHost::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  if (render_frame_host == manifest_manager_frame_)
    OnConnectionError();
}

void ManifestManagerHost::GetManifest(GetManifestCallback callback) {
  auto& manifest_manager = GetManifestManager();
  int request_id = callbacks_.Add(
      std::make_unique<GetManifestCallback>(std::move(callback)));
  manifest_manager.RequestManifest(
      base::BindOnce(&ManifestManagerHost::OnRequestManifestResponse,
                     base::Unretained(this), request_id));
}

void ManifestManagerHost::RequestManifestDebugInfo(
    blink::mojom::ManifestManager::RequestManifestDebugInfoCallback callback) {
  GetManifestManager().RequestManifestDebugInfo(std::move(callback));
}

blink::mojom::ManifestManager& ManifestManagerHost::GetManifestManager() {
  if (manifest_manager_frame_ != web_contents()->GetMainFrame())
    OnConnectionError();

  if (!manifest_manager_) {
    manifest_manager_frame_ = web_contents()->GetMainFrame();
    manifest_manager_frame_->GetRemoteInterfaces()->GetInterface(
        manifest_manager_.BindNewPipeAndPassReceiver());
    manifest_manager_.set_disconnect_handler(base::BindOnce(
        &ManifestManagerHost::OnConnectionError, base::Unretained(this)));
  }
  return *manifest_manager_;
}

void ManifestManagerHost::OnConnectionError() {
  manifest_manager_frame_ = nullptr;
  manifest_manager_.reset();
  std::vector<GetManifestCallback> callbacks;
  for (CallbackMap::iterator it(&callbacks_); !it.IsAtEnd(); it.Advance()) {
    callbacks.push_back(std::move(*it.GetCurrentValue()));
  }
  callbacks_.Clear();
  for (auto& callback : callbacks)
    std::move(callback).Run(GURL(), blink::Manifest());
}

void ManifestManagerHost::OnRequestManifestResponse(
    int request_id,
    const GURL& url,
    const blink::Manifest& manifest) {
  auto callback = std::move(*callbacks_.Lookup(request_id));
  callbacks_.Remove(request_id);
  std::move(callback).Run(url, manifest);
}

void ManifestManagerHost::ManifestUrlChanged(
    const base::Optional<GURL>& manifest_url) {
  if (manifest_url_change_observer_bindings_.GetCurrentTargetFrame() !=
      web_contents()->GetMainFrame()) {
    return;
  }
  static_cast<WebContentsImpl*>(web_contents())
      ->NotifyManifestUrlChanged(manifest_url);
}

}  // namespace content
