// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/test/web_engine_browser_test.h"

#include <fuchsia/mediacodec/cpp/fidl_test_base.h>

#include "base/files/file_path.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "content/public/test/browser_test.h"
#include "fuchsia/base/test/frame_test_util.h"
#include "fuchsia/base/test/test_navigation_listener.h"
#include "fuchsia/engine/switches.h"
#include "fuchsia/engine/test/test_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

// Currently, VP8 can only be decoded in software, and VP9 can be decoded in
// hardware and software. The tests rely on this.
// TODO(crbug.com/1207695): Rename play_vp8.html to play_video.html.
constexpr char kLoadSoftwareOnlyCodecUrl[] = "/play_vp8.html?codecs=vp8";
constexpr char kLoadHardwareAndSoftwareCodecUrl[] = "/play_vp8.html?codecs=vp9";
constexpr char kCanPlaySoftwareOnlyCodecUrl[] = "/can_play_vp8.html";

}  // namespace

class MediaTest : public cr_fuchsia::WebEngineBrowserTest {
 public:
  MediaTest() {
    set_test_server_root(base::FilePath(cr_fuchsia::kTestServerRoot));
  }
  ~MediaTest() override = default;

  MediaTest(const MediaTest&) = delete;
  MediaTest& operator=(const MediaTest&) = delete;

 protected:
  void SetUpOnMainThread() override {
    CHECK(embedded_test_server()->Start());
    cr_fuchsia::WebEngineBrowserTest::SetUpOnMainThread();
  }

  // Creates a Frame with |navigation_listener_| attached.
  fuchsia::web::FramePtr CreateFrame() {
    return WebEngineBrowserTest::CreateFrame(&navigation_listener_);
  }

  cr_fuchsia::TestNavigationListener navigation_listener_;
};

using SoftwareDecoderEnabledTest = MediaTest;

// MediaTest with switches::kDisableSoftwareVideoDecoders.
class SoftwareDecoderDisabledTest : public MediaTest {
 public:
  SoftwareDecoderDisabledTest() = default;
  ~SoftwareDecoderDisabledTest() override = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kDisableSoftwareVideoDecoders);
    MediaTest::SetUpCommandLine(command_line);
  }
};

// SoftwareDecoderDisabledTest with fuchsia.mediacodec.CodecFactory
// disconnected.
class SoftwareDecoderDisabledAndHardwareDecoderFailureTest
    : public SoftwareDecoderDisabledTest {
 public:
  SoftwareDecoderDisabledAndHardwareDecoderFailureTest() = default;
  ~SoftwareDecoderDisabledAndHardwareDecoderFailureTest() override = default;

 protected:
  // Removes the decoder service to cause calls to it to fail.
  void SetUpOnMainThread() override {
    component_context_.emplace(
        base::TestComponentContextForProcess::InitialState::kCloneAll);
    component_context_->additional_services()
        ->RemovePublicService<fuchsia::mediacodec::CodecFactory>();

    SoftwareDecoderDisabledTest::SetUpOnMainThread();
  }

  // Used to disconnect fuchsia.mediacodec.CodecFactory.
  absl::optional<base::TestComponentContextForProcess> component_context_;
};

// Verify that a codec only supported by a software decoder is reported as
// playable if kDisableSoftwareVideoDecoders is not present.
IN_PROC_BROWSER_TEST_F(SoftwareDecoderEnabledTest,
                       CanPlayTypeSoftwareOnlyCodecIsTrue) {
  const GURL kUrl(embedded_test_server()->GetURL(kCanPlaySoftwareOnlyCodecUrl));

  // TODO(crbug.com/1200314): Refactor these tests to use FrameForTest and
  // possibly to simplify the calls below since some of the details are not
  // interesting to the individual tests. In particular, has_user_activation is
  // more relevant than speclific LoadUrlParams.
  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), kUrl.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl, "can play vp8: true");
}

// Verify that a codec only supported by a software decoder is reported as not
// playable if kDisableSoftwareVideoDecoders is present.
IN_PROC_BROWSER_TEST_F(SoftwareDecoderDisabledTest,
                       CanPlayTypeSoftwareOnlyCodecIsFalse) {
  const GURL kUrl(embedded_test_server()->GetURL(kCanPlaySoftwareOnlyCodecUrl));

  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), kUrl.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl, "can play vp8: false");
}

// Verify that a codec only supported by a software decoder is loaded if
// kDisableSoftwareVideoDecoders is not present.
IN_PROC_BROWSER_TEST_F(SoftwareDecoderEnabledTest,
                       PlaySoftwareOnlyCodecSucceeds) {
  const GURL kUrl(embedded_test_server()->GetURL(kLoadSoftwareOnlyCodecUrl));

  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), kUrl.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl, "loaded");
}

// Verify that a codec only supported by a software decoder is not loaded if
// kDisableSoftwareVideoDecoders is present.
IN_PROC_BROWSER_TEST_F(SoftwareDecoderDisabledTest,
                       LoadSoftwareOnlyCodecFails) {
  const GURL kUrl(embedded_test_server()->GetURL(kLoadSoftwareOnlyCodecUrl));

  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), kUrl.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl, "media element error");
}

// Verify that a codec supported by hardware and software decoders plays if
// kDisableSoftwareVideoDecoders is present.
// Unlike the software-only codec, this codec loads and plays (when a hardware)
// decoder is actually available, such as on real hardware.
// TODO(crbug.com/1207695): Correct the final expected result to "playing".
// Currently, this fails in emulators. Fixing the bug will change this.
IN_PROC_BROWSER_TEST_F(SoftwareDecoderDisabledTest,
                       PlayHardwareAndSoftwareCodecSucceeds) {
  const GURL kUrl(
      embedded_test_server()->GetURL(kLoadHardwareAndSoftwareCodecUrl));

  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), cr_fuchsia::CreateLoadUrlParamsWithUserActivation(),
      kUrl.spec()));

  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl, "loaded");
  cr_fuchsia::ExecuteJavaScript(frame.get(), "bear.play()");

  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl, "media element error");
}

// Verify that a codec supported by hardware and software does not play if
// kDisableSoftwareVideoDecoders is present and the hardware decoder fails.
// Unlike the software-only codec, this codec loads because it is supposed to be
// supported but fails when the hardware decoder is unavailable.
IN_PROC_BROWSER_TEST_F(SoftwareDecoderDisabledAndHardwareDecoderFailureTest,
                       PlayHardwareAndSoftwareCodecFails) {
  const GURL kUrl(
      embedded_test_server()->GetURL(kLoadHardwareAndSoftwareCodecUrl));

  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), cr_fuchsia::CreateLoadUrlParamsWithUserActivation(),
      kUrl.spec()));

  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl, "loaded");
  cr_fuchsia::ExecuteJavaScript(frame.get(), "bear.play()");

  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl, "media element error");
}
