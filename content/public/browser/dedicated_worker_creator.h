// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEDICATED_WORKER_CREATOR_H_
#define CONTENT_PUBLIC_BROWSER_DEDICATED_WORKER_CREATOR_H_

#include "content/public/browser/global_routing_id.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace content {

// Represents a creator of a dedicated worker.
// Holds a GlobalRenderFrameHostId if the creator is a RenderFrameHost, and
// holds a blink::DedicatedWorkerToken for a nested worker.
using DedicatedWorkerCreator =
    absl::variant<GlobalRenderFrameHostId, blink::DedicatedWorkerToken>;

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEDICATED_WORKER_CREATOR_H_
