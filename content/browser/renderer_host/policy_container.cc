// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/policy_container.h"

namespace content {

PolicyContainer::PolicyContainer() = default;
PolicyContainer::PolicyContainer(network::mojom::ReferrerPolicy referrer_policy)
    : referrer_policy_(referrer_policy) {}

void PolicyContainer::SetReferrerPolicy(
    network::mojom::ReferrerPolicy referrer_policy) {
  referrer_policy_ = referrer_policy;
}

std::unique_ptr<PolicyContainer> PolicyContainer::Clone() const {
  std::unique_ptr<PolicyContainer> copy =
      std::make_unique<PolicyContainer>(referrer_policy());
  return copy;
}

}  // namespace content
