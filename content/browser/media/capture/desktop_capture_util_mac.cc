// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/desktop_capture_util_mac.h"

#include "base/functional/bind.h"
#include "base/task/bind_post_task.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/public/browser/browser_thread.h"
#include "media/webrtc/application_audio_capture_id_mac.h"
#include "third_party/webrtc/modules/desktop_capture/mac/window_list_utils.h"

namespace content {

namespace {

std::optional<desktop_capture::ApplicationAudioCaptureId> ConvertToContentId(
    const std::optional<media::ApplicationAudioCaptureId>& media_id) {
  if (!media_id) {
    return std::nullopt;
  }
  return desktop_capture::ApplicationAudioCaptureId{
      .bundle_id = media_id->bundle_id,
      .pid = media_id->pid,
  };
}

}  // namespace

void GetApplicationAudioCaptureIdInternal(
    DesktopMediaID desktop_media_id,
    desktop_capture::GetApplicationAudioCaptureIdCallback callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &GetApplicationAudioCaptureIdInternal, desktop_media_id,
            base::BindPostTaskToCurrentDefault(std::move(callback))));
    return;
  }

  if (desktop_media_id.id_type ==
      DesktopMediaID::IdType::kNativePickerSession) {
    auto convert_callback = base::BindOnce(
        [](desktop_capture::GetApplicationAudioCaptureIdCallback
               original_callback,
           const std::optional<media::ApplicationAudioCaptureId>& media_id) {
          std::move(original_callback).Run(ConvertToContentId(media_id));
        },
        std::move(callback));

    content::MediaStreamManager::GetInstance()
        ->video_capture_manager()
        ->GetApplicationAudioCaptureId(desktop_media_id.id,
                                       std::move(convert_callback));
  } else {
    std::optional<media::ApplicationAudioCaptureId> media_id =
        media::GetApplicationAudioCaptureIdForProcess(
            webrtc::GetWindowOwnerPid(desktop_media_id.id));
    std::move(callback).Run(ConvertToContentId(media_id));
  }
}

}  // namespace content
