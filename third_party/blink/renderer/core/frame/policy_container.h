// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_POLICY_CONTAINER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_POLICY_CONTAINER_H_

#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "third_party/blink/public/mojom/frame/policy_container.mojom-blink.h"
#include "third_party/blink/public/platform/web_policy_container.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

// PolicyContainer serves as a container for several security policies to be
// applied to a document. It is constructed at commit time with the data passed
// by the RenderFrameHost. Some member policies of the policy container can be
// updated also by Blink (this generally happens when Blink parses meta
// tags). The corresponding setters trigger also an update in the corresponding
// content::PolicyContainer owned by the RenderFrameHost via a mojo IPC.
class CORE_EXPORT PolicyContainer {
 public:
  PolicyContainer() = delete;
  PolicyContainer(
      mojo::PendingAssociatedRemote<mojom::blink::PolicyContainerHost> remote,
      mojom::blink::PolicyContainerDataPtr policies);
  PolicyContainer(const PolicyContainer&) = delete;
  PolicyContainer& operator=(const PolicyContainer&) = delete;
  ~PolicyContainer() = default;

  static std::unique_ptr<PolicyContainer> CreateFromWebPolicyContainerClient(
      std::unique_ptr<WebPolicyContainerClient> container);

  // Change the Referrer Policy and sync the new policy with the corresponding
  // PolicyContainer owned by the RenderFrameHost.
  void UpdateReferrerPolicy(network::mojom::blink::ReferrerPolicy policy);
  network::mojom::blink::ReferrerPolicy GetReferrerPolicy() const;

  const mojom::blink::PolicyContainerDataPtr& GetPolicies() const;

 private:
  mojom::blink::PolicyContainerDataPtr policies_;

  mojo::AssociatedRemote<mojom::blink::PolicyContainerHost>
      policy_container_host_remote_;
};

}  // namespace blink

#endif
