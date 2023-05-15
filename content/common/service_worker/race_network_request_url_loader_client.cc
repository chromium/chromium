// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/race_network_request_url_loader_client.h"
#include "content/common/service_worker/service_worker_resource_loader.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

namespace content {
ServiceWorkerRaceNetworkRequestURLLoaderClient::
    ServiceWorkerRaceNetworkRequestURLLoaderClient(
        const network::ResourceRequest& request,
        base::WeakPtr<ServiceWorkerResourceLoader> owner)
    : request_(request), owner_(std::move(owner)) {}

ServiceWorkerRaceNetworkRequestURLLoaderClient::
    ~ServiceWorkerRaceNetworkRequestURLLoaderClient() = default;

void ServiceWorkerRaceNetworkRequestURLLoaderClient::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  NOTREACHED();
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  network::RecordOnTransferSizeUpdatedUMA(
      network::OnTransferSizeUpdatedFrom::kServiceWorkerRaceNetworkRequest);
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  // Do nothing. Early Hints response will be handled by owner's
  // |url_loader_client_|.
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr head,
    mojo::ScopedDataPipeConsumerHandle body,
    absl::optional<mojo_base::BigBuffer> cached_metadata) {
  if (!owner_) {
    return;
  }
  // If |fetch_response_from_| is FetchResponseFrom::kServiceWorker, that
  // means the response was already received from the fetch handler. The
  // response from RaceNetworkRequest is simply discarded in that case.
  if (owner_->fetch_response_from() ==
      ServiceWorkerResourceLoader::FetchResponseFrom::kServiceWorker) {
    return;
  }
  // If the response is not 200, use the other response from the fetch handler
  // instead because it may have a response from the cache.
  // TODO(crbug.com/1420517): More comprehensive error handling may be needed,
  // especially the case when HTTP cache hit or redirect happened.
  if (head->headers->response_code() != net::HttpStatusCode::HTTP_OK) {
    return;
  }

  owner_->SetFetchResponseFrom(
      ServiceWorkerResourceLoader::FetchResponseFrom::kWithoutServiceWorker);

  head_ = std::move(head);
  owner_->CommitResponseHeaders(head_);
  owner_->CommitResponseBody(head_, std::move(body),
                             std::move(cached_metadata));
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  if (!owner_) {
    return;
  }
  // If fetch_response_from() is FetchResponseFrom::kServiceWorker, that
  // means the response was already received from the fetch handler. The
  // response from RaceNetworkRequest is simply discarded in that case.
  if (owner_->fetch_response_from() ==
      ServiceWorkerResourceLoader::FetchResponseFrom::kServiceWorker) {
    return;
  }
  owner_->SetFetchResponseFrom(
      ServiceWorkerResourceLoader::FetchResponseFrom::kWithoutServiceWorker);
  owner_->HandleRedirect(redirect_info, head);
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  if (!owner_) {
    return;
  }
  // If the fetch handler wins or there is a network error in
  // RaceNetworkRequest, do nothing. Defer the handling to the owner.
  if (owner_->fetch_response_from() !=
      ServiceWorkerResourceLoader::FetchResponseFrom::kWithoutServiceWorker) {
    return;
  }

  owner_->CommitCompleted(status.error_code,
                          "RaceNetworkRequest has completed.");
}

void ServiceWorkerRaceNetworkRequestURLLoaderClient::Bind(
    mojo::PendingRemote<network::mojom::URLLoaderClient>* remote) {
  receiver_.Bind(remote->InitWithNewPipeAndPassReceiver());
}

net::NetworkTrafficAnnotationTag
ServiceWorkerRaceNetworkRequestURLLoaderClient::NetworkTrafficAnnotationTag() {
  return net::DefineNetworkTrafficAnnotation(
      "service_worker_race_network_request",
      R"(
    semantics {
      sender: "ServiceWorkerRaceNetworkRequest"
      description:
        "This request is issued by a navigation to fetch the content of the "
        "page that is being navigated to, or by a renderer to fetch "
        "subresources in the case where a service worker has been registered "
        "for the page and the ServiceWorkerBypassFetchHandler feature and the "
        "RaceNetworkRequest param are enabled."
      trigger:
        "Navigating Chrome (by clicking on a link, bookmark, history item, "
        "using session restore, etc) and subsequent resource loading."
      data:
        "Arbitrary site-controlled data can be included in the URL, HTTP "
        "headers, and request body. Requests may include cookies and "
        "site-specific credentials."
      destination: WEBSITE
      internal {
        contacts {
          email: "chrome-worker@google.com"
        }
      }
      user_data {
        type: ARBITRARY_DATA
      }
      last_reviewed: "2023-03-22"
    }
    policy {
      cookies_allowed: YES
      cookies_store: "user"
      setting:
        "This request can be prevented by disabling service workers, which can "
        "be done by disabling cookie and site data under Settings, Content "
        "Settings, Cookies."
      chrome_policy {
        URLBlocklist {
          URLBlocklist: { entries: '*' }
        }
      }
      chrome_policy {
        URLAllowlist {
          URLAllowlist { }
        }
      }
    }
    comments:
      "Chrome would be unable to use service workers if this feature were "
      "disabled, which could result in a degraded experience for websites that "
      "register a service worker. Using either URLBlocklist or URLAllowlist "
      "policies (or a combination of both) limits the scope of these requests."
)");
}
}  // namespace content
