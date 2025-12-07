// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/video_capture_device_launcher.h"

#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/browser/renderer_host/media/video_capture_provider.h"
#include "content/public/browser/browser_thread.h"

namespace content {

// static
std::unique_ptr<VideoCaptureDeviceLauncher>
VideoCaptureDeviceLauncher::CreateDeviceLauncherFromMediaStreamManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  MediaStreamManager* msm = MediaStreamManager::GetInstance();
  if (!msm || !msm->video_capture_manager()) {
    return nullptr;
  }
  return msm->video_capture_manager()
      ->video_capture_provider()
      .CreateDeviceLauncher();
}

}  // namespace content
