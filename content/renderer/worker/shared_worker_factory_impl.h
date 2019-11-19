// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WORKER_SHARED_WORKER_FACTORY_IMPL_H_
#define CONTENT_RENDERER_WORKER_SHARED_WORKER_FACTORY_IMPL_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_factory.mojom.h"

namespace blink {
class URLLoaderFactoryBundleInfo;
}  // namespace blink

namespace content {

class SharedWorkerFactoryImpl : public blink::mojom::SharedWorkerFactory {
 public:
  static void Create(
      mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> receiver);

 private:
  SharedWorkerFactoryImpl();

  // mojom::SharedWorkerFactory methods:
  void CreateSharedWorker(
      blink::mojom::SharedWorkerInfoPtr info,
      const std::string& user_agent,
      bool pause_on_start,
      const base::UnguessableToken& devtools_worker_token,
      blink::mojom::RendererPreferencesPtr renderer_preferences,
      mojo::PendingReceiver<blink::mojom::RendererPreferenceWatcher>
          preference_watcher_receiver,
      mojo::PendingRemote<blink::mojom::WorkerContentSettingsProxy>
          content_settings,
      blink::mojom::ServiceWorkerProviderInfoForClientPtr
          service_worker_provider_info,
      const base::Optional<base::UnguessableToken>& appcache_host_id,
      blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
      std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
          subresource_loader_factories,
      blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
      mojo::PendingRemote<blink::mojom::SharedWorkerHost> host,
      mojo::PendingReceiver<blink::mojom::SharedWorker> receiver,
      service_manager::mojom::InterfaceProviderPtr interface_provider,
      mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
          browser_interface_broker) override;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerFactoryImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_WORKER_SHARED_WORKER_FACTORY_IMPL_H_
