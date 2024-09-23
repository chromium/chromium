// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_effects_processor.h"

#include <numeric>
#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/viz/test/test_context_provider.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/test_gpu_memory_buffer_manager.h"
#include "media/base/video_frame_metadata.h"
#include "media/base/video_types.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/video_capture_gpu_channel_host.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

namespace {

size_t GetBitsPerPixel(VideoPixelFormat format) {
  switch (format) {
    case VideoPixelFormat::PIXEL_FORMAT_ARGB:
      return 32;
    case VideoPixelFormat::PIXEL_FORMAT_I420:
    case VideoPixelFormat::PIXEL_FORMAT_NV12:
      return 12;
    default:
      NOTREACHED();
  }
}

class HandleProvider
    : public VideoCaptureDevice::Client::Buffer::HandleProvider {
 public:
  ~HandleProvider() override = default;

  base::UnsafeSharedMemoryRegion DuplicateAsUnsafeRegion() override {
    return {};
  }

  std::unique_ptr<VideoCaptureBufferHandle> GetHandleForInProcessAccess()
      override {
    return {};
  }

  gfx::GpuMemoryBufferHandle GetGpuMemoryBufferHandle() override { return {}; }
};

class ScopedAccessPermission
    : public VideoCaptureDevice::Client::Buffer::ScopedAccessPermission {
 public:
  ~ScopedAccessPermission() override = default;
};

class VideoEffectsProcessor
    : public video_effects::mojom::VideoEffectsProcessor {
 public:
  explicit VideoEffectsProcessor(
      mojo::PendingReceiver<video_effects::mojom::VideoEffectsProcessor>
          receiver)
      : receiver_(this, std::move(receiver)) {}

  void PostProcess(mojom::VideoBufferHandlePtr input_frame_data,
                   mojom::VideoFrameInfoPtr input_frame_info,
                   mojom::VideoBufferHandlePtr result_frame_data,
                   VideoPixelFormat result_pixel_format,
                   PostProcessCallback callback) override {
    input_frame_info->pixel_format = result_pixel_format;
    std::move(callback).Run(video_effects::mojom::PostProcessResult::NewSuccess(
        video_effects::mojom::PostProcessSuccess::New(
            std::move(input_frame_info))));
  }

 private:
  mojo::Receiver<video_effects::mojom::VideoEffectsProcessor> receiver_;
};

constexpr gfx::Size kValidFrameSize = gfx::Size(10, 10);
constexpr base::TimeDelta kValidTimeDelta = base::Seconds(1);
constexpr float kValidFrameRate = 1.0f;

}  // namespace

class VideoCaptureEffectsProcessorTest
    : public testing::TestWithParam<VideoPixelFormat> {
 public:
  void SetUp() override {
    test_sii_ = base::MakeRefCounted<gpu::TestSharedImageInterface>();

    mojo::PendingReceiver<video_effects::mojom::VideoEffectsProcessor>
        pending_receiver;
    capture_processor_.emplace(pending_receiver.InitWithNewPipeAndPassRemote());

    video_effects_processor_.emplace(std::move(pending_receiver));

    auto& gpu_channel_host = VideoCaptureGpuChannelHost::GetInstance();
    gpu_channel_host.SetSharedImageInterface(test_sii_);
    gpu_channel_host.SetGpuMemoryBufferManager(&test_gmb_manager_);
  }

  void TearDown() override {
    auto& gpu_channel_host = VideoCaptureGpuChannelHost::GetInstance();
    gpu_channel_host.SetGpuMemoryBufferManager(nullptr);
    gpu_channel_host.SetSharedImageInterface(nullptr);
  }

  VideoPixelFormat GetPixelFormat() { return GetParam(); }

 protected:
  base::test::TaskEnvironment task_environment_;

  scoped_refptr<gpu::TestSharedImageInterface> test_sii_;
  gpu::TestGpuMemoryBufferManager test_gmb_manager_;

  std::optional<VideoEffectsProcessor> video_effects_processor_;

  // Code-under-test. Created in `SetUp()`.
  std::optional<VideoCaptureEffectsProcessor> capture_processor_;
};

TEST_P(VideoCaptureEffectsProcessorTest, PostProcessDataSucceeds) {
  const VideoPixelFormat pixel_format = GetPixelFormat();

  base::test::TestFuture<base::expected<PostProcessDoneInfo,
                                        video_effects::mojom::PostProcessError>>
      post_process_future;

  const gfx::Size coded_size = kValidFrameSize;
  std::vector<uint8_t> frame_data(coded_size.Area64() *
                                  GetBitsPerPixel(pixel_format) / 8);
  std::iota(frame_data.begin(), frame_data.end(), 1);

  mojom::VideoFrameInfoPtr info = mojom::VideoFrameInfo::New(
      kValidTimeDelta, media::VideoFrameMetadata{}, pixel_format, coded_size,
      gfx::Rect(coded_size), /*is_premapped=*/false,
      gfx::ColorSpace::CreateREC709(), media::mojom::PlaneStridesPtr{});

  VideoCaptureDevice::Client::Buffer out_buffer(
      /*buffer_id=*/0, /*frame_feedback_id=*/0,
      std::make_unique<HandleProvider>(),
      std::make_unique<ScopedAccessPermission>());

  capture_processor_->PostProcessData(
      base::make_span(frame_data), std::move(info), std::move(out_buffer),
      VideoCaptureFormat(coded_size, kValidFrameRate,
                         VideoPixelFormat::PIXEL_FORMAT_NV12),
      VideoCaptureBufferType::kGpuMemoryBuffer,
      post_process_future.GetCallback());

  EXPECT_TRUE(post_process_future.Wait());
}

TEST_P(VideoCaptureEffectsProcessorTest, PostProcessBufferSucceeds) {
  const VideoPixelFormat pixel_format = GetPixelFormat();
  if (pixel_format != VideoPixelFormat::PIXEL_FORMAT_NV12) {
    // The post-processor does not support formats other than NV12 for on-GPU
    // data yet - skip this test.
    GTEST_SKIP();
  }

  base::test::TestFuture<base::expected<PostProcessDoneInfo,
                                        video_effects::mojom::PostProcessError>>
      post_process_future;

  const gfx::Size coded_size = kValidFrameSize;
  std::vector<uint8_t> frame_data(coded_size.Area64() *
                                  GetBitsPerPixel(pixel_format) / 8);
  std::iota(frame_data.begin(), frame_data.end(), 1);

  mojom::VideoFrameInfoPtr info = mojom::VideoFrameInfo::New(
      kValidTimeDelta, media::VideoFrameMetadata{}, pixel_format, coded_size,
      gfx::Rect(coded_size), /*is_premapped=*/false,
      gfx::ColorSpace::CreateREC709(), media::mojom::PlaneStridesPtr{});

  VideoCaptureDevice::Client::Buffer in_buffer(
      /*buffer_id=*/0, /*frame_feedback_id=*/0,
      std::make_unique<HandleProvider>(),
      std::make_unique<ScopedAccessPermission>());

  VideoCaptureDevice::Client::Buffer out_buffer(
      /*buffer_id=*/1, /*frame_feedback_id=*/1,
      std::make_unique<HandleProvider>(),
      std::make_unique<ScopedAccessPermission>());

  capture_processor_->PostProcessBuffer(
      std::move(in_buffer), std::move(info),
      VideoCaptureBufferType::kGpuMemoryBuffer, std::move(out_buffer),
      VideoCaptureFormat(coded_size, kValidFrameRate,
                         VideoPixelFormat::PIXEL_FORMAT_NV12),
      VideoCaptureBufferType::kGpuMemoryBuffer,
      post_process_future.GetCallback());

  EXPECT_TRUE(post_process_future.Wait());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    VideoCaptureEffectsProcessorTest,
    testing::Values(media::VideoPixelFormat::PIXEL_FORMAT_I420,
                    media::VideoPixelFormat::PIXEL_FORMAT_NV12,
                    media::VideoPixelFormat::PIXEL_FORMAT_ARGB),
    [](const testing::TestParamInfo<
        VideoCaptureEffectsProcessorTest::ParamType>& info) {
      return VideoPixelFormatToString(info.param);
    });

}  // namespace media
