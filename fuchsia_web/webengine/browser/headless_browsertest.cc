// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "base/test/scoped_command_line.h"
#include "content/public/test/browser_test.h"
#include "fuchsia_web/common/test/frame_for_test.h"
#include "fuchsia_web/common/test/frame_test_util.h"
#include "fuchsia_web/common/test/test_navigation_listener.h"
#include "fuchsia_web/webengine/switches.h"
#include "fuchsia_web/webengine/test/test_data.h"
#include "fuchsia_web/webengine/test/web_engine_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/switches.h"
#include "ui/ozone/public/ozone_switches.h"

namespace {

// Defines a suite of tests that exercise browser-level configuration and
// functionality.
class HeadlessTest : public WebEngineBrowserTest {
 public:
  HeadlessTest() { set_test_server_root(base::FilePath(kTestServerRoot)); }
  ~HeadlessTest() override = default;

  HeadlessTest(const HeadlessTest&) = delete;
  HeadlessTest& operator=(const HeadlessTest&) = delete;

 protected:
  void SetUp() override {
    command_line_.GetProcessCommandLine()->AppendSwitchNative(
        switches::kOzonePlatform, switches::kHeadless);
    command_line_.GetProcessCommandLine()->AppendSwitch(switches::kHeadless);
    WebEngineBrowserTest::SetUp();
  }

  base::test::ScopedCommandLine command_line_;
};

IN_PROC_BROWSER_TEST_F(HeadlessTest, AnimationRunsWhenFrameAlreadyActive) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kAnimationUrl(
      embedded_test_server()->GetURL("/css_animation.html"));

  auto frame =
      FrameForTest::Create(context(), fuchsia::web::CreateFrameParams());
  frame->EnableHeadlessRendering();
  LoadUrlAndExpectResponse(frame.GetNavigationController(),
                           fuchsia::web::LoadUrlParams(), kAnimationUrl.spec());
  frame.navigation_listener().RunUntilUrlAndTitleEquals(kAnimationUrl,
                                                        "animation finished");
}

IN_PROC_BROWSER_TEST_F(HeadlessTest, AnimationNeverRunsWhenInactive) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kAnimationUrl(
      embedded_test_server()->GetURL("/css_animation.html"));
  auto frame =
      FrameForTest::Create(context(), fuchsia::web::CreateFrameParams());
  LoadUrlAndExpectResponse(frame.GetNavigationController(),
                           fuchsia::web::LoadUrlParams(), kAnimationUrl.spec());
  frame.navigation_listener().RunUntilUrlAndTitleEquals(
      kAnimationUrl, "animation never started");
}

IN_PROC_BROWSER_TEST_F(HeadlessTest, ActivateFrameAfterDocumentLoad) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kAnimationUrl(
      embedded_test_server()->GetURL("/css_animation.html"));
  auto frame =
      FrameForTest::Create(context(), fuchsia::web::CreateFrameParams());
  LoadUrlAndExpectResponse(frame.GetNavigationController(),
                           fuchsia::web::LoadUrlParams(), kAnimationUrl.spec());
  frame.navigation_listener().RunUntilUrlAndTitleEquals(
      kAnimationUrl, "animation never started");

  frame->EnableHeadlessRendering();
  frame.navigation_listener().RunUntilUrlAndTitleEquals(kAnimationUrl,
                                                        "animation finished");
}

IN_PROC_BROWSER_TEST_F(HeadlessTest, DeactivationPreventsFutureAnimations) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kAnimationUrl(
      embedded_test_server()->GetURL("/css_animation.html"));

  // Activate the frame and load the page. The animation should run.
  auto frame =
      FrameForTest::Create(context(), fuchsia::web::CreateFrameParams());
  frame->EnableHeadlessRendering();
  LoadUrlAndExpectResponse(frame.GetNavigationController(),
                           fuchsia::web::LoadUrlParams(), kAnimationUrl.spec());
  frame.navigation_listener().RunUntilUrlAndTitleEquals(kAnimationUrl,
                                                        "animation finished");

  // Deactivate the page and reload it. The animation should no longer run.
  frame->DisableHeadlessRendering();
  frame.GetNavigationController()->Reload(fuchsia::web::ReloadType::NO_CACHE);
  frame.navigation_listener().RunUntilUrlAndTitleEquals(
      kAnimationUrl, "animation never started");
}

}  // namespace
