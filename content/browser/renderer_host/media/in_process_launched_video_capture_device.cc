// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/in_process_launched_video_capture_device.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/token.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/buildflags.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(ENABLE_SCREEN_CAPTURE) && !BUILDFLAG(IS_ANDROID)
#include "content/browser/media/capture/desktop_capture_device.h"
#endif

namespace {

void SetDesktopCaptureWindowIdOnDeviceThread(
    gfx::NativeViewId window_id,
    base::OnceClosure done_cb,
    media::VideoCaptureDevice* device) {
#if BUILDFLAG(ENABLE_SCREEN_CAPTURE) && !BUILDFLAG(IS_ANDROID)
  auto* desktop_device = static_cast<content::DesktopCaptureDevice*>(device);
  desktop_device->SetNotificationWindowId(window_id);
  VLOG(2) << "Screen capture notification window passed on device thread.";
#endif
  std::move(done_cb).Run();
}

}  // anonymous namespace

namespace content {

InProcessLaunchedVideoCaptureDevice::InProcessLaunchedVideoCaptureDevice(
    std::unique_ptr<media::VideoCaptureDevice> device,
    scoped_refptr<base::SingleThreadTaskRunner> device_task_runner)
    : device_(std::move(device_task_runner), std::move(device)) {}

InProcessLaunchedVideoCaptureDevice::~InProcessLaunchedVideoCaptureDevice() {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M154);
  CHECK(!device_.is_null(), base::NotFatalUntil::M154);
  device_.AsyncCall(&media::VideoCaptureDevice::StopAndDeAllocate);
}

void InProcessLaunchedVideoCaptureDevice::GetPhotoState(
    media::VideoCaptureDevice::GetPhotoStateCallback callback) {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M154);
  device_.AsyncCall(&media::VideoCaptureDevice::GetPhotoState)
      .WithArgs(std::move(callback));
}

void InProcessLaunchedVideoCaptureDevice::SetPhotoOptions(
    media::mojom::PhotoSettingsPtr settings,
    media::VideoCaptureDevice::SetPhotoOptionsCallback callback) {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M154);
  device_.AsyncCall(&media::VideoCaptureDevice::SetPhotoOptions)
      .WithArgs(std::move(settings), std::move(callback));
}

void InProcessLaunchedVideoCaptureDevice::TakePhoto(
    media::VideoCaptureDevice::TakePhotoCallback callback) {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M154);
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "InProcessLaunchedVideoCaptureDevice::TakePhoto",
                       TRACE_EVENT_SCOPE_PROCESS);
  device_.AsyncCall(&media::VideoCaptureDevice::TakePhoto)
      .WithArgs(std::move(callback));
}

void InProcessLaunchedVideoCaptureDevice::MaybeSuspendDevice() {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M154);
  device_.AsyncCall(&media::VideoCaptureDevice::MaybeSuspend);
}

void InProcessLaunchedVideoCaptureDevice::ResumeDevice() {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M154);
  device_.AsyncCall(&media::VideoCaptureDevice::Resume);
}

void InProcessLaunchedVideoCaptureDevice::ApplySubCaptureTarget(
    media::mojom::SubCaptureTargetType type,
    const base::Token& target,
    uint32_t sub_capture_target_version,
    base::OnceCallback<void(media::mojom::ApplySubCaptureTargetResult)>
        callback) {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M154);
  // Explicitly bind the callback to the I/O thread since the VideoCaptureDevice
  // ApplySubCaptureTarget method runs the callback on an unspecified thread.
  device_.AsyncCall(&media::VideoCaptureDevice::ApplySubCaptureTarget)
      .WithArgs(type, target, sub_capture_target_version,
                base::BindPostTask(content::GetIOThreadTaskRunner({}),
                                   std::move(callback)));
}

void InProcessLaunchedVideoCaptureDevice::RequestRefreshFrame() {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M154);
  device_.AsyncCall(&media::VideoCaptureDevice::RequestRefreshFrame);
}

void InProcessLaunchedVideoCaptureDevice::InvalidateBuffers() {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M154);
  device_.AsyncCall(&media::VideoCaptureDevice::InvalidateBuffers);
}

void InProcessLaunchedVideoCaptureDevice::SetDesktopCaptureWindowIdAsync(
    gfx::NativeViewId window_id,
    base::OnceClosure done_cb) {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M154);
  device_.PostTaskWithThisObject(base::BindOnce(
      &SetDesktopCaptureWindowIdOnDeviceThread, window_id, std::move(done_cb)));
}

void InProcessLaunchedVideoCaptureDevice::OnUtilizationReport(
    media::VideoCaptureFeedback feedback) {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M154);
  device_.AsyncCall(&media::VideoCaptureDevice::OnUtilizationReport)
      .WithArgs(feedback);
}

}  // namespace content
