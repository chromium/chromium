// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_LOADER_FACTORY_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_LOADER_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace content {

class ServiceWorkerCacheWriter;
class ServiceWorkerContextCore;
class ServiceWorkerProviderHost;

// Created per one running service worker for loading its scripts. This is kept
// alive while the WebServiceWorkerNetworkProvider in the renderer process is
// alive.
//
// This factory handles requests for scripts from service workers that were new
// (non-installed) when they started. For service workers that were already
// installed when they started, ServiceWorkerInstalledScriptsManager is used
// instead.
//
// This factory creates either a ServiceWorkerNewScriptLoader or a
// ServiceWorkerInstalledScriptLoader to load a script.
class CONTENT_EXPORT ServiceWorkerScriptLoaderFactory
    : public network::mojom::URLLoaderFactory {
 public:
  // |loader_factory_for_new_scripts| is used to load scripts. Typically
  // a new script will be loaded from the NetworkService. However,
  // |loader_factory_for_new_scripts| may internally contain non-NetworkService
  // factories used for non-http(s) URLs, e.g., a chrome-extension:// URL.
  //
  // |loader_factory_for_new_scripts| is null if this factory is created for an
  // installed service worker, which is expected to load its scripts via
  // ServiceWorkerInstalledScriptsManager, and only uses this factory for
  // loading non-installed scripts, in which case this factory returns network
  // error.
  ServiceWorkerScriptLoaderFactory(
      base::WeakPtr<ServiceWorkerContextCore> context,
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      scoped_refptr<network::SharedURLLoaderFactory>
          loader_factory_for_new_scripts);
  ~ServiceWorkerScriptLoaderFactory() override;

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& resource_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;

  void Update(scoped_refptr<network::SharedURLLoaderFactory> loader_factory);

 private:
  bool CheckIfScriptRequestIsValid(
      const network::ResourceRequest& resource_request);

  // Used only when ServiceWorkerImportedScriptUpdateCheck is enabled.
  //
  // The callback is called once the copy is done. It normally runs
  // asynchronously, and would be synchronous if the operation completes
  // synchronously. The first parameter of the callback is the new resource id
  // and the second parameter is the result of the operation. net::OK means
  // success.
  void CopyScript(const GURL& url,
                  int64_t resource_id,
                  base::OnceCallback<void(int64_t, net::Error)> callback);

  // This method is called to notify that the operation triggered by
  // CopyScript() completed.
  //
  // If the copy operation is successful, a ServiceWorkerInstalledScriptLoader
  // would be created to load the new copy.
  void OnCopyScriptFinished(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      uint32_t options,
      const network::ResourceRequest& resource_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      int64_t new_resource_id,
      net::Error error);

  base::WeakPtr<ServiceWorkerContextCore> context_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  // Can be null if this factory is for an installed service worker.
  scoped_refptr<network::SharedURLLoaderFactory>
      loader_factory_for_new_scripts_;

  mojo::ReceiverSet<network::mojom::URLLoaderFactory> receivers_;

  // Used to copy script started at CopyScript().
  std::unique_ptr<ServiceWorkerCacheWriter> cache_writer_;

  base::WeakPtrFactory<ServiceWorkerScriptLoaderFactory> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerScriptLoaderFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_LOADER_FACTORY_H_
