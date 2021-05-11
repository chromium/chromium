// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "base/macros.h"
#include "base/test/scoped_command_line.h"
#include "content/public/test/browser_test.h"
#include "fuchsia/base/test/frame_test_util.h"
#include "fuchsia/base/test/test_navigation_listener.h"
#include "fuchsia/engine/switches.h"
#include "fuchsia/engine/test/test_data.h"
#include "fuchsia/engine/test/web_engine_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/switches.h"
#include "ui/ozone/public/ozone_switches.h"

namespace {

// Defines a suite of tests that exercise browser-level configuration and
// functionality.
class HeadlessTest : public cr_fuchsia::WebEngineBrowserTest {
 public:
  HeadlessTest() {
    set_test_server_root(base::FilePath(cr_fuchsia::kTestServerRoot));
  }
  ~HeadlessTest() override = default;

 protected:
  void SetUp() override {
    command_line_.GetProcessCommandLine()->AppendSwitchNative(
        switches::kOzonePlatform, switches::kHeadless);
    command_line_.GetProcessCommandLine()->AppendSwitch(switches::kHeadless);
    cr_fuchsia::WebEngineBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    frame_ = CreateFrame();
    frame_->GetNavigationController(controller_.NewRequest());
  }

  // Creates a Frame with |navigation_listener_| attached.
  //
  fuchsia::web::FramePtr CreateFrame() {
    return WebEngineBrowserTest::CreateFrame(&navigation_listener_);
  }

  fuchsia::web::FramePtr frame_;
  fuchsia::web::NavigationControllerPtr controller_;
  cr_fuchsia::TestNavigationListener navigation_listener_;
  base::test::ScopedCommandLine command_line_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HeadlessTest);
};

IN_PROC_BROWSER_TEST_F(HeadlessTest, AnimationRunsWhenFrameAlreadyActive) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kAnimationUrl(
      embedded_test_server()->GetURL("/css_animation.html"));
  frame_->EnableHeadlessRendering();
  cr_fuchsia::LoadUrlAndExpectResponse(
      controller_.get(), fuchsia::web::LoadUrlParams(), kAnimationUrl.spec());
  navigation_listener_.RunUntilUrlAndTitleEquals(kAnimationUrl,
                                                 "animation finished");
}

IN_PROC_BROWSER_TEST_F(HeadlessTest, AnimationNeverRunsWhenInactive) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kAnimationUrl(
      embedded_test_server()->GetURL("/css_animation.html"));
  cr_fuchsia::LoadUrlAndExpectResponse(
      controller_.get(), fuchsia::web::LoadUrlParams(), kAnimationUrl.spec());
  navigation_listener_.RunUntilUrlAndTitleEquals(kAnimationUrl,
                                                 "animation never started");
}

IN_PROC_BROWSER_TEST_F(HeadlessTest, ActivateFrameAfterDocumentLoad) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kAnimationUrl(
      embedded_test_server()->GetURL("/css_animation.html"));
  cr_fuchsia::LoadUrlAndExpectResponse(
      controller_.get(), fuchsia::web::LoadUrlParams(), kAnimationUrl.spec());
  navigation_listener_.RunUntilUrlAndTitleEquals(kAnimationUrl,
                                                 "animation never started");

  frame_->EnableHeadlessRendering();
  navigation_listener_.RunUntilUrlAndTitleEquals(kAnimationUrl,
                                                 "animation finished");
}

IN_PROC_BROWSER_TEST_F(HeadlessTest, DeactivationPreventsFutureAnimations) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kAnimationUrl(
      embedded_test_server()->GetURL("/css_animation.html"));

  // Activate the frame and load the page. The animation should run.
  frame_->EnableHeadlessRendering();
  cr_fuchsia::LoadUrlAndExpectResponse(
      controller_.get(), fuchsia::web::LoadUrlParams(), kAnimationUrl.spec());
  navigation_listener_.RunUntilUrlAndTitleEquals(kAnimationUrl,
                                                 "animation finished");

  // Deactivate the page and reload it. The animation should no longer run.
  frame_->DisableHeadlessRendering();
  controller_->Reload(fuchsia::web::ReloadType::NO_CACHE);
  navigation_listener_.RunUntilUrlAndTitleEquals(kAnimationUrl,
                                                 "animation never started");
}

}  // namespace
