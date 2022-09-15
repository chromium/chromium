// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_TYPE_CONVERTERS_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_TYPE_CONVERTERS_H_

#include "content/browser/service_worker/service_worker_version.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_state.mojom.h"

namespace mojo {

template <>
struct TypeConverter<blink::mojom::ServiceWorkerState,
                     content::ServiceWorkerVersion::Status> {
  static blink::mojom::ServiceWorkerState Convert(
      content::ServiceWorkerVersion::Status status);
};

}  // namespace mojo

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_TYPE_CONVERTERS_H_
