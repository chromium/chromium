// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MOCK_VIDEO_CAPTURER_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MOCK_VIDEO_CAPTURER_SOURCE_H_

#include "third_party/blink/renderer/platform/video_capture/video_capturer_source.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

class MockVideoCapturerSource : public VideoCapturerSource {
 public:
  MockVideoCapturerSource() = default;

  MOCK_METHOD0(RequestRefreshFrame, void());
  MOCK_METHOD0(GetPreferredFormats, media::VideoCaptureFormats());
  MOCK_METHOD3(
      MockStartCapture,
      VideoCaptureRunState(const media::VideoCaptureParams& params,
                           VideoCaptureDeliverFrameCB new_frame_callback,
                           VideoCaptureRunningCallbackCB running_callback));
  MOCK_METHOD0(MockStopCapture, void());
  void StartCapture(const media::VideoCaptureParams& params,
                    VideoCaptureCallbacks video_capture_callbacks,
                    VideoCaptureRunningCallbackCB running_callback) override {
    running_cb_ = std::move(running_callback);
    capture_params_ = params;

    VideoCaptureRunState run_state = MockStartCapture(
        params, std::move(video_capture_callbacks.deliver_frame_cb),
        running_cb_);
    SetRunning(run_state);
  }
  void StopCapture() override { MockStopCapture(); }
  void SetRunning(VideoCaptureRunState run_state) {
    PostCrossThreadTask(*scheduler::GetSingleThreadTaskRunnerForTesting(),
                        FROM_HERE, CrossThreadBindOnce(running_cb_, run_state));
  }
  const media::VideoCaptureParams& capture_params() const {
    return capture_params_;
  }

 private:
  VideoCaptureRunningCallbackCB running_cb_;
  media::VideoCaptureParams capture_params_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MOCK_VIDEO_CAPTURER_SOURCE_H_
