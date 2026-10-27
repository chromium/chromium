// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_system_impl.h"

#include <utility>
#include <vector>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "media/capture/video/video_capture_device_factory.h"
#include "media/capture/video_capture_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace media {

TEST(VideoCaptureSystemImplTest,
     ConsolidateCaptureFormatsDeduplicatesIdenticalFormats) {
  VideoCaptureFormats formats = {
      VideoCaptureFormat(gfx::Size(1920, 1080), 30.0f, PIXEL_FORMAT_I420),
      VideoCaptureFormat(gfx::Size(1920, 1080), 30.0f, PIXEL_FORMAT_I420),
  };
  VideoCaptureSystemImpl::ConsolidateCaptureFormatsForTesting(&formats);
  EXPECT_EQ(formats.size(), 1u);
}

TEST(VideoCaptureSystemImplTest,
     ConsolidateCaptureFormatsDeduplicatesNormalizedFormatsSeparatedByNV12) {
  // A device reporting MJPEG, NV12, and YUY2 at the same resolution and frame
  // rate. MJPEG and YUY2 will both be normalized to I420.
  VideoCaptureFormats formats = {
      VideoCaptureFormat(gfx::Size(1920, 1080), 30.0f, PIXEL_FORMAT_MJPEG),
      VideoCaptureFormat(gfx::Size(1920, 1080), 30.0f, PIXEL_FORMAT_NV12),
      VideoCaptureFormat(gfx::Size(1920, 1080), 30.0f, PIXEL_FORMAT_YUY2),
  };

  VideoCaptureSystemImpl::ConsolidateCaptureFormatsForTesting(&formats);

  // We expect exactly 2 formats: one NV12 and one I420.
  EXPECT_THAT(
      formats,
      ::testing::UnorderedElementsAre(
          VideoCaptureFormat(gfx::Size(1920, 1080), 30.0f, PIXEL_FORMAT_I420),
          VideoCaptureFormat(gfx::Size(1920, 1080), 30.0f, PIXEL_FORMAT_NV12)));
}

TEST(VideoCaptureSystemImplTest,
     ConsolidateCaptureFormatsDeduplicatesIdenticalNV12SeparatedByI420) {
  VideoCaptureFormats formats = {
      VideoCaptureFormat(gfx::Size(1920, 1080), 30.0f, PIXEL_FORMAT_NV12),
      VideoCaptureFormat(gfx::Size(1920, 1080), 30.0f, PIXEL_FORMAT_I420),
      VideoCaptureFormat(gfx::Size(1920, 1080), 30.0f, PIXEL_FORMAT_NV12),
  };

  VideoCaptureSystemImpl::ConsolidateCaptureFormatsForTesting(&formats);

  // We expect exactly 2 formats: one NV12 and one I420.
  EXPECT_THAT(
      formats,
      ::testing::UnorderedElementsAre(
          VideoCaptureFormat(gfx::Size(1920, 1080), 30.0f, PIXEL_FORMAT_I420),
          VideoCaptureFormat(gfx::Size(1920, 1080), 30.0f, PIXEL_FORMAT_NV12)));
}

namespace {

class ControlledVideoCaptureDeviceFactory : public VideoCaptureDeviceFactory {
 public:
  VideoCaptureErrorOrDevice CreateDevice(
      const VideoCaptureDeviceDescriptor& device_descriptor) override {
    return VideoCaptureErrorOrDevice(
        VideoCaptureError::kVideoCaptureSystemDeviceIdNotFound);
  }

  void GetDevicesInfo(GetDevicesInfoCallback callback) override {
    GetDevicesInfo(
        mojom::VideoCaptureDeviceEnumerationMode::kDevicesWithCapabilities,
        std::move(callback));
  }

  void GetDevicesInfo(mojom::VideoCaptureDeviceEnumerationMode enumeration_mode,
                      GetDevicesInfoCallback callback) override {
    enumeration_modes.push_back(enumeration_mode);
    callbacks.push_back(std::move(callback));
  }

  void CompleteNextEnumeration(
      std::vector<VideoCaptureDeviceInfo> devices = {}) {
    ASSERT_FALSE(callbacks.empty());
    GetDevicesInfoCallback callback = std::move(callbacks.front());
    callbacks.erase(callbacks.begin());
    std::move(callback).Run(std::move(devices));
  }

  std::vector<mojom::VideoCaptureDeviceEnumerationMode> enumeration_modes;
  std::vector<GetDevicesInfoCallback> callbacks;
};

TEST(VideoCaptureSystemImplTest, MetricsWaitForNonemptyCapabilitiesResult) {
  base::HistogramTester histogram_tester;
  auto factory = std::make_unique<ControlledVideoCaptureDeviceFactory>();
  ControlledVideoCaptureDeviceFactory* factory_ptr = factory.get();
  VideoCaptureSystemImpl system(std::move(factory));

  auto enumerate = [&](std::vector<VideoCaptureDeviceInfo> devices) {
    system.GetDeviceInfosAsync(
        mojom::VideoCaptureDeviceEnumerationMode::kDevicesWithCapabilities,
        base::BindLambdaForTesting(
            [](const std::vector<VideoCaptureDeviceInfo>&) {}));
    factory_ptr->CompleteNextEnumeration(std::move(devices));
  };

  enumerate({});
  VideoCaptureDeviceInfo device;
  device.supported_formats.emplace_back(gfx::Size(640, 480), 30.0f,
                                        PIXEL_FORMAT_NV12);
  enumerate({device});
  histogram_tester.ExpectBucketCount(
      "Media.VideoCapture.Device.SupportedPixelFormat", PIXEL_FORMAT_NV12, 1);
}

TEST(VideoCaptureSystemImplTest,
     HasDeviceInfosWithCapabilitiesOnlyAfterResult) {
  auto factory = std::make_unique<ControlledVideoCaptureDeviceFactory>();
  ControlledVideoCaptureDeviceFactory* factory_ptr = factory.get();
  VideoCaptureSystemImpl system(std::move(factory));

  EXPECT_FALSE(system.HasDeviceInfosWithCapabilities());
  system.GetDeviceInfosAsync(
      mojom::VideoCaptureDeviceEnumerationMode::kDevicesWithCapabilities,
      base::BindLambdaForTesting(
          [&](const std::vector<VideoCaptureDeviceInfo>&) {
            EXPECT_TRUE(system.HasDeviceInfosWithCapabilities());
          }));
  EXPECT_FALSE(system.HasDeviceInfosWithCapabilities());

  factory_ptr->CompleteNextEnumeration();
  EXPECT_TRUE(system.HasDeviceInfosWithCapabilities());
}

#if BUILDFLAG(IS_WIN)
TEST(VideoCaptureSystemImplTest,
     DevicesWithCapabilitiesRequestWaitsForEnumeration) {
  auto factory = std::make_unique<ControlledVideoCaptureDeviceFactory>();
  ControlledVideoCaptureDeviceFactory* factory_ptr = factory.get();
  VideoCaptureSystemImpl system(std::move(factory));

  bool devices_only_callback_called = false;
  bool full_callback_called = false;
  system.GetDeviceInfosAsync(
      mojom::VideoCaptureDeviceEnumerationMode::kDevicesOnly,
      base::BindLambdaForTesting(
          [&](const std::vector<VideoCaptureDeviceInfo>&) {
            devices_only_callback_called = true;
          }));
  system.GetDeviceInfosAsync(
      mojom::VideoCaptureDeviceEnumerationMode::kDevicesWithCapabilities,
      base::BindLambdaForTesting(
          [&](const std::vector<VideoCaptureDeviceInfo>&) {
            full_callback_called = true;
          }));

  ASSERT_EQ(factory_ptr->callbacks.size(), 1U);
  ASSERT_EQ(factory_ptr->enumeration_modes.size(), 1U);
  EXPECT_EQ(factory_ptr->enumeration_modes[0],
            mojom::VideoCaptureDeviceEnumerationMode::kDevicesOnly);

  factory_ptr->CompleteNextEnumeration();
  EXPECT_TRUE(devices_only_callback_called);
  EXPECT_FALSE(full_callback_called);
  ASSERT_EQ(factory_ptr->callbacks.size(), 1U);
  ASSERT_EQ(factory_ptr->enumeration_modes.size(), 2U);
  EXPECT_EQ(factory_ptr->enumeration_modes[1],
            mojom::VideoCaptureDeviceEnumerationMode::kDevicesWithCapabilities);

  factory_ptr->CompleteNextEnumeration();
  EXPECT_TRUE(full_callback_called);
}

TEST(VideoCaptureSystemImplTest, CallbackCanStartDevicesOnlyEnumeration) {
  auto factory = std::make_unique<ControlledVideoCaptureDeviceFactory>();
  ControlledVideoCaptureDeviceFactory* factory_ptr = factory.get();
  VideoCaptureSystemImpl system(std::move(factory));

  bool devices_only_callback_called = false;
  system.GetDeviceInfosAsync(
      mojom::VideoCaptureDeviceEnumerationMode::kDevicesWithCapabilities,
      base::BindLambdaForTesting(
          [&](const std::vector<VideoCaptureDeviceInfo>&) {
            system.GetDeviceInfosAsync(
                mojom::VideoCaptureDeviceEnumerationMode::kDevicesOnly,
                base::BindLambdaForTesting(
                    [&](const std::vector<VideoCaptureDeviceInfo>&) {
                      devices_only_callback_called = true;
                    }));
          }));

  factory_ptr->CompleteNextEnumeration();
  ASSERT_EQ(factory_ptr->enumeration_modes.size(), 2U);
  EXPECT_EQ(factory_ptr->enumeration_modes[1],
            mojom::VideoCaptureDeviceEnumerationMode::kDevicesOnly);

  factory_ptr->CompleteNextEnumeration();
  EXPECT_TRUE(devices_only_callback_called);
  EXPECT_TRUE(factory_ptr->callbacks.empty());
}

TEST(VideoCaptureSystemImplTest, CallbackRequestWaitsForOlderRequest) {
  auto factory = std::make_unique<ControlledVideoCaptureDeviceFactory>();
  ControlledVideoCaptureDeviceFactory* factory_ptr = factory.get();
  VideoCaptureSystemImpl system(std::move(factory));

  std::vector<int> callback_order;
  system.GetDeviceInfosAsync(
      mojom::VideoCaptureDeviceEnumerationMode::kDevicesOnly,
      base::BindLambdaForTesting(
          [&](const std::vector<VideoCaptureDeviceInfo>&) {
            callback_order.push_back(1);
            system.GetDeviceInfosAsync(
                mojom::VideoCaptureDeviceEnumerationMode::kDevicesOnly,
                base::BindLambdaForTesting(
                    [&](const std::vector<VideoCaptureDeviceInfo>&) {
                      callback_order.push_back(3);
                    }));
          }));
  system.GetDeviceInfosAsync(
      mojom::VideoCaptureDeviceEnumerationMode::kDevicesWithCapabilities,
      base::BindLambdaForTesting(
          [&](const std::vector<VideoCaptureDeviceInfo>&) {
            callback_order.push_back(2);
          }));

  // Callback 1 queues another devices-only request. The older capabilities
  // enumeration must start next so callback 2 runs before callback 3.
  factory_ptr->CompleteNextEnumeration();
  EXPECT_EQ(callback_order, std::vector({1}));
  ASSERT_EQ(factory_ptr->enumeration_modes.size(), 2u);
  EXPECT_EQ(factory_ptr->enumeration_modes[1],
            mojom::VideoCaptureDeviceEnumerationMode::kDevicesWithCapabilities);

  factory_ptr->CompleteNextEnumeration();
  EXPECT_EQ(callback_order, std::vector({1, 2, 3}));
  EXPECT_TRUE(factory_ptr->callbacks.empty());
}

TEST(VideoCaptureSystemImplTest,
     DevicesOnlyResultCompletesAllSatisfiableRequests) {
  auto factory = std::make_unique<ControlledVideoCaptureDeviceFactory>();
  ControlledVideoCaptureDeviceFactory* factory_ptr = factory.get();
  VideoCaptureSystemImpl system(std::move(factory));

  bool first_devices_only_callback_called = false;
  bool full_callback_called = false;
  bool second_devices_only_callback_called = false;
  system.GetDeviceInfosAsync(
      mojom::VideoCaptureDeviceEnumerationMode::kDevicesOnly,
      base::BindLambdaForTesting(
          [&](const std::vector<VideoCaptureDeviceInfo>&) {
            first_devices_only_callback_called = true;
          }));
  system.GetDeviceInfosAsync(
      mojom::VideoCaptureDeviceEnumerationMode::kDevicesWithCapabilities,
      base::BindLambdaForTesting(
          [&](const std::vector<VideoCaptureDeviceInfo>&) {
            full_callback_called = true;
          }));
  system.GetDeviceInfosAsync(
      mojom::VideoCaptureDeviceEnumerationMode::kDevicesOnly,
      base::BindLambdaForTesting(
          [&](const std::vector<VideoCaptureDeviceInfo>&) {
            second_devices_only_callback_called = true;
          }));

  ASSERT_EQ(factory_ptr->enumeration_modes.size(), 1u);
  EXPECT_EQ(factory_ptr->enumeration_modes[0],
            mojom::VideoCaptureDeviceEnumerationMode::kDevicesOnly);

  factory_ptr->CompleteNextEnumeration();
  EXPECT_TRUE(first_devices_only_callback_called);
  EXPECT_FALSE(full_callback_called);
  EXPECT_TRUE(second_devices_only_callback_called);
  ASSERT_EQ(factory_ptr->enumeration_modes.size(), 2u);
  EXPECT_EQ(factory_ptr->enumeration_modes[1],
            mojom::VideoCaptureDeviceEnumerationMode::kDevicesWithCapabilities);

  factory_ptr->CompleteNextEnumeration();
  EXPECT_TRUE(full_callback_called);
  EXPECT_TRUE(factory_ptr->callbacks.empty());
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace
}  // namespace media
