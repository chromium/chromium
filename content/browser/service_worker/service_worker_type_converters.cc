// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_type_converters.h"

#include "base/notreached.h"

namespace mojo {

// TODO(falken): TypeConverter is deprecated, and we should change
// ServiceWorkerVersion to just use the mojom enum, but it will be a huge change
// so we use the mojo type converter temporarily as an identifier to track.
blink::mojom::ServiceWorkerState
TypeConverter<blink::mojom::ServiceWorkerState,
              content::ServiceWorkerVersion::Status>::
    Convert(content::ServiceWorkerVersion::Status status) {
  switch (status) {
    case content::ServiceWorkerVersion::NEW:
      return blink::mojom::ServiceWorkerState::kParsed;
    case content::ServiceWorkerVersion::INSTALLING:
      return blink::mojom::ServiceWorkerState::kInstalling;
    case content::ServiceWorkerVersion::INSTALLED:
      return blink::mojom::ServiceWorkerState::kInstalled;
    case content::ServiceWorkerVersion::ACTIVATING:
      return blink::mojom::ServiceWorkerState::kActivating;
    case content::ServiceWorkerVersion::ACTIVATED:
      return blink::mojom::ServiceWorkerState::kActivated;
    case content::ServiceWorkerVersion::REDUNDANT:
      return blink::mojom::ServiceWorkerState::kRedundant;
  }
  NOTREACHED() << status;
  return blink::mojom::ServiceWorkerState::kParsed;
}

}  // namespace mojo
