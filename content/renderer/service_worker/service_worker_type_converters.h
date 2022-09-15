// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_TYPE_CONVERTERS_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_TYPE_CONVERTERS_H_

#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom-forward.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_object_info.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_registration_object_info.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_proxy.h"

namespace mojo {

template <>
struct TypeConverter<blink::WebServiceWorkerObjectInfo,
                     blink::mojom::ServiceWorkerObjectInfoPtr> {
  static blink::WebServiceWorkerObjectInfo Convert(
      blink::mojom::ServiceWorkerObjectInfoPtr input);
};

template <>
struct TypeConverter<blink::WebServiceWorkerRegistrationObjectInfo,
                     blink::mojom::ServiceWorkerRegistrationObjectInfoPtr> {
  static blink::WebServiceWorkerRegistrationObjectInfo Convert(
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr input);
};

}  // namespace

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_TYPE_CONVERTERS_H_
