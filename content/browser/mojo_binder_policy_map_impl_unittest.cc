// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mojo_binder_policy_map_impl.h"

#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "content/browser/browser_interface_binders.h"
#include "content/public/test/mojo_capability_control_test_interfaces.mojom.h"
#include "content/test/test_content_browser_client.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class MojoBinderPolicyMapImplTest : public testing::Test {
 public:
  MojoBinderPolicyMapImplTest() = default;

 private:
  base::test::TaskEnvironment task_environment_;
};

// Verifies the SetNonAssociatedPolicy method works.
TEST_F(MojoBinderPolicyMapImplTest, SetNonAssociatedPolicy) {
  MojoBinderPolicyMapImpl policy_map;
  policy_map.SetNonAssociatedPolicy<content::mojom::TestInterfaceForDefer>(
      MojoBinderNonAssociatedPolicy::kDefer);
  EXPECT_EQ(
      policy_map.GetNonAssociatedMojoBinderPolicyOrDieForTesting(
          mojo::Remote<
              content::mojom::TestInterfaceForDefer>::InterfaceType::Name_),
      MojoBinderNonAssociatedPolicy::kDefer);
}

// Verifies the SetAssociatedPolicy method works.
TEST_F(MojoBinderPolicyMapImplTest, SetAssociatedPolicy) {
  MojoBinderPolicyMapImpl policy_map;
  policy_map.SetAssociatedPolicy<content::mojom::TestInterfaceForDefer>(
      MojoBinderAssociatedPolicy::kGrant);
  EXPECT_EQ(
      policy_map.GetAssociatedMojoBinderPolicyOrDieForTesting(
          mojo::Remote<
              content::mojom::TestInterfaceForDefer>::InterfaceType::Name_),
      MojoBinderAssociatedPolicy::kGrant);
}

// Verifies if the given interface is not found in the map,
// GetNonAssociatedMojoBinderPolicy will return the given `default_policy`.
TEST_F(MojoBinderPolicyMapImplTest, InterfaceNotFound) {
  MojoBinderPolicyMapImpl policy_map;
  EXPECT_EQ(
      policy_map.GetNonAssociatedMojoBinderPolicy(
          mojo::Remote<
              content::mojom::TestInterfaceForDefer>::InterfaceType::Name_,
          MojoBinderNonAssociatedPolicy::kDefer),
      MojoBinderNonAssociatedPolicy::kDefer);
  EXPECT_EQ(
      policy_map.GetNonAssociatedMojoBinderPolicy(
          mojo::Remote<
              content::mojom::TestInterfaceForDefer>::InterfaceType::Name_,
          MojoBinderNonAssociatedPolicy::kCancel),
      MojoBinderNonAssociatedPolicy::kCancel);
}

class MojoBinderPolicyTestContentBrowserClient
    : public TestContentBrowserClient {
 public:
  void RegisterMojoBinderPoliciesForSameOriginPrerendering(
      MojoBinderPolicyMap& policy_map) override {
    policy_map.SetNonAssociatedPolicy<content::mojom::TestInterfaceForDefer>(
        MojoBinderNonAssociatedPolicy::kDefer);
  }
};

// Verifies the embedder can register its policies via
// ContentBrowserClient::RegisterMojoBinderPoliciesForSameOriginPrerendering.
TEST_F(MojoBinderPolicyMapImplTest, RegisterMojoBinderPolicyMap) {
  MojoBinderPolicyTestContentBrowserClient test_browser_client;
  MojoBinderPolicyMapImpl policy_map;
  test_browser_client.RegisterMojoBinderPoliciesForSameOriginPrerendering(
      policy_map);
  EXPECT_EQ(
      policy_map.GetNonAssociatedMojoBinderPolicyOrDieForTesting(
          mojo::Remote<
              content::mojom::TestInterfaceForDefer>::InterfaceType::Name_),
      MojoBinderNonAssociatedPolicy::kDefer);
}

}  // namespace

}  // namespace content
