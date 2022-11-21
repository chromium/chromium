// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_object_host.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "content/browser/service_worker/service_worker_client_utils.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_type_converters.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace content {

namespace {

using StatusCallback = base::OnceCallback<void(blink::ServiceWorkerStatusCode)>;
using PrepareExtendableMessageEventCallback =
    base::OnceCallback<bool(blink::mojom::ExtendableMessageEventPtr*)>;

void DispatchExtendableMessageEventAfterStartWorker(
    scoped_refptr<ServiceWorkerVersion> worker,
    blink::TransferableMessage message,
    const url::Origin& source_origin,
    const absl::optional<base::TimeDelta>& timeout,
    StatusCallback callback,
    PrepareExtendableMessageEventCallback prepare_callback,
    blink::ServiceWorkerStatusCode start_worker_status) {
  if (start_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(start_worker_status);
    return;
  }

  blink::mojom::ExtendableMessageEventPtr event =
      blink::mojom::ExtendableMessageEvent::New();
  event->message = std::move(message);
  event->source_origin = source_origin;
  if (!std::move(prepare_callback).Run(&event)) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorFailed);
    return;
  }

  int request_id;
  if (timeout) {
    request_id = worker->StartRequestWithCustomTimeout(
        ServiceWorkerMetrics::EventType::MESSAGE, std::move(callback), *timeout,
        ServiceWorkerVersion::CONTINUE_ON_TIMEOUT);
  } else {
    request_id = worker->StartRequest(ServiceWorkerMetrics::EventType::MESSAGE,
                                      std::move(callback));
  }
  worker->endpoint()->DispatchExtendableMessageEvent(
      std::move(event), worker->CreateSimpleEventCallback(request_id));
}

void StartWorkerToDispatchExtendableMessageEvent(
    scoped_refptr<ServiceWorkerVersion> worker,
    blink::TransferableMessage message,
    const url::Origin& source_origin,
    const absl::optional<base::TimeDelta>& timeout,
    StatusCallback callback,
    PrepareExtendableMessageEventCallback prepare_callback) {
  // If not enough time is left to actually process the event don't even
  // bother starting the worker and sending the event.
  if (timeout && *timeout < base::Milliseconds(100)) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorTimeout);
    return;
  }

  // Abort if redundant. This is not strictly needed since RunAfterStartWorker
  // does the same, but avoids logging UMA about failed startups.
  if (worker->is_redundant()) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorRedundant);
    return;
  }

  // As we don't track tasks between workers and renderers, we can nullify the
  // message's parent task ID.
  message.parent_task_id = absl::nullopt;

  worker->RunAfterStartWorker(
      ServiceWorkerMetrics::EventType::MESSAGE,
      base::BindOnce(&DispatchExtendableMessageEventAfterStartWorker, worker,
                     std::move(message), source_origin, timeout,
                     std::move(callback), std::move(prepare_callback)));
}

bool PrepareExtendableMessageEventFromClient(
    base::WeakPtr<ServiceWorkerContextCore> context,
    int64_t registration_id,
    blink::mojom::ServiceWorkerClientInfoPtr source_client_info,
    blink::mojom::ExtendableMessageEventPtr* event) {
  if (!context) {
    return false;
  }
  DCHECK(source_client_info && !source_client_info->client_uuid.empty());
  (*event)->source_info_for_client = std::move(source_client_info);
  // Hide the client url if the client has a unique origin.
  if ((*event)->source_origin.opaque())
    (*event)->source_info_for_client->url = GURL();

  // Reset |registration->self_update_delay| iff postMessage is coming from a
  // client, to prevent workers from postMessage to another version to reset
  // the delay (https://crbug.com/805496).
  scoped_refptr<ServiceWorkerRegistration> registration =
      context->GetLiveRegistration(registration_id);
  DCHECK(registration) << "running workers should have a live registration";
  registration->set_self_update_delay(base::TimeDelta());

  return true;
}

// The output |event| must be sent over Mojo immediately after this function
// returns. See ServiceWorkerObjectHost::CreateCompleteObjectInfoToSend() for
// details.
bool PrepareExtendableMessageEventFromServiceWorker(
    scoped_refptr<ServiceWorkerVersion> worker,
    base::WeakPtr<ServiceWorkerContainerHost> source_container_host,
    blink::mojom::ExtendableMessageEventPtr* event) {
  // The service worker execution context may have been destroyed by the time we
  // get here.
  if (!source_container_host)
    return false;

  DCHECK(source_container_host->IsContainerForServiceWorker());
  blink::mojom::ServiceWorkerObjectInfoPtr source_worker_info;
  base::WeakPtr<ServiceWorkerObjectHost> service_worker_object_host =
      worker->worker_host()
          ->container_host()
          ->GetOrCreateServiceWorkerObjectHost(
              source_container_host->service_worker_host()->version());
  if (service_worker_object_host) {
    // CreateCompleteObjectInfoToSend() is safe because |source_worker_info|
    // will be sent immediately by the caller of this function.
    source_worker_info =
        service_worker_object_host->CreateCompleteObjectInfoToSend();
  }

  (*event)->source_info_for_service_worker = std::move(source_worker_info);
  // Hide the service worker url if the service worker has a unique origin.
  if ((*event)->source_origin.opaque())
    (*event)->source_info_for_service_worker->url = GURL();
  return true;
}

void DispatchExtendableMessageEventFromClient(
    base::WeakPtr<ServiceWorkerContextCore> context,
    scoped_refptr<ServiceWorkerVersion> worker,
    blink::TransferableMessage message,
    const url::Origin& source_origin,
    StatusCallback callback,
    blink::mojom::ServiceWorkerClientInfoPtr source_client_info) {
  if (!context) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorAbort);
    return;
  }
  // |source_client_info| may be null if a client sent the message but its
  // info could not be retrieved.
  if (!source_client_info) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorFailed);
    return;
  }

  StartWorkerToDispatchExtendableMessageEvent(
      worker, std::move(message), source_origin, absl::nullopt /* timeout */,
      std::move(callback),
      base::BindOnce(&PrepareExtendableMessageEventFromClient, context,
                     worker->registration_id(), std::move(source_client_info)));
}

void DispatchExtendableMessageEventFromServiceWorker(
    scoped_refptr<ServiceWorkerVersion> worker,
    blink::TransferableMessage message,
    const url::Origin& source_origin,
    const absl::optional<base::TimeDelta>& timeout,
    StatusCallback callback,
    base::WeakPtr<ServiceWorkerContainerHost> source_container_host) {
  if (!source_container_host) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorFailed);
    return;
  }

  DCHECK(source_container_host->IsContainerForServiceWorker());
  StartWorkerToDispatchExtendableMessageEvent(
      worker, std::move(message), source_origin, timeout, std::move(callback),
      base::BindOnce(&PrepareExtendableMessageEventFromServiceWorker, worker,
                     std::move(source_container_host)));
}

}  // namespace

ServiceWorkerObjectHost::ServiceWorkerObjectHost(
    base::WeakPtr<ServiceWorkerContextCore> context,
    base::WeakPtr<ServiceWorkerContainerHost> container_host,
    scoped_refptr<ServiceWorkerVersion> version)
    : context_(context),
      container_host_(container_host),
      container_origin_(url::Origin::Create(container_host_->url())),
      version_(std::move(version)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(context_ && container_host_ && version_);
  DCHECK(context_->GetLiveRegistration(version_->registration_id()));
  version_->AddObserver(this);
  receivers_.set_disconnect_handler(base::BindRepeating(
      &ServiceWorkerObjectHost::OnConnectionError, base::Unretained(this)));
}

ServiceWorkerObjectHost::~ServiceWorkerObjectHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  version_->RemoveObserver(this);
}

void ServiceWorkerObjectHost::OnVersionStateChanged(
    ServiceWorkerVersion* version) {
  DCHECK(version);
  blink::mojom::ServiceWorkerState state =
      mojo::ConvertTo<blink::mojom::ServiceWorkerState>(version->status());
  for (auto& remote_object : remote_objects_)
    remote_object->StateChanged(state);
}

blink::mojom::ServiceWorkerObjectInfoPtr
ServiceWorkerObjectHost::CreateCompleteObjectInfoToSend() {
  auto info = CreateIncompleteObjectInfo();
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerObject> remote_object;
  info->receiver = remote_object.BindNewEndpointAndPassReceiver();
  remote_objects_.Add(std::move(remote_object));
  return info;
}

blink::mojom::ServiceWorkerObjectInfoPtr
ServiceWorkerObjectHost::CreateIncompleteObjectInfo() {
  auto info = blink::mojom::ServiceWorkerObjectInfo::New();
  info->url = version_->script_url();
  info->state =
      mojo::ConvertTo<blink::mojom::ServiceWorkerState>(version_->status());
  info->version_id = version_->version_id();
  receivers_.Add(this, info->host_remote.InitWithNewEndpointAndPassReceiver());
  return info;
}

void ServiceWorkerObjectHost::AddRemoteObjectPtrAndUpdateState(
    mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerObject>
        pending_object,
    blink::mojom::ServiceWorkerState sent_state) {
  DCHECK(pending_object.is_valid());
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerObject> remote_object;
  remote_object.Bind(std::move(pending_object));
  auto state =
      mojo::ConvertTo<blink::mojom::ServiceWorkerState>(version_->status());
  if (sent_state != state)
    remote_object->StateChanged(state);
  remote_objects_.Add(std::move(remote_object));
}

base::WeakPtr<ServiceWorkerObjectHost> ServiceWorkerObjectHost::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ServiceWorkerObjectHost::PostMessageToServiceWorker(
    ::blink::TransferableMessage message) {
  // When this method is called the encoded_message inside message could just
  // point to the IPC message's buffer. But that buffer can become invalid
  // before the message is passed on to the service worker, so make sure
  // message owns its data.
  message.EnsureDataIsOwned();

  DispatchExtendableMessageEvent(std::move(message), base::DoNothing());
}

void ServiceWorkerObjectHost::TerminateForTesting(
    TerminateForTestingCallback callback) {
  version_->StopWorker(std::move(callback));
}

void ServiceWorkerObjectHost::DispatchExtendableMessageEvent(
    ::blink::TransferableMessage message,
    StatusCallback callback) {
  DCHECK(container_host_);
  if (!context_) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorAbort);
    return;
  }
  DCHECK_EQ(container_origin_, url::Origin::Create(container_host_->url()));

  // As we don't track tasks between workers and renderers, we can nullify the
  // message's parent task ID.
  message.parent_task_id = absl::nullopt;

  if (container_host_->IsContainerForServiceWorker()) {
    // Clamp timeout to the sending worker's remaining timeout, to prevent
    // postMessage from keeping workers alive forever.
    base::TimeDelta timeout =
        container_host_->service_worker_host()->version()->remaining_timeout();

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&DispatchExtendableMessageEventFromServiceWorker,
                       version_, std::move(message), container_origin_,
                       absl::make_optional(timeout), std::move(callback),
                       container_host_->GetWeakPtr()));
  } else if (container_host_->IsContainerForWindowClient()) {
    service_worker_client_utils::GetClient(
        container_host_.get(),
        base::BindOnce(&DispatchExtendableMessageEventFromClient, context_,
                       version_, std::move(message), container_origin_,
                       std::move(callback)));
  } else {
    DCHECK(container_host_->IsContainerForWorkerClient());

    // Web workers don't yet have access to ServiceWorker objects, so they
    // can't postMessage to one (https://crbug.com/371690).
    NOTREACHED();
  }
}

void ServiceWorkerObjectHost::OnConnectionError() {
  // If there are still receivers, |this| is still being used.
  if (!receivers_.empty())
    return;
  DCHECK(container_host_);
  // Will destroy |this|.
  container_host_->RemoveServiceWorkerObjectHost(version_->version_id());
}

}  // namespace content
