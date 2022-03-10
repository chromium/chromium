// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_utils.h"

#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"
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

void MediaStreamUtils::CreateNativeAudioMediaStreamTrack(
    MediaStreamComponent* component,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  MediaStreamSource* source = component->Source();
  MediaStreamAudioSource* audio_source = MediaStreamAudioSource::From(source);

  // At this point, a MediaStreamAudioSource instance must exist. The one
  // exception is when a WebAudio destination node is acting as a source of
  // audio.
  if (!audio_source && source->RequiresAudioConsumer()) {
    DVLOG(1) << "Creating WebAudio media stream source.";
    audio_source = new WebAudioMediaStreamSource(source, task_runner);
    // |source| takes ownership of |audio_source|.
    source->SetPlatformSource(
        base::WrapUnique(audio_source));  // Takes ownership.

    MediaStreamSource::Capabilities capabilities;
    capabilities.device_id = source->Id();
    capabilities.echo_cancellation = Vector<bool>({false});
    capabilities.auto_gain_control = Vector<bool>({false});
    capabilities.noise_suppression = Vector<bool>({false});
    capabilities.sample_size = {
        media::SampleFormatToBitsPerChannel(media::kSampleFormatS16),  // min
        media::SampleFormatToBitsPerChannel(media::kSampleFormatS16)   // max
    };
    auto parameters = audio_source->GetAudioParameters();
    if (parameters.IsValid()) {
      capabilities.channel_count = {1, parameters.channels()};
      capabilities.sample_rate = {parameters.sample_rate(),
                                  parameters.sample_rate()};
      capabilities.latency = {parameters.GetBufferDuration().InSecondsF(),
                              parameters.GetBufferDuration().InSecondsF()};
    }
    source->SetCapabilities(capabilities);
  }

  if (audio_source)
    audio_source->ConnectToTrack(component);
  else
    LOG(DFATAL) << "MediaStreamSource missing its MediaStreamAudioSource.";
}

// TODO(crbug.com/704136): Change this method to take the task
// runner instance, and use per thread task runner on the call site.
void MediaStreamUtils::DidCreateMediaStreamTrack(
    MediaStreamComponent* component) {
  DCHECK(component);
  DCHECK(!component->GetPlatformTrack());
  DCHECK(component->Source());

  switch (component->Source()->GetType()) {
    case MediaStreamSource::kTypeAudio:
      CreateNativeAudioMediaStreamTrack(component,
                                        Thread::MainThread()->GetTaskRunner());
      break;
    case MediaStreamSource::kTypeVideo:
      CreateNativeVideoMediaStreamTrack(component);
      break;
  }
}

}  // namespace blink
