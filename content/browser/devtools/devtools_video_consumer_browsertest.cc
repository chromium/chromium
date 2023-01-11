// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "content/browser/devtools/devtools_video_consumer.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/controllable_http_response.h"

namespace {

// Helper to verify that |frame| dimensions are in the expected ranges.
bool VerifyFrameSize(scoped_refptr<media::VideoFrame> frame,
                     gfx::Size min_frame_size,
                     gfx::Size max_frame_size) {
  // Returns true if |min_frame_size| <= |frame| <= |max_frame_size|.
  return min_frame_size.width() <= frame->visible_rect().width() &&
         min_frame_size.height() <= frame->visible_rect().height() &&
         frame->visible_rect().width() <= max_frame_size.width() &&
         frame->visible_rect().height() <= max_frame_size.height();
}

// The default video pixel format.
// TODO(sundarrajs): Add |FrameSinkVideoCapturerImpl::kDefaultPixelFormat| to
// frame_sink_video_capture.mojom and use that instead of
// |kVideoCapturerDefaultPixelFormat|.
constexpr media::VideoPixelFormat kVideoCapturerDefaultPixelFormat =
    media::PIXEL_FORMAT_I420;

}  // namespace

namespace content {

class DevToolsVideoConsumerTest : public ContentBrowserTest {
 public:
  DevToolsVideoConsumerTest() {}

  void SetUpOnMainThread() override {
    consumer_ = std::make_unique<DevToolsVideoConsumer>(base::BindRepeating(
        &DevToolsVideoConsumerTest::OnFrameFromVideoConsumer,
        base::Unretained(this)));
    consumer_->SetFrameSinkId(
        static_cast<content::WebContentsImpl*>(shell()->web_contents())
            ->GetRenderViewHost()
            ->GetWidget()
            ->GetFrameSinkId());
  }

  scoped_refptr<media::VideoFrame> GetFrame(int i) { return frames_[i]; }

  size_t NumberOfFramesReceived() { return frames_.size(); }

  void OnFrameFromVideoConsumer(scoped_refptr<media::VideoFrame> frame) {
    run_loop_->Quit();
    frames_.push_back(std::move(frame));
  }

  void WaitUntilFrameReceived() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  static gfx::Size GetVideoConsumerDefaultMinFrameSize() {
    return DevToolsVideoConsumer::kDefaultMinFrameSize;
  }

  static gfx::Size GetVideoConsumerDefaultMaxFrameSize() {
    return DevToolsVideoConsumer::kDefaultMaxFrameSize;
  }

 protected:
  std::unique_ptr<DevToolsVideoConsumer> consumer_;

 private:
  std::vector<scoped_refptr<media::VideoFrame>> frames_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

// Tests that setting new frame dimensions via SetMinAndMaxFrameSizes
// produces frames of the new dimensions.
IN_PROC_BROWSER_TEST_F(DevToolsVideoConsumerTest,
                       DISABLED_SetMinAndMaxFramesChangesDimensions) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Complete navigation to a page and then start capture. Since navigation is
  // complete before capturing, we only expect the refresh frame from the
  // StartCapture call. Wait for this refresh frame and verify its info.
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), embedded_test_server()->GetURL("/devtools/navigation.html"), 1);
  consumer_->StartCapture();
  WaitUntilFrameReceived();
  size_t cur_frame_index = 0;
  size_t num_frames_received = 1;
  EXPECT_EQ(num_frames_received, NumberOfFramesReceived());
  scoped_refptr<media::VideoFrame> frame = GetFrame(cur_frame_index);
  EXPECT_EQ(frame->format(), kVideoCapturerDefaultPixelFormat);
  EXPECT_TRUE(VerifyFrameSize(frame, GetVideoConsumerDefaultMinFrameSize(),
                              GetVideoConsumerDefaultMaxFrameSize()));

  // Setting |kNewMinFrameSize| > |kDefaultMaxFrameSize| to disambiguate
  // frames with older vs. newer dimensions.
  const gfx::Size kNewMinFrameSize =
      gfx::Size(GetVideoConsumerDefaultMaxFrameSize().width() + 1,
                GetVideoConsumerDefaultMaxFrameSize().height() + 1);
  // |kNewMaxFrameSize| only needs to be >= |kNewMinFrameSize|.
  const gfx::Size kNewMaxFrameSize = gfx::Size(kNewMinFrameSize.width() + 500,
                                               kNewMinFrameSize.height() + 500);
  // Set the min and max frame sizes. This will call
  // FrameSinkVideoCapturer::SetResolutionConstraints which triggers sending a
  // refresh frame. Wait for this refresh frame and verify it's info.
  consumer_->SetMinAndMaxFrameSize(kNewMinFrameSize, kNewMaxFrameSize);
  WaitUntilFrameReceived();
  EXPECT_EQ(++num_frames_received, NumberOfFramesReceived());
  frame = GetFrame(++cur_frame_index);
  EXPECT_EQ(frame->format(), kVideoCapturerDefaultPixelFormat);
  EXPECT_TRUE(VerifyFrameSize(frame, kNewMinFrameSize, kNewMaxFrameSize));

  // Stop capturing.
  consumer_->StopCapture();
}

}  // namespace content
