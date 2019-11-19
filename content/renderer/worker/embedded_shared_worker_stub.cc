// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/worker/embedded_shared_worker_stub.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "content/public/common/network_service_util.h"
#include "content/renderer/loader/child_url_loader_factory_bundle.h"
#include "content/renderer/loader/navigation_response_override_parameters.h"
#include "content/renderer/loader/web_worker_fetch_context_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/url_loader_factory_bundle.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_shared_worker.h"
#include "url/origin.h"

namespace content {

EmbeddedSharedWorkerStub::EmbeddedSharedWorkerStub(
    blink::mojom::SharedWorkerInfoPtr info,
    const std::string& user_agent,
    bool pause_on_start,
    const base::UnguessableToken& devtools_worker_token,
    const blink::mojom::RendererPreferences& renderer_preferences,
    mojo::PendingReceiver<blink::mojom::RendererPreferenceWatcher>
        preference_watcher_receiver,
    mojo::PendingRemote<blink::mojom::WorkerContentSettingsProxy>
        content_settings,
    blink::mojom::ServiceWorkerProviderInfoForClientPtr
        service_worker_provider_info,
    const base::UnguessableToken& appcache_host_id,
    blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
    std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
        subresource_loader_factory_bundle_info,
    blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
    mojo::PendingRemote<blink::mojom::SharedWorkerHost> host,
    mojo::PendingReceiver<blink::mojom::SharedWorker> receiver,
    service_manager::mojom::InterfaceProviderPtr interface_provider,
    mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
        browser_interface_broker)
    : receiver_(this, std::move(receiver)),
      host_(std::move(host)),
      url_(info->url),
      renderer_preferences_(renderer_preferences),
      preference_watcher_receiver_(std::move(preference_watcher_receiver)) {
  DCHECK(main_script_load_params);
  DCHECK(subresource_loader_factory_bundle_info);

  // Initialize the response override for the main worker script loaded by the
  // browser process.
  response_override_ = std::make_unique<NavigationResponseOverrideParameters>();
  response_override_->url_loader_client_endpoints =
      std::move(main_script_load_params->url_loader_client_endpoints);
  response_override_->response_head =
      std::move(main_script_load_params->response_head);
  response_override_->response_body =
      std::move(main_script_load_params->response_body);
  response_override_->redirect_responses =
      std::move(main_script_load_params->redirect_response_heads);
  response_override_->redirect_infos = main_script_load_params->redirect_infos;

  // If the network service crashes, then self-destruct so clients don't get
  // stuck with a worker with a broken loader. Self-destruction is effectively
  // the same as the worker's process crashing.
  if (IsOutOfProcessNetworkService()) {
    default_factory_disconnect_handler_holder_.Bind(std::move(
        subresource_loader_factory_bundle_info->pending_default_factory()));
    default_factory_disconnect_handler_holder_->Clone(
        subresource_loader_factory_bundle_info->pending_default_factory()
            .InitWithNewPipeAndPassReceiver());
    default_factory_disconnect_handler_holder_.set_disconnect_handler(
        base::BindOnce(&EmbeddedSharedWorkerStub::Terminate,
                       base::Unretained(this)));
  }

  // Initialize the subresource loader factory bundle passed by the browser
  // process.
  subresource_loader_factory_bundle_ =
      base::MakeRefCounted<ChildURLLoaderFactoryBundle>(
          std::make_unique<ChildURLLoaderFactoryBundleInfo>(
              std::move(subresource_loader_factory_bundle_info)));

  if (service_worker_provider_info) {
    service_worker_provider_context_ =
        base::MakeRefCounted<ServiceWorkerProviderContext>(
            blink::mojom::ServiceWorkerProviderType::kForDedicatedWorker,
            std::move(service_worker_provider_info->client_receiver),
            std::move(service_worker_provider_info->host_remote),
            std::move(controller_info), subresource_loader_factory_bundle_);
  }

  impl_ = blink::WebSharedWorker::Create(this);
  impl_->StartWorkerContext(
      url_, blink::WebString::FromUTF8(info->name),
      blink::WebString::FromUTF8(user_agent),
      blink::WebString::FromUTF8(info->content_security_policy),
      info->content_security_policy_type, info->creation_address_space,
      appcache_host_id, devtools_worker_token, content_settings.PassPipe(),
      interface_provider.PassInterface().PassHandle(),
      browser_interface_broker.PassPipe(), pause_on_start);

  // If the host drops its connection, then self-destruct.
  receiver_.set_disconnect_handler(base::BindOnce(
      &EmbeddedSharedWorkerStub::Terminate, base::Unretained(this)));
}

EmbeddedSharedWorkerStub::~EmbeddedSharedWorkerStub() {
  // Destruction closes our connection to the host, triggering the host to
  // cleanup and notify clients of this worker going away.
}

void EmbeddedSharedWorkerStub::WorkerReadyForInspection(
    mojo::ScopedMessagePipeHandle devtools_agent_remote_handle,
    mojo::ScopedMessagePipeHandle devtools_agent_host_receiver_handle) {
  mojo::PendingRemote<blink::mojom::DevToolsAgent> remote(
      std::move(devtools_agent_remote_handle),
      blink::mojom::DevToolsAgent::Version_);
  mojo::PendingReceiver<blink::mojom::DevToolsAgentHost> receiver(
      std::move(devtools_agent_host_receiver_handle));
  host_->OnReadyForInspection(std::move(remote), std::move(receiver));
}

void EmbeddedSharedWorkerStub::WorkerScriptLoadFailed() {
  host_->OnScriptLoadFailed();
  pending_channels_.clear();
}

void EmbeddedSharedWorkerStub::WorkerScriptEvaluated(bool success) {
  DCHECK(!running_);
  running_ = true;
  // Process any pending connections.
  for (auto& item : pending_channels_)
    ConnectToChannel(item.first, std::move(item.second));
  pending_channels_.clear();
}

void EmbeddedSharedWorkerStub::CountFeature(blink::mojom::WebFeature feature) {
  host_->OnFeatureUsed(feature);
}

void EmbeddedSharedWorkerStub::WorkerContextClosed() {
  host_->OnContextClosed();
}

void EmbeddedSharedWorkerStub::WorkerContextDestroyed() {
  delete this;
}

scoped_refptr<blink::WebWorkerFetchContext>
EmbeddedSharedWorkerStub::CreateWorkerFetchContext() {
  // Make the factory used for service worker network fallback (that should
  // skip AppCache if it is provided).
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> fallback_factory =
      subresource_loader_factory_bundle_->CloneWithoutAppCacheFactory();

  // |pending_subresource_loader_updater| is not used for shared workers.
  scoped_refptr<WebWorkerFetchContextImpl> worker_fetch_context =
      WebWorkerFetchContextImpl::Create(
          service_worker_provider_context_.get(),
          std::move(renderer_preferences_),
          std::move(preference_watcher_receiver_),
          subresource_loader_factory_bundle_->Clone(),
          std::move(fallback_factory),
          /*pending_subresource_loader_updater*/ mojo::NullReceiver());

  // TODO(horo): To get the correct first_party_to_cookies for the shared
  // worker, we need to check the all documents bounded by the shared worker.
  // (crbug.com/723553)
  // https://tools.ietf.org/html/draft-ietf-httpbis-cookie-same-site-07#section-2.1.2
  worker_fetch_context->set_site_for_cookies(url_);
  worker_fetch_context->set_origin_url(url_.GetOrigin());

  DCHECK(response_override_);
  worker_fetch_context->SetResponseOverrideForMainScript(
      std::move(response_override_));

  return worker_fetch_context;
}

void EmbeddedSharedWorkerStub::ConnectToChannel(
    int connection_request_id,
    blink::MessagePortChannel channel) {
  impl_->Connect(std::move(channel));
  host_->OnConnected(connection_request_id);
}

void EmbeddedSharedWorkerStub::Connect(int connection_request_id,
                                       mojo::ScopedMessagePipeHandle port) {
  blink::MessagePortChannel channel(std::move(port));
  if (running_) {
    ConnectToChannel(connection_request_id, std::move(channel));
  } else {
    // If two documents try to load a SharedWorker at the same time, the
    // mojom::SharedWorker::Connect() for one of the documents can come in
    // before the worker is started. Just queue up the connect and deliver it
    // once the worker starts.
    pending_channels_.emplace_back(connection_request_id, std::move(channel));
  }
}

void EmbeddedSharedWorkerStub::Terminate() {
  // After this we should ignore any IPC for this stub.
  running_ = false;
  impl_->TerminateWorkerContext();
}

}  // namespace content
