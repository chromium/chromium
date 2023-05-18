// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_frame_handle.h"

#include "base/synchronization/lock.h"
#include "media/base/video_frame.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame_monitor.h"
#include "third_party/blink/renderer/modules/webcodecs/webcodecs_logger.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {

VideoFrameHandle::VideoFrameHandle(scoped_refptr<media::VideoFrame> frame,
                                   ExecutionContext* context,
                                   std::string monitoring_source_id)
    : frame_(std::move(frame)),
      monitoring_source_id_(std::move(monitoring_source_id)),
      timestamp_(frame_->timestamp()),
      duration_(frame_->metadata().frame_duration) {
  DCHECK(frame_);
  DCHECK(context);

  close_auditor_ = WebCodecsLogger::From(*context).GetCloseAuditor();
  DCHECK(close_auditor_);

  MaybeMonitorOpenFrame();
}

VideoFrameHandle::VideoFrameHandle(scoped_refptr<media::VideoFrame> frame,
                                   sk_sp<SkImage> sk_image,
                                   ExecutionContext* context,
                                   std::string monitoring_source_id)
    : VideoFrameHandle(std::move(frame),
                       context,
                       std::move(monitoring_source_id)) {
  sk_image_ = std::move(sk_image);
}

VideoFrameHandle::VideoFrameHandle(
    scoped_refptr<media::VideoFrame> frame,
    sk_sp<SkImage> sk_image,
    scoped_refptr<WebCodecsLogger::VideoFrameCloseAuditor> close_auditor,
    std::string monitoring_source_id)
    : sk_image_(std::move(sk_image)),
      frame_(std::move(frame)),
      close_auditor_(std::move(close_auditor)),
      monitoring_source_id_(std::move(monitoring_source_id)),
      timestamp_(frame_->timestamp()),
      duration_(frame_->metadata().frame_duration) {
  DCHECK(frame_);
  MaybeMonitorOpenFrame();
}

VideoFrameHandle::VideoFrameHandle(scoped_refptr<media::VideoFrame> frame,
                                   sk_sp<SkImage> sk_image,
                                   std::string monitoring_source_id)
    : sk_image_(std::move(sk_image)),
      frame_(std::move(frame)),
      monitoring_source_id_(std::move(monitoring_source_id)),
      timestamp_(frame_->timestamp()),
      duration_(frame_->metadata().frame_duration) {
  DCHECK(frame_);
  MaybeMonitorOpenFrame();
}

VideoFrameHandle::~VideoFrameHandle() {
  MaybeMonitorCloseFrame();
  // If we still have a valid |close_auditor_|, Invalidate() was never
  // called and corresponding frames never received a call to close() before
  // being garbage collected.
  if (close_auditor_)
    close_auditor_->ReportUnclosedFrame();
}

scoped_refptr<media::VideoFrame> VideoFrameHandle::frame() {
  base::AutoLock locker(lock_);
  return frame_;
}

sk_sp<SkImage> VideoFrameHandle::sk_image() {
  base::AutoLock locker(lock_);
  return sk_image_;
}

void VideoFrameHandle::Invalidate() {
  base::AutoLock locker(lock_);
  InvalidateLocked();
}

void VideoFrameHandle::SetCloseOnClone() {
  base::AutoLock locker(lock_);
  close_on_clone_ = true;
}

scoped_refptr<VideoFrameHandle> VideoFrameHandle::Clone() {
  base::AutoLock locker(lock_);
  auto cloned_handle =
      frame_ ? base::MakeRefCounted<VideoFrameHandle>(
                   frame_, sk_image_, close_auditor_, monitoring_source_id_)
             : nullptr;

  if (close_on_clone_)
    InvalidateLocked();

  return cloned_handle;
}

scoped_refptr<VideoFrameHandle> VideoFrameHandle::CloneForInternalUse() {
  base::AutoLock locker(lock_);
  return frame_ ? base::MakeRefCounted<VideoFrameHandle>(frame_, sk_image_,
                                                         monitoring_source_id_)
                : nullptr;
}

void VideoFrameHandle::InvalidateLocked() {
  MaybeMonitorCloseFrame();
  frame_.reset();
  sk_image_.reset();
  close_auditor_.reset();
  NotifyExpiredLocked();
}

void VideoFrameHandle::MaybeMonitorOpenFrame() {
  if (frame_ && !monitoring_source_id_.empty()) {
    VideoFrameMonitor::Instance().OnOpenFrame(monitoring_source_id_,
                                              frame_->unique_id());
  }
}

void VideoFrameHandle::MaybeMonitorCloseFrame() {
  if (frame_ && !monitoring_source_id_.empty()) {
    VideoFrameMonitor::Instance().OnCloseFrame(monitoring_source_id_,
                                               frame_->unique_id());
  }
}

bool VideoFrameHandle::WebGPURegisterExternalTextureExpireCallback(
    WebGPUExternalTextureExpireCallback
        webgpu_external_texture_expire_callback) {
  base::AutoLock locker(lock_);
  if (!frame_)
    return false;
  webgpu_external_texture_expire_callbacks_.push_back(
      std::move(webgpu_external_texture_expire_callback));
  return true;
}

void VideoFrameHandle::NotifyExpiredLocked() {
  DCHECK(!frame_);
  Vector<WebGPUExternalTextureExpireCallback>
      webgpu_external_texture_expire_callbacks =
          std::move(webgpu_external_texture_expire_callbacks_);
  for (auto& callback : webgpu_external_texture_expire_callbacks) {
    std::move(callback).Run();
  }
}

}  // namespace blink
