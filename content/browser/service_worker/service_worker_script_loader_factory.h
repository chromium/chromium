// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_LOADER_FACTORY_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_LOADER_FACTORY_H_

#include "base/macros.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerProviderHost;

// S13nServiceWorker:
// Created per one running service worker for loading its scripts. This is kept
// alive while ServiceWorkerNetworkProvider in the renderer process is alive.
//
// This factory handles requests for the scripts of a new (installing)
// service worker. For installed workers, service worker script streaming
// (ServiceWorkerInstalledScriptsSender) is typically used instead. However,
// this factory can still be used when an installed worker imports a
// non-installed script. In this case, this factory just returns a network
// error as the spec disallows it.
//
// This factory creates either a ServiceWorkerNewScriptLoader or a
// ServiceWorkerInstalledScriptLoader to load a script.
class CONTENT_EXPORT ServiceWorkerScriptLoaderFactory
    : public network::mojom::URLLoaderFactory {
 public:
  // |loader_factory| is used to load scripts. Typically
  // a new script will be loaded from the NetworkService. However,
  // |loader_factory| may internally contain non-NetworkService
  // factories used for non-http(s) URLs, e.g., a chrome-extension:// URL.
  ServiceWorkerScriptLoaderFactory(
      base::WeakPtr<ServiceWorkerContextCore> context,
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory);
  ~ServiceWorkerScriptLoaderFactory() override;

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& resource_request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(network::mojom::URLLoaderFactoryRequest request) override;

 private:
  bool CheckIfScriptRequestIsValid(
      const network::ResourceRequest& resource_request);

  base::WeakPtr<ServiceWorkerContextCore> context_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;

  mojo::BindingSet<network::mojom::URLLoaderFactory> bindings_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerScriptLoaderFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_LOADER_FACTORY_H_
