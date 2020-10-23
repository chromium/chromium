// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_policy_container.h"

namespace blink {

WebPolicyContainerClient::WebPolicyContainerClient(
    WebPolicyContainerData policies,
    CrossVariantMojoAssociatedRemote<mojom::PolicyContainerHostInterfaceBase>
        remote)
    : policies(std::move(policies)), remote(std::move(remote)) {}

}  // namespace blink
