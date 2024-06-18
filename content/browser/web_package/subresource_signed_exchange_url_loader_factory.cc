// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/subresource_signed_exchange_url_loader_factory.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "content/browser/web_package/signed_exchange_inner_response_url_loader.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/initiator_lock_compatibility.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "storage/browser/blob/blob_data_handle.h"

namespace content {

namespace {

bool IsValidRequestInitiator(const network::ResourceRequest& request,
                             const url::Origin& request_initiator_origin_lock) {
  // TODO(lukasza): Deduplicate the check below by reusing parts of
  // CorsURLLoaderFactory::IsValidRequest (potentially also reusing the parts
  // that validate non-initiator-related parts of a ResourceRequest).
  network::InitiatorLockCompatibility initiator_lock_compatibility =
      network::VerifyRequestInitiatorLock(request_initiator_origin_lock,
                                          request.request_initiator);
  switch (initiator_lock_compatibility) {
    case network::InitiatorLockCompatibility::kBrowserProcess:
      // kBrowserProcess cannot happen outside of NetworkService.
      NOTREACHED_IN_MIGRATION();
      return false;

    case network::InitiatorLockCompatibility::kNoLock:
    case network::InitiatorLockCompatibility::kNoInitiator:
      // Only browser-initiated navigations can specify no initiator and we only
      // expect subresource requests (i.e. non-navigations) to go through
      // SubresourceSignedExchangeURLLoaderFactory::CreateLoaderAndStart.
      NOTREACHED_IN_MIGRATION();
      return false;

    case network::InitiatorLockCompatibility::kCompatibleLock:
      return true;

    case network::InitiatorLockCompatibility::kIncorrectLock:
      // This branch indicates that either 1) the CreateLoaderAndStart IPC was
      // forged by a malicious/compromised renderer process or 2) there are
      // renderer-side bugs.
      NOTREACHED_IN_MIGRATION();
      return false;
  }

  // Failing safely for an unrecognied `network::InitiatorLockCompatibility`
  // enum value.
  NOTREACHED_IN_MIGRATION();
  return false;
}

}  // namespace

SubresourceSignedExchangeURLLoaderFactory::
    SubresourceSignedExchangeURLLoaderFactory(
        mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
        std::unique_ptr<const PrefetchedSignedExchangeCacheEntry> entry,
        const url::Origin& request_initiator_origin_lock)
    : entry_(std::move(entry)),
      request_initiator_origin_lock_(request_initiator_origin_lock) {
  receivers_.Add(this, std::move(receiver));
  receivers_.set_disconnect_handler(base::BindRepeating(
      &SubresourceSignedExchangeURLLoaderFactory::OnMojoDisconnect,
      base::Unretained(this)));
}

SubresourceSignedExchangeURLLoaderFactory::
    ~SubresourceSignedExchangeURLLoaderFactory() = default;

void SubresourceSignedExchangeURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  if (!IsValidRequestInitiator(request, request_initiator_origin_lock_)) {
    NOTREACHED_IN_MIGRATION();
    network::debug::ScopedResourceRequestCrashKeys request_crash_keys(request);
    network::debug::ScopedRequestInitiatorOriginLockCrashKey lock_crash_keys(
        request_initiator_origin_lock_);
    mojo::ReportBadMessage(
        "SubresourceSignedExchangeURLLoaderFactory: "
        "lock VS initiator mismatch");
    mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
        ->OnComplete(
            network::URLLoaderCompletionStatus(net::ERR_INVALID_ARGUMENT));
    return;
  }

  DCHECK_EQ(request.url, entry_->inner_url());
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<SignedExchangeInnerResponseURLLoader>(
          request, entry_->inner_response().Clone(),
          std::make_unique<const storage::BlobDataHandle>(
              *entry_->blob_data_handle()),
          *entry_->completion_status(), std::move(client),
          false /* is_navigation_request */, orb_state_),
      std::move(loader));
}

void SubresourceSignedExchangeURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void SubresourceSignedExchangeURLLoaderFactory::OnMojoDisconnect() {
  if (!receivers_.empty()) {
    return;
  }
  delete this;
}

}  // namespace content
