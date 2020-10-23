// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_POLICY_CONTAINER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_POLICY_CONTAINER_H_

#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "third_party/blink/public/mojom/frame/policy_container.mojom-shared.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"

namespace blink {

// TODO(antoniosartori): Remove this when CommitNavigation IPC will be handled
// directly in blink.
struct WebPolicyContainerData {
  network::mojom::ReferrerPolicy referrer_policy;
};

// TODO(antoniosartori): Remove this when CommitNavigation IPC will be handled
// directly in blink.
struct WebPolicyContainerClient {
  WebPolicyContainerClient(
      WebPolicyContainerData policies,
      CrossVariantMojoAssociatedRemote<mojom::PolicyContainerHostInterfaceBase>
          remote);

  WebPolicyContainerData policies;
  CrossVariantMojoAssociatedRemote<mojom::PolicyContainerHostInterfaceBase>
      remote;
};

}  // namespace blink

#endif
