// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/linux/video_capture_device_factory_v4l2.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "media/capture/video/linux/fake_device_provider.h"
#include "media/capture/video/linux/fake_v4l2_impl.h"
#include "media/capture/video/mock_video_capture_device_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace media {

class VideoCaptureDeviceFactoryV4L2Test
    : public ::testing::TestWithParam<VideoCaptureDeviceDescriptor> {
 public:
  VideoCaptureDeviceFactoryV4L2Test() {}
  ~VideoCaptureDeviceFactoryV4L2Test() override = default;

  void SetUp() override {
    factory_ = std::make_unique<VideoCaptureDeviceFactoryV4L2>(
        base::SingleThreadTaskRunner::GetCurrentDefault());
    auto fake_v4l2 = base::MakeRefCounted<FakeV4L2Impl>();
    fake_v4l2_ = fake_v4l2.get();
    auto fake_device_provider = std::make_unique<FakeDeviceProvider>();
    fake_device_provider_ = fake_device_provider.get();
    factory_->SetV4L2EnvironmentForTesting(std::move(fake_v4l2),
                                           std::move(fake_device_provider));
  }

  void TearDown() override { task_environment_.RunUntilIdle(); }

  // Adds one fake device with |config| and returns the distinct frame sizes
  // the factory lists for it, in listing order.
  std::vector<gfx::Size> ListFrameSizes(const FakeV4L2DeviceConfig& config) {
    fake_device_provider_->AddDevice(config.descriptor);
    fake_v4l2_->AddDevice(config.descriptor.device_id, config);
    std::vector<VideoCaptureDeviceInfo> devices_info;
    base::RunLoop run_loop;
    factory_->GetDevicesInfo(base::BindLambdaForTesting(
        [&devices_info, &run_loop](std::vector<VideoCaptureDeviceInfo> result) {
          devices_info = std::move(result);
          run_loop.Quit();
        }));
    run_loop.Run();
    std::vector<gfx::Size> sizes;
    if (devices_info.size() != 1u) {
      return sizes;
    }
    for (const VideoCaptureFormat& format : devices_info[0].supported_formats) {
      if (!std::ranges::contains(sizes, format.frame_size)) {
        sizes.push_back(format.frame_size);
      }
    }
    return sizes;
  }

  // Expects |sizes| to be a non-empty list of modes of |range|: each within its
  // bounds and on its step grid, which starts at the minimum.
  static void ExpectSizesInRange(const std::vector<gfx::Size>& sizes,
                                 const v4l2_frmsize_stepwise& range) {
    ASSERT_THAT(sizes, ::testing::Not(::testing::IsEmpty()));
    for (const gfx::Size& size : sizes) {
      const uint32_t width = size.width();
      const uint32_t height = size.height();
      EXPECT_TRUE(width >= range.min_width && width <= range.max_width &&
                  height >= range.min_height && height <= range.max_height)
          << size.ToString();
      EXPECT_EQ(0u, (width - range.min_width) % range.step_width)
          << size.ToString();
      EXPECT_EQ(0u, (height - range.min_height) % range.step_height)
          << size.ToString();
    }
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<VideoCaptureDeviceFactoryV4L2> factory_;
  raw_ptr<FakeV4L2Impl> fake_v4l2_;
  raw_ptr<FakeDeviceProvider> fake_device_provider_;
};

TEST_P(VideoCaptureDeviceFactoryV4L2Test, EnumerateSingleFakeV4L2DeviceUsing) {
  // Setup
  const VideoCaptureDeviceDescriptor& descriptor = GetParam();
  fake_device_provider_->AddDevice(descriptor);
  fake_v4l2_->AddDevice(descriptor.device_id, FakeV4L2DeviceConfig(descriptor));

  // Exercise
  std::vector<media::VideoCaptureDeviceInfo> devices_info;
  base::RunLoop run_loop;
  factory_->GetDevicesInfo(base::BindLambdaForTesting(
      [&devices_info,
       &run_loop](std::vector<media::VideoCaptureDeviceInfo> result) {
        devices_info = std::move(result);
        run_loop.Quit();
      }));
  run_loop.Run();

  // Verification
  ASSERT_EQ(1u, devices_info.size());
  EXPECT_EQ(descriptor.device_id, devices_info[0].descriptor.device_id);
  EXPECT_EQ(descriptor.display_name(),
            devices_info[0].descriptor.display_name());
  EXPECT_EQ(descriptor.control_support().pan,
            devices_info[0].descriptor.control_support().pan);
  EXPECT_EQ(descriptor.control_support().tilt,
            devices_info[0].descriptor.control_support().tilt);
  EXPECT_EQ(descriptor.control_support().zoom,
            devices_info[0].descriptor.control_support().zoom);
}

TEST_F(VideoCaptureDeviceFactoryV4L2Test, DiscreteDeviceListsItsSize) {
  FakeV4L2DeviceConfig config(
      VideoCaptureDeviceDescriptor("Fake Device 0", "/dev/video0",
                                   VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE));
  EXPECT_THAT(ListFrameSizes(config),
              ::testing::ElementsAre(gfx::Size(640, 480)));
}

// A stepwise range is not dropped: its maximum size, the current size and the
// common sizes on its step grid are listed once each, largest first.
TEST_F(VideoCaptureDeviceFactoryV4L2Test, StepwiseDeviceListsSizesInRange) {
  FakeV4L2DeviceConfig config(
      VideoCaptureDeviceDescriptor("Fake Device 0", "/dev/video0",
                                   VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE));
  config.stepwise_frame_sizes = v4l2_frmsize_stepwise{.min_width = 320,
                                                      .max_width = 1296,
                                                      .step_width = 8,
                                                      .min_height = 240,
                                                      .max_height = 736,
                                                      .step_height = 1};
  config.crop_bounds = gfx::Size(1296, 736);
  EXPECT_THAT(ListFrameSizes(config),
              ::testing::ElementsAre(gfx::Size(1296, 736), gfx::Size(1280, 720),
                                     gfx::Size(800, 600), gfx::Size(640, 480),
                                     gfx::Size(640, 360), gfx::Size(352, 288),
                                     gfx::Size(320, 240)));
}

// VIDIOC_TRY_FMT is optional: without it, the sizes on the step grid are
// listed as they are.
TEST_F(VideoCaptureDeviceFactoryV4L2Test,
       StepwiseDeviceWithoutTryFmtListsSizesOnGrid) {
  FakeV4L2DeviceConfig config(
      VideoCaptureDeviceDescriptor("Fake Device 0", "/dev/video0",
                                   VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE));
  config.stepwise_frame_sizes = v4l2_frmsize_stepwise{.min_width = 320,
                                                      .max_width = 1296,
                                                      .step_width = 8,
                                                      .min_height = 240,
                                                      .max_height = 736,
                                                      .step_height = 1};
  config.crop_bounds = gfx::Size(1296, 736);
  config.try_fmt_supported = false;
  EXPECT_THAT(ListFrameSizes(config),
              ::testing::ElementsAre(gfx::Size(1296, 736), gfx::Size(1280, 720),
                                     gfx::Size(800, 600), gfx::Size(640, 480),
                                     gfx::Size(640, 360), gfx::Size(352, 288),
                                     gfx::Size(320, 240)));
}

// The crop bounds are listed; they do not limit the other sizes.
TEST_F(VideoCaptureDeviceFactoryV4L2Test, StepwiseDeviceListsCropBounds) {
  FakeV4L2DeviceConfig config(
      VideoCaptureDeviceDescriptor("Fake Device 0", "/dev/video0",
                                   VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE));
  config.stepwise_frame_sizes = v4l2_frmsize_stepwise{.min_width = 16,
                                                      .max_width = 16384,
                                                      .step_width = 2,
                                                      .min_height = 16,
                                                      .max_height = 8640,
                                                      .step_height = 2};
  config.crop_bounds = gfx::Size(1296, 736);
  const std::vector<gfx::Size> sizes = ListFrameSizes(config);
  ASSERT_EQ(16u, sizes.size());
  EXPECT_EQ(gfx::Size(16384, 8640), sizes.front());
  EXPECT_TRUE(std::ranges::contains(sizes, gfx::Size(1296, 736)));
  EXPECT_TRUE(std::ranges::contains(sizes, gfx::Size(3840, 2160)));
}

// The range maximum is listed whatever its size.
TEST_F(VideoCaptureDeviceFactoryV4L2Test, StepwiseDeviceListsRangeMaximum) {
  FakeV4L2DeviceConfig config(
      VideoCaptureDeviceDescriptor("Fake Device 0", "/dev/video0",
                                   VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE));
  config.stepwise_frame_sizes = v4l2_frmsize_stepwise{.min_width = 16,
                                                      .max_width = 16384,
                                                      .step_width = 2,
                                                      .min_height = 16,
                                                      .max_height = 8640,
                                                      .step_height = 2};
  EXPECT_THAT(ListFrameSizes(config),
              ::testing::Contains(gfx::Size(16384, 8640)));
}

// A grid that neither the maximum nor any common size lies on still yields
// its minimum.
TEST_F(VideoCaptureDeviceFactoryV4L2Test,
       StepwiseDeviceOffGridFallsBackToMinimum) {
  FakeV4L2DeviceConfig config(
      VideoCaptureDeviceDescriptor("Fake Device 0", "/dev/video0",
                                   VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE));
  config.stepwise_frame_sizes = v4l2_frmsize_stepwise{.min_width = 321,
                                                      .max_width = 16384,
                                                      .step_width = 8,
                                                      .min_height = 241,
                                                      .max_height = 8640,
                                                      .step_height = 8};
  EXPECT_THAT(ListFrameSizes(config),
              ::testing::ElementsAre(gfx::Size(321, 241)));
}

TEST_F(VideoCaptureDeviceFactoryV4L2Test, ContinuousDeviceListsSizesInRange) {
  FakeV4L2DeviceConfig config(
      VideoCaptureDeviceDescriptor("Fake Device 0", "/dev/video0",
                                   VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE));
  config.stepwise_frame_sizes = v4l2_frmsize_stepwise{.min_width = 100,
                                                      .max_width = 2000,
                                                      .step_width = 1,
                                                      .min_height = 100,
                                                      .max_height = 2000,
                                                      .step_height = 1};
  config.frame_size_type = V4L2_FRMSIZE_TYPE_CONTINUOUS;
  const std::vector<gfx::Size> sizes = ListFrameSizes(config);
  ASSERT_EQ(13u, sizes.size());
  EXPECT_EQ(gfx::Size(2000, 2000), sizes.front());
  EXPECT_TRUE(std::ranges::contains(sizes, gfx::Size(640, 480)));
}

// Sizes the driver would change are not listed.
TEST_F(VideoCaptureDeviceFactoryV4L2Test,
       StepwiseDeviceDoesNotListSizesTheDriverChanges) {
  FakeV4L2DeviceConfig config(
      VideoCaptureDeviceDescriptor("Fake Device 0", "/dev/video0",
                                   VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE));
  config.stepwise_frame_sizes = v4l2_frmsize_stepwise{.min_width = 320,
                                                      .max_width = 1296,
                                                      .step_width = 8,
                                                      .min_height = 240,
                                                      .max_height = 736,
                                                      .step_height = 1};
  config.crop_bounds = gfx::Size(1296, 736);
  config.height_alignment = 16;
  EXPECT_THAT(ListFrameSizes(config),
              ::testing::ElementsAre(gfx::Size(1296, 736), gfx::Size(1280, 720),
                                     gfx::Size(640, 480), gfx::Size(352, 288),
                                     gfx::Size(320, 240)));
}

// A 4K camera range: its maximum is listed, and every listed size is a mode of
// the range.
TEST_F(VideoCaptureDeviceFactoryV4L2Test, StepwiseDeviceLists4KMaximum) {
  FakeV4L2DeviceConfig config(
      VideoCaptureDeviceDescriptor("Fake Device 0", "/dev/video0",
                                   VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE));
  const v4l2_frmsize_stepwise range{.min_width = 160,
                                    .max_width = 3840,
                                    .step_width = 8,
                                    .min_height = 120,
                                    .max_height = 2160,
                                    .step_height = 8};
  config.stepwise_frame_sizes = range;
  const std::vector<gfx::Size> sizes = ListFrameSizes(config);
  EXPECT_THAT(sizes, ::testing::Contains(gfx::Size(3840, 2160)));
  ExpectSizesInRange(sizes, range);
}

// An 8K camera range: its maximum is listed although it is larger than every
// common size, and every listed size is a mode of the range.
TEST_F(VideoCaptureDeviceFactoryV4L2Test, StepwiseDeviceLists8KMaximum) {
  FakeV4L2DeviceConfig config(
      VideoCaptureDeviceDescriptor("Fake Device 0", "/dev/video0",
                                   VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE));
  const v4l2_frmsize_stepwise range{.min_width = 160,
                                    .max_width = 7680,
                                    .step_width = 8,
                                    .min_height = 120,
                                    .max_height = 4320,
                                    .step_height = 8};
  config.stepwise_frame_sizes = range;
  const std::vector<gfx::Size> sizes = ListFrameSizes(config);
  EXPECT_THAT(sizes, ::testing::Contains(gfx::Size(7680, 4320)));
  ExpectSizesInRange(sizes, range);
}

// On a grid that few common sizes lie on, every listed size is still a mode of
// the range.
TEST_F(VideoCaptureDeviceFactoryV4L2Test,
       StepwiseDeviceListsOnlySizesOnItsGrid) {
  FakeV4L2DeviceConfig config(
      VideoCaptureDeviceDescriptor("Fake Device 0", "/dev/video0",
                                   VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE));
  const v4l2_frmsize_stepwise range{.min_width = 100,
                                    .max_width = 2000,
                                    .step_width = 3,
                                    .min_height = 100,
                                    .max_height = 2000,
                                    .step_height = 7};
  config.stepwise_frame_sizes = range;
  ExpectSizesInRange(ListFrameSizes(config), range);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    VideoCaptureDeviceFactoryV4L2Test,
    ::testing::Values(
        VideoCaptureDeviceDescriptor("Fake Device 0",
                                     "/dev/video0",
                                     VideoCaptureApi::UNKNOWN,
                                     /*control_support=*/{false, false, false}),
        VideoCaptureDeviceDescriptor("Fake Device 0",
                                     "/dev/video0",
                                     VideoCaptureApi::UNKNOWN,
                                     /*control_support=*/{false, false, true}),
        VideoCaptureDeviceDescriptor("Fake Device 0",
                                     "/dev/video0",
                                     VideoCaptureApi::UNKNOWN,
                                     /*control_support=*/{true, true, true})));

TEST_F(VideoCaptureDeviceFactoryV4L2Test,
       ReceiveFramesFromSinglePlaneFakeDevice) {
  // Setup
  const std::string stub_display_name = "Fake Device 0";
  const std::string stub_device_id = "/dev/video0";
  VideoCaptureDeviceDescriptor descriptor(
      stub_display_name, stub_device_id,
      VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE);
  fake_device_provider_->AddDevice(descriptor);
  fake_v4l2_->AddDevice(stub_device_id, FakeV4L2DeviceConfig(descriptor));

  // Exercise
  auto device = factory_->CreateDevice(descriptor).ReleaseDevice();
  VideoCaptureParams arbitrary_params;
  arbitrary_params.requested_format.frame_size = gfx::Size(1280, 720);
  arbitrary_params.requested_format.frame_rate = 30.0f;
  arbitrary_params.requested_format.pixel_format = PIXEL_FORMAT_I420;
  auto client = std::make_unique<NiceMockVideoCaptureDeviceClient>();
  MockVideoCaptureDeviceClient* client_ptr = client.get();

  base::RunLoop wait_loop;
  static const int kFrameToReceive = 3;
  EXPECT_CALL(*client_ptr, OnIncomingCapturedData)
      .WillRepeatedly([&wait_loop]() {
        static int received_frame_count = 0;
        received_frame_count++;
        if (received_frame_count == kFrameToReceive) {
          wait_loop.Quit();
        }
      });

  device->AllocateAndStart(arbitrary_params, std::move(client));
  wait_loop.Run();

  device->StopAndDeAllocate();
}

}  // namespace media
