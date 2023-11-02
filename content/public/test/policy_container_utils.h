// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_POLICY_CONTAINER_UTILS_H_
#define CONTENT_PUBLIC_TEST_POLICY_CONTAINER_UTILS_H_

#include "third_party/blink/public/mojom/frame/policy_container.mojom-forward.h"

namespace content {

blink::mojom::PolicyContainerPtr CreateStubPolicyContainer();

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_POLICY_CONTAINER_UTILS_H_
