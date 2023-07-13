// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/linux/video_capture_device_linux.h"

#include <stddef.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "media/capture/video/linux/v4l2_capture_delegate.h"

#if BUILDFLAG(IS_OPENBSD)
#include <sys/videoio.h>
#else
#include <linux/videodev2.h>
#endif

namespace media {

namespace {

int TranslatePowerLineFrequencyToV4L2(PowerLineFrequency frequency) {
  switch (frequency) {
    case PowerLineFrequency::k50Hz:
      return V4L2_CID_POWER_LINE_FREQUENCY_50HZ;
    case PowerLineFrequency::k60Hz:
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
      task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::TaskPriority::USER_BLOCKING, base::MayBlock(),
           base::WithBaseSyncPrimitives()},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED)),
      rotation_(0) {}

VideoCaptureDeviceLinux::~VideoCaptureDeviceLinux() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!capture_impl_)
      << "StopAndDeAllocate() must be called before destruction.";
}

void VideoCaptureDeviceLinux::AllocateAndStart(
    const VideoCaptureParams& params,
    std::unique_ptr<VideoCaptureDevice::Client> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!capture_impl_);

  const int line_frequency =
      TranslatePowerLineFrequencyToV4L2(GetPowerLineFrequency(params));
  capture_impl_ = std::make_unique<V4L2CaptureDelegate>(
      v4l2_.get(), device_descriptor_, task_runner_, line_frequency, rotation_);
  if (!capture_impl_) {
    client->OnError(VideoCaptureError::
                        kDeviceCaptureLinuxFailedToCreateVideoCaptureDelegate,
                    FROM_HERE, "Failed to create VideoCaptureDelegate");
    return;
  }

  if (gmb_support_test_) {
    capture_impl_->SetGPUEnvironmentForTesting(  // IN-TEST
        std::move(gmb_support_test_));           // IN-TEST
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&V4L2CaptureDelegate::AllocateAndStart,
                     capture_impl_->GetWeakPtr(),
                     params.requested_format.frame_size.width(),
                     params.requested_format.frame_size.height(),
                     params.requested_format.frame_rate, std::move(client)));
}

void VideoCaptureDeviceLinux::StopAndDeAllocate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!capture_impl_)
    return;  // Wrong state.

  // Shutdown must be synchronous, otherwise the next created capture device
  // may conflict.
  base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
  if (task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&VideoCaptureDeviceLinux::StopAndDeAllocateInternal,
                         base::Unretained(this), base::Unretained(&waiter)))) {
    waiter.Wait();
  }
}

void VideoCaptureDeviceLinux::TakePhoto(TakePhotoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(capture_impl_);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&V4L2CaptureDelegate::TakePhoto,
                     capture_impl_->GetWeakPtr(), std::move(callback)));
}

void VideoCaptureDeviceLinux::GetPhotoState(GetPhotoStateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(capture_impl_);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&V4L2CaptureDelegate::GetPhotoState,
                     capture_impl_->GetWeakPtr(), std::move(callback)));
}

void VideoCaptureDeviceLinux::SetPhotoOptions(
    mojom::PhotoSettingsPtr settings,
    SetPhotoOptionsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(capture_impl_);
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&V4L2CaptureDelegate::SetPhotoOptions,
                                capture_impl_->GetWeakPtr(),
                                std::move(settings), std::move(callback)));
}

void VideoCaptureDeviceLinux::SetRotation(int rotation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(capture_impl_);
  rotation_ = rotation;
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&V4L2CaptureDelegate::SetRotation,
                                        capture_impl_->GetWeakPtr(), rotation));
}

void VideoCaptureDeviceLinux::StopAndDeAllocateInternal(
    base::WaitableEvent* waiter) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(capture_impl_);
  capture_impl_->StopAndDeAllocate();
  capture_impl_.reset();
  waiter->Signal();
}

void VideoCaptureDeviceLinux::SetGPUEnvironmentForTesting(
    std::unique_ptr<gpu::GpuMemoryBufferSupport> gmb_support) {
  gmb_support_test_ = std::move(gmb_support);
}

}  // namespace media
