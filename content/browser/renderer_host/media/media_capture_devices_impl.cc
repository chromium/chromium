// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_capture_devices_impl.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {

void EnsureMonitorCaptureDevices() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &MediaStreamManager::EnsureDeviceMonitorStarted,
          base::Unretained(
              BrowserMainLoop::GetInstance()->media_stream_manager())));
}

}  // namespace

MediaCaptureDevices* MediaCaptureDevices::GetInstance() {
  return MediaCaptureDevicesImpl::GetInstance();
}

MediaCaptureDevicesImpl* MediaCaptureDevicesImpl::GetInstance() {
  return base::Singleton<MediaCaptureDevicesImpl>::get();
}

const blink::MediaStreamDevices&
MediaCaptureDevicesImpl::GetAudioCaptureDevices() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!devices_enumerated_) {
    EnsureMonitorCaptureDevices();
    devices_enumerated_ = true;
  }
  return audio_devices_;
}

const blink::MediaStreamDevices&
MediaCaptureDevicesImpl::GetVideoCaptureDevices() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!devices_enumerated_) {
    EnsureMonitorCaptureDevices();
    devices_enumerated_ = true;
  }
  return video_devices_;
}

void MediaCaptureDevicesImpl::AddVideoCaptureObserver(
    media::VideoCaptureObserver* observer) {
  MediaStreamManager* media_stream_manager =
      BrowserMainLoop::GetInstance()->media_stream_manager();
  if (media_stream_manager != nullptr) {
    base::PostTask(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&MediaStreamManager::AddVideoCaptureObserver,
                       base::Unretained(media_stream_manager), observer));
  } else {
    DVLOG(3) << "media_stream_manager is null.";
  }
}

void MediaCaptureDevicesImpl::RemoveAllVideoCaptureObservers() {
  MediaStreamManager* media_stream_manager =
      BrowserMainLoop::GetInstance()->media_stream_manager();
  if (media_stream_manager != nullptr) {
    base::PostTask(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&MediaStreamManager::RemoveAllVideoCaptureObservers,
                       base::Unretained(media_stream_manager)));
  } else {
    DVLOG(3) << "media_stream_manager is null.";
  }
}

void MediaCaptureDevicesImpl::OnAudioCaptureDevicesChanged(
    const blink::MediaStreamDevices& devices) {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    UpdateAudioDevicesOnUIThread(devices);
  } else {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&MediaCaptureDevicesImpl::UpdateAudioDevicesOnUIThread,
                       base::Unretained(this), devices));
  }
}

void MediaCaptureDevicesImpl::OnVideoCaptureDevicesChanged(
    const blink::MediaStreamDevices& devices) {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    UpdateVideoDevicesOnUIThread(devices);
  } else {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&MediaCaptureDevicesImpl::UpdateVideoDevicesOnUIThread,
                       base::Unretained(this), devices));
  }
}

MediaCaptureDevicesImpl::MediaCaptureDevicesImpl()
    : devices_enumerated_(false) {
}

MediaCaptureDevicesImpl::~MediaCaptureDevicesImpl() {
}

void MediaCaptureDevicesImpl::UpdateAudioDevicesOnUIThread(
    const blink::MediaStreamDevices& devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  devices_enumerated_ = true;
  audio_devices_ = devices;
}

void MediaCaptureDevicesImpl::UpdateVideoDevicesOnUIThread(
    const blink::MediaStreamDevices& devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  devices_enumerated_ = true;
  video_devices_ = devices;
}

}  // namespace content
