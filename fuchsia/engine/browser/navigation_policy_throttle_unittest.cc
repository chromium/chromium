// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/navigation_policy_throttle.h"

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include "base/test/task_environment.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_renderer_host.h"
#include "fuchsia/engine/browser/fake_navigation_policy_provider.h"
#include "fuchsia/engine/browser/navigation_policy_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

class NavigationPolicyThrottleTest : public testing::Test {
 public:
  NavigationPolicyThrottleTest()
      : policy_provider_binding_(&policy_provider_) {}

  ~NavigationPolicyThrottleTest() override = default;

  NavigationPolicyThrottleTest(const NavigationPolicyThrottleTest&) = delete;
  NavigationPolicyThrottleTest& operator=(const NavigationPolicyThrottleTest&) =
      delete;

  void SetUp() override {
    fuchsia::web::NavigationPolicyProviderParams params;
    *params.mutable_main_frame_phases() = fuchsia::web::NavigationPhase::START;
    *params.mutable_subframe_phases() = fuchsia::web::NavigationPhase::REDIRECT;

    policy_handler_ = std::make_unique<NavigationPolicyHandler>(
        std::move(params), policy_provider_binding_.NewBinding());
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  std::unique_ptr<NavigationPolicyHandler> policy_handler_;
  fidl::Binding<fuchsia::web::NavigationPolicyProvider>
      policy_provider_binding_;
  FakeNavigationPolicyProvider policy_provider_;
};

TEST_F(NavigationPolicyThrottleTest, WillStartRequest) {
  policy_provider_.set_should_reject_request(false);
  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_is_same_document(true);

  NavigationPolicyThrottle throttle(&navigation_handle, policy_handler_.get());

  throttle.set_cancel_deferred_navigation_callback_for_testing(
      base::BindRepeating(
          [](content::NavigationThrottle::ThrottleCheckResult result) {
            EXPECT_EQ(content::NavigationThrottle::PROCEED, result);
          }));

  auto result = throttle.WillStartRequest();
  EXPECT_EQ(content::NavigationThrottle::DEFER, result);
}

TEST_F(NavigationPolicyThrottleTest, WillProcessResponse) {
  content::MockNavigationHandle navigation_handle;

  NavigationPolicyThrottle throttle(&navigation_handle, policy_handler_.get());

  throttle.set_cancel_deferred_navigation_callback_for_testing(
      base::BindRepeating(
          [](content::NavigationThrottle::ThrottleCheckResult result) {
            EXPECT_EQ(content::NavigationThrottle::PROCEED, result);
          }));

  auto result = throttle.WillProcessResponse();
  EXPECT_EQ(content::NavigationThrottle::PROCEED, result);
}

TEST_F(NavigationPolicyThrottleTest, WillRedirectRequest) {
  content::MockNavigationHandle navigation_handle;

  NavigationPolicyThrottle throttle(&navigation_handle, policy_handler_.get());

  throttle.set_cancel_deferred_navigation_callback_for_testing(
      base::BindRepeating(
          [](content::NavigationThrottle::ThrottleCheckResult result) {
            EXPECT_EQ(content::NavigationThrottle::PROCEED, result);
          }));

  auto result = throttle.WillProcessResponse();
  EXPECT_EQ(content::NavigationThrottle::PROCEED, result);
}
