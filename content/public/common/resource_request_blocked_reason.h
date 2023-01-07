// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_RESOURCE_REQUEST_BLOCKED_REASON_H_
#define CONTENT_PUBLIC_COMMON_RESOURCE_REQUEST_BLOCKED_REASON_H_

#include "third_party/blink/public/platform/resource_request_blocked_reason.h"

namespace content {

// Extends blink::ResourceRequestBlockedReason with Content specific reasons.
enum class ResourceRequestBlockedReason {
  kContentStart =
      static_cast<int>(blink::ResourceRequestBlockedReason::kMax) + 1,

  // Any reasons specific to content/ should be added here, and the `kMax` value
  // below should be appropriately modified.
  kMax = kContentStart,
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_RESOURCE_REQUEST_BLOCKED_REASON_H_
