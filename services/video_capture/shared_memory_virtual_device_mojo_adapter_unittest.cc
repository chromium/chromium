// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/shared_memory_virtual_device_mojo_adapter.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "media/capture/video/video_capture_device_info.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/cpp/mock_producer.h"
#include "services/video_capture/public/cpp/mock_video_frame_handler.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace video_capture {
namespace {

const std::string kTestDeviceId = "/test/device";
const std::string kTestDeviceName = "Test Device";
const gfx::Size kTestFrameSize = {640 /* width */, 480 /* height */};

std::vector<media::VideoPixelFormat>
GetSupportedUncompressedVideoPixelFormats() {
  std::vector<media::VideoPixelFormat> formats;
  for (int i =
           static_cast<int>(media::VideoPixelFormat::PIXEL_FORMAT_UNKNOWN) + 1;
       i <= static_cast<int>(media::VideoPixelFormat::PIXEL_FORMAT_MAX); ++i) {
    auto format = static_cast<media::VideoPixelFormat>(i);
    if (IsSupportedVideoPixelFormat(format) &&
        format != media::VideoPixelFormat::PIXEL_FORMAT_MJPEG) {
      formats.push_back(format);
    }
  }
  return formats;
}

class SharedMemoryVirtualDeviceMojoAdapterTest : public ::testing::Test {
 public:
  struct SetupResult {
    int32_t buffer_id;
    std::unique_ptr<MockVideoFrameHandler> video_frame_handler;
  };

  SharedMemoryVirtualDeviceMojoAdapterTest() = default;
  ~SharedMemoryVirtualDeviceMojoAdapterTest() override = default;

  void SetUp() override {
    device_info_.descriptor.device_id = kTestDeviceId;
    device_info_.descriptor.set_display_name(kTestDeviceName);
  }

  SharedMemoryVirtualDeviceMojoAdapter* device_adapter() {
    return device_adapter_.get();
  }

  SetupResult RecreateAdapterAndRequestBuffer(
      media::VideoPixelFormat format,
      const gfx::Size& coded_size = kTestFrameSize,
      media::mojom::PlaneStridesPtr strides = nullptr) {
    mojo::Remote<mojom::Producer> producer;
    producer_ =
        std::make_unique<MockProducer>(producer.BindNewPipeAndPassReceiver());
    device_adapter_ = std::make_unique<SharedMemoryVirtualDeviceMojoAdapter>(
        std::move(producer));

    mojo::PendingRemote<mojom::VideoFrameHandler> handler_remote;
    auto video_frame_handler = std::make_unique<MockVideoFrameHandler>(
        handler_remote.InitWithNewPipeAndPassReceiver());

    base::RunLoop start_run_loop;
    EXPECT_CALL(*video_frame_handler, OnStarted).WillOnce([&start_run_loop]() {
      start_run_loop.Quit();
    });
    EXPECT_CALL(*video_frame_handler, DoOnNewBuffer)
        .Times(testing::AnyNumber());

    device_adapter_->Start(media::VideoCaptureParams(),
                           std::move(handler_remote));
    start_run_loop.Run();

    EXPECT_CALL(*producer_, DoOnNewBuffer)
        .WillRepeatedly([](int32_t buffer_id,
                           media::mojom::VideoBufferHandlePtr* handle,
                           mojom::Producer::OnNewBufferCallback& callback) {
          std::move(callback).Run();
        });

    int32_t buffer_id = -1;
    base::RunLoop request_run_loop;
    device_adapter_->RequestFrameBuffer(
        coded_size, format, std::move(strides),
        base::BindOnce(
            [](base::OnceClosure quit_closure, int32_t* out_id, int32_t id) {
              *out_id = id;
              std::move(quit_closure).Run();
            },
            request_run_loop.QuitClosure(), &buffer_id));
    request_run_loop.Run();
    CHECK_NE(buffer_id, -1);

    return {buffer_id, std::move(video_frame_handler)};
  }

 private:
  std::unique_ptr<SharedMemoryVirtualDeviceMojoAdapter> device_adapter_;
  std::unique_ptr<MockProducer> producer_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  media::VideoCaptureDeviceInfo device_info_;
};

TEST_F(SharedMemoryVirtualDeviceMojoAdapterTest, AcceptsValidFrame) {
  for (media::VideoPixelFormat format :
       GetSupportedUncompressedVideoPixelFormats()) {
    SetupResult setup = RecreateAdapterAndRequestBuffer(format);
    base::RunLoop run_loop;
    EXPECT_CALL(*setup.video_frame_handler,
                DoOnFrameReadyInBuffer(setup.buffer_id, _, _))
        .WillOnce([&run_loop]() { run_loop.Quit(); });
    media::mojom::VideoFrameInfoPtr info = media::mojom::VideoFrameInfo::New();
    info->pixel_format = format;
    info->coded_size = kTestFrameSize;
    info->visible_rect = gfx::Rect(kTestFrameSize);
    info->natural_size = kTestFrameSize;
    device_adapter()->OnFrameReadyInBuffer(setup.buffer_id, std::move(info));
    run_loop.Run();
  }
}

TEST_F(SharedMemoryVirtualDeviceMojoAdapterTest,
       AcceptsValidFrameWithCustomStrides) {
  for (media::VideoPixelFormat format :
       GetSupportedUncompressedVideoPixelFormats()) {
    const size_t num_planes = media::VideoFrame::NumPlanes(format);
    std::vector<uint32_t> stride_values;
    std::vector<size_t> default_strides =
        media::VideoFrame::ComputeStrides(format, kTestFrameSize);
    for (size_t i = 0; i < num_planes; ++i) {
      stride_values.push_back(default_strides[i] + 32);
    }
    // Pad strides vector to exactly 4 elements to satisfy Mojo serialization
    // rules.
    while (stride_values.size() < 4) {
      stride_values.push_back(0);
    }
    auto strides = media::mojom::PlaneStrides::New(stride_values);

    SetupResult setup = RecreateAdapterAndRequestBuffer(format, kTestFrameSize,
                                                        strides.Clone());
    base::RunLoop run_loop;
    EXPECT_CALL(*setup.video_frame_handler,
                DoOnFrameReadyInBuffer(setup.buffer_id, _, _))
        .WillOnce([&run_loop]() { run_loop.Quit(); });

    auto info = media::mojom::VideoFrameInfo::New();
    info->pixel_format = format;
    info->coded_size = kTestFrameSize;
    info->visible_rect = gfx::Rect(kTestFrameSize);
    info->natural_size = kTestFrameSize;
    info->strides = std::move(strides);

    device_adapter()->OnFrameReadyInBuffer(setup.buffer_id, std::move(info));
    run_loop.Run();
  }
}

TEST_F(SharedMemoryVirtualDeviceMojoAdapterTest, AcceptsMjpegFrame) {
  // Allocate a raw I420 buffer (which is contiguous and large enough).
  SetupResult setup = RecreateAdapterAndRequestBuffer(
      media::VideoPixelFormat::PIXEL_FORMAT_I420);

  base::RunLoop run_loop;
  EXPECT_CALL(*setup.video_frame_handler,
              DoOnFrameReadyInBuffer(setup.buffer_id, _, _))
      .WillOnce([&run_loop]() { run_loop.Quit(); });

  // Deliver the buffer tagged as MJPEG.
  media::mojom::VideoFrameInfoPtr info = media::mojom::VideoFrameInfo::New();
  info->pixel_format = media::VideoPixelFormat::PIXEL_FORMAT_MJPEG;
  info->coded_size = kTestFrameSize;
  info->visible_rect = gfx::Rect(kTestFrameSize);
  info->natural_size = kTestFrameSize;

  device_adapter()->OnFrameReadyInBuffer(setup.buffer_id, std::move(info));
  run_loop.Run();
}

TEST_F(SharedMemoryVirtualDeviceMojoAdapterTest, RejectsInvalidPixelFormat) {
  for (int format_int = -1;
       format_int <=
       static_cast<int>(media::VideoPixelFormat::PIXEL_FORMAT_MAX) + 1;
       ++format_int) {
    media::VideoPixelFormat format =
        static_cast<media::VideoPixelFormat>(format_int);
    if (IsSupportedVideoPixelFormat(format)) {
      continue;
    }

    SetupResult setup = RecreateAdapterAndRequestBuffer(
        media::VideoPixelFormat::PIXEL_FORMAT_I420);

    EXPECT_CALL(*setup.video_frame_handler, DoOnFrameReadyInBuffer).Times(0);
    auto info = media::mojom::VideoFrameInfo::New();
    info->pixel_format = format;
    info->coded_size = kTestFrameSize;
    info->visible_rect = gfx::Rect(kTestFrameSize);
    info->natural_size = kTestFrameSize;
    device_adapter()->OnFrameReadyInBuffer(setup.buffer_id, std::move(info));
  }
}

TEST_F(SharedMemoryVirtualDeviceMojoAdapterTest, RejectsTooFewStrides) {
  for (media::VideoPixelFormat format :
       GetSupportedUncompressedVideoPixelFormats()) {
    SetupResult setup = RecreateAdapterAndRequestBuffer(format);

    EXPECT_CALL(*setup.video_frame_handler, DoOnFrameReadyInBuffer).Times(0);

    auto strides = media::mojom::PlaneStrides::New();
    auto info = media::mojom::VideoFrameInfo::New();
    info->pixel_format = format;
    info->coded_size = kTestFrameSize;
    info->visible_rect = gfx::Rect(kTestFrameSize);
    info->natural_size = kTestFrameSize;
    info->strides = std::move(strides);

    device_adapter()->OnFrameReadyInBuffer(setup.buffer_id, std::move(info));
  }
}

TEST_F(SharedMemoryVirtualDeviceMojoAdapterTest, RejectsExcessiveStrides) {
  for (media::VideoPixelFormat format :
       GetSupportedUncompressedVideoPixelFormats()) {
    SetupResult setup = RecreateAdapterAndRequestBuffer(format);

    EXPECT_CALL(*setup.video_frame_handler, DoOnFrameReadyInBuffer).Times(0);

    const size_t num_planes = media::VideoFrame::NumPlanes(format);
    std::vector<uint32_t> stride_values(num_planes, 99999);
    auto strides = media::mojom::PlaneStrides::New(stride_values);

    auto info = media::mojom::VideoFrameInfo::New();
    info->pixel_format = format;
    info->coded_size = kTestFrameSize;
    info->visible_rect = gfx::Rect(kTestFrameSize);
    info->natural_size = kTestFrameSize;
    info->strides = std::move(strides);

    device_adapter()->OnFrameReadyInBuffer(setup.buffer_id, std::move(info));
  }
}

TEST_F(SharedMemoryVirtualDeviceMojoAdapterTest, RejectsOverflowingStrides) {
  for (media::VideoPixelFormat format :
       GetSupportedUncompressedVideoPixelFormats()) {
    SetupResult setup = RecreateAdapterAndRequestBuffer(format);

    EXPECT_CALL(*setup.video_frame_handler, DoOnFrameReadyInBuffer).Times(0);

    const size_t num_planes = media::VideoFrame::NumPlanes(format);
    std::vector<uint32_t> stride_values(num_planes,
                                        std::numeric_limits<uint32_t>::max());
    auto strides = media::mojom::PlaneStrides::New(stride_values);

    auto info = media::mojom::VideoFrameInfo::New();
    info->pixel_format = format;
    info->coded_size = kTestFrameSize;
    info->visible_rect = gfx::Rect(kTestFrameSize);
    info->natural_size = kTestFrameSize;
    info->strides = std::move(strides);

    device_adapter()->OnFrameReadyInBuffer(setup.buffer_id, std::move(info));
  }
}

TEST_F(SharedMemoryVirtualDeviceMojoAdapterTest, RejectsInvalidCodedSize) {
  for (media::VideoPixelFormat format :
       GetSupportedUncompressedVideoPixelFormats()) {
    SetupResult setup = RecreateAdapterAndRequestBuffer(format);
    EXPECT_NE(setup.buffer_id, -1);
    if (setup.buffer_id == -1) {
      continue;
    }

    EXPECT_CALL(*setup.video_frame_handler, DoOnFrameReadyInBuffer).Times(0);

    auto info = media::mojom::VideoFrameInfo::New();
    info->pixel_format = format;
    info->coded_size = gfx::Size(0, 0);
    info->visible_rect = gfx::Rect(0, 0);
    info->natural_size = gfx::Size(0, 0);
    device_adapter()->OnFrameReadyInBuffer(setup.buffer_id, std::move(info));
  }
}

TEST_F(SharedMemoryVirtualDeviceMojoAdapterTest, RejectsShortBufferAllocation) {
  for (media::VideoPixelFormat format :
       GetSupportedUncompressedVideoPixelFormats()) {
    const gfx::Size kTinySize = {16, 16};
    SetupResult setup = RecreateAdapterAndRequestBuffer(format, kTinySize);
    EXPECT_NE(setup.buffer_id, -1);
    if (setup.buffer_id == -1) {
      continue;
    }

    EXPECT_CALL(*setup.video_frame_handler, DoOnFrameReadyInBuffer).Times(0);

    auto info = media::mojom::VideoFrameInfo::New();
    info->pixel_format = format;
    info->coded_size = kTestFrameSize;
    info->visible_rect = gfx::Rect(kTestFrameSize);
    info->natural_size = kTestFrameSize;

    device_adapter()->OnFrameReadyInBuffer(setup.buffer_id, std::move(info));
  }
}

}  // namespace
}  // namespace video_capture
