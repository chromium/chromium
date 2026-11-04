// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/media_recorder.h"

#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "content/browser/devtools/devtools_io_context.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests MediaRecorder::StartDisplayCapture(). The tests use a window ID that
// does not exist, so the device does not launch. These are browser tests,
// because the recording starts the media encoding service.

namespace content::protocol {

#if BUILDFLAG(ENABLE_LIBAOM)
namespace {

DesktopMediaID WindowId() {
  return DesktopMediaID(DesktopMediaID::TYPE_WINDOW, 42);
}

}  // namespace

class MediaRecorderBrowserTest : public ContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    recorder_ =
        std::make_unique<MediaRecorder>(&io_context_, base::DoNothing());
  }
  void TearDownOnMainThread() override {
    recorder_.reset();
    io_context_.DiscardAllStreams();
  }

  MediaRecorder* recorder() { return recorder_.get(); }

  DevToolsIOContext io_context_;
  std::unique_ptr<MediaRecorder> recorder_;
};

// Checks that a display capture creates the stream, and that Stop() still
// returns it after the device failed to launch.
IN_PROC_BROWSER_TEST_F(MediaRecorderBrowserTest, DisplayCaptureStartAndStop) {
  bool started = false;
  recorder()->StartDisplayCapture(
      WindowId(), shell()->web_contents(), /*audio=*/false,
      /*max_width=*/800, /*max_height=*/600, /*frame_rate=*/30,
      base::BindLambdaForTesting(
          [&](Response response) { started = response.IsSuccess(); }));
  EXPECT_TRUE(started);
  const std::string stream = recorder()->GetStream();
  EXPECT_FALSE(stream.empty());

  std::string stopped_stream;
  base::RunLoop run_loop;
  recorder()->Stop(base::BindLambdaForTesting([&](std::string handle) {
    stopped_stream = handle;
    run_loop.Quit();
  }));
  run_loop.Run();
  EXPECT_EQ(stream, stopped_stream);
}
#endif  // BUILDFLAG(ENABLE_LIBAOM)

}  // namespace content::protocol
