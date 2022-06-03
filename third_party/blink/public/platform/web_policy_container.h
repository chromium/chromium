// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_POLICY_CONTAINER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_POLICY_CONTAINER_H_

#include "services/network/public/mojom/ip_address_space.mojom-shared.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "third_party/blink/public/mojom/frame/policy_container.mojom-shared.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/public/platform/web_content_security_policy_struct.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace blink {

// TODO(antoniosartori): Remove this when CommitNavigation IPC will be handled
// directly in blink.
struct WebPolicyContainerPolicies {
  network::mojom::ReferrerPolicy referrer_policy;
  network::mojom::IPAddressSpace ip_address_space;
  WebVector<WebContentSecurityPolicy> content_security_policies;
};

// TODO(antoniosartori): Remove this when CommitNavigation IPC will be handled
// directly in blink.
struct WebPolicyContainer {
  WebPolicyContainer(
      WebPolicyContainerPolicies policies,
      CrossVariantMojoAssociatedRemote<mojom::PolicyContainerHostInterfaceBase>
          remote)
      : policies(std::move(policies)), remote(std::move(remote)) {}

  WebPolicyContainerPolicies policies;
  CrossVariantMojoAssociatedRemote<mojom::PolicyContainerHostInterfaceBase>
      remote;
};

struct WebPolicyContainerBindParams {
  CrossVariantMojoAssociatedReceiver<mojom::PolicyContainerHostInterfaceBase>
      receiver;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_POLICY_CONTAINER_H_
