// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_object_host.h"

#include "content/browser/service_worker/service_worker_client_utils.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_type_converters.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/browser_side_navigation_policy.h"

namespace content {

namespace {

using StatusCallback = base::OnceCallback<void(blink::ServiceWorkerStatusCode)>;
using PrepareExtendableMessageEventCallback =
    base::OnceCallback<bool(mojom::ExtendableMessageEventPtr*)>;

void DispatchExtendableMessageEventAfterStartWorker(
    scoped_refptr<ServiceWorkerVersion> worker,
    blink::TransferableMessage message,
    const url::Origin& source_origin,
    const base::Optional<base::TimeDelta>& timeout,
    StatusCallback callback,
    PrepareExtendableMessageEventCallback prepare_callback,
    blink::ServiceWorkerStatusCode start_worker_status) {
  if (start_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(start_worker_status);
    return;
  }

  mojom::ExtendableMessageEventPtr event = mojom::ExtendableMessageEvent::New();
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
    const base::Optional<base::TimeDelta>& timeout,
    StatusCallback callback,
    PrepareExtendableMessageEventCallback prepare_callback) {
  // If not enough time is left to actually process the event don't even
  // bother starting the worker and sending the event.
  if (timeout && *timeout < base::TimeDelta::FromMilliseconds(100)) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorTimeout);
    return;
  }

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
    mojom::ExtendableMessageEventPtr* event) {
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
  ServiceWorkerRegistration* registration =
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
    base::WeakPtr<ServiceWorkerProviderHost>
        source_service_worker_provider_host,
    mojom::ExtendableMessageEventPtr* event) {
  // The service worker execution context may have been destroyed by the time we
  // get here.
  if (!source_service_worker_provider_host)
    return false;

  DCHECK_EQ(blink::mojom::ServiceWorkerProviderType::kForServiceWorker,
            source_service_worker_provider_host->provider_type());
  blink::mojom::ServiceWorkerObjectInfoPtr source_worker_info;
  base::WeakPtr<ServiceWorkerObjectHost> service_worker_object_host =
      worker->provider_host()->GetOrCreateServiceWorkerObjectHost(
          source_service_worker_provider_host->running_hosted_version());
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
      worker, std::move(message), source_origin, base::nullopt /* timeout */,
      std::move(callback),
      base::BindOnce(&PrepareExtendableMessageEventFromClient, context,
                     worker->registration_id(), std::move(source_client_info)));
}

void DispatchExtendableMessageEventFromServiceWorker(
    scoped_refptr<ServiceWorkerVersion> worker,
    blink::TransferableMessage message,
    const url::Origin& source_origin,
    const base::Optional<base::TimeDelta>& timeout,
    StatusCallback callback,
    base::WeakPtr<ServiceWorkerProviderHost>
        source_service_worker_provider_host) {
  if (!source_service_worker_provider_host) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorFailed);
    return;
  }

  DCHECK_EQ(blink::mojom::ServiceWorkerProviderType::kForServiceWorker,
            source_service_worker_provider_host->provider_type());
  StartWorkerToDispatchExtendableMessageEvent(
      worker, std::move(message), source_origin, timeout, std::move(callback),
      base::BindOnce(&PrepareExtendableMessageEventFromServiceWorker, worker,
                     source_service_worker_provider_host));
}

}  // namespace

ServiceWorkerObjectHost::ServiceWorkerObjectHost(
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerProviderHost* provider_host,
    scoped_refptr<ServiceWorkerVersion> version)
    : context_(context),
      provider_host_(provider_host),
      provider_origin_(url::Origin::Create(provider_host->document_url())),
      version_(std::move(version)),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(context_ && provider_host_ && version_);
  DCHECK(context_->GetLiveRegistration(version_->registration_id()));
  version_->AddObserver(this);
  bindings_.set_connection_error_handler(base::BindRepeating(
      &ServiceWorkerObjectHost::OnConnectionError, base::Unretained(this)));
}

ServiceWorkerObjectHost::~ServiceWorkerObjectHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  version_->RemoveObserver(this);
}

void ServiceWorkerObjectHost::OnVersionStateChanged(
    ServiceWorkerVersion* version) {
  DCHECK(version);
  blink::mojom::ServiceWorkerState state =
      mojo::ConvertTo<blink::mojom::ServiceWorkerState>(version->status());
  remote_objects_.ForAllPtrs(
      [state](blink::mojom::ServiceWorkerObject* remote_object) {
        remote_object->StateChanged(state);
      });
}

blink::mojom::ServiceWorkerObjectInfoPtr
ServiceWorkerObjectHost::CreateCompleteObjectInfoToSend() {
  auto info = CreateIncompleteObjectInfo();
  blink::mojom::ServiceWorkerObjectAssociatedPtr remote_object;
  info->request = mojo::MakeRequest(&remote_object);
  remote_objects_.AddPtr(std::move(remote_object));
  return info;
}

blink::mojom::ServiceWorkerObjectInfoPtr
ServiceWorkerObjectHost::CreateIncompleteObjectInfo() {
  auto info = blink::mojom::ServiceWorkerObjectInfo::New();
  info->url = version_->script_url();
  info->state =
      mojo::ConvertTo<blink::mojom::ServiceWorkerState>(version_->status());
  info->version_id = version_->version_id();
  bindings_.AddBinding(this, mojo::MakeRequest(&info->host_ptr_info));
  return info;
}

void ServiceWorkerObjectHost::AddRemoteObjectPtrAndUpdateState(
    blink::mojom::ServiceWorkerObjectAssociatedPtrInfo remote_object_ptr_info,
    blink::mojom::ServiceWorkerState sent_state) {
  DCHECK(remote_object_ptr_info.is_valid());
  blink::mojom::ServiceWorkerObjectAssociatedPtr remote_object;
  remote_object.Bind(std::move(remote_object_ptr_info));
  auto state =
      mojo::ConvertTo<blink::mojom::ServiceWorkerState>(version_->status());
  if (sent_state != state)
    remote_object->StateChanged(state);
  remote_objects_.AddPtr(std::move(remote_object));
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
  if (!context_) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorAbort);
    return;
  }
  DCHECK_EQ(provider_origin_,
            url::Origin::Create(provider_host_->document_url()));
  switch (provider_host_->provider_type()) {
    case blink::mojom::ServiceWorkerProviderType::kForWindow:
      service_worker_client_utils::GetClient(
          provider_host_,
          base::BindOnce(&DispatchExtendableMessageEventFromClient, context_,
                         version_, std::move(message), provider_origin_,
                         std::move(callback)));
      return;
    case blink::mojom::ServiceWorkerProviderType::kForServiceWorker: {
      // Clamp timeout to the sending worker's remaining timeout, to prevent
      // postMessage from keeping workers alive forever.
      base::TimeDelta timeout =
          provider_host_->running_hosted_version()->remaining_timeout();

      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&DispatchExtendableMessageEventFromServiceWorker,
                         version_, std::move(message), provider_origin_,
                         base::make_optional(timeout), std::move(callback),
                         provider_host_->AsWeakPtr()));
      return;
    }
    case blink::mojom::ServiceWorkerProviderType::kForSharedWorker:
    // Shared workers don't yet have access to ServiceWorker objects, so they
    // can't postMessage to one (https://crbug.com/371690).
    case blink::mojom::ServiceWorkerProviderType::kUnknown:
      break;
  }
  NOTREACHED() << provider_host_->provider_type();
}

void ServiceWorkerObjectHost::OnConnectionError() {
  // If there are still bindings, |this| is still being used.
  if (!bindings_.empty())
    return;
  // Will destroy |this|.
  provider_host_->RemoveServiceWorkerObjectHost(version_->version_id());
}

}  // namespace content
