// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/screen_capture_device_android.h"

#include <utility>

#include "base/check_op.h"
#include "media/capture/content/android/thread_safe_capture_oracle.h"

namespace content {

ScreenCaptureDeviceAndroid::ScreenCaptureDeviceAndroid() = default;

ScreenCaptureDeviceAndroid::~ScreenCaptureDeviceAndroid() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!oracle_proxy_);
}

void ScreenCaptureDeviceAndroid::AllocateAndStart(
    const media::VideoCaptureParams& params,
    std::unique_ptr<Client> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (params.requested_format.pixel_format != media::PIXEL_FORMAT_I420) {
    client->OnError(
        media::VideoCaptureError::kAndroidScreenCaptureUnsupportedFormat,
        FROM_HERE,
        "unsupported format: " +
            media::VideoCaptureFormat::ToString(params.requested_format));
    return;
  }

  DCHECK(!oracle_proxy_);
  oracle_proxy_ = new media::ThreadSafeCaptureOracle(std::move(client), params);

  if (!capture_machine_.Start(oracle_proxy_, params)) {
    oracle_proxy_->ReportError(
        media::VideoCaptureError::
            kAndroidScreenCaptureFailedToStartCaptureMachine,
        FROM_HERE, "Failed to start capture machine.");
    StopAndDeAllocate();
  } else {
    // The |capture_machine_| will later report to the |oracle_proxy_| whether
    // the device started successfully.
  }
}

void ScreenCaptureDeviceAndroid::StopAndDeAllocate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!oracle_proxy_) {
    return;  // Device is already stopped.
  }

  oracle_proxy_->Stop();
  oracle_proxy_ = nullptr;
  capture_machine_.Stop();
}

void ScreenCaptureDeviceAndroid::RequestRefreshFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!oracle_proxy_) {
    return;  // Device is stopped.
  }
  capture_machine_.MaybeCaptureForRefresh();
}

void ScreenCaptureDeviceAndroid::OnUtilizationReport(
    media::VideoCaptureFeedback feedback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(oracle_proxy_);
  DCHECK(feedback.frame_id.has_value());

  oracle_proxy_->OnConsumerReportingUtilization(feedback.frame_id.value(),
                                                feedback);
}

}  // namespace content
