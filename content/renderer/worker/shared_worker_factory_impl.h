// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WORKER_SHARED_WORKER_FACTORY_IMPL_H_
#define CONTENT_RENDERER_WORKER_SHARED_WORKER_FACTORY_IMPL_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
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

  SharedWorkerFactoryImpl(const SharedWorkerFactoryImpl&) = delete;
  SharedWorkerFactoryImpl& operator=(const SharedWorkerFactoryImpl&) = delete;

 private:
  SharedWorkerFactoryImpl();

  // mojom::SharedWorkerFactory methods:
  void CreateSharedWorker(
      blink::mojom::SharedWorkerInfoPtr info,
      const blink::SharedWorkerToken& token,
      const blink::StorageKey& constructor_key,
      const url::Origin& origin,
      bool is_constructor_secure_context,
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
      blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          subresource_loader_factories,
      blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
      blink::mojom::PolicyContainerPtr policy_container,
      mojo::PendingRemote<blink::mojom::SharedWorkerHost> host,
      mojo::PendingReceiver<blink::mojom::SharedWorker> receiver,
      mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
          browser_interface_broker,
      ukm::SourceId ukm_source_id,
      bool require_cross_site_request_for_cookies) override;
};

}  // namespace content

#endif  // CONTENT_RENDERER_WORKER_SHARED_WORKER_FACTORY_IMPL_H_
