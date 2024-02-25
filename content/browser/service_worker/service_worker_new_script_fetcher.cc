// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_new_script_fetcher.h"

#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_loader_helpers.h"
#include "content/browser/service_worker/service_worker_new_script_loader.h"
#include "content/public/browser/global_request_id.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

namespace {

const net::NetworkTrafficAnnotationTag
    kServiceWorkerScriptLoadTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("service_worker_script_load",
                                            R"(
      semantics {
        sender: "ServiceWorker System"
        description:
          "This request is issued by a service worker registration attempt, to "
          "fetch the service worker's main script."
        trigger:
          "Calling navigator.serviceWorker.register()."
        data:
          "No body. 'Service-Worker: script' header is attached. Requests may "
          "include cookies and credentials."
        destination: WEBSITE
      }
      policy {
        cookies_allowed: YES
        cookies_store: "user"
        setting:
          "Users can control this feature via the 'Cookies' setting under "
          "'Privacy, Site settings'. If cookies are disabled for a single "
          "site, serviceworkers are disabled for the site only. If they are "
          "totally disabled, all serviceworker requests will be stopped."
        chrome_policy {
          CookiesBlockedForUrls {
            CookiesBlockedForUrls: { entries: '*' }
          }
        }
        chrome_policy {
          CookiesAllowedForUrls {
            CookiesAllowedForUrls { }
          }
        }
        chrome_policy {
          DefaultCookiesSetting {
            DefaultCookiesSetting: 2
          }
        }
      }
)");

}  // namespace

ServiceWorkerNewScriptFetcher::ServiceWorkerNewScriptFetcher(
    ServiceWorkerContextCore& context,
    scoped_refptr<ServiceWorkerVersion> version,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    blink::mojom::FetchClientSettingsObjectPtr fetch_client_settings_object,
    const GlobalRenderFrameHostId& requesting_frame_id)
    : context_(context),
      version_(std::move(version)),
      loader_factory_(std::move(loader_factory)),
      fetch_client_settings_object_(std::move(fetch_client_settings_object)),
      request_id_(GlobalRequestID::MakeBrowserInitiated().request_id),
      requesting_frame_id_(requesting_frame_id) {}

ServiceWorkerNewScriptFetcher::~ServiceWorkerNewScriptFetcher() = default;

void ServiceWorkerNewScriptFetcher::Start(StartCallback callback) {
  callback_ = std::move(callback);

  context_->GetStorageControl()->GetNewResourceId(base::BindOnce(
      &ServiceWorkerNewScriptFetcher::StartScriptLoadingWithNewResourceID,
      weak_factory_.GetWeakPtr()));
}

void ServiceWorkerNewScriptFetcher::StartScriptLoadingWithNewResourceID(
    int64_t resource_id) {
  BrowserContext* browser_context = context_->wrapper()->browser_context();
  if (!browser_context) {
    std::move(callback_).Run(/*main_script_load_params=*/nullptr);
    return;
  }
  network::ResourceRequest request =
      service_worker_loader_helpers::CreateRequestForServiceWorkerScript(
          version_->script_url(), version_->key(),
          /*is_main_script=*/true, version_->script_type(),
          *fetch_client_settings_object_, *browser_context);
  // Request SSLInfo. It will be persisted in service worker storage and may be
  // used by ServiceWorkerMainResourceLoader for navigations handled by this
  // service worker.
  uint32_t options = network::mojom::kURLLoadOptionSendSSLInfoWithResponse;

  // Notify to DevTools that the request for fetching the service worker script
  // is about to start. It fires `Network.onRequestWillBeSent` event.
  devtools_instrumentation::OnServiceWorkerMainScriptRequestWillBeSent(
      requesting_frame_id_, context_->wrapper(), version_->version_id(),
      request);

  mojo::MakeSelfOwnedReceiver(
      ServiceWorkerNewScriptLoader::CreateAndStart(
          request_id_, options, request,
          url_loader_client_receiver_.BindNewPipeAndPassRemote(),
          std::move(version_), std::move(loader_factory_),
          net::MutableNetworkTrafficAnnotationTag(
              kServiceWorkerScriptLoadTrafficAnnotation),
          resource_id, /*is_throttle_needed=*/true, requesting_frame_id_),
      url_loader_remote_.BindNewPipeAndPassReceiver());
}

void ServiceWorkerNewScriptFetcher::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {}

void ServiceWorkerNewScriptFetcher::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  if (!response_body)
    return;

  blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params =
      blink::mojom::WorkerMainScriptLoadParams::New();
  main_script_load_params->request_id = request_id_;
  main_script_load_params->response_head = std::move(response_head);
  main_script_load_params->response_body = std::move(response_body);
  main_script_load_params->url_loader_client_endpoints =
      network::mojom::URLLoaderClientEndpoints::New(
          url_loader_remote_.Unbind(), url_loader_client_receiver_.Unbind());

  std::move(callback_).Run(std::move(main_script_load_params));
}

void ServiceWorkerNewScriptFetcher::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr response_head) {
  // ServiceWorkerNewScriptFetcher doesn't receive redirects because
  // ServiceWorkerNewScriptLoader disallows it and completes the network request
  // with an error.
  url_loader_client_receiver_.ReportBadMessage("SWNSF_BAD_MSG");
}
void ServiceWorkerNewScriptFetcher::OnUploadProgress(int64_t,
                                                     int64_t,
                                                     OnUploadProgressCallback) {
  url_loader_client_receiver_.ReportBadMessage("SWNSF_BAD_MSG");
}
void ServiceWorkerNewScriptFetcher::OnTransferSizeUpdated(int32_t) {
  network::RecordOnTransferSizeUpdatedUMA(
      network::OnTransferSizeUpdatedFrom::kServiceWorkerNewScriptFetcher);

  url_loader_client_receiver_.ReportBadMessage("SWNSF_BAD_MSG");
}
void ServiceWorkerNewScriptFetcher::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  // OnComplete can be called only when loading fails before receiving the
  // header and the body.
  if (status.error_code == net::OK) {
    url_loader_client_receiver_.ReportBadMessage("SWNSF_BAD_OK");
    // Do not continue with further script processing, but let the |callback_|
    // hang. This renderer process would be killed soon anyways.
    return;
  }
  std::move(callback_).Run(/*main_script_load_params=*/nullptr);
}

}  // namespace content
