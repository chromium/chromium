// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/policy_container_host.h"

#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(PolicyContainerTest, ReferrerPolicy) {
  PolicyContainerHost policy_container;
  EXPECT_EQ(network::mojom::ReferrerPolicy::kDefault,
            policy_container.referrer_policy());

  static_cast<blink::mojom::PolicyContainerHost*>(&policy_container)
      ->SetReferrerPolicy(network::mojom::ReferrerPolicy::kAlways);
  EXPECT_EQ(network::mojom::ReferrerPolicy::kAlways,
            policy_container.referrer_policy());
}

}  // namespace content
