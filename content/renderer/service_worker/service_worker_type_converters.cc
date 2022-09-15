// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_type_converters.h"

#include <utility>

#include "mojo/public/cpp/bindings/associated_interface_ptr_info.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace mojo {

blink::WebServiceWorkerObjectInfo
TypeConverter<blink::WebServiceWorkerObjectInfo,
              blink::mojom::ServiceWorkerObjectInfoPtr>::
    Convert(blink::mojom::ServiceWorkerObjectInfoPtr input) {
  if (!input) {
    return blink::WebServiceWorkerObjectInfo(
        blink::mojom::kInvalidServiceWorkerVersionId,
        blink::mojom::ServiceWorkerState::kParsed, blink::WebURL(),
        {} /* host_remote */, {} /* receiver */);
  }
  return blink::WebServiceWorkerObjectInfo(
      input->version_id, input->state, input->url,
      std::move(input->host_remote), std::move(input->receiver));
}

blink::WebServiceWorkerRegistrationObjectInfo
TypeConverter<blink::WebServiceWorkerRegistrationObjectInfo,
              blink::mojom::ServiceWorkerRegistrationObjectInfoPtr>::
    Convert(blink::mojom::ServiceWorkerRegistrationObjectInfoPtr input) {
  if (!input) {
    return blink::WebServiceWorkerRegistrationObjectInfo(
        blink::mojom::kInvalidServiceWorkerRegistrationId, blink::WebURL(),
        blink::mojom::ServiceWorkerUpdateViaCache::kImports,
        {} /* host_remote */, {} /* receiver */,
        blink::mojom::ServiceWorkerObjectInfoPtr()
            .To<blink::WebServiceWorkerObjectInfo>() /* installing */,
        blink::mojom::ServiceWorkerObjectInfoPtr()
            .To<blink::WebServiceWorkerObjectInfo>() /* waiting */,
        blink::mojom::ServiceWorkerObjectInfoPtr()
            .To<blink::WebServiceWorkerObjectInfo>() /* active */);
  }
  return blink::WebServiceWorkerRegistrationObjectInfo(
      input->registration_id, input->scope, input->update_via_cache,
      std::move(input->host_remote), std::move(input->receiver),
      std::move(input->installing).To<blink::WebServiceWorkerObjectInfo>(),
      std::move(input->waiting).To<blink::WebServiceWorkerObjectInfo>(),
      std::move(input->active).To<blink::WebServiceWorkerObjectInfo>());
}

}  // namespace mojo
