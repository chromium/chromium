// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_container_client.h"

#include <memory>
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_provider.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"

namespace blink {

ServiceWorkerContainerClient::ServiceWorkerContainerClient(
    Document& document,
    std::unique_ptr<WebServiceWorkerProvider> provider)
    : Supplement<Document>(document), provider_(std::move(provider)) {}

ServiceWorkerContainerClient::~ServiceWorkerContainerClient() = default;

const char ServiceWorkerContainerClient::kSupplementName[] =
    "ServiceWorkerContainerClient";

ServiceWorkerRegistration*
ServiceWorkerContainerClient::GetOrCreateServiceWorkerRegistration(
    WebServiceWorkerRegistrationObjectInfo info) {
  if (info.registration_id == mojom::blink::kInvalidServiceWorkerRegistrationId)
    return nullptr;

  ServiceWorkerRegistration* registration =
      service_worker_registration_objects_.at(info.registration_id);
  if (registration) {
    registration->Attach(std::move(info));
    return registration;
  }

  registration =
      new ServiceWorkerRegistration(GetSupplementable(), std::move(info));
  service_worker_registration_objects_.Set(info.registration_id, registration);
  return registration;
}

ServiceWorker* ServiceWorkerContainerClient::GetOrCreateServiceWorker(
    WebServiceWorkerObjectInfo info) {
  if (info.version_id == mojom::blink::kInvalidServiceWorkerVersionId)
    return nullptr;
  ServiceWorker* worker = service_worker_objects_.at(info.version_id);
  if (!worker) {
    worker = new ServiceWorker(GetSupplementable(), std::move(info));
    service_worker_objects_.Set(info.version_id, worker);
  }
  return worker;
}

ServiceWorkerContainerClient* ServiceWorkerContainerClient::From(
    Document* document) {
  if (!document)
    return nullptr;
  if (!document->GetFrame() || !document->GetFrame()->Client())
    return nullptr;

  ServiceWorkerContainerClient* client =
      Supplement<Document>::From<ServiceWorkerContainerClient>(document);
  if (!client) {
    client = new ServiceWorkerContainerClient(
        *document,
        document->GetFrame()->Client()->CreateServiceWorkerProvider());
    Supplement<Document>::ProvideTo(*document, client);
  }
  return client;
}

void ServiceWorkerContainerClient::Trace(blink::Visitor* visitor) {
  visitor->Trace(service_worker_registration_objects_);
  visitor->Trace(service_worker_objects_);
  Supplement<Document>::Trace(visitor);
}

}  // namespace blink
