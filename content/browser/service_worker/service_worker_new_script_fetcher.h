// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_NEW_SCRIPT_FETCHER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_NEW_SCRIPT_FETCHER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom.h"
#include "third_party/blink/public/mojom/worker/worker_main_script_load_params.mojom.h"

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerVersion;

// PlzServiceWorker:
// Fetches the service worker's main script and creates
// blink::mojom::WorkerMainScriptLoadParams to start a worker.
//
// This is used only when the worker is newly installed and the byte-for-byte
// update checking is not invoked. The worker script needs to be loaded on the
// browser process before selecting a process for the worker thread because we
// need to know whether the worker thread needs to be in the COOP-COEP protected
// process or not.
//
// Internally, this class creates ServiceWorkerNewScriptLoader to start loading
// the script. After receiving the response header and the Mojo data pipe for
// the body, the Mojo endpoints owned by this instance gets unbound and passed
// to the struct blink::mojom::WorkerMainScriptLoadParams along with the
// response header and the Mojo data pipe so that the renderer takes over
// ownership of ServiceWorkerNewScriptLoader.
class CONTENT_EXPORT ServiceWorkerNewScriptFetcher
    : public network::mojom::URLLoaderClient {
 public:
  ServiceWorkerNewScriptFetcher(
      ServiceWorkerContextCore& context,
      scoped_refptr<ServiceWorkerVersion> version,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      blink::mojom::FetchClientSettingsObjectPtr fetch_client_settings_object,
      const GlobalRenderFrameHostId& requesting_frame_id);
  ~ServiceWorkerNewScriptFetcher() override;

  // Callback called when the main script load params are ready. Called with
  // null struct on an error.
  using StartCallback =
      base::OnceCallback<void(blink::mojom::WorkerMainScriptLoadParamsPtr)>;
  void Start(StartCallback callback);

 private:
  void StartScriptLoadingWithNewResourceID(int64_t resource_id);

  // network::mojom::URLLoaderClient
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr response_head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  const raw_ref<ServiceWorkerContextCore> context_;
  scoped_refptr<ServiceWorkerVersion> version_;
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;
  blink::mojom::FetchClientSettingsObjectPtr fetch_client_settings_object_;
  // Request ID for a browser-initiated request.
  const int request_id_;

  // Called when the header and the data pipe for the body are ready.
  StartCallback callback_;

  // The global routing id of the frame that made the SW registration request.
  // Note: This is only valid with PlzServiceWorker.
  const GlobalRenderFrameHostId requesting_frame_id_;

  // Mojo endpoint connected to ServiceWorkerNewScriptLoader.
  mojo::Remote<network::mojom::URLLoader> url_loader_remote_;

  mojo::Receiver<network::mojom::URLLoaderClient> url_loader_client_receiver_{
      this};

  base::WeakPtrFactory<ServiceWorkerNewScriptFetcher> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_NEW_SCRIPT_FETCHER_H_
