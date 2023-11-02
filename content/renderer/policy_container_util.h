// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_POLICY_CONTAINER_UTIL_H_
#define CONTENT_RENDERER_POLICY_CONTAINER_UTIL_H_

#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/frame/policy_container.mojom-forward.h"
#include "third_party/blink/public/platform/web_policy_container.h"

namespace content {

// Convert a PolicyContainer into a WebPolicyContainer. These two
// classes represent the exact same thing, but one is in content, the other is
// in blink.
CONTENT_EXPORT
std::unique_ptr<blink::WebPolicyContainer> ToWebPolicyContainer(
    blink::mojom::PolicyContainerPtr);

}  // namespace content

#endif  // CONTENT_RENDERER_POLICY_CONTAINER_UTIL_H_
