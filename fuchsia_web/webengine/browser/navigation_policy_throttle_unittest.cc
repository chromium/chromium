// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/navigation_policy_throttle.h"

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include "base/test/task_environment.h"
#include "content/public/test/mock_navigation_handle.h"
#include "fuchsia_web/webengine/browser/fake_navigation_policy_provider.h"
#include "fuchsia_web/webengine/browser/navigation_policy_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kUrl1[] = "http://test.net/";
const char kUrl2[] = "http://page.net/";

void CheckRequestedNavigationFieldsEqual(
    fuchsia::web::RequestedNavigation* requested_navigation,
    const std::string& url,
    bool is_same_document) {
  ASSERT_TRUE(requested_navigation->has_url() &&
              requested_navigation->has_is_same_document());
  EXPECT_EQ(requested_navigation->url(), url);
  EXPECT_EQ(requested_navigation->is_same_document(), is_same_document);
}

}  // namespace

class MockNavigationPolicyHandle : public content::MockNavigationHandle {
 public:
  explicit MockNavigationPolicyHandle(const GURL& url)
      : content::MockNavigationHandle(url, nullptr) {}
  ~MockNavigationPolicyHandle() override = default;

  MockNavigationPolicyHandle(const MockNavigationPolicyHandle&) = delete;
  MockNavigationPolicyHandle& operator=(const MockNavigationPolicyHandle&) =
      delete;

  void set_is_main_frame(bool is_main_frame) { is_main_frame_ = is_main_frame; }

  bool IsInMainFrame() const override { return is_main_frame_; }

 private:
  bool is_main_frame_ = true;
};

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
    *params.mutable_main_frame_phases() =
        fuchsia::web::NavigationPhase::START |
        fuchsia::web::NavigationPhase::PROCESS_RESPONSE;
    *params.mutable_subframe_phases() =
        fuchsia::web::NavigationPhase::REDIRECT |
        fuchsia::web::NavigationPhase::FAIL;
    policy_handler_ = std::make_unique<NavigationPolicyHandler>(
        std::move(params), policy_provider_binding_.NewBinding());
  }

  FakeNavigationPolicyProvider* policy_provider() { return &policy_provider_; }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  std::unique_ptr<NavigationPolicyHandler> policy_handler_;
  fidl::Binding<fuchsia::web::NavigationPolicyProvider>
      policy_provider_binding_;
  FakeNavigationPolicyProvider policy_provider_;
};

// The navigation is expected to be evaluated, based on the params and
// NavigationPhase. The navigation is set to be aborted.
TEST_F(NavigationPolicyThrottleTest, WillStartRequest_MainFrame) {
  MockNavigationPolicyHandle navigation_handle((GURL(kUrl1)));
  navigation_handle.set_is_same_document(true);

  policy_provider()->set_should_abort_navigation(true);
  NavigationPolicyThrottle throttle(&navigation_handle, policy_handler_.get());
  auto result = throttle.WillStartRequest();
  EXPECT_EQ(content::NavigationThrottle::DEFER, result);

  base::RunLoop run_loop;
  throttle.set_cancel_deferred_navigation_callback_for_testing(
      base::BindRepeating(
          [](base::RunLoop* run_loop,
             content::NavigationThrottle::ThrottleCheckResult result) {
            EXPECT_EQ(content::NavigationThrottle::CANCEL, result);
            run_loop->Quit();
          },
          base::Unretained(&run_loop)));
  run_loop.Run();

  CheckRequestedNavigationFieldsEqual(policy_provider()->requested_navigation(),
                                      kUrl1, true);
  EXPECT_EQ(policy_provider()->num_evaluated_navigations(), 1);
}

// Based on the params, the client is not interested in WillStartRequests for
// subframes. It will not be evaluated and the navigation is expected to
// proceed, even if the NavigationPolicyProvider is set to abort the current
// request.
TEST_F(NavigationPolicyThrottleTest, WillStartRequest_SubFrame) {
  MockNavigationPolicyHandle navigation_handle((GURL(kUrl2)));
  navigation_handle.set_is_main_frame(false);
  navigation_handle.set_is_same_document(false);

  policy_provider()->set_should_abort_navigation(true);
  NavigationPolicyThrottle throttle(&navigation_handle, policy_handler_.get());
  auto result = throttle.WillStartRequest();

  EXPECT_EQ(content::NavigationThrottle::PROCEED, result);
}

// This is equivalent to WillStartRequest_SubFrame with a different
// NavigationPhase.
TEST_F(NavigationPolicyThrottleTest, WillRedirectRequest) {
  MockNavigationPolicyHandle navigation_handle((GURL(kUrl2)));
  navigation_handle.set_is_same_document(false);

  policy_provider()->set_should_abort_navigation(true);
  NavigationPolicyThrottle throttle(&navigation_handle, policy_handler_.get());
  auto result = throttle.WillRedirectRequest();

  EXPECT_EQ(content::NavigationThrottle::PROCEED, result);
}

// The navigation will be evaluated, and will proceed due to the value set in
// |policy_provider_|.
TEST_F(NavigationPolicyThrottleTest, WillFailRequest) {
  MockNavigationPolicyHandle navigation_handle((GURL(kUrl1)));
  navigation_handle.set_is_main_frame(false);
  navigation_handle.set_is_same_document(true);

  policy_provider()->set_should_abort_navigation(false);
  NavigationPolicyThrottle throttle(&navigation_handle, policy_handler_.get());
  auto result = throttle.WillFailRequest();
  EXPECT_EQ(content::NavigationThrottle::DEFER, result);

  base::RunLoop run_loop;
  throttle.set_resume_callback_for_testing(run_loop.QuitClosure());
  run_loop.Run();

  CheckRequestedNavigationFieldsEqual(policy_provider()->requested_navigation(),
                                      kUrl1, true);
  EXPECT_EQ(policy_provider()->num_evaluated_navigations(), 1);
}

// This navigation will be evaluated and will proceed.
TEST_F(NavigationPolicyThrottleTest, WillProcessResponse) {
  MockNavigationPolicyHandle navigation_handle((GURL(kUrl2)));
  navigation_handle.set_is_same_document(true);

  policy_provider()->set_should_abort_navigation(false);
  NavigationPolicyThrottle throttle(&navigation_handle, policy_handler_.get());
  auto result = throttle.WillProcessResponse();
  EXPECT_EQ(content::NavigationThrottle::DEFER, result);

  base::RunLoop run_loop;
  throttle.set_resume_callback_for_testing(run_loop.QuitClosure());
  run_loop.Run();

  CheckRequestedNavigationFieldsEqual(policy_provider()->requested_navigation(),
                                      kUrl2, true);
  EXPECT_EQ(policy_provider()->num_evaluated_navigations(), 1);
}
