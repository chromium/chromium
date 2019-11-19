// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/test/web_engine_browser_test.h"

#include "base/files/file_path.h"
#include "base/test/bind_test_util.h"
#include "base/test/test_timeouts.h"
#include "fuchsia/base/frame_test_util.h"
#include "fuchsia/base/test_navigation_listener.h"
#include "fuchsia/engine/switches.h"
#include "fuchsia/engine/test/test_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MediaTest : public cr_fuchsia::WebEngineBrowserTest {
 public:
  MediaTest()
      : run_timeout_(TestTimeouts::action_timeout(),
                     base::MakeExpectedNotRunClosure(FROM_HERE)) {
    set_test_server_root(base::FilePath(cr_fuchsia::kTestServerRoot));
  }
  ~MediaTest() override = default;

  void SetUpOnMainThread() override {
    CHECK(embedded_test_server()->Start());
    cr_fuchsia::WebEngineBrowserTest::SetUpOnMainThread();
  }

 protected:
  // Creates a Frame with |navigation_listener_| attached.
  fuchsia::web::FramePtr CreateFrame() {
    return WebEngineBrowserTest::CreateFrame(&navigation_listener_);
  }

  cr_fuchsia::TestNavigationListener navigation_listener_;

 private:
  const base::RunLoop::ScopedRunTimeoutForTest run_timeout_;

  DISALLOW_COPY_AND_ASSIGN(MediaTest);
};

// VP8 can presently only be decoded in software.
// Verify that the --disable-software-video-decoders flag results in VP8
// media being reported as unplayable.
class SoftwareDecoderDisabledTest : public MediaTest {
 public:
  SoftwareDecoderDisabledTest() = default;
  ~SoftwareDecoderDisabledTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableSoftwareVideoDecoders);
    cr_fuchsia::WebEngineBrowserTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SoftwareDecoderDisabledTest);
};

IN_PROC_BROWSER_TEST_F(SoftwareDecoderDisabledTest, VP8IsTypeSupported) {
  const GURL kUrl(embedded_test_server()->GetURL("/can_play_vp8.html"));

  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), kUrl.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl, "can play vp8: false");
}

using SoftwareDecoderEnabledTest = MediaTest;

// Verify that VP8 is reported as playable if --disable-software-video-decoders
// is unset.
IN_PROC_BROWSER_TEST_F(SoftwareDecoderEnabledTest, VP8IsTypeSupported) {
  const GURL kUrl(embedded_test_server()->GetURL("/can_play_vp8.html"));

  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), kUrl.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl, "can play vp8: true");
}

// Verify that a VP8 video is loaded if --disable-software-video-decoders is
// unset.
IN_PROC_BROWSER_TEST_F(SoftwareDecoderEnabledTest, PlayVP8) {
  const GURL kUrl(embedded_test_server()->GetURL("/play_vp8.html"));

  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), kUrl.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl, "loaded");
}

// Verifies that VP8 videos won't play if --disable-software-video-decoders is
// set.
IN_PROC_BROWSER_TEST_F(SoftwareDecoderDisabledTest, PlayVP8Disabled) {
  const GURL kUrl(embedded_test_server()->GetURL("/play_vp8.html"));

  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), kUrl.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl, "error");
}

}  // namespace
