// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_provider_context.h"

#include <set>
#include <utility>
#include <vector>

#include "base/containers/cxx20_erase.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_split.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "content/public/common/content_features.h"
#include "content/renderer/service_worker/controller_service_worker_connector.h"
#include "content/renderer/service_worker/service_worker_subresource_loader.h"
#include "content/renderer/service_worker/web_service_worker_provider_impl.h"
#include "content/renderer/worker/worker_thread_registry.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom-shared.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_worker_client.mojom.h"

namespace content {

namespace {

void CreateSubresourceLoaderFactoryForProviderContext(
    mojo::PendingRemote<blink::mojom::ServiceWorkerContainerHost>
        remote_container_host,
    mojo::PendingRemote<blink::mojom::ControllerServiceWorker>
        remote_controller,
    const std::string& client_id,
    blink::mojom::ServiceWorkerFetchHandlerBypassOption
        fetch_handler_bypass_option,
    absl::optional<blink::ServiceWorkerRouterRules> router_rules,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_fallback_factory,
    mojo::PendingReceiver<blink::mojom::ControllerServiceWorkerConnector>
        connector_receiver,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  auto connector = base::MakeRefCounted<ControllerServiceWorkerConnector>(
      std::move(remote_container_host), std::move(remote_controller), client_id,
      fetch_handler_bypass_option, router_rules);
  connector->AddBinding(std::move(connector_receiver));
  ServiceWorkerSubresourceLoaderFactory::Create(
      std::move(connector),
      network::SharedURLLoaderFactory::Create(
          std::move(pending_fallback_factory)),
      std::move(receiver), std::move(task_runner));
}

// Returns the set of hash strings of fetch handlers which can be bypassed.
const base::flat_set<std::string> FetchHandlerBypassedHashStrings() {
  const static base::NoDestructor<base::flat_set<std::string>> result(
      base::SplitString(
          features::kServiceWorkerBypassFetchHandlerBypassedHashStrings.Get(),
          ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY));

  return *result;
}

bool ShouldBypassFetchHandlerForSubresource(
    absl::optional<std::string> sha256_script_checksum) {
  if (!base::FeatureList::IsEnabled(
          features::kServiceWorkerBypassFetchHandler)) {
    return false;
  }
  if (features::kServiceWorkerBypassFetchHandlerTarget.Get() !=
      features::ServiceWorkerBypassFetchHandlerTarget::kSubResource) {
    return false;
  }

  switch (features::kServiceWorkerBypassFetchHandlerStrategy.Get()) {
    case features::ServiceWorkerBypassFetchHandlerStrategy::kFeatureOptIn:
      return true;
    case features::ServiceWorkerBypassFetchHandlerStrategy::kAllowList:
      return FetchHandlerBypassedHashStrings().contains(sha256_script_checksum);
  }
}

}  // namespace

ServiceWorkerProviderContext::ServiceWorkerProviderContext(
    blink::mojom::ServiceWorkerContainerType container_type,
    mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainer>
        receiver,
    mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainerHost>
        host_remote,
    blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
    scoped_refptr<network::SharedURLLoaderFactory> fallback_loader_factory)
    : container_type_(container_type),
      main_thread_task_runner_(
          base::SingleThreadTaskRunner::GetCurrentDefault()),
      receiver_(this, std::move(receiver)),
      fallback_loader_factory_(std::move(fallback_loader_factory)) {
  if (host_remote.is_valid())
    container_host_.Bind(std::move(host_remote));

  // Set up the URL loader factory for sending subresource requests to
  // the controller.
  if (controller_info) {
    SetController(std::move(controller_info),
                  false /* should_notify_controllerchange */);
  }
}

ServiceWorkerProviderContext::~ServiceWorkerProviderContext() {
  if (weak_wrapped_subresource_loader_factory_)
    weak_wrapped_subresource_loader_factory_->Detach();
}

blink::mojom::ServiceWorkerObjectInfoPtr
ServiceWorkerProviderContext::TakeController() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  return std::move(controller_);
}

int64_t ServiceWorkerProviderContext::GetControllerVersionId() const {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  return controller_version_id_;
}

network::mojom::URLLoaderFactory*
ServiceWorkerProviderContext::GetSubresourceLoaderFactoryInternal() {
  if (!remote_controller_ && !controller_connector_) {
    // No controller is attached.
    return nullptr;
  }

  if (controller_mode_ !=
      blink::mojom::ControllerServiceWorkerMode::kControlled) {
    // The controller does not exist or has no fetch event handler.
    return nullptr;
  }

  if (effective_fetch_handler_type_ !=
      blink::mojom::ServiceWorkerFetchHandlerType::kNotSkippable) {
    // The fetch handler can be skipped.  The service worker process should
    // not be ready for this case.
    CountFeature(
        blink::mojom::WebFeature::kServiceWorkerSkippedForSubresourceLoad);
    return nullptr;
  }

  if (fetch_handler_bypass_option_ ==
      blink::mojom::ServiceWorkerFetchHandlerBypassOption::
          kBypassOnlyIfServiceWorkerNotStarted) {
    // If the fetch handler for the main resource is skipped by
    // ServiceWorkerBypassFetchHandler, the fetch handler doesn't handle
    // subresources too.
    return nullptr;
  }

  if (ShouldBypassFetchHandlerForSubresource(sha256_script_checksum_)) {
    CountFeature(blink::mojom::WebFeature::
                     kServiceWorkerBypassFetchHandlerForSubResource);
    return nullptr;
  }

  if (!subresource_loader_factory_) {
    DCHECK(!controller_connector_);
    DCHECK(remote_controller_);

    mojo::PendingRemote<blink::mojom::ServiceWorkerContainerHost>
        remote_container_host = CloneRemoteContainerHost();
    if (!remote_container_host)
      return nullptr;

    // Create a SubresourceLoaderFactory on a background thread to avoid
    // extra contention on the main thread.
    auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&CreateSubresourceLoaderFactoryForProviderContext,
                       std::move(remote_container_host),
                       std::move(remote_controller_), client_id_,
                       fetch_handler_bypass_option_, router_rules_,
                       fallback_loader_factory_->Clone(),
                       controller_connector_.BindNewPipeAndPassReceiver(),
                       subresource_loader_factory_.BindNewPipeAndPassReceiver(),
                       task_runner));

    DCHECK(!weak_wrapped_subresource_loader_factory_);
    weak_wrapped_subresource_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            subresource_loader_factory_.get());
  }
  return subresource_loader_factory_.get();
}

scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
ServiceWorkerProviderContext::GetSubresourceLoaderFactory() {
  // If we can't get our internal factory it means the state is not currently
  // good to process new requests regardless of the presence of an existing
  // weak_wrapped_subresource_loader_factory.
  if (!GetSubresourceLoaderFactoryInternal()) {
    return nullptr;
  }

  return weak_wrapped_subresource_loader_factory_;
}

blink::mojom::ServiceWorkerContainerHost*
ServiceWorkerProviderContext::container_host() const {
  DCHECK_EQ(blink::mojom::ServiceWorkerContainerType::kForWindow,
            container_type_);
  return container_host_ ? container_host_.get() : nullptr;
}

const std::set<blink::mojom::WebFeature>&
ServiceWorkerProviderContext::used_features() const {
  return used_features_;
}

const base::UnguessableToken&
ServiceWorkerProviderContext::fetch_request_window_id() const {
  return fetch_request_window_id_;
}

void ServiceWorkerProviderContext::SetWebServiceWorkerProvider(
    base::WeakPtr<WebServiceWorkerProviderImpl> provider) {
  web_service_worker_provider_ = std::move(provider);
}

void ServiceWorkerProviderContext::RegisterWorkerClient(
    mojo::PendingRemote<blink::mojom::ServiceWorkerWorkerClient>
        pending_client) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  mojo::Remote<blink::mojom::ServiceWorkerWorkerClient> client(
      std::move(pending_client));
  client.set_disconnect_handler(base::BindOnce(
      &ServiceWorkerProviderContext::UnregisterWorkerFetchContext,
      base::Unretained(this), client.get()));
  worker_clients_.push_back(std::move(client));
}

void ServiceWorkerProviderContext::CloneWorkerClientRegistry(
    mojo::PendingReceiver<blink::mojom::ServiceWorkerWorkerClientRegistry>
        receiver) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  worker_client_registry_receivers_.Add(this, std::move(receiver));
}

void ServiceWorkerProviderContext::OnNetworkProviderDestroyed() {
  container_host_.reset();
}

void ServiceWorkerProviderContext::DispatchNetworkQuiet() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());

  if (controller_mode_ ==
      blink::mojom::ControllerServiceWorkerMode::kNoController) {
    return;
  }

  if (!container_host_)
    return;

  container_host_->HintToUpdateServiceWorker();
}

void ServiceWorkerProviderContext::NotifyExecutionReady() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(container_type(),
            blink::mojom::ServiceWorkerContainerType::kForWindow)
      << "only windows need to send this message; shared workers have "
         "execution ready set on the browser-side when the response is "
         "committed";
  if (!container_host_)
    return;
  if (sent_execution_ready_) {
    // Sometimes a new document can be created for a frame without a proper
    // navigation, in cases like about:blank and javascript: URLs. In these
    // cases the provider is not recreated and Blink can tell us that it's
    // execution ready more than once. The browser-side host doesn't support
    // changing the URL of the provider in these cases, so just ignore these
    // notifications.
    return;
  }
  sent_execution_ready_ = true;
  container_host_->OnExecutionReady();
}

void ServiceWorkerProviderContext::BindServiceWorkerWorkerClientRemote(
    blink::CrossVariantMojoRemote<
        blink::mojom::ServiceWorkerWorkerClientInterfaceBase> pending_client) {
  RegisterWorkerClient(std::move(pending_client));
}

void ServiceWorkerProviderContext::
    BindServiceWorkerWorkerClientRegistryReceiver(
        blink::CrossVariantMojoReceiver<
            blink::mojom::ServiceWorkerWorkerClientRegistryInterfaceBase>
            receiver) {
  CloneWorkerClientRegistry(std::move(receiver));
}

blink::CrossVariantMojoRemote<
    blink::mojom::ServiceWorkerContainerHostInterfaceBase>
ServiceWorkerProviderContext::CloneRemoteContainerHost() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  if (!container_host_) {
    return blink::CrossVariantMojoRemote<
        blink::mojom::ServiceWorkerContainerHostInterfaceBase>();
  }
  mojo::PendingRemote<blink::mojom::ServiceWorkerContainerHost>
      remote_container_host;
  container_host_->CloneContainerHost(
      remote_container_host.InitWithNewPipeAndPassReceiver());
  return std::move(remote_container_host);
}

blink::mojom::ControllerServiceWorkerMode
ServiceWorkerProviderContext::GetControllerServiceWorkerMode() const {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  return controller_mode_;
}

blink::mojom::ServiceWorkerFetchHandlerType
ServiceWorkerProviderContext::GetFetchHandlerType() const {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_NE(controller_version_id_,
            blink::mojom::kInvalidServiceWorkerVersionId);
  return fetch_handler_type_;
}

const blink::WebString ServiceWorkerProviderContext::client_id() const {
  return blink::WebString::FromUTF8(client_id_);
}

void ServiceWorkerProviderContext::UnregisterWorkerFetchContext(
    blink::mojom::ServiceWorkerWorkerClient* client) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  base::EraseIf(
      worker_clients_,
      [client](const mojo::Remote<blink::mojom::ServiceWorkerWorkerClient>&
                   remote_client) { return remote_client.get() == client; });
}

void ServiceWorkerProviderContext::SetController(
    blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
    bool should_notify_controllerchange) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());

  controller_ = std::move(controller_info->object_info);
  controller_version_id_ = controller_
                               ? controller_->version_id
                               : blink::mojom::kInvalidServiceWorkerVersionId;
  // The client id should never change once set.
  DCHECK(client_id_.empty() || client_id_ == controller_info->client_id);
  client_id_ = controller_info->client_id;

  if (controller_info->fetch_request_window_id) {
    DCHECK(controller_);
    fetch_request_window_id_ = *controller_info->fetch_request_window_id;
  } else {
    fetch_request_window_id_ = base::UnguessableToken();
  }

  DCHECK((controller_info->mode ==
              blink::mojom::ControllerServiceWorkerMode::kNoController &&
          !controller_) ||
         (controller_info->mode !=
              blink::mojom::ControllerServiceWorkerMode::kNoController &&
          controller_));
  controller_mode_ = controller_info->mode;
  fetch_handler_type_ = controller_info->fetch_handler_type;
  effective_fetch_handler_type_ = controller_info->effective_fetch_handler_type;
  remote_controller_ = std::move(controller_info->remote_controller);
  fetch_handler_bypass_option_ = controller_info->fetch_handler_bypass_option;
  sha256_script_checksum_ = controller_info->sha256_script_checksum;
  router_rules_ = controller_info->router_rules;

  // Propagate the controller to workers related to this provider.
  if (controller_) {
    DCHECK_NE(blink::mojom::kInvalidServiceWorkerVersionId,
              controller_->version_id);
    for (const auto& worker : worker_clients_) {
      // This is a Mojo interface call to the (dedicated or shared) worker
      // thread.
      worker->OnControllerChanged(controller_mode_);
    }
  }
  for (blink::mojom::WebFeature feature : controller_info->used_features)
    used_features_.insert(feature);

  // Reset connector state for subresource loader factory if necessary.
  if (CanCreateSubresourceLoaderFactory()) {
    // There could be four patterns:
    //  (A) Had a controller, and got a new controller.
    //  (B) Had a controller, and lost the controller.
    //  (C) Didn't have a controller, and got a new controller.
    //  (D) Didn't have a controller, and lost the controller (nothing to do).
    if (controller_connector_) {
      // Used to have a controller at least once and have created a
      // subresource loader factory before (if no subresource factory was
      // created before, then the right controller, if any, will be used when
      // the factory is created in GetSubresourceLoaderFactory, so there's
      // nothing to do here).
      // Update the connector's controller so that subsequent resource requests
      // will get the new controller in case (A)/(C), or fallback to the network
      // in case (B). Inflight requests that are already dispatched may just use
      // the existing controller or may use the new controller settings
      // depending on when the request is actually passed to the factory (this
      // part is inherently racy).
      controller_connector_->UpdateController(std::move(remote_controller_));
    }
  }

  // The WebServiceWorkerProviderImpl might not exist yet because the document
  // has not yet been created (as WebServiceWorkerProviderImpl is created for a
  // ServiceWorkerContainer). In that case, once it's created it will still get
  // the controller from |this| via WebServiceWorkerProviderImpl::SetClient().
  if (web_service_worker_provider_) {
    web_service_worker_provider_->SetController(
        std::move(controller_), used_features_, should_notify_controllerchange);
  }
}

void ServiceWorkerProviderContext::PostMessageToClient(
    blink::mojom::ServiceWorkerObjectInfoPtr source,
    blink::TransferableMessage message) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());

  if (web_service_worker_provider_) {
    web_service_worker_provider_->PostMessageToClient(std::move(source),
                                                      std::move(message));
  }
}

void ServiceWorkerProviderContext::CountFeature(
    blink::mojom::WebFeature feature) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());

  // ServiceWorkerProviderContext keeps track of features in order to propagate
  // it to WebServiceWorkerProviderClient, which actually records the
  // UseCounter.
  used_features_.insert(feature);
  if (web_service_worker_provider_) {
    web_service_worker_provider_->CountFeature(feature);
  }
}

bool ServiceWorkerProviderContext::CanCreateSubresourceLoaderFactory() const {
  // |fallback_loader_factory| could be null in unit tests.
  return fallback_loader_factory_ != nullptr;
}

void ServiceWorkerProviderContext::DestructOnMainThread() const {
  if (!main_thread_task_runner_->RunsTasksInCurrentSequence() &&
      main_thread_task_runner_->DeleteSoon(FROM_HERE, this)) {
    return;
  }
  delete this;
}

}  // namespace content
