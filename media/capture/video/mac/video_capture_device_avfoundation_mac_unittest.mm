// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/mac/video_capture_device_avfoundation_mac.h"
#include "media/capture/video/mac/test/fake_av_capture_device_format.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/mac/scoped_nsobject.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "media/base/video_types.h"
#include "media/capture/video/mac/sample_buffer_transformer_mac.h"
#include "media/capture/video/mac/test/mock_video_capture_device_avfoundation_frame_receiver_mac.h"
#include "media/capture/video/mac/test/pixel_buffer_test_utils_mac.h"
#include "media/capture/video/mac/test/video_capture_test_utils_mac.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"

using testing::_;
using testing::Gt;
using testing::Ne;
using testing::Return;

namespace media {

// TODO(https://crbug.com/1383901): Fix and re-enable these tests.
TEST(VideoCaptureDeviceAVFoundationMacTest,
     DISABLED_OutputsNv12WithoutScalingByDefault) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kInCapturerScaling);

  RunTestCase(base::BindOnce([] {
    NSString* deviceId = GetFirstDeviceId();
    if (!deviceId) {
      LOG(ERROR) << "No camera available. Exiting test.";
      return;
    }

    testing::NiceMock<MockVideoCaptureDeviceAVFoundationFrameReceiver>
        frame_receiver;
    base::scoped_nsobject<VideoCaptureDeviceAVFoundation> captureDevice(
        [[VideoCaptureDeviceAVFoundation alloc]
            initWithFrameReceiver:&frame_receiver]);

    NSString* errorMessage = nil;
    ASSERT_TRUE([captureDevice setCaptureDevice:deviceId
                                   errorMessage:&errorMessage]);
    ASSERT_TRUE([captureDevice startCapture]);

    bool has_received_first_frame = false;
    base::RunLoop first_frame_received(
        base::RunLoop::Type::kNestableTasksAllowed);
    EXPECT_CALL(frame_receiver, ReceiveExternalGpuMemoryBufferFrame)
        .WillRepeatedly(testing::Invoke(
            [&](CapturedExternalVideoBuffer frame,
                std::vector<CapturedExternalVideoBuffer> scaled_frames,
                base::TimeDelta timestamp) {
              if (has_received_first_frame) {
                // Ignore subsequent frames.
                return;
              }
              EXPECT_EQ(frame.format.pixel_format, PIXEL_FORMAT_NV12);
              EXPECT_TRUE(scaled_frames.empty());
              has_received_first_frame = true;
              first_frame_received.Quit();
            }));
    first_frame_received.Run();

    [captureDevice stopCapture];
  }));
}

// TODO(https://crbug.com/1383901): Fix and re-enable these tests.
TEST(VideoCaptureDeviceAVFoundationMacTest,
     DISABLED_SpecifiedScalingIsIgnoredWhenInCapturerScalingIsNotEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  // By default, kInCapturerScaling is false.
  EXPECT_FALSE(base::FeatureList::IsEnabled(kInCapturerScaling));

  RunTestCase(base::BindOnce([] {
    NSString* deviceId = GetFirstDeviceId();
    if (!deviceId) {
      LOG(ERROR) << "No camera available. Exiting test.";
      return;
    }

    testing::NiceMock<MockVideoCaptureDeviceAVFoundationFrameReceiver>
        frame_receiver;
    base::scoped_nsobject<VideoCaptureDeviceAVFoundation> captureDevice(
        [[VideoCaptureDeviceAVFoundation alloc]
            initWithFrameReceiver:&frame_receiver]);

    NSString* errorMessage = nil;
    ASSERT_TRUE([captureDevice setCaptureDevice:deviceId
                                   errorMessage:&errorMessage]);

    std::vector<gfx::Size> scaled_resolutions;
    scaled_resolutions.emplace_back(320, 240);
    [captureDevice setScaledResolutions:scaled_resolutions];

    ASSERT_TRUE([captureDevice startCapture]);

    bool has_received_first_frame = false;
    base::RunLoop first_frame_received(
        base::RunLoop::Type::kNestableTasksAllowed);
    EXPECT_CALL(frame_receiver, ReceiveExternalGpuMemoryBufferFrame)
        .WillRepeatedly(testing::Invoke(
            [&](CapturedExternalVideoBuffer frame,
                std::vector<CapturedExternalVideoBuffer> scaled_frames,
                base::TimeDelta timestamp) {
              if (has_received_first_frame) {
                // Ignore subsequent frames.
                return;
              }
              EXPECT_TRUE(scaled_frames.empty());
              has_received_first_frame = true;
              first_frame_received.Quit();
            }));
    first_frame_received.Run();

    [captureDevice stopCapture];
  }));
}

// TODO(https://crbug.com/1383901): Fix and re-enable these tests.
TEST(VideoCaptureDeviceAVFoundationMacTest,
     DISABLED_SpecifiedScalingOutputsNv12) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kInCapturerScaling);

  RunTestCase(base::BindOnce([] {
    NSString* deviceId = GetFirstDeviceId();
    if (!deviceId) {
      LOG(ERROR) << "No camera available. Exiting test.";
      return;
    }

    testing::NiceMock<MockVideoCaptureDeviceAVFoundationFrameReceiver>
        frame_receiver;
    base::scoped_nsobject<VideoCaptureDeviceAVFoundation> captureDevice(
        [[VideoCaptureDeviceAVFoundation alloc]
            initWithFrameReceiver:&frame_receiver]);

    NSString* errorMessage = nil;
    ASSERT_TRUE([captureDevice setCaptureDevice:deviceId
                                   errorMessage:&errorMessage]);

    std::vector<gfx::Size> scaled_resolutions;
    scaled_resolutions.emplace_back(320, 240);
    [captureDevice setScaledResolutions:scaled_resolutions];

    ASSERT_TRUE([captureDevice startCapture]);

    bool has_received_first_frame = false;
    base::RunLoop first_frame_received(
        base::RunLoop::Type::kNestableTasksAllowed);
    EXPECT_CALL(frame_receiver, ReceiveExternalGpuMemoryBufferFrame)
        .WillRepeatedly(testing::Invoke(
            [&](CapturedExternalVideoBuffer frame,
                std::vector<CapturedExternalVideoBuffer> scaled_frames,
                base::TimeDelta timestamp) {
              if (has_received_first_frame) {
                // Ignore subsequent frames.
                return;
              }
              EXPECT_EQ(frame.format.pixel_format, PIXEL_FORMAT_NV12);
              ASSERT_EQ(scaled_frames.size(), 1u);
              EXPECT_EQ(scaled_frames[0].format.frame_size,
                        scaled_resolutions[0]);
              EXPECT_EQ(scaled_frames[0].format.pixel_format,
                        PIXEL_FORMAT_NV12);
              has_received_first_frame = true;
              first_frame_received.Quit();
            }));
    first_frame_received.Run();

    [captureDevice stopCapture];
  }));
}

// TODO(https://crbug.com/1383901): Fix and re-enable these tests.
TEST(VideoCaptureDeviceAVFoundationMacTest,
     DISABLED_SpecifiedScalingCanChangeDuringCapture) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kInCapturerScaling);

  RunTestCase(base::BindOnce([] {
    NSString* deviceId = GetFirstDeviceId();
    if (!deviceId) {
      LOG(ERROR) << "No camera available. Exiting test.";
      return;
    }

    testing::NiceMock<MockVideoCaptureDeviceAVFoundationFrameReceiver>
        frame_receiver;
    base::scoped_nsobject<VideoCaptureDeviceAVFoundation> captureDevice(
        [[VideoCaptureDeviceAVFoundation alloc]
            initWithFrameReceiver:&frame_receiver]);

    NSString* errorMessage = nil;
    ASSERT_TRUE([captureDevice setCaptureDevice:deviceId
                                   errorMessage:&errorMessage]);

    // Start capture without scaling and wait until frames are flowing.
    [captureDevice setScaledResolutions:{}];
    ASSERT_TRUE([captureDevice startCapture]);

    bool has_received_first_frame = false;
    base::RunLoop first_frame_received(
        base::RunLoop::Type::kNestableTasksAllowed);
    EXPECT_CALL(frame_receiver, ReceiveExternalGpuMemoryBufferFrame)
        .WillRepeatedly(testing::Invoke(
            [&](CapturedExternalVideoBuffer frame,
                std::vector<CapturedExternalVideoBuffer> scaled_frames,
                base::TimeDelta timestamp) {
              if (has_received_first_frame) {
                // Ignore subsequent frames.
                return;
              }
              EXPECT_TRUE(scaled_frames.empty());
              has_received_first_frame = true;
              first_frame_received.Quit();
            }));
    first_frame_received.Run();

    // Specify scaling and wait for scaled frames to arrive.
    std::vector<gfx::Size> scaled_resolutions;
    scaled_resolutions.emplace_back(320, 240);
    [captureDevice setScaledResolutions:scaled_resolutions];

    bool has_received_scaled_frame = false;
    base::RunLoop scaled_frame_received(
        base::RunLoop::Type::kNestableTasksAllowed);
    EXPECT_CALL(frame_receiver, ReceiveExternalGpuMemoryBufferFrame)
        .WillRepeatedly(testing::Invoke(
            [&](CapturedExternalVideoBuffer frame,
                std::vector<CapturedExternalVideoBuffer> scaled_frames,
                base::TimeDelta timestamp) {
              if (has_received_scaled_frame || scaled_frames.empty()) {
                // Ignore subsequent frames.
                return;
              }
              has_received_scaled_frame = true;
              scaled_frame_received.Quit();
            }));
    scaled_frame_received.Run();

    [captureDevice stopCapture];
  }));
}

// TODO(https://crbug.com/1383901): Fix and re-enable these tests.
TEST(VideoCaptureDeviceAVFoundationMacTest,
     DISABLED_SpecifiedScalingUsesGoodSizesButNotBadSizes) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kInCapturerScaling);

  RunTestCase(base::BindOnce([] {
    VideoCaptureDeviceFactoryMac video_capture_device_factory;
    std::vector<VideoCaptureDeviceInfo> device_infos =
        GetDevicesInfo(&video_capture_device_factory);
    if (device_infos.empty()) {
      LOG(ERROR) << "No camera available. Exiting test.";
      return;
    }
    const auto& device_info = device_infos.front();
    NSString* deviceId =
        base::SysUTF8ToNSString(device_info.descriptor.device_id);
    VideoCaptureFormat camera_format = device_info.supported_formats.front();

    testing::NiceMock<MockVideoCaptureDeviceAVFoundationFrameReceiver>
        frame_receiver;
    base::scoped_nsobject<VideoCaptureDeviceAVFoundation> captureDevice(
        [[VideoCaptureDeviceAVFoundation alloc]
            initWithFrameReceiver:&frame_receiver]);

    NSString* errorMessage = nil;
    ASSERT_TRUE([captureDevice setCaptureDevice:deviceId
                                   errorMessage:&errorMessage]);

    // Capture at a lower resolution than we request to scale.
    ASSERT_TRUE([captureDevice
        setCaptureHeight:camera_format.frame_size.height()
                   width:camera_format.frame_size.width()
               frameRate:camera_format.frame_rate]);
    std::vector<gfx::Size> scaled_resolutions;
    // Bad resolution because it causes upscale.
    scaled_resolutions.emplace_back(camera_format.frame_size.width() * 2,
                                    camera_format.frame_size.height() * 2);
    // Bad resolution because it is the same as the captured resolution.
    scaled_resolutions.push_back(camera_format.frame_size);
    // Good resolution because it causes downscale in both dimensions.
    scaled_resolutions.emplace_back(camera_format.frame_size.width() / 2,
                                    camera_format.frame_size.height() / 2);
    // Good resolution because it causes downscale in both dimensions.
    scaled_resolutions.emplace_back(camera_format.frame_size.width() / 4,
                                    camera_format.frame_size.height() / 4);
    // Good resolution because it causes downscale in one dimension (stretch).
    scaled_resolutions.emplace_back(camera_format.frame_size.width() / 2,
                                    camera_format.frame_size.height());
    [captureDevice setScaledResolutions:scaled_resolutions];

    ASSERT_TRUE([captureDevice startCapture]);

    bool has_received_first_frame = false;
    base::RunLoop first_frame_received(
        base::RunLoop::Type::kNestableTasksAllowed);
    EXPECT_CALL(frame_receiver, ReceiveExternalGpuMemoryBufferFrame)
        .WillRepeatedly(testing::Invoke(
            [&](CapturedExternalVideoBuffer frame,
                std::vector<CapturedExternalVideoBuffer> scaled_frames,
                base::TimeDelta timestamp) {
              if (has_received_first_frame) {
                // Normally we have time to stop capturing before multiple
                // frames are received but in order for the test to be able to
                // run on slow bots we are prepared for this method to be
                // invoked any number of times. Frames subsequent the first one
                // are ignored.
                return;
              }

              EXPECT_EQ(scaled_frames.size(), 3u);
              // The bad resolutions were ignored and the good resolutions are
              // outputted in the requested order.
              EXPECT_EQ(scaled_frames[0].format.frame_size,
                        scaled_resolutions[2]);
              EXPECT_EQ(scaled_frames[1].format.frame_size,
                        scaled_resolutions[3]);
              EXPECT_EQ(scaled_frames[2].format.frame_size,
                        scaled_resolutions[4]);

              has_received_first_frame = true;
              first_frame_received.Quit();
            }));
    first_frame_received.Run();

    [captureDevice stopCapture];
  }));
}

// This is approximately the same test as the one above except it does not rely
// on having a camera. Instead we mock-invoke processPixelBufferNV12IOSurface

// TODO(https://crbug.com/1383901): Fix and re-enable these tests.// from the
// test as-if a camera had produced a frame.
TEST(VideoCaptureDeviceAVFoundationMacTest,
     DISABLED_ProcessPixelBufferNV12IOSurfaceWithGoodAndBadScaling) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kInCapturerScaling);

  RunTestCase(base::BindOnce([] {
    testing::NiceMock<MockVideoCaptureDeviceAVFoundationFrameReceiver>
        frame_receiver;
    base::scoped_nsobject<VideoCaptureDeviceAVFoundation> captureDevice(
        [[VideoCaptureDeviceAVFoundation alloc]
            initWithFrameReceiver:&frame_receiver]);

    // Capture at a lower resolution than we request to scale.
    gfx::Size capture_resolution(640, 360);
    std::vector<gfx::Size> scaled_resolutions;
    // Bad resolution because it causes upscale.
    scaled_resolutions.emplace_back(capture_resolution.width() * 2,
                                    capture_resolution.height() * 2);
    // Bad resolution because it is the same as the captured resolution.
    scaled_resolutions.push_back(capture_resolution);
    // Good resolution because it causes downscale in one dimension (stretch).
    scaled_resolutions.emplace_back(capture_resolution.width() / 2,
                                    capture_resolution.height());
    // Good resolution because it causes downscale in both dimensions.
    scaled_resolutions.emplace_back(capture_resolution.width() / 2,
                                    capture_resolution.height() / 2);
    // Good resolution because it causes downscale in both dimensions.
    scaled_resolutions.emplace_back(capture_resolution.width() / 4,
                                    capture_resolution.height() / 4);
    [captureDevice setScaledResolutions:scaled_resolutions];

    // Create a blank NV12 pixel buffer that we pretend was captured.
    VideoCaptureFormat capture_format(capture_resolution, 30,
                                      PIXEL_FORMAT_NV12);
    std::unique_ptr<ByteArrayPixelBuffer> yuvs_buffer =
        CreateYuvsPixelBufferFromSingleRgbColor(
            capture_resolution.width(), capture_resolution.height(), 0, 0, 0);
    base::ScopedCFTypeRef<CVPixelBufferRef> pixelBuffer =
        PixelBufferPool::Create(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
                                capture_resolution.width(),
                                capture_resolution.height(), 1)
            ->CreateBuffer();
    DCHECK(PixelBufferTransferer().TransferImage(yuvs_buffer->pixel_buffer,
                                                 pixelBuffer));

    [captureDevice
        callLocked:base::BindLambdaForTesting([&] {
          EXPECT_CALL(frame_receiver, ReceiveExternalGpuMemoryBufferFrame)
              .WillOnce(testing::Invoke(
                  [&](CapturedExternalVideoBuffer frame,
                      std::vector<CapturedExternalVideoBuffer> scaled_frames,
                      base::TimeDelta timestamp) {
                    EXPECT_EQ(scaled_frames.size(), 3u);
                    // The bad resolutions were ignored and the good
                    // resolutions are outputted in the requested order.
                    EXPECT_EQ(scaled_frames[0].format.frame_size,
                              scaled_resolutions[2]);
                    EXPECT_EQ(scaled_frames[1].format.frame_size,
                              scaled_resolutions[3]);
                    EXPECT_EQ(scaled_frames[2].format.frame_size,
                              scaled_resolutions[4]);
                  }));
          [captureDevice
              processPixelBufferNV12IOSurface:pixelBuffer
                                captureFormat:capture_format
                                   colorSpace:gfx::ColorSpace::CreateSRGB()
                                    timestamp:base::TimeDelta()];
        })];
  }));
}

class VideoCaptureDeviceAVFoundationMacTakePhotoTest
    : public testing::TestWithParam<bool> {
 public:
  base::scoped_nsobject<VideoCaptureDeviceAVFoundation> CreateCaptureDevice(
      testing::NiceMock<MockVideoCaptureDeviceAVFoundationFrameReceiver>*
          frame_receiver) {
    base::scoped_nsobject<VideoCaptureDeviceAVFoundation> captureDevice(
        [[VideoCaptureDeviceAVFoundation alloc]
            initWithFrameReceiver:frame_receiver]);
    [captureDevice setForceLegacyStillImageApiForTesting:GetParam()];
    return captureDevice;
  }
};

TEST_P(VideoCaptureDeviceAVFoundationMacTakePhotoTest, TakePhoto) {
  RunTestCase(base::BindOnce(
      [](VideoCaptureDeviceAVFoundationMacTakePhotoTest* thiz) {
        NSString* deviceId = GetFirstDeviceId();
        if (!deviceId) {
          DVLOG(1) << "No camera available. Exiting test.";
          return;
        }

        testing::NiceMock<MockVideoCaptureDeviceAVFoundationFrameReceiver>
            frame_receiver;
        base::scoped_nsobject<VideoCaptureDeviceAVFoundation> captureDevice =
            thiz->CreateCaptureDevice(&frame_receiver);

        NSString* errorMessage = nil;
        ASSERT_TRUE([captureDevice setCaptureDevice:deviceId
                                       errorMessage:&errorMessage]);
        ASSERT_TRUE([captureDevice startCapture]);

        base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
        EXPECT_CALL(frame_receiver, OnPhotoTaken)

            .WillOnce([&run_loop](const uint8_t* image_data,
                                  size_t image_length,
                                  const std::string& mime_type) {
              EXPECT_TRUE(image_data);
              EXPECT_GT(image_length, 0u);
              EXPECT_EQ(mime_type, "image/jpeg");
              run_loop.Quit();
            });
        [captureDevice takePhoto];
        run_loop.Run();
      },
      this));
}

TEST_P(VideoCaptureDeviceAVFoundationMacTakePhotoTest,
       StopCaptureWhileTakingPhoto) {
  RunTestCase(base::BindOnce(
      [](VideoCaptureDeviceAVFoundationMacTakePhotoTest* thiz) {
        NSString* deviceId = GetFirstDeviceId();
        if (!deviceId) {
          DVLOG(1) << "No camera available. Exiting test.";
          return;
        }

        testing::NiceMock<MockVideoCaptureDeviceAVFoundationFrameReceiver>
            frame_receiver;
        base::scoped_nsobject<VideoCaptureDeviceAVFoundation> captureDevice =
            thiz->CreateCaptureDevice(&frame_receiver);

        NSString* errorMessage = nil;
        ASSERT_TRUE([captureDevice setCaptureDevice:deviceId
                                       errorMessage:&errorMessage]);
        ASSERT_TRUE([captureDevice startCapture]);

        base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
        EXPECT_CALL(frame_receiver, OnPhotoError())
            .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
        [captureDevice takePhoto];
        // There is no risk that takePhoto() has successfully finishes before
        // stopCapture() because the takePhoto() call involes a
        // PostDelayedTask() that cannot run until RunLoop::Run() below.
        [captureDevice stopCapture];
        run_loop.Run();
      },
      this));
}

TEST_P(VideoCaptureDeviceAVFoundationMacTakePhotoTest,
       MultiplePendingTakePhotos) {
  RunTestCase(base::BindOnce(
      [](VideoCaptureDeviceAVFoundationMacTakePhotoTest* thiz) {
        NSString* deviceId = GetFirstDeviceId();
        if (!deviceId) {
          DVLOG(1) << "No camera available. Exiting test.";
          return;
        }

        testing::NiceMock<MockVideoCaptureDeviceAVFoundationFrameReceiver>
            frame_receiver;
        base::scoped_nsobject<VideoCaptureDeviceAVFoundation> captureDevice =
            thiz->CreateCaptureDevice(&frame_receiver);

        NSString* errorMessage = nil;
        ASSERT_TRUE([captureDevice setCaptureDevice:deviceId
                                       errorMessage:&errorMessage]);
        ASSERT_TRUE([captureDevice startCapture]);

        base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
        EXPECT_CALL(frame_receiver, OnPhotoTaken(Ne(nullptr), Gt(0u), _))
            .WillOnce(Return())
            .WillOnce(Return())
            .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
        [captureDevice takePhoto];
        [captureDevice takePhoto];
        [captureDevice takePhoto];
        run_loop.Run();
      },
      this));
}

TEST_P(VideoCaptureDeviceAVFoundationMacTakePhotoTest,
       StopCaptureWhileMultiplePendingTakePhotos) {
  RunTestCase(base::BindOnce(
      [](VideoCaptureDeviceAVFoundationMacTakePhotoTest* thiz) {
        NSString* deviceId = GetFirstDeviceId();
        if (!deviceId) {
          DVLOG(1) << "No camera available. Exiting test.";
          return;
        }

        testing::NiceMock<MockVideoCaptureDeviceAVFoundationFrameReceiver>
            frame_receiver;
        base::scoped_nsobject<VideoCaptureDeviceAVFoundation> captureDevice =
            thiz->CreateCaptureDevice(&frame_receiver);

        NSString* errorMessage = nil;
        ASSERT_TRUE([captureDevice setCaptureDevice:deviceId
                                       errorMessage:&errorMessage]);
        ASSERT_TRUE([captureDevice startCapture]);

        base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
        EXPECT_CALL(frame_receiver, OnPhotoError)
            .WillOnce(Return())
            .WillOnce(Return())
            .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
        [captureDevice takePhoto];
        [captureDevice takePhoto];
        [captureDevice takePhoto];
        // There is no risk that takePhoto() has successfully finishes before
        // stopCapture() because the takePhoto() calls involes a
        // PostDelayedTask() that cannot run until RunLoop::Run() below.
        [captureDevice stopCapture];
        run_loop.Run();
      },
      this));
}

TEST_P(VideoCaptureDeviceAVFoundationMacTakePhotoTest,
       StopPhotoOutputWhenNoLongerTakingPhotos) {
  RunTestCase(base::BindOnce(
      [](VideoCaptureDeviceAVFoundationMacTakePhotoTest* thiz) {
        NSString* deviceId = GetFirstDeviceId();
        if (!deviceId) {
          DVLOG(1) << "No camera available. Exiting test.";
          return;
        }

        testing::NiceMock<MockVideoCaptureDeviceAVFoundationFrameReceiver>
            frame_receiver;
        base::scoped_nsobject<VideoCaptureDeviceAVFoundation> captureDevice =
            thiz->CreateCaptureDevice(&frame_receiver);

        NSString* errorMessage = nil;
        ASSERT_TRUE([captureDevice setCaptureDevice:deviceId
                                       errorMessage:&errorMessage]);
        ASSERT_TRUE([captureDevice startCapture]);

        base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
        [captureDevice
            setOnPhotoOutputStoppedForTesting:run_loop.QuitClosure()];
        base::TimeTicks start_time = base::TimeTicks::Now();
        [captureDevice takePhoto];
        // The RunLoop automatically advances mocked time when there are delayed
        // tasks pending. This allows the test to run fast and still assert how
        // much mocked time has elapsed.
        run_loop.Run();
        auto time_elapsed = base::TimeTicks::Now() - start_time;
        // Still image output is not stopped until 60 seconds of inactivity, so
        // the mocked time must have advanced at least this much.
        EXPECT_GE(time_elapsed.InSeconds(), 60);
      },
      this));
}

TEST_P(VideoCaptureDeviceAVFoundationMacTakePhotoTest,
       TakePhotoAndShutDownWithoutWaiting) {
  RunTestCase(base::BindOnce(
      [](VideoCaptureDeviceAVFoundationMacTakePhotoTest* thiz) {
        NSString* deviceId = GetFirstDeviceId();
        if (!deviceId) {
          DVLOG(1) << "No camera available. Exiting test.";
          return;
        }

        testing::NiceMock<MockVideoCaptureDeviceAVFoundationFrameReceiver>
            frame_receiver;
        base::scoped_nsobject<VideoCaptureDeviceAVFoundation> captureDevice =
            thiz->CreateCaptureDevice(&frame_receiver);

        NSString* errorMessage = nil;
        ASSERT_TRUE([captureDevice setCaptureDevice:deviceId
                                       errorMessage:&errorMessage]);
        ASSERT_TRUE([captureDevice startCapture]);

        [captureDevice takePhoto];
      },
      this));
}

// When not forcing legacy API, AVCapturePhotoOutput is used if available
// (macOS 10.15+). Otherwise AVCaptureStillImageOutput is used.
INSTANTIATE_TEST_SUITE_P(VideoCaptureDeviceAVFoundationMacTakePhotoTest,
                         VideoCaptureDeviceAVFoundationMacTakePhotoTest,
                         // Force legacy API?
                         testing::Values(false, true));

TEST(VideoCaptureDeviceAVFoundationMacTest, ForwardsOddPixelBufferResolution) {
  // See crbug/1168112.
  RunTestCase(base::BindOnce([] {
    testing::NiceMock<MockVideoCaptureDeviceAVFoundationFrameReceiver>
        frame_receiver;
    base::scoped_nsobject<VideoCaptureDeviceAVFoundation> captureDevice(
        [[VideoCaptureDeviceAVFoundation alloc]
            initWithFrameReceiver:&frame_receiver]);

    gfx::Size size(1280, 719);
    VideoCaptureFormat format(size, 30, PIXEL_FORMAT_YUY2);
    std::unique_ptr<ByteArrayPixelBuffer> buffer =
        CreateYuvsPixelBufferFromSingleRgbColor(size.width(), size.height(), 0,
                                                0, 0);
    [captureDevice
        callLocked:base::BindLambdaForTesting([&] {
          EXPECT_CALL(frame_receiver, ReceiveFrame(_, _, format, _, _, _, _));
          [captureDevice processPixelBufferPlanes:buffer->pixel_buffer
                                    captureFormat:format
                                       colorSpace:gfx::ColorSpace::CreateSRGB()
                                        timestamp:base::TimeDelta()];
        })];
  }));
}

TEST(VideoCaptureDeviceAVFoundationMacTest, FrameRateFloatInaccuracyIsHandled) {
  // See crbug/1299812.
  RunTestCase(base::BindOnce([] {
    double max_frame_rate = 30.000030;
    AVCaptureDeviceFormat* format1 =
        [[FakeAVCaptureDeviceFormat alloc] initWithWidth:100
                                                  height:100
                                                  fourCC:'420v'
                                               frameRate:max_frame_rate];
    AVCaptureDeviceFormat* format2 =
        [[FakeAVCaptureDeviceFormat alloc] initWithWidth:100
                                                  height:100
                                                  fourCC:'420v'
                                               frameRate:10];

    NSArray<AVCaptureDeviceFormat*>* formats = @[ format1, format2 ];
    // Cast the actual max_frame_rate to a float, to match what would be
    // requested once the true max has been cast when crossing our mojo etc
    // interfaces which use float rather than double.
    float desired_frame_rate = (float)max_frame_rate;
    // For these values, the float version will be higher than the double max,
    // due to loss of precision.
    ASSERT_LT(max_frame_rate, desired_frame_rate);

    AVCaptureDeviceFormat* chosen_format =
        FindBestCaptureFormat(formats, 100, 100, desired_frame_rate);

    ASSERT_EQ(1UL, [[chosen_format videoSupportedFrameRateRanges] count]);
    // The actual max_frame_rate should be chosen, even though the desired rate
    // was very slightly larger.
    EXPECT_EQ(max_frame_rate, [[[chosen_format videoSupportedFrameRateRanges]
                                  firstObject] minFrameRate]);
  }));
}

}  // namespace media
