// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/test/web_engine_browser_test.h"

#include "base/files/file_path.h"
#include "content/public/test/browser_test.h"
#include "fuchsia/base/frame_test_util.h"
#include "fuchsia/base/test_navigation_listener.h"
#include "fuchsia/engine/browser/frame_impl.h"
#include "fuchsia/engine/test/test_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom.h"

class AutoplayTest : public cr_fuchsia::WebEngineBrowserTest {
 public:
  AutoplayTest() {
    set_test_server_root(base::FilePath(cr_fuchsia::kTestServerRoot));
  }
  ~AutoplayTest() override = default;

  AutoplayTest(const AutoplayTest&) = delete;
  AutoplayTest& operator=(const AutoplayTest&) = delete;

  void SetUpOnMainThread() override {
    CHECK(embedded_test_server()->Start());
    cr_fuchsia::WebEngineBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) final {
    SetHeadlessInCommandLine(command_line);
    cr_fuchsia::WebEngineBrowserTest::SetUpCommandLine(command_line);
  }

 protected:
  // Creates a Frame with |navigation_listener_| attached and |policy|
  // applied.
  fuchsia::web::FramePtr CreateFrame(fuchsia::web::AutoplayPolicy policy) {
    fuchsia::web::CreateFrameParams params;
    params.set_autoplay_policy(policy);
    fuchsia::web::FramePtr frame = WebEngineBrowserTest::CreateFrameWithParams(
        &navigation_listener_, std::move(params));
    frame->GetNavigationController(controller_.NewRequest());
    return frame;
  }

  cr_fuchsia::TestNavigationListener navigation_listener_;
  fuchsia::web::NavigationControllerPtr controller_;
};

IN_PROC_BROWSER_TEST_F(
    AutoplayTest,
    UserActivationPolicy_UserActivatedViaSimulatedInteraction) {
  const GURL kUrl(embedded_test_server()->GetURL("/play_vp8.html?autoplay=1"));
  constexpr const char kPageLoadedTitle[] = "initial title";

  fuchsia::web::FramePtr frame =
      CreateFrame(fuchsia::web::AutoplayPolicy::REQUIRE_USER_ACTIVATION);

  fuchsia::web::LoadUrlParams params;
  EXPECT_TRUE(
      cr_fuchsia::LoadUrlAndExpectResponse(controller_.get(), {}, kUrl.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl, kPageLoadedTitle);

  context_impl()
      ->GetFrameImplForTest(&frame)
      ->web_contents_for_test()
      ->GetMainFrame()
      ->NotifyUserActivation(
          blink::mojom::UserActivationNotificationType::kTest);

  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl, "playing");
}

IN_PROC_BROWSER_TEST_F(AutoplayTest,
                       UserActivationPolicy_UserActivatedNavigation) {
  const GURL kUrl(embedded_test_server()->GetURL("/play_vp8.html?autoplay=1"));

  fuchsia::web::FramePtr frame =
      CreateFrame(fuchsia::web::AutoplayPolicy::REQUIRE_USER_ACTIVATION);

  fuchsia::web::LoadUrlParams params;
  params.set_was_user_activated(true);

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller_.get(), std::move(params), kUrl.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl, "playing");
}

IN_PROC_BROWSER_TEST_F(AutoplayTest, UserActivationPolicy_NoUserActivation) {
  const GURL kUrl(embedded_test_server()->GetURL("/play_vp8.html?autoplay=1"));

  fuchsia::web::FramePtr frame =
      CreateFrame(fuchsia::web::AutoplayPolicy::REQUIRE_USER_ACTIVATION);

  fuchsia::web::LoadUrlParams params;
  params.set_was_user_activated(false);

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller_.get(), std::move(params), kUrl.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl, "blocked");
}

IN_PROC_BROWSER_TEST_F(AutoplayTest,
                       AllowAllPolicy_DefaultNotUserActivatedNavigation) {
  const GURL kUrl(embedded_test_server()->GetURL("/play_vp8.html?autoplay=1"));

  fuchsia::web::FramePtr frame =
      CreateFrame(fuchsia::web::AutoplayPolicy::ALLOW);

  // The page is deliberately not user activated.
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller_.get(), fuchsia::web::LoadUrlParams(), kUrl.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl, "playing");
}
