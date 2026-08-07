// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/media_recorder.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "content/browser/devtools/devtools_io_context.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "content/services/devtools_media_encoding_service/public/mojom/devtools_media_encoding_service.mojom.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/video_frame.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "media/capture/video/video_frame_receiver_types.h"
#include "media/media_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace content::protocol {

class MediaRecorderTest : public RenderViewHostTestHarness {
 public:
  MediaRecorderTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  TestWebContents* contents() {
    return static_cast<TestWebContents*>(web_contents());
  }

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("https://example.test/"));
    recorder_ = std::make_unique<MediaRecorder>(
        &io_context_,
        base::BindRepeating(&MediaRecorderTest::OnStopRecording,
                            base::Unretained(this)));
  }

  void TearDown() override {
    recorder_.reset();
    io_context_.DiscardAllStreams();
    RenderViewHostTestHarness::TearDown();
  }

  void OnStopRecording() {
    stop_recording_called_ = true;
    if (on_stop_recording_loop_) {
      on_stop_recording_loop_->Quit();
    }
  }

  std::string ReadStream(const std::string& handle) {
    scoped_refptr<DevToolsIOContext::Stream> stream =
        io_context_.GetByHandle(handle);
    if (!stream) {
      return std::string();
    }
    std::string result;
    base::RunLoop run_loop;
    stream->Read(0, 1024,
                 base::BindLambdaForTesting(
                     [&](std::unique_ptr<std::string> data,
                         bool base64_encoded, int status) {
                       if (data) {
                         result = *data;
                       }
                       run_loop.Quit();
                     }));
    run_loop.Run();
    return result;
  }

 protected:
  DevToolsIOContext io_context_;
  std::unique_ptr<MediaRecorder> recorder_;
  bool stop_recording_called_ = false;
  raw_ptr<base::RunLoop> on_stop_recording_loop_ = nullptr;
};

#if !BUILDFLAG(ENABLE_LIBAOM)
TEST_F(MediaRecorderTest, StartWithoutLibaom) {
  Response response = recorder_->Start(
      contents()->GetPrimaryMainFrame(), /*audio=*/false, /*max_width=*/800,
      /*max_height=*/600, /*frame_rate=*/30);
  EXPECT_TRUE(response.IsError());
  EXPECT_EQ("Video recording is not supported without AV1/libaom enabled.",
            response.Message());
}
#endif

TEST_F(MediaRecorderTest, StopWithoutStart) {
  EXPECT_TRUE(recorder_->GetStream().empty());

  std::string stream_handle = "not_empty";
  base::RunLoop run_loop;
  recorder_->Stop(base::BindLambdaForTesting([&](std::string handle) {
    stream_handle = handle;
    run_loop.Quit();
  }));
  run_loop.Run();

  EXPECT_TRUE(stream_handle.empty());
  EXPECT_TRUE(recorder_->GetStream().empty());
}

TEST_F(MediaRecorderTest, StopWithoutCallback) {
  EXPECT_TRUE(recorder_->GetStream().empty());
  // Calling Stop with a null callback should not crash.
  recorder_->Stop(base::OnceCallback<void(std::string)>());
  EXPECT_TRUE(recorder_->GetStream().empty());
}

TEST_F(MediaRecorderTest, OnDataWithoutStreamFile) {
  // Calling OnData before Start should not crash when stream_file_ is null.
  auto* client = static_cast<
      devtools_media_encoding_service::mojom::DevToolsMediaEncodingServiceClient*>(
      recorder_.get());
  const std::vector<uint8_t> kTestData = {'T', 'e', 's', 't'};
  client->OnData(kTestData);
  EXPECT_TRUE(recorder_->GetStream().empty());
}

TEST_F(MediaRecorderTest, VideoBufferLifecycle) {
  // 1) Test ReadOnlySharedMemoryRegion buffer.
  const int kReadOnlyBufferId = 1;
  const gfx::Size kSize(320, 240);
  const size_t kAllocationSize =
      media::VideoFrame::AllocationSize(media::PIXEL_FORMAT_I420, kSize);

  auto read_only_region =
      base::ReadOnlySharedMemoryRegion::Create(kAllocationSize);
  ASSERT_TRUE(read_only_region.IsValid());

  recorder_->OnNewBuffer(
      kReadOnlyBufferId,
      media::mojom::VideoBufferHandle::NewReadOnlyShmemRegion(
          std::move(read_only_region.region)));

  media::mojom::VideoFrameInfoPtr read_only_info =
      media::mojom::VideoFrameInfo::New(
          base::Milliseconds(10), media::VideoFrameMetadata(),
          media::PIXEL_FORMAT_I420, kSize, gfx::Rect(kSize),
          /*natural_size=*/kSize, /*is_premapped=*/false,
          gfx::ColorSpace::CreateREC709(), nullptr);

  recorder_->OnFrameReadyInBuffer(media::ReadyFrameInBuffer(
      kReadOnlyBufferId, 0, nullptr, std::move(read_only_info)));

  recorder_->OnBufferRetired(kReadOnlyBufferId);

  // 2) Test UnsafeSharedMemoryRegion buffer.
  const int kUnsafeBufferId = 2;
  auto unsafe_region = base::UnsafeSharedMemoryRegion::Create(kAllocationSize);
  ASSERT_TRUE(unsafe_region.IsValid());

  recorder_->OnNewBuffer(kUnsafeBufferId,
                         media::mojom::VideoBufferHandle::NewUnsafeShmemRegion(
                             std::move(unsafe_region)));

  media::mojom::VideoFrameInfoPtr unsafe_info =
      media::mojom::VideoFrameInfo::New(
          base::Milliseconds(20), media::VideoFrameMetadata(),
          media::PIXEL_FORMAT_I420, kSize, gfx::Rect(kSize),
          /*natural_size=*/kSize, /*is_premapped=*/false,
          gfx::ColorSpace::CreateREC709(), nullptr);

  recorder_->OnFrameReadyInBuffer(media::ReadyFrameInBuffer(
      kUnsafeBufferId, 0, nullptr, std::move(unsafe_info)));

  recorder_->OnBufferRetired(kUnsafeBufferId);
}

TEST_F(MediaRecorderTest, VideoBufferInvalidSize) {
  const int kBufferId = 1;
  // Create a buffer that is too small for a 320x240 I420 frame.
  auto small_region = base::ReadOnlySharedMemoryRegion::Create(16);
  ASSERT_TRUE(small_region.IsValid());

  recorder_->OnNewBuffer(
      kBufferId,
      media::mojom::VideoBufferHandle::NewReadOnlyShmemRegion(
          std::move(small_region.region)));

  const gfx::Size kSize(320, 240);
  media::mojom::VideoFrameInfoPtr info = media::mojom::VideoFrameInfo::New(
      base::Milliseconds(10), media::VideoFrameMetadata(),
      media::PIXEL_FORMAT_I420, kSize, gfx::Rect(kSize),
      /*natural_size=*/kSize, /*is_premapped=*/false,
      gfx::ColorSpace::CreateREC709(), nullptr);

  // Should return without crashing when shared memory size is too small.
  recorder_->OnFrameReadyInBuffer(media::ReadyFrameInBuffer(
      kBufferId, 0, nullptr, std::move(info)));

  recorder_->OnBufferRetired(kBufferId);
}

TEST_F(MediaRecorderTest, AudioCaptureWithoutService) {
  media::AudioCapturerSource::CaptureCallback* capture_callback =
      static_cast<media::AudioCapturerSource::CaptureCallback*>(
          recorder_.get());

  std::unique_ptr<media::AudioBus> audio_bus =
      media::AudioBus::Create(/*channels=*/2, /*frames=*/480);
  audio_bus->Zero();

  // Should return early without crash when service_ is null.
  capture_callback->Capture(audio_bus.get(), base::TimeTicks::Now(),
                            media::AudioGlitchInfo(), /*volume=*/1.0);
  capture_callback->OnCaptureMuted(true);
  capture_callback->OnCaptureError(
      media::AudioCapturerSource::ErrorCode::kUnknown, "Test error");
}

TEST_F(MediaRecorderTest, OnClosedInvokesCallback) {
  auto* client = static_cast<
      devtools_media_encoding_service::mojom::DevToolsMediaEncodingServiceClient*>(
      recorder_.get());
  base::RunLoop run_loop;
  on_stop_recording_loop_ = &run_loop;
  client->OnClosed();
  run_loop.Run();
  on_stop_recording_loop_ = nullptr;
  EXPECT_TRUE(stop_recording_called_);
}

TEST_F(MediaRecorderTest, StopAfterReceivingFrames) {
  const int kBufferId = 1;
  const gfx::Size kSize(320, 240);
  const size_t kAllocationSize =
      media::VideoFrame::AllocationSize(media::PIXEL_FORMAT_I420, kSize);

  auto read_only_region =
      base::ReadOnlySharedMemoryRegion::Create(kAllocationSize);
  ASSERT_TRUE(read_only_region.IsValid());

  recorder_->OnNewBuffer(
      kBufferId,
      media::mojom::VideoBufferHandle::NewReadOnlyShmemRegion(
          std::move(read_only_region.region)));

  media::mojom::VideoFrameInfoPtr read_only_info =
      media::mojom::VideoFrameInfo::New(
          base::Milliseconds(10), media::VideoFrameMetadata(),
          media::PIXEL_FORMAT_I420, kSize, gfx::Rect(kSize),
          /*natural_size=*/kSize, /*is_premapped=*/false,
          gfx::ColorSpace::CreateREC709(), nullptr);

  recorder_->OnFrameReadyInBuffer(media::ReadyFrameInBuffer(
      kBufferId, 0, nullptr, std::move(read_only_info)));

  std::string stream_handle = "not_empty";
  base::RunLoop run_loop;
  recorder_->Stop(base::BindLambdaForTesting([&](std::string handle) {
    stream_handle = handle;
    run_loop.Quit();
  }));
  run_loop.Run();

  EXPECT_TRUE(stream_handle.empty());
  EXPECT_TRUE(recorder_->GetStream().empty());
}

TEST_F(MediaRecorderTest, OnNewBufferInvalidRegion) {
  const int kBufferId = 1;
  base::ReadOnlySharedMemoryRegion invalid_region;
  EXPECT_FALSE(invalid_region.IsValid());

  recorder_->OnNewBuffer(
      kBufferId,
      media::mojom::VideoBufferHandle::NewReadOnlyShmemRegion(
          std::move(invalid_region)));

  const gfx::Size kSize(320, 240);
  media::mojom::VideoFrameInfoPtr info = media::mojom::VideoFrameInfo::New(
      base::Milliseconds(10), media::VideoFrameMetadata(),
      media::PIXEL_FORMAT_I420, kSize, gfx::Rect(kSize),
      /*natural_size=*/kSize, /*is_premapped=*/false,
      gfx::ColorSpace::CreateREC709(), nullptr);

  // Since the region was invalid, OnFrameReadyInBuffer should ignore it safely.
  recorder_->OnFrameReadyInBuffer(media::ReadyFrameInBuffer(
      kBufferId, 0, nullptr, std::move(info)));
}

TEST_F(MediaRecorderTest, UnknownBufferIds) {
  const int kUnknownBufferId = 999;
  const gfx::Size kSize(320, 240);
  media::mojom::VideoFrameInfoPtr info = media::mojom::VideoFrameInfo::New(
      base::Milliseconds(10), media::VideoFrameMetadata(),
      media::PIXEL_FORMAT_I420, kSize, gfx::Rect(kSize),
      /*natural_size=*/kSize, /*is_premapped=*/false,
      gfx::ColorSpace::CreateREC709(), nullptr);

  // Calling OnFrameReadyInBuffer with an ID that was never added via OnNewBuffer.
  recorder_->OnFrameReadyInBuffer(media::ReadyFrameInBuffer(
      kUnknownBufferId, 0, nullptr, std::move(info)));

  // Calling OnBufferRetired with an unknown ID.
  recorder_->OnBufferRetired(kUnknownBufferId);
}

TEST_F(MediaRecorderTest, MultipleBuffersLifecycle) {
  const int kBufferId1 = 10;
  const int kBufferId2 = 20;
  const gfx::Size kSize(320, 240);
  const size_t kAllocationSize =
      media::VideoFrame::AllocationSize(media::PIXEL_FORMAT_I420, kSize);

  auto region1 = base::ReadOnlySharedMemoryRegion::Create(kAllocationSize);
  auto region2 = base::ReadOnlySharedMemoryRegion::Create(kAllocationSize);
  ASSERT_TRUE(region1.IsValid());
  ASSERT_TRUE(region2.IsValid());

  recorder_->OnNewBuffer(
      kBufferId1,
      media::mojom::VideoBufferHandle::NewReadOnlyShmemRegion(
          std::move(region1.region)));
  recorder_->OnNewBuffer(
      kBufferId2,
      media::mojom::VideoBufferHandle::NewReadOnlyShmemRegion(
          std::move(region2.region)));

  auto make_info = [](base::TimeDelta ts, const gfx::Size& size) {
    return media::mojom::VideoFrameInfo::New(
        ts, media::VideoFrameMetadata(), media::PIXEL_FORMAT_I420, size,
        gfx::Rect(size), /*natural_size=*/size, /*is_premapped=*/false,
        gfx::ColorSpace::CreateREC709(), nullptr);
  };

  recorder_->OnFrameReadyInBuffer(media::ReadyFrameInBuffer(
      kBufferId2, 0, nullptr, make_info(base::Milliseconds(15), kSize)));
  recorder_->OnFrameReadyInBuffer(media::ReadyFrameInBuffer(
      kBufferId1, 0, nullptr, make_info(base::Milliseconds(30), kSize)));

  recorder_->OnBufferRetired(kBufferId2);
  recorder_->OnBufferRetired(kBufferId1);
}

}  // namespace content::protocol
