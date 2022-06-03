// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/service_worker/service_worker_type_converters.h"

#include "base/notreached.h"

namespace mojo {

blink::ServiceWorkerStatusCode
TypeConverter<blink::ServiceWorkerStatusCode,
              blink::mojom::ServiceWorkerEventStatus>::
    Convert(blink::mojom::ServiceWorkerEventStatus status) {
  switch (status) {
    case blink::mojom::ServiceWorkerEventStatus::COMPLETED:
      return blink::ServiceWorkerStatusCode::kOk;
    case blink::mojom::ServiceWorkerEventStatus::REJECTED:
      return blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected;
    case blink::mojom::ServiceWorkerEventStatus::ABORTED:
      return blink::ServiceWorkerStatusCode::kErrorAbort;
    case blink::mojom::ServiceWorkerEventStatus::TIMEOUT:
      return blink::ServiceWorkerStatusCode::kErrorTimeout;
  }
  NOTREACHED() << status;
  return blink::ServiceWorkerStatusCode::kErrorFailed;
}

blink::ServiceWorkerStatusCode
TypeConverter<blink::ServiceWorkerStatusCode,
              blink::mojom::ServiceWorkerStartStatus>::
    Convert(blink::mojom::ServiceWorkerStartStatus status) {
  switch (status) {
    case blink::mojom::ServiceWorkerStartStatus::kNormalCompletion:
      return blink::ServiceWorkerStatusCode::kOk;
    case blink::mojom::ServiceWorkerStartStatus::kAbruptCompletion:
      return blink::ServiceWorkerStatusCode::kErrorScriptEvaluateFailed;
  }
  NOTREACHED() << status;
  return blink::ServiceWorkerStatusCode::kErrorFailed;
}

}  // namespace mojo
