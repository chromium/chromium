// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/worker/dedicated_worker_host_factory_client.h"

#include <utility>
#include "content/renderer/loader/child_url_loader_factory_bundle.h"
#include "content/renderer/loader/navigation_response_override_parameters.h"
#include "content/renderer/loader/web_worker_fetch_context_impl.h"
#include "content/renderer/service_worker/service_worker_provider_context.h"
#include "content/renderer/worker/fetch_client_settings_object_helpers.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/mojom/worker/worker_main_script_load_params.mojom.h"
#include "third_party/blink/public/platform/web_dedicated_worker.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"

namespace content {

DedicatedWorkerHostFactoryClient::DedicatedWorkerHostFactoryClient(
    blink::WebDedicatedWorker* worker,
    service_manager::InterfaceProvider* interface_provider)
    : worker_(worker) {
  DCHECK(interface_provider);
  interface_provider->GetInterface(factory_.BindNewPipeAndPassReceiver());
}

DedicatedWorkerHostFactoryClient::~DedicatedWorkerHostFactoryClient() = default;

void DedicatedWorkerHostFactoryClient::CreateWorkerHostDeprecated(
    const blink::WebSecurityOrigin& script_origin) {
  DCHECK(!base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));
  service_manager::mojom::InterfaceProviderPtr interface_provider_ptr;
  mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
      browser_interface_broker;
  factory_->CreateWorkerHost(
      script_origin, mojo::MakeRequest(&interface_provider_ptr),
      browser_interface_broker.InitWithNewPipeAndPassReceiver(),
      remote_host_.BindNewPipeAndPassReceiver());
  OnWorkerHostCreated(std::move(interface_provider_ptr),
                      std::move(browser_interface_broker));
}

void DedicatedWorkerHostFactoryClient::CreateWorkerHost(
    const blink::WebURL& script_url,
    const blink::WebSecurityOrigin& script_origin,
    network::mojom::CredentialsMode credentials_mode,
    const blink::WebSecurityOrigin& fetch_client_security_origin,
    const blink::WebFetchClientSettingsObject& fetch_client_settings_object,
    mojo::ScopedMessagePipeHandle blob_url_token) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));

  factory_->CreateWorkerHostAndStartScriptLoad(
      script_url, script_origin, credentials_mode,
      FetchClientSettingsObjectFromWebToMojom(fetch_client_settings_object),
      mojo::PendingRemote<blink::mojom::BlobURLToken>(
          std::move(blob_url_token), blink::mojom::BlobURLToken::Version_),
      receiver_.BindNewPipeAndPassRemote(),
      remote_host_.BindNewPipeAndPassReceiver());
}

scoped_refptr<blink::WebWorkerFetchContext>
DedicatedWorkerHostFactoryClient::CloneWorkerFetchContext(
    blink::WebWorkerFetchContext* web_worker_fetch_context,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  scoped_refptr<WebWorkerFetchContextImpl> worker_fetch_context;
  if (base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker)) {
    worker_fetch_context =
        static_cast<WebWorkerFetchContextImpl*>(web_worker_fetch_context)
            ->CloneForNestedWorker(
                service_worker_provider_context_.get(),
                subresource_loader_factory_bundle_->Clone(),
                subresource_loader_factory_bundle_
                    ->CloneWithoutAppCacheFactory(),
                std::move(pending_subresource_loader_updater_),
                std::move(task_runner));
    worker_fetch_context->SetResponseOverrideForMainScript(
        std::move(response_override_for_main_script_));
  } else {
    worker_fetch_context =
        static_cast<WebWorkerFetchContextImpl*>(web_worker_fetch_context)
            ->CloneForNestedWorkerDeprecated(std::move(task_runner));
  }
  return worker_fetch_context;
}

void DedicatedWorkerHostFactoryClient::LifecycleStateChanged(
    blink::mojom::FrameLifecycleState state) {
  if (remote_host_)
    remote_host_->LifecycleStateChanged(state);
}

scoped_refptr<WebWorkerFetchContextImpl>
DedicatedWorkerHostFactoryClient::CreateWorkerFetchContext(
    blink::mojom::RendererPreferences renderer_preference,
    mojo::PendingReceiver<blink::mojom::RendererPreferenceWatcher>
        watcher_receiver) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));
  DCHECK(subresource_loader_factory_bundle_);
  scoped_refptr<WebWorkerFetchContextImpl> worker_fetch_context =
      WebWorkerFetchContextImpl::Create(
          service_worker_provider_context_.get(),
          std::move(renderer_preference), std::move(watcher_receiver),
          subresource_loader_factory_bundle_->Clone(),
          subresource_loader_factory_bundle_->CloneWithoutAppCacheFactory(),
          std::move(pending_subresource_loader_updater_));
  worker_fetch_context->SetResponseOverrideForMainScript(
      std::move(response_override_for_main_script_));
  return worker_fetch_context;
}

void DedicatedWorkerHostFactoryClient::OnWorkerHostCreated(
    service_manager::mojom::InterfaceProviderPtr interface_provider,
    mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
        browser_interface_broker) {
  worker_->OnWorkerHostCreated(interface_provider.PassInterface().PassHandle(),
                               browser_interface_broker.PassPipe());
}

void DedicatedWorkerHostFactoryClient::OnScriptLoadStarted(
    blink::mojom::ServiceWorkerProviderInfoForClientPtr
        service_worker_provider_info,
    blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
    std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
        subresource_loader_factory_bundle_info,
    mojo::PendingReceiver<blink::mojom::SubresourceLoaderUpdater>
        subresource_loader_updater,
    blink::mojom::ControllerServiceWorkerInfoPtr controller_info) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));
  DCHECK(main_script_load_params);
  DCHECK(subresource_loader_factory_bundle_info);

  // Initialize the loader factory bundle passed by the browser process.
  DCHECK(!subresource_loader_factory_bundle_);
  subresource_loader_factory_bundle_ =
      base::MakeRefCounted<ChildURLLoaderFactoryBundle>(
          std::make_unique<ChildURLLoaderFactoryBundleInfo>(
              std::move(subresource_loader_factory_bundle_info)));

  DCHECK(!pending_subresource_loader_updater_);
  pending_subresource_loader_updater_ = std::move(subresource_loader_updater);

  DCHECK(!service_worker_provider_context_);
  if (service_worker_provider_info) {
    service_worker_provider_context_ =
        base::MakeRefCounted<ServiceWorkerProviderContext>(
            blink::mojom::ServiceWorkerProviderType::kForDedicatedWorker,
            std::move(service_worker_provider_info->client_receiver),
            std::move(service_worker_provider_info->host_remote),
            std::move(controller_info), subresource_loader_factory_bundle_);
  }

  // Initialize the response override for the main worker script loaded by the
  // browser process.
  DCHECK(!response_override_for_main_script_);
  response_override_for_main_script_ =
      std::make_unique<NavigationResponseOverrideParameters>();
  response_override_for_main_script_->url_loader_client_endpoints =
      std::move(main_script_load_params->url_loader_client_endpoints);
  response_override_for_main_script_->response_head =
      std::move(main_script_load_params->response_head);
  response_override_for_main_script_->response_body =
      std::move(main_script_load_params->response_body);
  response_override_for_main_script_->redirect_responses =
      std::move(main_script_load_params->redirect_response_heads);
  response_override_for_main_script_->redirect_infos =
      main_script_load_params->redirect_infos;

  worker_->OnScriptLoadStarted();
}

void DedicatedWorkerHostFactoryClient::OnScriptLoadStartFailed() {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));
  worker_->OnScriptLoadStartFailed();
  // |this| may be destroyed at this point.
}

}  // namespace content
