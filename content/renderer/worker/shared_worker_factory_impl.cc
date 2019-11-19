// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/worker/shared_worker_factory_impl.h"

#include "base/memory/ptr_util.h"
#include "content/renderer/worker/embedded_shared_worker_stub.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/common/loader/url_loader_factory_bundle.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom.h"

namespace content {

// static
void SharedWorkerFactoryImpl::Create(
    mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> receiver) {
  mojo::MakeSelfOwnedReceiver<blink::mojom::SharedWorkerFactory>(
      base::WrapUnique(new SharedWorkerFactoryImpl()), std::move(receiver));
}

SharedWorkerFactoryImpl::SharedWorkerFactoryImpl() {}

void SharedWorkerFactoryImpl::CreateSharedWorker(
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
        browser_interface_broker) {
  // Bound to the lifetime of the underlying blink::WebSharedWorker instance.
  new EmbeddedSharedWorkerStub(
      std::move(info), user_agent, pause_on_start, devtools_worker_token,
      *renderer_preferences, std::move(preference_watcher_receiver),
      std::move(content_settings), std::move(service_worker_provider_info),
      appcache_host_id.value_or(base::UnguessableToken()),
      std::move(main_script_load_params),
      std::move(subresource_loader_factories), std::move(controller_info),
      std::move(host), std::move(receiver), std::move(interface_provider),
      std::move(browser_interface_broker));
}

}  // namespace content
