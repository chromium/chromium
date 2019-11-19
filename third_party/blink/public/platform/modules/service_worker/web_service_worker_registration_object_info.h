// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_SERVICE_WORKER_WEB_SERVICE_WORKER_REGISTRATION_OBJECT_INFO_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_SERVICE_WORKER_WEB_SERVICE_WORKER_REGISTRATION_OBJECT_INFO_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom-shared.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_object_info.h"
#include "third_party/blink/public/platform/web_url.h"

namespace blink {

// This is to carry blink.mojom.ServiceWorkerRegistrationObjectInfo data from
// //content across the boundary into Blink.
// TODO(crbug.com/879019): Remove this class once we make the following Mojo
// interfaces receive blink.mojom.ServiceWorkerRegistrationObjectInfo directly
// inside Blink.
//  - content.mojom.ServiceWorker
//  - content.mojom.ServiceWorkerContainer
//
// As we're on the border line between non-Blink and Blink variants, we need
// to use mojo::ScopedInterfaceEndpointHandle to pass Mojo types.
struct WebServiceWorkerRegistrationObjectInfo {
  WebServiceWorkerRegistrationObjectInfo(
      int64_t registration_id,
      WebURL scope,
      mojom::ServiceWorkerUpdateViaCache update_via_cache,
      mojo::ScopedInterfaceEndpointHandle host_remote,
      mojo::ScopedInterfaceEndpointHandle receiver,
      WebServiceWorkerObjectInfo installing,
      WebServiceWorkerObjectInfo waiting,
      WebServiceWorkerObjectInfo active)
      : registration_id(registration_id),
        scope(std::move(scope)),
        update_via_cache(update_via_cache),
        host_remote(std::move(host_remote)),
        receiver(std::move(receiver)),
        installing(std::move(installing)),
        waiting(std::move(waiting)),
        active(std::move(active)) {}
  WebServiceWorkerRegistrationObjectInfo(
      WebServiceWorkerRegistrationObjectInfo&& other) = default;

  int64_t registration_id;

  WebURL scope;
  mojom::ServiceWorkerUpdateViaCache update_via_cache;

  // For
  // mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerRegistrationObjectHost>.
  mojo::ScopedInterfaceEndpointHandle host_remote;
  // For
  // mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerRegistrationObject>.
  mojo::ScopedInterfaceEndpointHandle receiver;

  WebServiceWorkerObjectInfo installing;
  WebServiceWorkerObjectInfo waiting;
  WebServiceWorkerObjectInfo active;

  DISALLOW_COPY_AND_ASSIGN(WebServiceWorkerRegistrationObjectInfo);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_SERVICE_WORKER_WEB_SERVICE_WORKER_REGISTRATION_OBJECT_INFO_H_
