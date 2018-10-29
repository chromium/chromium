// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_SERVICE_WORKER_WEB_SERVICE_WORKER_CLIENTS_INFO_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_SERVICE_WORKER_WEB_SERVICE_WORKER_CLIENTS_INFO_H_

#include "services/network/public/mojom/request_context_frame_type.mojom-shared.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-shared.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom-shared.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"

namespace blink {

// The only usage of this class is to carry the source of an extendable message
// dispatched via content.mojom.ServiceWorker across the boundary of Content and
// Blink.
// TODO(crbug.com/879019): Remove this class once we move
// content.mojom.ServiceWorker impl into Blink.
struct WebServiceWorkerClientInfo {
  WebServiceWorkerClientInfo()
      : page_visibility_state(mojom::PageVisibilityState::kMaxValue),
        is_focused(false),
        frame_type(network::mojom::RequestContextFrameType::kNone),
        client_type(mojom::ServiceWorkerClientType::kWindow) {}

  WebString uuid;

  mojom::PageVisibilityState page_visibility_state;
  bool is_focused;
  WebURL url;
  network::mojom::RequestContextFrameType frame_type;
  mojom::ServiceWorkerClientType client_type;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_SERVICE_WORKER_WEB_SERVICE_WORKER_CLIENTS_INFO_H_
