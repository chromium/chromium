// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/modules/mediastream/web_platform_media_stream_source.h"

#include <utility>

#include "base/check.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"

namespace blink {

const char WebPlatformMediaStreamSource::kSourceId[] = "sourceId";

WebPlatformMediaStreamSource::WebPlatformMediaStreamSource(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

WebPlatformMediaStreamSource::~WebPlatformMediaStreamSource() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(stop_callback_.is_null());
  owner_ = nullptr;
}

void WebPlatformMediaStreamSource::StopSource() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DoStopSource();
  FinalizeStopSource();
}

void WebPlatformMediaStreamSource::FinalizeStopSource() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!stop_callback_.is_null()) {
    std::move(stop_callback_).Run(Owner());
  }
  if (Owner()) {
    Owner().SetReadyState(WebMediaStreamSource::kReadyStateEnded);
  }
}

void WebPlatformMediaStreamSource::SetSourceMuted(bool is_muted) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  // Although this change is valid only if the ready state isn't already Ended,
  // there's code further along (like in MediaStreamTrack) which filters
  // that out already.
  if (!Owner()) {
    return;
  }
  Owner().SetReadyState(is_muted ? WebMediaStreamSource::kReadyStateMuted
                                 : WebMediaStreamSource::kReadyStateLive);
}

void WebPlatformMediaStreamSource::SetDevice(const MediaStreamDevice& device) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  device_ = device;
}

void WebPlatformMediaStreamSource::SetCaptureHandle(
    media::mojom::CaptureHandlePtr capture_handle) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!device_.display_media_info) {
    DVLOG(1) << "Not a display-capture device.";
    return;
  }
  device_.display_media_info->capture_handle = std::move(capture_handle);
}

void WebPlatformMediaStreamSource::SetStopCallback(
    SourceStoppedCallback stop_callback) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(stop_callback_.is_null());
  stop_callback_ = std::move(stop_callback);
}

void WebPlatformMediaStreamSource::ResetSourceStoppedCallback() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  stop_callback_.Reset();
}

void WebPlatformMediaStreamSource::ChangeSource(
    const MediaStreamDevice& new_device) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DoChangeSource(new_device);
}

WebMediaStreamSource WebPlatformMediaStreamSource::Owner() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return WebMediaStreamSource(owner_.Get());
}

#if INSIDE_BLINK
void WebPlatformMediaStreamSource::SetOwner(MediaStreamSource* owner) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!owner_);
  owner_ = owner;
}
#endif

base::SingleThreadTaskRunner* WebPlatformMediaStreamSource::GetTaskRunner()
    const {
  return task_runner_.get();
}

}  // namespace blink
