// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WORKER_SHARED_WORKER_FACTORY_IMPL_H_
#define CONTENT_RENDERER_WORKER_SHARED_WORKER_FACTORY_IMPL_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom-forward.h"
#include "third_party/blink/public/mojom/worker/shared_worker_factory.mojom.h"

namespace blink {
class PendingURLLoaderFactoryBundle;
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
      const blink::SharedWorkerToken& token,
      const url::Origin& constructor_origin,
      const std::string& user_agent,
      const blink::UserAgentMetadata& ua_metadata,
      bool pause_on_start,
      const base::UnguessableToken& devtools_worker_token,
      const blink::RendererPreferences& renderer_preferences,
      mojo::PendingReceiver<blink::mojom::RendererPreferenceWatcher>
          preference_watcher_receiver,
      mojo::PendingRemote<blink::mojom::WorkerContentSettingsProxy>
          content_settings,
      blink::mojom::ServiceWorkerContainerInfoForClientPtr
          service_worker_container_info,
      const base::Optional<base::UnguessableToken>& appcache_host_id,
      blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          subresource_loader_factories,
      blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
      mojo::PendingRemote<blink::mojom::SharedWorkerHost> host,
      mojo::PendingReceiver<blink::mojom::SharedWorker> receiver,
      mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
          browser_interface_broker,
      ukm::SourceId ukm_source_id) override;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerFactoryImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_WORKER_SHARED_WORKER_FACTORY_IMPL_H_
