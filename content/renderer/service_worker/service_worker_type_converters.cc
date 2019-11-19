// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_type_converters.h"

#include "base/logging.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr_info.h"
#include "mojo/public/cpp/bindings/associated_interface_request.h"

namespace mojo {

blink::WebServiceWorkerObjectInfo
TypeConverter<blink::WebServiceWorkerObjectInfo,
              blink::mojom::ServiceWorkerObjectInfoPtr>::
    Convert(const blink::mojom::ServiceWorkerObjectInfoPtr& input) {
  if (!input) {
    return blink::WebServiceWorkerObjectInfo(
        blink::mojom::kInvalidServiceWorkerVersionId,
        blink::mojom::ServiceWorkerState::kParsed, blink::WebURL(),
        mojo::ScopedInterfaceEndpointHandle() /* host_remote */,
        mojo::ScopedInterfaceEndpointHandle() /* receiver */);
  }
  return blink::WebServiceWorkerObjectInfo(
      input->version_id, input->state, input->url,
      input->host_remote.PassHandle(), input->receiver.PassHandle());
}

blink::WebServiceWorkerRegistrationObjectInfo
TypeConverter<blink::WebServiceWorkerRegistrationObjectInfo,
              blink::mojom::ServiceWorkerRegistrationObjectInfoPtr>::
    Convert(const blink::mojom::ServiceWorkerRegistrationObjectInfoPtr& input) {
  if (!input) {
    return blink::WebServiceWorkerRegistrationObjectInfo(
        blink::mojom::kInvalidServiceWorkerRegistrationId, blink::WebURL(),
        blink::mojom::ServiceWorkerUpdateViaCache::kImports,
        mojo::ScopedInterfaceEndpointHandle() /* host_remote */,
        mojo::ScopedInterfaceEndpointHandle() /* receiver */,
        blink::mojom::ServiceWorkerObjectInfoPtr()
            .To<blink::WebServiceWorkerObjectInfo>() /* installing */,
        blink::mojom::ServiceWorkerObjectInfoPtr()
            .To<blink::WebServiceWorkerObjectInfo>() /* waiting */,
        blink::mojom::ServiceWorkerObjectInfoPtr()
            .To<blink::WebServiceWorkerObjectInfo>() /* active */);
  }
  return blink::WebServiceWorkerRegistrationObjectInfo(
      input->registration_id, input->scope, input->update_via_cache,
      input->host_remote.PassHandle(), input->receiver.PassHandle(),
      input->installing.To<blink::WebServiceWorkerObjectInfo>(),
      input->waiting.To<blink::WebServiceWorkerObjectInfo>(),
      input->active.To<blink::WebServiceWorkerObjectInfo>());
}

}  // namespace mojo
