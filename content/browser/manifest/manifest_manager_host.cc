// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/manifest/manifest_manager_host.h"

#include <stdint.h>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/message.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-data-view.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest_manager.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

void DispatchManifestNotFound(
    std::vector<ManifestManagerHost::GetManifestCallback> callbacks) {
  for (ManifestManagerHost::GetManifestCallback& callback : callbacks) {
    std::move(callback).Run(
        blink::mojom::ManifestRequestResult::kUnexpectedFailure, GURL(),
        blink::mojom::Manifest::New());
  }
}

std::optional<std::string> MaybeGetBadMessageStringForManifest(
    blink::mojom::ManifestRequestResult result,
    const blink::mojom::Manifest& manifest) {
  if (result == blink::mojom::ManifestRequestResult::kSuccess &&
      blink::IsEmptyManifest(manifest)) {
    return "RequestManifest reported success but didn't return a manifest";
  }

  if (!blink::IsEmptyManifest(manifest)) {
    // `start_url`, `id`, and `scope` MUST be populated if the manifest is not
    // empty.
    bool start_url_valid = manifest.start_url.is_valid();
    bool id_valid = manifest.id.is_valid();
    bool scope_valid = manifest.scope.is_valid();
    if (!start_url_valid || !id_valid || !scope_valid) {
      constexpr auto valid_to_string = [](bool b) -> std::string_view {
        return b ? "valid" : "invalid";
      };
      return base::StrCat(
          {"RequestManifest's manifest must "
           "either be empty or populate the "
           "the start_url (",
           valid_to_string(start_url_valid), "), id (",
           valid_to_string(id_valid), "), and scope (",
           valid_to_string(scope_valid), ")."});
    }
  }
  return std::nullopt;
}

}  // namespace

ManifestManagerHost::ManifestManagerHost(Page& page)
    : PageUserData<ManifestManagerHost>(page) {}

ManifestManagerHost::~ManifestManagerHost() {
  std::vector<GetManifestCallback> callbacks = ExtractPendingCallbacks();
  if (callbacks.empty()) {
    return;
  }
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
  auto& manifest_manager = GetManifestManager();
  int request_id = callbacks_.Add(
      std::make_unique<GetManifestCallback>(std::move(callback)));
  manifest_manager.RequestManifest(
      base::BindOnce(&ManifestManagerHost::OnRequestManifestResponse,
                     base::Unretained(this), request_id));
}

base::CallbackListSubscription ManifestManagerHost::GetSpecifiedManifest(
    ManifestCallbackList::CallbackType callback) {
  auto result = developer_manifest_callback_list_.Add(std::move(callback));
  if (last_manifest_success_result_.has_value()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ManifestManagerHost::NotifySubscriptionsIfSuccessCached,
                       weak_factory_.GetWeakPtr()));
  } else {
    MaybeFetchManifestForSubscriptions();
  }
  return result;
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

blink::mojom::ManifestPtr ManifestManagerHost::ValidateAndMaybeOverrideManifest(
    blink::mojom::ManifestRequestResult result,
    blink::mojom::ManifestPtr manifest) {
  // Mojo bindings guarantee that `manifest` isn't null.
  CHECK(manifest);
  if (std::optional<std::string> bad_message_error =
          MaybeGetBadMessageStringForManifest(result, *manifest);
      bad_message_error.has_value()) {
    mojo::ReportBadMessage(*bad_message_error);
    return blink::mojom::Manifest::New();
  }
  if (!blink::IsEmptyManifest(manifest)) {
    // The manifest overriding infrastructure does not support empty manifests
    // from errors.
    GetContentClient()->browser()->MaybeOverrideManifest(
        &page().GetMainDocument(), manifest);
  }
  return manifest;
}

std::vector<ManifestManagerHost::GetManifestCallback>
ManifestManagerHost::ExtractPendingCallbacks() {
  std::vector<GetManifestCallback> callbacks;
  for (CallbackMap::iterator it(&callbacks_); !it.IsAtEnd(); it.Advance()) {
    callbacks.push_back(std::move(*it.GetCurrentValue()));
  }
  callbacks_.Clear();
  return callbacks;
}

void ManifestManagerHost::OnConnectionError() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DispatchManifestNotFound(ExtractPendingCallbacks());
  if (GetForPage(page())) {
    DeleteForPage(page());
  }
}

void ManifestManagerHost::OnRequestManifestResponse(
    int request_id,
    blink::mojom::ManifestRequestResult result,
    const GURL& url,
    blink::mojom::ManifestPtr manifest) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  manifest = ValidateAndMaybeOverrideManifest(result, std::move(manifest));
  auto callback = std::move(*callbacks_.Lookup(request_id));
  callbacks_.Remove(request_id);

  std::move(callback).Run(result, url, std::move(manifest));
}

void ManifestManagerHost::OnRequestManifestAndErrors(
    base::expected<blink::mojom::ManifestPtr,
                   blink::mojom::RequestManifestErrorPtr> result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (result.has_value()) {
    result = ValidateAndMaybeOverrideManifest(
        blink::mojom::ManifestRequestResult::kSuccess, std::move(*result));
    // Handle BadMessage case with an explicit error state instead of an empty
    // manifest.
    if (blink::IsEmptyManifest(*result)) {
      result = base::unexpected(blink::mojom::RequestManifestError::New(
          blink::mojom::ManifestRequestResult::kUnexpectedFailure,
          std::vector<blink::mojom::ManifestErrorPtr>()));
    } else if ((*result)->manifest_url.is_empty()) {
      // If a manifest URL clearing raced with the request, then a default
      // manifest can be returned. In this case, do not call observers and
      // instead wait for the next manifest URL change to request the manifest
      // again.
      return;
    }
  } else {
    blink::mojom::ManifestRequestResult error_result = result.error()->error;
    if (error_result == blink::mojom::ManifestRequestResult::kSuccess ||
        error_result ==
            blink::mojom::ManifestRequestResult::kNoManifestSpecified) {
      mojo::ReportBadMessage(
          "Manifest result error cannot be a success value.");
      result.error()->error =
          blink::mojom::ManifestRequestResult::kUnexpectedFailure;
    }
  }
  if (result.has_value()) {
    last_manifest_success_result_ = std::move(result.value());
    NotifySubscriptionsIfSuccessCached();
  } else {
    developer_manifest_callback_list_.Notify(result);
  }
}

void ManifestManagerHost::ManifestUrlChanged(const GURL& manifest_url) {
  last_manifest_success_result_ = std::nullopt;
  static_cast<PageImpl&>(page()).UpdateManifestUrl(manifest_url);
  if (!developer_manifest_callback_list_.empty()) {
    MaybeFetchManifestForSubscriptions();
  }
}

void ManifestManagerHost::MaybeFetchManifestForSubscriptions() {
  if (!page().GetManifestUrl().has_value() ||
      !page().GetManifestUrl()->is_valid()) {
    return;
  }
  auto& manifest_manager = GetManifestManager();
  manifest_manager.RequestManifestAndErrors(
      base::BindOnce(&ManifestManagerHost::OnRequestManifestAndErrors,
                     weak_factory_.GetWeakPtr()));
}

void ManifestManagerHost::NotifySubscriptionsIfSuccessCached() {
  if (last_manifest_success_result_.has_value()) {
    // This Clone COULD be avoided if we cached the full expected result.
    // However - that gets really confusing if we also have it be optional.
    // Since we only care about caching success, it is simpler to clone here,
    // and just cache the success results as an optional.
    base::expected<blink::mojom::ManifestPtr,
                   blink::mojom::RequestManifestErrorPtr>
        result = base::ok(last_manifest_success_result_->Clone());
    developer_manifest_callback_list_.Notify(result);
  }
}

PAGE_USER_DATA_KEY_IMPL(ManifestManagerHost);

// static
PageManifestManager* PageManifestManager::GetOrCreate(Page& page) {
  return ManifestManagerHost::GetOrCreateForPage(page);
}

}  // namespace content
