// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/apple/video_capture_device_avfoundation.h"
#include "media/capture/video/apple/test/fake_av_capture_device_format.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "media/base/video_types.h"
#include "media/capture/video/apple/sample_buffer_transformer.h"
#include "media/capture/video/apple/test/mock_video_capture_device_avfoundation_frame_receiver.h"
#include "media/capture/video/apple/test/pixel_buffer_test_utils.h"
#include "media/capture/video/apple/test/video_capture_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"

using testing::_;
using testing::Gt;
using testing::Ne;
using testing::Return;
using testing::WithArg;

namespace media {

// TODO: https://crbug.com/40253946 - Fix and re-enable these tests.
TEST(VideoCaptureDeviceAVFoundationMacTest,
     DISABLED_OutputsNv12WithoutScalingByDefault) {
  RunTestCase(base::BindOnce([] {
    NSString* deviceId = GetFirstDeviceId();
    if (!deviceId) {
      LOG(ERROR) << "No camera available. Exiting test.";
      return;
    }

    testing::NiceMock<MockVideoCaptureDeviceAVFoundationFrameReceiver>
        frame_receiver;
    VideoCaptureDeviceAVFoundation* captureDevice =
        [[VideoCaptureDeviceAVFoundation alloc]
            initWithFrameReceiver:&frame_receiver];

    NSString* errorMessage = nil;
    ASSERT_TRUE([captureDevice setCaptureDevice:deviceId
                                   errorMessage:&errorMessage]);
    ASSERT_TRUE([captureDevice startCapture]);

    bool has_received_first_frame = false;
    base::RunLoop first_frame_received(
        base::RunLoop::Type::kNestableTasksAllowed);
    EXPECT_CALL(frame_receiver, ReceiveExternalGpuMemoryBufferFrame)
        .WillRepeatedly(
            testing::Invoke(WithArg<0>([&](CapturedExternalVideoBuffer frame) {
              if (has_received_first_frame) {
                // Ignore subsequent frames.
                return;
              }
              EXPECT_EQ(frame.format.pixel_format, PIXEL_FORMAT_NV12);
              has_received_first_frame = true;
              first_frame_received.Quit();
            })));
    first_frame_received.Run();

    [captureDevice stopCapture];
  }));
}

// TODO: https://crbug.com/40253946 - Fix and re-enable these tests.
TEST(VideoCaptureDeviceAVFoundationMacTest, DISABLED_TakePhoto) {
  RunTestCase(
      base::BindOnce([] {
        NSString* deviceId = GetFirstDeviceId();
        if (!deviceId) {
          DVLOG(1) << "No camera available. Exiting test.";
          return;
        }

        testing::NiceMock<MockVideoCaptureDeviceAVFoundationFrameReceiver>
            frame_receiver;
        VideoCaptureDeviceAVFoundation* captureDevice =
            [[VideoCaptureDeviceAVFoundation alloc]
                initWithFrameReceiver:&frame_receiver];

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
      }));
}

// TODO: https://crbug.com/40253946 - Fix and re-enable these tests.
TEST(VideoCaptureDeviceAVFoundationMacTest,
     DISABLED_StopCaptureWhileTakingPhoto) {
  RunTestCase(
      base::BindOnce([] {
        NSString* deviceId = GetFirstDeviceId();
        if (!deviceId) {
          DVLOG(1) << "No camera available. Exiting test.";
          return;
        }

        testing::NiceMock<MockVideoCaptureDeviceAVFoundationFrameReceiver>
            frame_receiver;
        VideoCaptureDeviceAVFoundation* captureDevice =
            [[VideoCaptureDeviceAVFoundation alloc]
                initWithFrameReceiver:&frame_receiver];

        NSString* errorMessage = nil;
        ASSERT_TRUE([captureDevice setCaptureDevice:deviceId
                                       errorMessage:&errorMessage]);
        ASSERT_TRUE([captureDevice startCapture]);

        base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
        EXPECT_CALL(frame_receiver, OnPhotoError())
            .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
        [captureDevice takePhoto];
        // There is no risk that takePhoto() has successfully finishes before
        // stopCapture() because the takePhoto() call involves a
        // PostDelayedTask() that cannot run until RunLoop::Run() below.
        [captureDevice stopCapture];
        run_loop.Run();
      }));
}

// TODO: https://crbug.com/40253946 - Fix and re-enable these tests.
TEST(VideoCaptureDeviceAVFoundationMacTest,
     DISABLED_MultiplePendingTakePhotos) {
  RunTestCase(
      base::BindOnce([] {
        NSString* deviceId = GetFirstDeviceId();
        if (!deviceId) {
          DVLOG(1) << "No camera available. Exiting test.";
          return;
        }

        testing::NiceMock<MockVideoCaptureDeviceAVFoundationFrameReceiver>
            frame_receiver;
        VideoCaptureDeviceAVFoundation* captureDevice =
            [[VideoCaptureDeviceAVFoundation alloc]
                initWithFrameReceiver:&frame_receiver];

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
      }));
}

// TODO: https://crbug.com/40253946 - Fix and re-enable these tests.
TEST(VideoCaptureDeviceAVFoundationMacTest,
     DISABLED_StopCaptureWhileMultiplePendingTakePhotos) {
  RunTestCase(
      base::BindOnce([] {
        NSString* deviceId = GetFirstDeviceId();
        if (!deviceId) {
          DVLOG(1) << "No camera available. Exiting test.";
          return;
        }

        testing::NiceMock<MockVideoCaptureDeviceAVFoundationFrameReceiver>
            frame_receiver;
        VideoCaptureDeviceAVFoundation* captureDevice =
            [[VideoCaptureDeviceAVFoundation alloc]
                initWithFrameReceiver:&frame_receiver];

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
        // stopCapture() because the takePhoto() calls involves a
        // PostDelayedTask() that cannot run until RunLoop::Run() below.
        [captureDevice stopCapture];
        run_loop.Run();
      }));
}

// TODO: https://crbug.com/40253946 - Fix and re-enable these tests.
TEST(VideoCaptureDeviceAVFoundationMacTest,
     DISABLED_StopPhotoOutputWhenNoLongerTakingPhotos) {
  RunTestCase(base::BindOnce([] {
    NSString* deviceId = GetFirstDeviceId();
    if (!deviceId) {
      DVLOG(1) << "No camera available. Exiting test.";
      return;
    }

    testing::NiceMock<MockVideoCaptureDeviceAVFoundationFrameReceiver>
        frame_receiver;
    VideoCaptureDeviceAVFoundation* captureDevice =
        [[VideoCaptureDeviceAVFoundation alloc]
            initWithFrameReceiver:&frame_receiver];

    NSString* errorMessage = nil;
    ASSERT_TRUE([captureDevice setCaptureDevice:deviceId
                                   errorMessage:&errorMessage]);
    ASSERT_TRUE([captureDevice startCapture]);

    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    [captureDevice setOnPhotoOutputStoppedForTesting:run_loop.QuitClosure()];
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
  }));
}

// TODO: https://crbug.com/40253946 - Fix and re-enable these tests.
TEST(VideoCaptureDeviceAVFoundationMacTest,
     DISABLED_TakePhotoAndShutDownWithoutWaiting) {
  RunTestCase(base::BindOnce([] {
    NSString* deviceId = GetFirstDeviceId();
    if (!deviceId) {
      DVLOG(1) << "No camera available. Exiting test.";
      return;
    }

    testing::NiceMock<MockVideoCaptureDeviceAVFoundationFrameReceiver>
        frame_receiver;
    VideoCaptureDeviceAVFoundation* captureDevice =
        [[VideoCaptureDeviceAVFoundation alloc]
            initWithFrameReceiver:&frame_receiver];

    NSString* errorMessage = nil;
    ASSERT_TRUE([captureDevice setCaptureDevice:deviceId
                                   errorMessage:&errorMessage]);
    ASSERT_TRUE([captureDevice startCapture]);

    [captureDevice takePhoto];
  }));
}

TEST(VideoCaptureDeviceAVFoundationMacTest, ForwardsOddPixelBufferResolution) {
  // See crbug/1168112.
  RunTestCase(base::BindOnce([] {
    testing::NiceMock<MockVideoCaptureDeviceAVFoundationFrameReceiver>
        frame_receiver;
    VideoCaptureDeviceAVFoundation* captureDevice =
        [[VideoCaptureDeviceAVFoundation alloc]
            initWithFrameReceiver:&frame_receiver];

    gfx::Size size(1280, 719);
    VideoCaptureFormat format(size, 30, PIXEL_FORMAT_YUY2);
    std::unique_ptr<ByteArrayPixelBuffer> buffer =
        CreateYuvsPixelBufferFromSingleRgbColor(size.width(), size.height(), 0,
                                                0, 0);
    [captureDevice callLocked:base::BindLambdaForTesting([&] {
                     EXPECT_CALL(frame_receiver,
                                 ReceiveFrame(_, _, format, _, _, _, _, _, _));
                     [captureDevice
                         processPixelBufferPlanes:buffer->pixel_buffer.get()
                                    captureFormat:format
                                       colorSpace:gfx::ColorSpace::CreateSRGB()
                                        timestamp:base::TimeDelta()
                               capture_begin_time:std::nullopt];
                   })];
  }));
}

TEST(VideoCaptureDeviceAVFoundationMacTest, FrameRateFloatInaccuracyIsHandled) {
  // See https://crbug.com/1299812.
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

    ASSERT_EQ(1UL, chosen_format.videoSupportedFrameRateRanges.count);
    // The actual max_frame_rate should be chosen, even though the desired rate
    // was very slightly larger.
    EXPECT_EQ(
        max_frame_rate,
        chosen_format.videoSupportedFrameRateRanges.firstObject.minFrameRate);
  }));
}

}  // namespace media
