// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CLIENT_INFO_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CLIENT_INFO_H_

#include "content/public/browser/global_routing_id.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace content {

using DedicatedOrSharedWorkerToken =
    absl::variant<blink::DedicatedWorkerToken, blink::SharedWorkerToken>;

using ServiceWorkerClientInfo = absl::variant<GlobalRenderFrameHostId,
                                              blink::DedicatedWorkerToken,
                                              blink::SharedWorkerToken>;

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CLIENT_INFO_H_
