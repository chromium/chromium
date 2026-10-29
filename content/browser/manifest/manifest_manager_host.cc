// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/manifest/manifest_manager_host.h"

#include <stdint.h>

#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/message.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/manifest/manifest_invariants.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest_manager.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

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
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  if (page().GetMainDocument().GetLastCommittedURL().SchemeIs(
          url::kAboutScheme)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       blink::mojom::ManifestRequestResult::kNoManifestAllowed,
                       GURL(), blink::mojom::Manifest::New()));
    return;
  }
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
        base::BindOnce(
            &ManifestManagerHost::NotifyOnceSubscriptionsIfSuccessCached,
            weak_factory_.GetWeakPtr()));
  } else {
    MaybeFetchManifestForSubscriptions();
  }
  return result;
}

base::CallbackListSubscription ManifestManagerHost::GetAllSpecifiedManifests(
    AllManifestsCallbackList::CallbackType callback) {
  auto result = all_manifests_callback_list_.Add(callback);
  if (last_manifest_success_result_.has_value()) {
    callback.Run(base::ok(last_manifest_success_result_->Clone()));
  } else {
    MaybeFetchManifestForSubscriptions();
  }
  return result;
}

void ManifestManagerHost::RequestManifestDebugInfo(
    blink::mojom::ManifestManager::RequestManifestDebugInfoCallback callback) {
  if (page().GetMainDocument().GetLastCommittedURL().SchemeIs(
          url::kAboutScheme)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), GURL(),
                                  blink::mojom::Manifest::New(),
                                  blink::mojom::ManifestDebugInfo::New()));
    return;
  }
  GetManifestManager().RequestManifestDebugInfo(
      base::BindOnce(&ManifestManagerHost::OnRequestManifestDebugInfoResponse,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ManifestManagerHost::OnRequestManifestDebugInfoResponse(
    blink::mojom::ManifestManager::RequestManifestDebugInfoCallback callback,
    const GURL& url,
    blink::mojom::ManifestPtr parsed_manifest,
    blink::mojom::ManifestDebugInfoPtr debug_info) {
  if (parsed_manifest && !blink::IsEmptyManifest(*parsed_manifest)) {
    base::expected<blink::Manifest, blink::ManifestInvariantError> converted =
        blink::ManifestFromMojom(*parsed_manifest);
    if (!converted.has_value()) {
      base::UmaHistogramEnumeration("WebApp.Manifest.InvariantViolation",
                                    converted.error());
      mojo::ReportBadMessage(
          blink::ManifestInvariantErrorToString(converted.error()));
      parsed_manifest = blink::mojom::Manifest::New();
    } else {
      parsed_manifest = blink::ManifestToMojom(*converted);
    }
  }
  std::move(callback).Run(url, std::move(parsed_manifest),
                          std::move(debug_info));
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

base::expected<blink::Manifest, blink::mojom::ManifestRequestResult>
ManifestManagerHost::ValidateAndMaybeOverrideManifest(
    blink::mojom::ManifestRequestResult result,
    std::optional<blink::Manifest> manifest) {
  // Intrinsic invariants already hold (enforced in ManifestFromMojom).
  if (!manifest) {
    if (result == blink::mojom::ManifestRequestResult::kSuccess) {
      mojo::ReportBadMessage(
          "RequestManifest reported success but didn't return a manifest");
      return base::unexpected(
          blink::mojom::ManifestRequestResult::kUnexpectedFailure);
    }
    return base::unexpected(result);
  }
  const url::Origin& document_origin =
      page().GetMainDocument().GetLastCommittedOrigin();
  // Navigating to an opaque origin can race with the manifest request; do not
  // report a bad message.
  if (document_origin.opaque()) {
    base::UmaHistogramBoolean("WebApp.Manifest.ForOpaqueOrigin", true);
    return base::unexpected(
        blink::mojom::ManifestRequestResult::kUnexpectedFailure);
  }
  if (!document_origin.IsSameOriginWith(manifest->GetOrigin())) {
    mojo::ReportBadMessage("Manifest origin must match the document origin.");
    return base::unexpected(
        blink::mojom::ManifestRequestResult::kUnexpectedFailure);
  }

  if (std::optional<blink::Manifest> overridden =
          GetContentClient()->browser()->MaybeOverrideManifest(
              &page().GetMainDocument(), *manifest)) {
    CHECK(overridden->GetOrigin() == manifest->GetOrigin());
    return *std::move(overridden);
  }
  return *std::move(manifest);
}

void ManifestManagerHost::
    ValidateAndMaybeOverrideManifestForTesting(  // IN-TEST
        blink::mojom::ManifestRequestResult result,
        blink::mojom::ManifestPtr manifest) {
  CHECK_IS_TEST();
  std::optional<blink::Manifest> parsed_manifest;
  if (manifest && !blink::IsEmptyManifest(*manifest)) {
    base::expected<blink::Manifest, blink::ManifestInvariantError> converted =
        blink::ManifestFromMojom(*manifest);
    if (!converted.has_value()) {
      base::UmaHistogramEnumeration("WebApp.Manifest.InvariantViolation",
                                    converted.error());
      mojo::ReportBadMessage(
          blink::ManifestInvariantErrorToString(converted.error()));
      return;
    }
    parsed_manifest = std::move(*converted);
  }
  std::ignore =
      ValidateAndMaybeOverrideManifest(result, std::move(parsed_manifest));
}

void ManifestManagerHost::ParseManifestFromString(
    const GURL& document_url,
    const GURL& manifest_url,
    const std::string& manifest_contents,
    ParseManifestCallback callback) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  if (!document_url.is_valid() || url::Origin::Create(document_url).opaque()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), blink::mojom::Manifest::New()));
    return;
  }
  GetManifestManager().ParseManifestFromString(
      document_url, manifest_url, manifest_contents,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(
              &ManifestManagerHost::OnParseManifestFromStringResponse,
              weak_factory_.GetWeakPtr(), document_url, manifest_url,
              std::move(callback)),
          blink::mojom::ManifestPtr()));
}

void ManifestManagerHost::OnParseManifestFromStringResponse(
    const GURL& document_url,
    const GURL& manifest_url,
    ParseManifestCallback callback,
    blink::mojom::ManifestPtr manifest) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  manifest = ValidateParsedManifestFromString(document_url, manifest_url,
                                              std::move(manifest));
  std::move(callback).Run(std::move(manifest));
}

blink::mojom::ManifestPtr ManifestManagerHost::ValidateParsedManifestFromString(
    const GURL& document_url,
    const GURL& manifest_url,
    blink::mojom::ManifestPtr manifest) {
  if (!manifest || blink::IsEmptyManifest(manifest)) {
    return blink::mojom::Manifest::New();
  }

  base::expected<blink::Manifest, blink::ManifestInvariantError> converted =
      blink::ManifestFromMojom(*manifest);
  if (!converted.has_value()) {
    base::UmaHistogramEnumeration("WebApp.Manifest.InvariantViolation",
                                  converted.error());
    mojo::ReportBadMessage(
        blink::ManifestInvariantErrorToString(converted.error()));
    return blink::mojom::Manifest::New();
  }

  // The returned manifest URL must match the requested manifest_url.
  if (converted->manifest_url() != manifest_url) {
    mojo::ReportBadMessage("Returned manifest has incorrect manifest URL");
    return blink::mojom::Manifest::New();
  }

  // document_url was verified valid and non-opaque before sending the IPC.
  url::Origin document_origin = url::Origin::Create(document_url);
  CHECK(!document_origin.opaque());

  // The manifest origin must match the document origin.
  if (!document_origin.IsSameOriginWith(converted->GetOrigin())) {
    mojo::ReportBadMessage("Manifest origin must match the document origin.");
    return blink::mojom::Manifest::New();
  }

  return blink::ManifestToMojom(*converted);
}

blink::mojom::ManifestPtr
ManifestManagerHost::ValidateParsedManifestFromStringForTesting(  // IN-TEST
    const GURL& document_url,
    const GURL& manifest_url,
    blink::mojom::ManifestPtr manifest) {
  CHECK_IS_TEST();
  return ValidateParsedManifestFromString(document_url, manifest_url,
                                          std::move(manifest));
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
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  base::WeakPtr<ManifestManagerHost> weak_this = weak_factory_.GetWeakPtr();
  DispatchManifestNotFound(ExtractPendingCallbacks());
  if (!weak_this) {
    return;
  }
  ManifestResult error_result =
      base::unexpected(blink::mojom::RequestManifestError::New(
          blink::mojom::ManifestRequestResult::kUnexpectedFailure,
          std::vector<blink::mojom::ManifestErrorPtr>()));
  developer_manifest_callback_list_.Notify(error_result);
  if (!weak_this) {
    return;
  }
  all_manifests_callback_list_.Notify(error_result);
  if (!weak_this) {
    return;
  }
  DeleteForPage(page());
}

void ManifestManagerHost::OnRequestManifestResponse(
    int request_id,
    blink::mojom::ManifestRequestResult result,
    const GURL& url,
    blink::mojom::ManifestPtr manifest) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  std::optional<blink::Manifest> parsed_manifest;
  if (manifest && !blink::IsEmptyManifest(*manifest)) {
    base::expected<blink::Manifest, blink::ManifestInvariantError> converted =
        blink::ManifestFromMojom(*manifest);
    if (!converted.has_value()) {
      base::UmaHistogramEnumeration("WebApp.Manifest.InvariantViolation",
                                    converted.error());
      mojo::ReportBadMessage(
          blink::ManifestInvariantErrorToString(converted.error()));
      result = blink::mojom::ManifestRequestResult::kUnexpectedFailure;
    } else {
      parsed_manifest = std::move(*converted);
    }
  }

  base::expected<blink::Manifest, blink::mojom::ManifestRequestResult>
      validated_manifest =
          ValidateAndMaybeOverrideManifest(result, std::move(parsed_manifest));

  blink::mojom::ManifestPtr response_manifest;
  if (validated_manifest.has_value()) {
    response_manifest = blink::ManifestToMojom(*validated_manifest);
  } else {
    result = validated_manifest.error();
    response_manifest = blink::mojom::Manifest::New();
  }
  auto callback = std::move(*callbacks_.Lookup(request_id));
  callbacks_.Remove(request_id);

  std::move(callback).Run(result, url, std::move(response_manifest));
}

void ManifestManagerHost::OnRequestManifestAndErrors(
    const GURL& manifest_url_for_fetch,
    base::expected<blink::mojom::ManifestPtr,
                   blink::mojom::RequestManifestErrorPtr> result) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  // Reset the information for the current manifest url being fetched if it
  // matches the request that was sent to the ManifestManager, and only process
  // requests for the latest manifest url on the page.
  // This helps filter out stale manifest fetches if the manifest url on the
  // page has changed dynamically.
  if (current_fetching_manifest_url_ != manifest_url_for_fetch) {
    return;
  }

  current_fetching_manifest_url_.reset();
  if (result.has_value()) {
    std::optional<blink::Manifest> parsed_manifest;
    if (*result && !blink::IsEmptyManifest(**result)) {
      base::expected<blink::Manifest, blink::ManifestInvariantError> converted =
          blink::ManifestFromMojom(**result);
      if (!converted.has_value()) {
        base::UmaHistogramEnumeration("WebApp.Manifest.InvariantViolation",
                                      converted.error());
        mojo::ReportBadMessage(
            blink::ManifestInvariantErrorToString(converted.error()));
        result = base::unexpected(blink::mojom::RequestManifestError::New(
            blink::mojom::ManifestRequestResult::kUnexpectedFailure,
            std::vector<blink::mojom::ManifestErrorPtr>()));
      } else {
        parsed_manifest = std::move(*converted);
      }
    }
    if (result.has_value()) {
      base::expected<blink::Manifest, blink::mojom::ManifestRequestResult>
          validated = ValidateAndMaybeOverrideManifest(
              blink::mojom::ManifestRequestResult::kSuccess,
              std::move(parsed_manifest));
      if (!validated.has_value()) {
        result = base::unexpected(blink::mojom::RequestManifestError::New(
            blink::mojom::ManifestRequestResult::kUnexpectedFailure,
            std::vector<blink::mojom::ManifestErrorPtr>()));
      } else if (validated->manifest_url().is_empty()) {
        // If a manifest URL clearing raced with the request, then a default
        // manifest can be returned. In this case, do not call observers and
        // instead wait for the next manifest URL change to request the manifest
        // again.
        return;
      } else {
        *result = blink::ManifestToMojom(*validated);
      }
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
    // This Clone COULD be avoided if we cached the full expected result.
    // However - that gets really confusing if we also have it be optional.
    // Since we only care about caching success, it is simpler to clone here,
    // and just cache the success results as an optional.
    last_manifest_success_result_ = result->Clone();
  }
  developer_manifest_callback_list_.Notify(result);
  all_manifests_callback_list_.Notify(result);
}

void ManifestManagerHost::ManifestUrlChanged(const GURL& manifest_url) {
  last_manifest_success_result_ = std::nullopt;
  static_cast<PageImpl&>(page()).UpdateManifestUrl(manifest_url);
  if (HasManifestSubscriptions()) {
    MaybeFetchManifestForSubscriptions();
  }
}

void ManifestManagerHost::MaybeFetchManifestForSubscriptions() {
  if (page().GetMainDocument().GetLastCommittedURL().SchemeIs(
          url::kAboutScheme)) {
    return;
  }
  bool is_manifest_fetch_in_progress =
      current_fetching_manifest_url_.has_value() &&
      current_fetching_manifest_url_ == page().GetManifestUrl();
  if (!page().GetManifestUrl().has_value() ||
      !page().GetManifestUrl()->is_valid() || is_manifest_fetch_in_progress) {
    return;
  }
  current_fetching_manifest_url_ = page().GetManifestUrl();
  auto& manifest_manager = GetManifestManager();
  manifest_manager.RequestManifestAndErrors(base::BindOnce(
      &ManifestManagerHost::OnRequestManifestAndErrors,
      weak_factory_.GetWeakPtr(), *current_fetching_manifest_url_));
}

void ManifestManagerHost::NotifyOnceSubscriptionsIfSuccessCached() {
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

bool ManifestManagerHost::HasManifestSubscriptions() const {
  return !developer_manifest_callback_list_.empty() ||
         !all_manifests_callback_list_.empty();
}

PAGE_USER_DATA_KEY_IMPL(ManifestManagerHost);

// static
PageManifestManager* PageManifestManager::GetOrCreate(Page& page) {
  return ManifestManagerHost::GetOrCreateForPage(page);
}

}  // namespace content
