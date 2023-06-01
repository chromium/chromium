// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_utils.h"

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

namespace blink {

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
  const display::ScreenInfo& info =
      frame->GetChromeClient().GetScreenInfo(*frame);
  // If no screen size info, use the default.
  if (info.rect.size().IsEmpty()) {
    return kDefaultScreenSize;
  }
  return info.rect.size();
}

}  // namespace blink
