// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/manifest/manifest_manager_host.h"

#include <stdint.h>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/message.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

void DispatchManifestNotFound(
    std::vector<ManifestManagerHost::GetManifestCallback> callbacks) {
  for (ManifestManagerHost::GetManifestCallback& callback : callbacks)
    std::move(callback).Run(
        blink::mojom::ManifestRequestResult::kUnexpectedFailure, GURL(),
        blink::mojom::Manifest::New());
}

}  // namespace

ManifestManagerHost::ManifestManagerHost(Page& page)
    : PageUserData<ManifestManagerHost>(page) {}

ManifestManagerHost::~ManifestManagerHost() {
  std::vector<GetManifestCallback> callbacks = ExtractPendingCallbacks();
  if (callbacks.empty())
    return;
  // PostTask the pending callbacks so they run outside of this destruction
  // stack frame.
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(DispatchManifestNotFound, std::move(callbacks)));
}

void ManifestManagerHost::BindObserver(
    mojo::PendingAssociatedReceiver<blink::mojom::ManifestUrlChangeObserver>
        receiver) {
  manifest_url_change_observer_receiver_.Bind(std::move(receiver));
  manifest_url_change_observer_receiver_.SetFilter(
      static_cast<RenderFrameHostImpl&>(page().GetMainDocument())
          .CreateMessageFilterForAssociatedReceiver(
              blink::mojom::ManifestUrlChangeObserver::Name_));
}

void ManifestManagerHost::GetManifest(GetManifestCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Do not call into MaybeOverrideManifest in a non primary page since
  // it checks the url from PreRedirectionURLObserver that works only in
  // a primary page.
  // TODO(crbug.com/40214638): Maybe cancel prerendering if it hits this.
  if (!page().IsPrimary()) {
    std::move(callback).Run(
        blink::mojom::ManifestRequestResult::kUnexpectedFailure, GURL(),
        blink::mojom::Manifest::New());
    return;
  }

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
  if (!manifest_manager_) {
    page().GetMainDocument().GetRemoteInterfaces()->GetInterface(
        manifest_manager_.BindNewPipeAndPassReceiver());
    manifest_manager_.set_disconnect_handler(base::BindOnce(
        &ManifestManagerHost::OnConnectionError, base::Unretained(this)));
  }
  return *manifest_manager_;
}

std::vector<ManifestManagerHost::GetManifestCallback>
ManifestManagerHost::ExtractPendingCallbacks() {
  std::vector<GetManifestCallback> callbacks;
  for (CallbackMap::iterator it(&callbacks_); !it.IsAtEnd(); it.Advance())
    callbacks.push_back(std::move(*it.GetCurrentValue()));
  callbacks_.Clear();
  return callbacks;
}

void ManifestManagerHost::OnConnectionError() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DispatchManifestNotFound(ExtractPendingCallbacks());
  if (GetForPage(page()))
    DeleteForPage(page());
}

void ManifestManagerHost::OnRequestManifestResponse(
    int request_id,
    blink::mojom::ManifestRequestResult result,
    const GURL& url,
    blink::mojom::ManifestPtr manifest) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Mojo bindings guarantee that `manifest` isn't null.
  CHECK(manifest);
  if (result == blink::mojom::ManifestRequestResult::kSuccess &&
      blink::IsEmptyManifest(manifest)) {
    mojo::ReportBadMessage(
        "RequestManifest reported success but didn't return a manifest");
  }
  if (!blink::IsEmptyManifest(manifest)) {
    // `start_url`, `id`, and `scope` MUST be populated if the manifest is not
    // empty.
    bool start_url_valid = manifest->start_url.is_valid();
    bool id_valid = manifest->id.is_valid();
    bool scope_valid = manifest->scope.is_valid();
    if (!start_url_valid || !id_valid || !scope_valid) {
      manifest = blink::mojom::Manifest::New();
      constexpr auto valid_to_string = [](bool b) -> std::string_view {
        return b ? "valid" : "invalid";
      };
      mojo::ReportBadMessage(
          base::StrCat({"RequestManifest's manifest must "
                        "either be empty or populate the "
                        "the start_url (",
                        valid_to_string(start_url_valid), "), id(",
                        valid_to_string(id_valid), "), and scope (",
                        valid_to_string(scope_valid), ")."}));
    }
  }
  // Empty manifests means it failed to parse or an unresolvable problem like
  // the frame is destroying. Since AppBannerManager completely ignores these
  // manifests, avoid overriding them as well to prevent the overriding
  // infrastructure from seeing manifests without the default members.
  if (!blink::IsEmptyManifest(manifest)) {
    GetContentClient()->browser()->MaybeOverrideManifest(
        &page().GetMainDocument(), manifest);
  }
  auto callback = std::move(*callbacks_.Lookup(request_id));
  callbacks_.Remove(request_id);

  std::move(callback).Run(result, url, std::move(manifest));
}

void ManifestManagerHost::ManifestUrlChanged(const GURL& manifest_url) {
  static_cast<PageImpl&>(page()).UpdateManifestUrl(manifest_url);
}

PAGE_USER_DATA_KEY_IMPL(ManifestManagerHost);
}  // namespace content
