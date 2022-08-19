// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_utils.h"

#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
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

namespace blink {

namespace {

void CreateNativeVideoMediaStreamTrack(MediaStreamComponent* component) {
  DCHECK(!component->GetPlatformTrack());
  MediaStreamSource* source = component->Source();
  DCHECK_EQ(source->GetType(), MediaStreamSource::kTypeVideo);
  MediaStreamVideoSource* native_source =
      MediaStreamVideoSource::GetVideoSource(source);
  DCHECK(native_source);
  component->SetPlatformTrack(std::make_unique<blink::MediaStreamVideoTrack>(
      native_source, blink::MediaStreamVideoSource::ConstraintsOnceCallback(),
      component->Enabled()));
}

}  // namespace

void MediaStreamUtils::DidCreateMediaStreamTrack(
    MediaStreamComponent* component) {
  DCHECK(component);
  DCHECK(!component->GetPlatformTrack());
  DCHECK(component->Source());

  switch (component->GetSourceType()) {
    case MediaStreamSource::kTypeAudio:
      MediaStreamAudioSource::From(component->Source())
          ->ConnectToTrack(component);
      break;
    case MediaStreamSource::kTypeVideo:
      CreateNativeVideoMediaStreamTrack(component);
      break;
  }
}

MediaStreamTrack* MediaStreamUtils::CreateLocalAudioTrack(
    ExecutionContext* execution_context,
    MediaStreamSource* source) {
  DCHECK_EQ(source->GetType(), MediaStreamSource::kTypeAudio);
  DCHECK(!source->Remote());
  // TODO(crbug.com/1302689): Provide a local MediaStreamAudioTrack instance in
  // the Component constructor.
  auto* component = MakeGarbageCollected<MediaStreamComponentImpl>(source);
  DidCreateMediaStreamTrack(component);
  return MakeGarbageCollected<MediaStreamTrackImpl>(execution_context,
                                                    component);
}

}  // namespace blink
