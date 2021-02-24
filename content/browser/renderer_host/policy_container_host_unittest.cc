// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/policy_container_host.h"

#include "base/run_loop.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(PolicyContainerHostTest, ReferrerPolicy) {
  scoped_refptr<PolicyContainerHost> policy_container =
      base::MakeRefCounted<PolicyContainerHost>();
  EXPECT_EQ(network::mojom::ReferrerPolicy::kDefault,
            policy_container->referrer_policy());

  static_cast<blink::mojom::PolicyContainerHost*>(policy_container.get())
      ->SetReferrerPolicy(network::mojom::ReferrerPolicy::kAlways);
  EXPECT_EQ(network::mojom::ReferrerPolicy::kAlways,
            policy_container->referrer_policy());
}

TEST(PolicyContainerHostTest, AssociateWithFrameToken) {
  // We need to satisfy DCHECK_CURRENTLY_ON(BrowserThread::UI).
  content::BrowserTaskEnvironment task_environment;

  scoped_refptr<PolicyContainerHost> policy_container_host =
      base::MakeRefCounted<PolicyContainerHost>();
  blink::LocalFrameToken token;
  policy_container_host->AssociateWithFrameToken(token);
  EXPECT_EQ(policy_container_host.get(),
            PolicyContainerHost::FromFrameToken(token));

  // Check that we can associate a new PolicyContainerHost to the same frame
  // token and everything works correctly.
  scoped_refptr<PolicyContainerHost> policy_container_host_2 =
      base::MakeRefCounted<PolicyContainerHost>();
  policy_container_host_2->AssociateWithFrameToken(token);
  EXPECT_EQ(policy_container_host_2.get(),
            PolicyContainerHost::FromFrameToken(token));
}

TEST(PolicyContainerHostTest, KeepAliveThroughBlinkPolicyContainerRemote) {
  // Enable tasks and RunLoop on the main thread and satisfy
  // DCHECK_CURRENTLY_ON(BrowserThread::UI).
  content::BrowserTaskEnvironment task_environment;

  scoped_refptr<PolicyContainerHost> policy_container_host =
      base::MakeRefCounted<PolicyContainerHost>();
  blink::LocalFrameToken token;
  policy_container_host->AssociateWithFrameToken(token);

  blink::mojom::PolicyContainerPtr blink_policy_container =
      policy_container_host->CreatePolicyContainerForBlink();

  PolicyContainerHost* raw_pointer = policy_container_host.get();
  EXPECT_EQ(raw_pointer, PolicyContainerHost::FromFrameToken(token));

  policy_container_host.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(raw_pointer, PolicyContainerHost::FromFrameToken(token));

  blink_policy_container->remote.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(PolicyContainerHost::FromFrameToken(token));
}

// Test that the disconnect handler is called when all keep alive handles
// disconnect.
TEST(PolicyContainerHostTest, KeepAliveThroughKeepAlives) {
  // Enable tasks and RunLoop on the main thread and satisfy
  // DCHECK_CURRENTLY_ON(BrowserThread::UI).
  content::BrowserTaskEnvironment task_environment;

  scoped_refptr<PolicyContainerHost> policy_container_host =
      base::MakeRefCounted<PolicyContainerHost>();
  blink::LocalFrameToken token;
  policy_container_host->AssociateWithFrameToken(token);

  mojo::PendingRemote<blink::mojom::PolicyContainerHostKeepAliveHandle>
      keep_alive;
  policy_container_host->IssueKeepAliveHandle(
      keep_alive.InitWithNewPipeAndPassReceiver());

  PolicyContainerHost* raw_pointer = policy_container_host.get();
  EXPECT_EQ(raw_pointer, PolicyContainerHost::FromFrameToken(token));

  policy_container_host.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(raw_pointer, PolicyContainerHost::FromFrameToken(token));

  keep_alive.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(PolicyContainerHost::FromFrameToken(token));
}

}  // namespace content
