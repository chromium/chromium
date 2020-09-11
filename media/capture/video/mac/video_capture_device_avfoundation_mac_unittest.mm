// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/mac/video_capture_device_avfoundation_mac.h"

#include <memory>

#include "base/bind.h"
#include "base/mac/scoped_nsobject.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#import "media/capture/video/mac/test/mock_video_capture_device_avfoundation_frame_receiver_mac.h"
#import "media/capture/video/mac/test/video_capture_test_utils_mac.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace media {

TEST(VideoCaptureDeviceAVFoundationMacTest, TakePhoto) {
  RunTestCase(base::BindOnce([] {
    NSString* deviceId = GetFirstDeviceId();
    if (!deviceId) {
      DVLOG(1) << "No camera available. Exiting test.";
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

    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    EXPECT_CALL(frame_receiver, OnPhotoTaken)
        .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
    [captureDevice takePhoto];
    run_loop.Run();
  }));
}

TEST(VideoCaptureDeviceAVFoundationMacTest, StopCaptureWhileTakingPhoto) {
  RunTestCase(base::BindOnce([] {
    NSString* deviceId = GetFirstDeviceId();
    if (!deviceId) {
      DVLOG(1) << "No camera available. Exiting test.";
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

    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    EXPECT_CALL(frame_receiver, OnPhotoError())
        .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
    [captureDevice takePhoto];
    // There is no risk that takePhoto() has successfully finishes before
    // stopCapture() because the takePhoto() call involes a PostDelayedTask()
    // that cannot run until RunLoop::Run() below.
    [captureDevice stopCapture];
    run_loop.Run();
  }));
}

TEST(VideoCaptureDeviceAVFoundationMacTest, MultiplePendingTakePhotos) {
  RunTestCase(base::BindOnce([] {
    NSString* deviceId = GetFirstDeviceId();
    if (!deviceId) {
      DVLOG(1) << "No camera available. Exiting test.";
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

    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    size_t photos_taken_count = 0;
    EXPECT_CALL(frame_receiver, OnPhotoTaken)
        .WillRepeatedly([&photos_taken_count, &run_loop] {
          ++photos_taken_count;
          if (photos_taken_count == 3) {
            run_loop.Quit();
          }
        });
    [captureDevice takePhoto];
    [captureDevice takePhoto];
    [captureDevice takePhoto];
    run_loop.Run();
  }));
}

TEST(VideoCaptureDeviceAVFoundationMacTest,
     StopCaptureWhileMultiplePendingTakePhotos) {
  RunTestCase(base::BindOnce([] {
    NSString* deviceId = GetFirstDeviceId();
    if (!deviceId) {
      DVLOG(1) << "No camera available. Exiting test.";
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

    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    size_t photo_error_count = 0;
    EXPECT_CALL(frame_receiver, OnPhotoError)
        .WillRepeatedly([&photo_error_count, &run_loop] {
          ++photo_error_count;
          if (photo_error_count == 3) {
            run_loop.Quit();
          }
        });
    [captureDevice takePhoto];
    [captureDevice takePhoto];
    [captureDevice takePhoto];
    // There is no risk that takePhoto() has successfully finishes before
    // stopCapture() because the takePhoto() calls involes a PostDelayedTask()
    // that cannot run until RunLoop::Run() below.
    [captureDevice stopCapture];
    run_loop.Run();
  }));
}

TEST(VideoCaptureDeviceAVFoundationMacTest,
     StopStillImageOutputWhenNoLongerTakingPhotos) {
  RunTestCase(base::BindOnce([] {
    NSString* deviceId = GetFirstDeviceId();
    if (!deviceId) {
      DVLOG(1) << "No camera available. Exiting test.";
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

    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    [captureDevice
        setOnStillImageOutputStoppedForTesting:run_loop.QuitClosure()];
    base::TimeTicks start_time = base::TimeTicks::Now();
    [captureDevice takePhoto];
    // The RunLoop automatically advances mocked time when there are delayed
    // tasks pending. This allows the test to run fast and still assert how much
    // mocked time has elapsed.
    run_loop.Run();
    auto time_elapsed = base::TimeTicks::Now() - start_time;
    // Still image output is not stopped until 60 seconds of inactivity, so the
    // mocked time must have advanced at least this much.
    EXPECT_GE(time_elapsed.InSeconds(), 60);
  }));
}

// This test ensures we don't crash even if we leave operations pending.
TEST(VideoCaptureDeviceAVFoundationMacTest,
     TakePhotoAndShutDownWithoutWaiting) {
  RunTestCase(base::BindOnce([] {
    NSString* deviceId = GetFirstDeviceId();
    if (!deviceId) {
      DVLOG(1) << "No camera available. Exiting test.";
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

    [captureDevice takePhoto];
  }));
}

}  // namespace media
