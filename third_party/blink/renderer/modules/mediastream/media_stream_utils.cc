// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_utils.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_video_content.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track_impl.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/mediastream/webaudio_media_stream_source.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "ui/display/screen_info.h"
#include "ui/display/screen_infos.h"

namespace blink {

// Makes getDisplayMedia take into account the device's scale factor
// to compute screen sizes in calls without size constraints.
BASE_FEATURE(kGetDisplayMediaScreenScaleFactor,
             "GetDisplayMediaScreenScaleFactor",
#if BUILDFLAG(IS_CHROMEOS)
             // Causes crash/timeouts on some ChromeOS devices.
             // See https://issuetracker.google.com/issues/284804471
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_CHROMEOS)
);

MediaStreamTrack* MediaStreamUtils::CreateLocalAudioTrack(
    ExecutionContext* execution_context,
    MediaStreamSource* source) {
  DCHECK_EQ(source->GetType(), MediaStreamSource::kTypeAudio);
  DCHECK(!source->Remote());
  auto* component = MakeGarbageCollected<MediaStreamComponentImpl>(
      source, std::make_unique<MediaStreamAudioTrack>(/*is_local=*/true));
  MediaStreamAudioSource::From(component->Source())
      ->ConnectToInitializedTrack(component);
  return MakeGarbageCollected<MediaStreamTrackImpl>(execution_context,
                                                    component);
}

gfx::Size MediaStreamUtils::GetScreenSize(LocalFrame* frame) {
  const gfx::Size kDefaultScreenSize(kDefaultScreenCastWidth,
                                     kDefaultScreenCastHeight);
  // Can be null in tests.
  if (!frame) {
    return kDefaultScreenSize;
  }
  int max_width = 0;
  int max_height = 0;
  const auto& infos = frame->GetChromeClient().GetScreenInfos(*frame);
  for (const display::ScreenInfo& info : infos.screen_infos) {
    int width = info.rect.width();
    int height = info.rect.height();
    if (base::FeatureList::IsEnabled(kGetDisplayMediaScreenScaleFactor) &&
        info.device_scale_factor > 0) {
      width = ceil(width * info.device_scale_factor);
      height = ceil(height * info.device_scale_factor);
    }
    if (width > max_width) {
      max_width = width;
    }
    if (height > max_height) {
      max_height = height;
    }
  }
  if (max_width == 0 || max_height == 0) {
    return kDefaultScreenSize;
  }
  return gfx::Size(max_width, max_height);
}

}  // namespace blink
