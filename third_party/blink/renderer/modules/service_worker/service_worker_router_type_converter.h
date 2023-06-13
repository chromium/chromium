// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_ROUTER_TYPE_CONVERTER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_ROUTER_TYPE_CONVERTER_H_

#include "mojo/public/cpp/bindings/type_converter.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_router_rule.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class RouterRule;

}  // namespace blink

namespace mojo {

template <>
struct MODULES_EXPORT
    TypeConverter<absl::optional<blink::ServiceWorkerRouterRule>,
                  blink::RouterRule*> {
  static absl::optional<blink::ServiceWorkerRouterRule> Convert(
      const blink::RouterRule* input);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_ROUTER_TYPE_CONVERTER_H_
