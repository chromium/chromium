// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WORKER_EMBEDDED_SHARED_WORKER_STUB_H_
#define CONTENT_RENDERER_WORKER_EMBEDDED_SHARED_WORKER_STUB_H_

#include <memory>
#include <vector>

#include "base/unguessable_token.h"
#include "content/renderer/service_worker/service_worker_provider_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom-forward.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/policy_container.mojom.h"
#include "third_party/blink/public/mojom/renderer_preference_watcher.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom-forward.h"
#include "third_party/blink/public/mojom/worker/shared_worker.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_host.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_info.mojom.h"
#include "third_party/blink/public/mojom/worker/worker_content_settings_proxy.mojom-forward.h"
#include "third_party/blink/public/mojom/worker/worker_main_script_load_params.mojom.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/web/web_shared_worker_client.h"
#include "url/gurl.h"

namespace blink {
class ChildURLLoaderFactoryBundle;
class WebSharedWorker;
}  // namespace blink

namespace blink {
class MessagePortDescriptor;
class PendingURLLoaderFactoryBundle;
}  // namespace blink

namespace content {

// A stub class to receive IPC from browser process and talk to
// blink::WebSharedWorker. Implements blink::WebSharedWorkerClient.
// This class is self-destructed (no one explicitly owns this). It deletes
// itself when WorkerContextDestroyed() is called by blink::WebSharedWorker.
//
// This class owns blink::WebSharedWorker.
class EmbeddedSharedWorkerStub : public blink::WebSharedWorkerClient,
                                 public blink::mojom::SharedWorker {
 public:
  EmbeddedSharedWorkerStub(
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
          pending_subresource_loader_factory_bundle,
      blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
      blink::mojom::PolicyContainerPtr policy_container,
      mojo::PendingRemote<blink::mojom::SharedWorkerHost> host,
      mojo::PendingReceiver<blink::mojom::SharedWorker> receiver,
      mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
          browser_interface_broker,
      ukm::SourceId ukm_source_id,
      bool require_cross_site_request_for_cookies,
      const std::vector<std::string>& cors_exempt_header_list);

  EmbeddedSharedWorkerStub(const EmbeddedSharedWorkerStub&) = delete;
  EmbeddedSharedWorkerStub& operator=(const EmbeddedSharedWorkerStub&) = delete;

  ~EmbeddedSharedWorkerStub() override;

  // blink::WebSharedWorkerClient implementation.
  void WorkerContextDestroyed() override;

 private:
  // mojom::SharedWorker methods:
  // TODO(nhiroki): Move these implementation into blink::WebSharedWorkerImpl.
  void Connect(int connection_request_id,
               blink::MessagePortDescriptor port) override;
  void Terminate() override;

  scoped_refptr<blink::WebWorkerFetchContext> CreateWorkerFetchContext(
      const blink::StorageKey& constructor_key,
      const blink::RendererPreferences& renderer_preferences,
      mojo::PendingReceiver<blink::mojom::RendererPreferenceWatcher>
          preference_watcher_receiver,
      const std::vector<std::string>& cors_exempt_header_list,
      bool require_cross_site_request_for_cookies);

  mojo::Receiver<blink::mojom::SharedWorker> receiver_;
  std::unique_ptr<blink::WebSharedWorker> impl_;

  scoped_refptr<ServiceWorkerProviderContext> service_worker_provider_context_;

  // The factory bundle used for loading subresources for this shared worker.
  scoped_refptr<blink::ChildURLLoaderFactoryBundle>
      subresource_loader_factory_bundle_;

  // Out-of-process NetworkService:
  // Detects disconnection from the default factory of the loader factory bundle
  // used by this worker (typically the network service).
  mojo::Remote<network::mojom::URLLoaderFactory>
      default_factory_disconnect_handler_holder_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_WORKER_EMBEDDED_SHARED_WORKER_STUB_H_
