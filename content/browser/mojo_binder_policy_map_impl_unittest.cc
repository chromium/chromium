// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mojo_binder_policy_map_impl.h"

#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "content/browser/browser_interface_binders.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_mojo_binder_policy_applier_unittest.mojom.h"
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

// Verifies SetPolicy function works.
TEST_F(MojoBinderPolicyMapImplTest, SetPolicy) {
  MojoBinderPolicyMapImpl policy_map;
  policy_map.SetPolicy<content::mojom::TestInterfaceForDefer>(
      MojoBinderPolicy::kDefer);
  EXPECT_EQ(
      policy_map.GetMojoBinderPolicyOrDieForTesting(
          mojo::Remote<
              content::mojom::TestInterfaceForDefer>::InterfaceType::Name_),
      MojoBinderPolicy::kDefer);
}

// Verifies if the given interface is not found in the map, GetMojoBinderPolicy
// will return the given `default_policy`.
TEST_F(MojoBinderPolicyMapImplTest, InterfaceNotFound) {
  MojoBinderPolicyMapImpl policy_map;
  EXPECT_EQ(
      policy_map.GetMojoBinderPolicy(
          mojo::Remote<
              content::mojom::TestInterfaceForDefer>::InterfaceType::Name_,
          MojoBinderPolicy::kDefer),
      MojoBinderPolicy::kDefer);
  EXPECT_EQ(
      policy_map.GetMojoBinderPolicy(
          mojo::Remote<
              content::mojom::TestInterfaceForDefer>::InterfaceType::Name_,
          MojoBinderPolicy::kCancel),
      MojoBinderPolicy::kCancel);
}

class MojoBinderPolicyTestContentBrowserClient
    : public TestContentBrowserClient {
 public:
  void RegisterMojoBinderPoliciesForSameOriginPrerendering(
      MojoBinderPolicyMap& policy_map) override {
    policy_map.SetPolicy<content::mojom::TestInterfaceForDefer>(
        MojoBinderPolicy::kDefer);
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
      policy_map.GetMojoBinderPolicyOrDieForTesting(
          mojo::Remote<
              content::mojom::TestInterfaceForDefer>::InterfaceType::Name_),
      MojoBinderPolicy::kDefer);
}

}  // namespace

}  // namespace content
