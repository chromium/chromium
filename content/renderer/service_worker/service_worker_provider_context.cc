// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_provider_context.h"

#include <set>
#include <utility>
#include <vector>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_split.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "content/common/features.h"
#include "content/public/common/content_features.h"
#include "content/renderer/service_worker/controller_service_worker_connector.h"
#include "content/renderer/service_worker/service_worker_subresource_loader.h"
#include "content/renderer/service_worker/web_service_worker_provider_impl.h"
#include "content/renderer/worker/worker_thread_registry.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom-shared.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_fetch_handler_bypass_option.mojom-shared.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_worker_client.mojom.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_error.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_provider.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_provider_client.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace content {

namespace {

const char kLostConnectionErrorMessage[] =
    "Lost connection to the service worker system.";

void CreateSubresourceLoaderFactoryForProviderContext(
    mojo::PendingRemote<blink::mojom::ServiceWorkerContainerHost>
        remote_container_host,
    mojo::PendingRemote<blink::mojom::ControllerServiceWorker>
        remote_controller,
    mojo::PendingRemote<blink::mojom::CacheStorage> remote_cache_storage,
    const std::string& client_id,
    blink::mojom::ServiceWorkerFetchHandlerBypassOption
        fetch_handler_bypass_option,
    std::optional<blink::ServiceWorkerRouterRules> router_rules,
    std::optional<blink::EmbeddedWorkerStatus> initial_running_status,
    mojo::PendingReceiver<blink::mojom::ServiceWorkerRunningStatusCallback>
        running_status_receiver,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_fallback_factory,
    mojo::PendingReceiver<blink::mojom::ControllerServiceWorkerConnector>
        connector_receiver,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  auto connector = base::MakeRefCounted<ControllerServiceWorkerConnector>(
      std::move(remote_container_host), std::move(remote_controller),
      std::move(remote_cache_storage), client_id, fetch_handler_bypass_option,
      router_rules, initial_running_status, std::move(running_status_receiver));
  connector->AddBinding(std::move(connector_receiver));
  ServiceWorkerSubresourceLoaderFactory::Create(
      std::move(connector),
      network::SharedURLLoaderFactory::Create(
          std::move(pending_fallback_factory)),
      std::move(receiver), std::move(task_runner));
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
  CHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  return std::move(controller_);
}

bool ServiceWorkerProviderContext::container_is_blob_url_shared_worker() const {
  return container_is_blob_url_shared_worker_;
}

int64_t ServiceWorkerProviderContext::GetControllerVersionId() const {
  CHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  return controller_version_id_;
}

network::mojom::URLLoaderFactory*
ServiceWorkerProviderContext::GetSubresourceLoaderFactoryInternal() {
  if (!need_router_evaluate_) {
    if (!remote_controller_ && !controller_connector_) {
      // No controller is attached.
      return nullptr;
    }

    if (controller_mode_ !=
        blink::mojom::ControllerServiceWorkerMode::kControlled) {
      // The controller does not exist or has no fetch event handler.
      return nullptr;
    }

    if (fetch_handler_type_ !=
        blink::mojom::ServiceWorkerFetchHandlerType::kNotSkippable) {
      // The fetch handler can be skipped.  The service worker process should
      // not be ready for this case.
      CountFeature(
          blink::mojom::WebFeature::kServiceWorkerSkippedForSubresourceLoad);
      return nullptr;
    }
  }

  if (fetch_handler_bypass_option_ ==
          blink::mojom::ServiceWorkerFetchHandlerBypassOption::
              kSyntheticResponse ||
      fetch_handler_bypass_option_ ==
          blink::mojom::ServiceWorkerFetchHandlerBypassOption::
              kSyntheticResponseDryRunMode) {
    return nullptr;
  }

  if (!subresource_loader_factory_) {
    CHECK(!controller_connector_);
    CHECK(remote_controller_ || need_router_evaluate_);

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
        base::BindOnce(
            &CreateSubresourceLoaderFactoryForProviderContext,
            std::move(remote_container_host), std::move(remote_controller_),
            std::move(remote_cache_storage_), client_id_,
            fetch_handler_bypass_option_, router_rules_,
            initial_running_status_, std::move(running_status_receiver_),
            fallback_loader_factory_->Clone(),
            controller_connector_.BindNewPipeAndPassReceiver(),
            subresource_loader_factory_.BindNewPipeAndPassReceiver(),
            task_runner));

    CHECK(!weak_wrapped_subresource_loader_factory_);
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
  CHECK(base::FeatureList::IsEnabled(
            blink::features::kServiceWorkerInDedicatedWorker) ||
        main_thread_task_runner_->RunsTasksInCurrentSequence());

  if (!main_thread_task_runner_->RunsTasksInCurrentSequence()) {
    // Queue a task to run this method on the main thread.
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceWorkerProviderContext::RegisterWorkerClient,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(pending_client)));
    return;
  }

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
  CHECK(base::FeatureList::IsEnabled(
            blink::features::kServiceWorkerInDedicatedWorker) ||
        main_thread_task_runner_->RunsTasksInCurrentSequence());

  if (!main_thread_task_runner_->RunsTasksInCurrentSequence()) {
    // Queue a task to run this method on the main thread.
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceWorkerProviderContext::CloneWorkerClientRegistry,
                       weak_ptr_factory_.GetWeakPtr(), std::move(receiver)));
    return;
  }

  worker_client_registry_receivers_.Add(this, std::move(receiver));
}

void ServiceWorkerProviderContext::OnNetworkProviderDestroyed() {
  CHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  container_host_.reset();
}

bool ServiceWorkerProviderContext::has_container_host_for_testing() const {
  CHECK_IS_TEST();
  return container_host_.is_bound();
}

void ServiceWorkerProviderContext::DispatchNetworkQuiet() {
  CHECK(base::FeatureList::IsEnabled(
            blink::features::kServiceWorkerInDedicatedWorker) ||
        main_thread_task_runner_->RunsTasksInCurrentSequence());

  if (!main_thread_task_runner_->RunsTasksInCurrentSequence()) {
    // Queue a task to run this method on the main thread.
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceWorkerProviderContext::DispatchNetworkQuiet,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (controller_mode_ ==
      blink::mojom::ControllerServiceWorkerMode::kNoController) {
    return;
  }

  if (!container_host_)
    return;

  container_host_->HintToUpdateServiceWorker();
}

void ServiceWorkerProviderContext::NotifyExecutionReady() {
  CHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  CHECK_EQ(container_type(),
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
  CHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
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
  CHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  return controller_mode_;
}

blink::mojom::ServiceWorkerFetchHandlerType
ServiceWorkerProviderContext::GetFetchHandlerType() const {
  CHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  CHECK_NE(controller_version_id_,
           blink::mojom::kInvalidServiceWorkerVersionId);
  return fetch_handler_type_;
}

blink::mojom::ServiceWorkerFetchHandlerBypassOption
ServiceWorkerProviderContext::GetFetchHandlerBypassOption() const {
  CHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  CHECK_NE(controller_version_id_,
           blink::mojom::kInvalidServiceWorkerVersionId);
  return fetch_handler_bypass_option_;
}

const blink::WebString ServiceWorkerProviderContext::client_id() const {
  return blink::WebString::FromUTF8(client_id_);
}

std::unique_ptr<blink::WebServiceWorkerProvider>
ServiceWorkerProviderContext::CreateServiceWorkerProvider() {
  return std::make_unique<content::WebServiceWorkerProviderImpl>(this);
}

void ServiceWorkerProviderContext::Destroy() const {
  DestructOnMainThread();
}

void ServiceWorkerProviderContext::UnregisterWorkerFetchContext(
    blink::mojom::ServiceWorkerWorkerClient* client) {
  CHECK(base::FeatureList::IsEnabled(
            blink::features::kServiceWorkerInDedicatedWorker) ||
        main_thread_task_runner_->RunsTasksInCurrentSequence());

  if (!main_thread_task_runner_->RunsTasksInCurrentSequence()) {
    // Queue a task to run this method on the main thread.
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ServiceWorkerProviderContext::UnregisterWorkerFetchContext,
            weak_ptr_factory_.GetWeakPtr(), client));
    return;
  }
  std::erase_if(
      worker_clients_,
      [client](const mojo::Remote<blink::mojom::ServiceWorkerWorkerClient>&
                   remote_client) { return remote_client.get() == client; });
}

void ServiceWorkerProviderContext::SetController(
    blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
    bool should_notify_controllerchange) {
  CHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());

  controller_ = std::move(controller_info->object_info);
  controller_version_id_ = controller_
                               ? controller_->version_id
                               : blink::mojom::kInvalidServiceWorkerVersionId;
  // The client id should never change once set.
  CHECK(client_id_.empty() || client_id_ == controller_info->client_id);
  client_id_ = controller_info->client_id;

  if (controller_info->fetch_request_window_id) {
    CHECK(controller_);
    fetch_request_window_id_ = *controller_info->fetch_request_window_id;
  } else {
    fetch_request_window_id_ = base::UnguessableToken();
  }

  CHECK((controller_info->mode ==
             blink::mojom::ControllerServiceWorkerMode::kNoController &&
         !controller_) ||
        (controller_info->mode !=
             blink::mojom::ControllerServiceWorkerMode::kNoController &&
         controller_));
  controller_mode_ = controller_info->mode;
  fetch_handler_type_ = controller_info->fetch_handler_type;
  need_router_evaluate_ = controller_info->need_router_evaluate;
  remote_controller_ = std::move(controller_info->remote_controller);
  fetch_handler_bypass_option_ = controller_info->fetch_handler_bypass_option;
  sha256_script_checksum_ = controller_info->sha256_script_checksum;
  if (controller_info->router_data) {
    router_rules_ = controller_info->router_data->router_rules;
    initial_running_status_ =
        controller_info->router_data->initial_running_status;
    running_status_receiver_ =
        std::move(controller_info->router_data->running_status_receiver);
    remote_cache_storage_ =
        std::move(controller_info->router_data->remote_cache_storage);
  }

  // Propagate the controller to workers related to this provider.
  if (controller_) {
    CHECK_NE(blink::mojom::kInvalidServiceWorkerVersionId,
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
  CHECK(base::FeatureList::IsEnabled(
            blink::features::kServiceWorkerInDedicatedWorker) ||
        main_thread_task_runner_->RunsTasksInCurrentSequence());

  if (!main_thread_task_runner_->RunsTasksInCurrentSequence()) {
    // Queue a task to run this method on the main thread.
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceWorkerProviderContext::PostMessageToClient,
                       weak_ptr_factory_.GetWeakPtr(), std::move(source),
                       std::move(message)));
    return;
  }

  if (web_service_worker_provider_) {
    web_service_worker_provider_->PostMessageToClient(std::move(source),
                                                      std::move(message));
  }
}

void ServiceWorkerProviderContext::CountFeature(
    blink::mojom::WebFeature feature) {
  CHECK(base::FeatureList::IsEnabled(
            blink::features::kServiceWorkerInDedicatedWorker) ||
        main_thread_task_runner_->RunsTasksInCurrentSequence());

  if (!main_thread_task_runner_->RunsTasksInCurrentSequence()) {
    // Queue a task to run this method on the main thread.
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ServiceWorkerProviderContext::CountFeature,
                                  weak_ptr_factory_.GetWeakPtr(), feature));
    return;
  }

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

void ServiceWorkerProviderContext::Register(
    const GURL& script_url,
    blink::mojom::ServiceWorkerRegistrationOptionsPtr options,
    blink::mojom::FetchClientSettingsObjectPtr fetch_client_settings,
    blink::mojom::ServiceWorkerContainerHost::RegisterCallback callback) {
  CHECK(base::FeatureList::IsEnabled(
            blink::features::kServiceWorkerInDedicatedWorker) ||
        main_thread_task_runner_->RunsTasksInCurrentSequence());

  if (!main_thread_task_runner_->RunsTasksInCurrentSequence()) {
    // Queue a task to run this method on the main thread and wrap the callback
    // to ensure it runs back on this thread.
    auto wrapped_callback = base::BindPostTask(
        base::SingleThreadTaskRunner::GetCurrentDefault(), std::move(callback));

    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceWorkerProviderContext::Register,
                       weak_ptr_factory_.GetWeakPtr(), script_url,
                       std::move(options), std::move(fetch_client_settings),
                       std::move(wrapped_callback)));
    return;
  }

  if (container_host_) {
    TRACE_EVENT_BEGIN("ServiceWorker",
                      "WebServiceWorkerProviderImpl::RegisterServiceWorker",
                      perfetto::Track::FromPointer(this), "Scope",
                      options->scope.spec(), "Script URL", script_url.spec());

    container_host_->Register(std::move(script_url), std::move(options),
                              std::move(fetch_client_settings),
                              std::move(callback));
  } else {
    const std::string error_prefix("Failed to register a ServiceWorker: ");
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kAbort,
                            error_prefix + kLostConnectionErrorMessage,
                            nullptr);
  }
}

void ServiceWorkerProviderContext::GetRegistration(
    const GURL& document_url,
    blink::mojom::ServiceWorkerContainerHost::GetRegistrationCallback
        callback) {
  CHECK(base::FeatureList::IsEnabled(
            blink::features::kServiceWorkerInDedicatedWorker) ||
        main_thread_task_runner_->RunsTasksInCurrentSequence());

  if (!main_thread_task_runner_->RunsTasksInCurrentSequence()) {
    // Queue a task to run this method on the main thread and wrap the callback
    // to ensure it runs back on this thread.
    auto wrapped_callback = base::BindPostTask(
        base::SingleThreadTaskRunner::GetCurrentDefault(), std::move(callback));

    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceWorkerProviderContext::GetRegistration,
                       weak_ptr_factory_.GetWeakPtr(), document_url,
                       std::move(wrapped_callback)));
    return;
  }

  if (container_host_) {
    TRACE_EVENT_BEGIN("ServiceWorker",
                      "WebServiceWorkerProviderImpl::GetRegistration",
                      perfetto::Track::FromPointer(this), "Document URL",
                      document_url.spec());

    container_host_->GetRegistration(document_url, std::move(callback));
  } else {
    const std::string error_prefix(
        "Failed to get a ServiceWorkerRegistration: ");
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kAbort,
                            error_prefix + kLostConnectionErrorMessage,
                            nullptr);
  }
}

void ServiceWorkerProviderContext::GetRegistrations(
    blink::mojom::ServiceWorkerContainerHost::GetRegistrationsCallback
        callback) {
  CHECK(base::FeatureList::IsEnabled(
            blink::features::kServiceWorkerInDedicatedWorker) ||
        main_thread_task_runner_->RunsTasksInCurrentSequence());

  if (!main_thread_task_runner_->RunsTasksInCurrentSequence()) {
    // Queue a task to run this method on the main thread and wrap the callback
    // to ensure it runs back on this thread.
    auto wrapped_callback = base::BindPostTask(
        base::SingleThreadTaskRunner::GetCurrentDefault(), std::move(callback));
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceWorkerProviderContext::GetRegistrations,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(wrapped_callback)));
    return;
  }

  if (container_host_) {
    TRACE_EVENT_BEGIN("ServiceWorker",
                      "WebServiceWorkerProviderImpl::GetRegistrations",
                      perfetto::Track::FromPointer(this));

    container_host_->GetRegistrations(std::move(callback));
  } else {
    const std::string error_prefix(
        "Failed to get ServiceWorkerRegistration objects: ");
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kAbort,
                            error_prefix + kLostConnectionErrorMessage,
                            std::nullopt);
  }
}

void ServiceWorkerProviderContext::GetRegistrationForReady(
    blink::mojom::ServiceWorkerContainerHost::GetRegistrationForReadyCallback
        callback) {
  CHECK(base::FeatureList::IsEnabled(
            blink::features::kServiceWorkerInDedicatedWorker) ||
        main_thread_task_runner_->RunsTasksInCurrentSequence());

  if (!main_thread_task_runner_->RunsTasksInCurrentSequence()) {
    // Queue a task to run this method on the main thread and wrap the callback
    // to ensure it runs back on this thread.
    auto wrapped_callback = base::BindPostTask(
        base::SingleThreadTaskRunner::GetCurrentDefault(), std::move(callback));
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceWorkerProviderContext::GetRegistrationForReady,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(wrapped_callback)));
    return;
  }

  if (container_host_) {
    TRACE_EVENT_BEGIN("ServiceWorker",
                      "WebServiceWorkerProviderImpl::GetRegistrationForReady",
                      perfetto::Track::FromPointer(this));

    container_host_->GetRegistrationForReady(std::move(callback));
  }
}

}  // namespace content
