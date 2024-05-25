// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_object_host.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_type_converters.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace content {

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
  auto info = blink::mojom::ServiceWorkerObjectInfo::New();
  info->url = version_->script_url();
  info->state =
      mojo::ConvertTo<blink::mojom::ServiceWorkerState>(version_->status());
  info->version_id = version_->version_id();
  receivers_.Add(this, info->host_remote.InitWithNewEndpointAndPassReceiver());

  mojo::AssociatedRemote<blink::mojom::ServiceWorkerObject> remote_object;
  info->receiver = remote_object.BindNewEndpointAndPassReceiver();
  remote_objects_.Add(std::move(remote_object));
  return info;
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
    base::OnceCallback<void(blink::ServiceWorkerStatusCode)> callback) {
  DCHECK(container_host_);
  if (!context_) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorAbort);
    return;
  }
  DCHECK_EQ(container_origin_, url::Origin::Create(container_host_->url()));

  // As we don't track tasks between workers and renderers, we can nullify the
  // message's parent task ID.
  message.parent_task_id = std::nullopt;

  container_host_->DispatchExtendableMessageEvent(version_, std::move(message),
                                                  std::move(callback));
}

void ServiceWorkerObjectHost::OnConnectionError() {
  // If there are still receivers, |this| is still being used.
  if (!receivers_.empty())
    return;
  DCHECK(container_host_);
  // Will destroy |this|.
  container_host_->version_object_manager().RemoveHost(version_->version_id());
}

}  // namespace content
