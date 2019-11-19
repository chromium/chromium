// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/linux/video_capture_device_linux.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "media/capture/video/linux/v4l2_capture_delegate.h"

#if defined(OS_OPENBSD)
#include <sys/videoio.h>
#else
#include <linux/videodev2.h>
#endif

namespace media {

namespace {

int TranslatePowerLineFrequencyToV4L2(PowerLineFrequency frequency) {
  switch (frequency) {
    case PowerLineFrequency::FREQUENCY_50HZ:
      return V4L2_CID_POWER_LINE_FREQUENCY_50HZ;
    case PowerLineFrequency::FREQUENCY_60HZ:
      return V4L2_CID_POWER_LINE_FREQUENCY_60HZ;
    default:
      // If we have no idea of the frequency, at least try and set it to AUTO.
      return V4L2_CID_POWER_LINE_FREQUENCY_AUTO;
  }
}

}  // namespace

// Translates Video4Linux pixel formats to Chromium pixel formats.
// static
VideoPixelFormat VideoCaptureDeviceLinux::V4l2FourCcToChromiumPixelFormat(
    uint32_t v4l2_fourcc) {
  return V4L2CaptureDelegate::V4l2FourCcToChromiumPixelFormat(v4l2_fourcc);
}

// Gets a list of usable Four CC formats prioritized.
// static
std::vector<uint32_t> VideoCaptureDeviceLinux::GetListOfUsableFourCCs(
    bool favour_mjpeg) {
  return V4L2CaptureDelegate::GetListOfUsableFourCcs(favour_mjpeg);
}

VideoCaptureDeviceLinux::VideoCaptureDeviceLinux(
    scoped_refptr<V4L2CaptureDevice> v4l2,
    const VideoCaptureDeviceDescriptor& device_descriptor)
    : device_descriptor_(device_descriptor),
      v4l2_(std::move(v4l2)),
      v4l2_thread_("V4L2CaptureThread"),
      rotation_(0) {}

VideoCaptureDeviceLinux::~VideoCaptureDeviceLinux() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Check if the thread is running.
  // This means that the device has not been StopAndDeAllocate()d properly.
  DCHECK(!v4l2_thread_.IsRunning());
  v4l2_thread_.Stop();
}

void VideoCaptureDeviceLinux::AllocateAndStart(
    const VideoCaptureParams& params,
    std::unique_ptr<VideoCaptureDevice::Client> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!capture_impl_);
  if (v4l2_thread_.IsRunning())
    return;  // Wrong state.
  v4l2_thread_.Start();

  const int line_frequency =
      TranslatePowerLineFrequencyToV4L2(GetPowerLineFrequency(params));
  capture_impl_ = std::make_unique<V4L2CaptureDelegate>(
      v4l2_.get(), device_descriptor_, v4l2_thread_.task_runner(),
      line_frequency, rotation_);
  if (!capture_impl_) {
    client->OnError(VideoCaptureError::
                        kDeviceCaptureLinuxFailedToCreateVideoCaptureDelegate,
                    FROM_HERE, "Failed to create VideoCaptureDelegate");
    return;
  }
  v4l2_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&V4L2CaptureDelegate::AllocateAndStart,
                     capture_impl_->GetWeakPtr(),
                     params.requested_format.frame_size.width(),
                     params.requested_format.frame_size.height(),
                     params.requested_format.frame_rate, std::move(client)));

  for (auto& request : photo_requests_queue_)
    v4l2_thread_.task_runner()->PostTask(FROM_HERE, std::move(request));
  photo_requests_queue_.clear();
}

void VideoCaptureDeviceLinux::StopAndDeAllocate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!v4l2_thread_.IsRunning())
    return;  // Wrong state.
  v4l2_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V4L2CaptureDelegate::StopAndDeAllocate,
                                capture_impl_->GetWeakPtr()));
  v4l2_thread_.task_runner()->DeleteSoon(FROM_HERE, capture_impl_.release());
  v4l2_thread_.Stop();

  capture_impl_ = nullptr;
}

void VideoCaptureDeviceLinux::TakePhoto(TakePhotoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(capture_impl_);
  auto functor =
      base::BindOnce(&V4L2CaptureDelegate::TakePhoto,
                     capture_impl_->GetWeakPtr(), std::move(callback));
  if (!v4l2_thread_.IsRunning()) {
    // We have to wait until we get the device AllocateAndStart()ed.
    photo_requests_queue_.push_back(std::move(functor));
    return;
  }
  v4l2_thread_.task_runner()->PostTask(FROM_HERE, std::move(functor));
}

void VideoCaptureDeviceLinux::GetPhotoState(GetPhotoStateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto functor =
      base::BindOnce(&V4L2CaptureDelegate::GetPhotoState,
                     capture_impl_->GetWeakPtr(), std::move(callback));
  if (!v4l2_thread_.IsRunning()) {
    // We have to wait until we get the device AllocateAndStart()ed.
    photo_requests_queue_.push_back(std::move(functor));
    return;
  }
  v4l2_thread_.task_runner()->PostTask(FROM_HERE, std::move(functor));
}

void VideoCaptureDeviceLinux::SetPhotoOptions(
    mojom::PhotoSettingsPtr settings,
    SetPhotoOptionsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto functor = base::BindOnce(&V4L2CaptureDelegate::SetPhotoOptions,
                                capture_impl_->GetWeakPtr(),
                                std::move(settings), std::move(callback));
  if (!v4l2_thread_.IsRunning()) {
    // We have to wait until we get the device AllocateAndStart()ed.
    photo_requests_queue_.push_back(std::move(functor));
    return;
  }
  v4l2_thread_.task_runner()->PostTask(FROM_HERE, std::move(functor));
}

void VideoCaptureDeviceLinux::SetRotation(int rotation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rotation_ = rotation;
  if (v4l2_thread_.IsRunning()) {
    v4l2_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&V4L2CaptureDelegate::SetRotation,
                                  capture_impl_->GetWeakPtr(), rotation));
  }
}

}  // namespace media
