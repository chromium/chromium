// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_utils.h"

#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"
#include "third_party/blink/renderer/platform/mediastream/webaudio_media_stream_source.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"

namespace blink {

namespace {

void CreateNativeVideoMediaStreamTrack(blink::WebMediaStreamTrack track) {
  DCHECK(!track.GetPlatformTrack());
  blink::WebMediaStreamSource source = track.Source();
  DCHECK_EQ(source.GetType(), blink::WebMediaStreamSource::kTypeVideo);
  blink::MediaStreamVideoSource* native_source =
      blink::MediaStreamVideoSource::GetVideoSource(source);
  DCHECK(native_source);
  track.SetPlatformTrack(std::make_unique<blink::MediaStreamVideoTrack>(
      native_source, blink::MediaStreamVideoSource::ConstraintsCallback(),
      track.IsEnabled()));
}

}  // namespace

void MediaStreamUtils::CreateNativeAudioMediaStreamTrack(
    const blink::WebMediaStreamTrack& track,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  WebMediaStreamSource source = track.Source();
  MediaStreamAudioSource* media_stream_source =
      blink::MediaStreamAudioSource::From(source);

  // At this point, a MediaStreamAudioSource instance must exist. The one
  // exception is when a WebAudio destination node is acting as a source of
  // audio.
  //
  // TODO(miu): This needs to be moved to an appropriate location. A WebAudio
  // source should have been created before this method was called so that this
  // special case code isn't needed here.
  if (!media_stream_source && source.RequiresAudioConsumer()) {
    DVLOG(1) << "Creating WebAudio media stream source.";
    media_stream_source =
        new blink::WebAudioMediaStreamSource(&source, task_runner);
    source.SetPlatformSource(
        base::WrapUnique(media_stream_source));  // Takes ownership.

    blink::WebMediaStreamSource::Capabilities capabilities;
    capabilities.device_id = source.Id();
    // TODO(crbug.com/704136): Switch away from std::vector.
    capabilities.echo_cancellation = std::vector<bool>({false});
    capabilities.auto_gain_control = std::vector<bool>({false});
    capabilities.noise_suppression = std::vector<bool>({false});
    capabilities.sample_size = {
        media::SampleFormatToBitsPerChannel(media::kSampleFormatS16),  // min
        media::SampleFormatToBitsPerChannel(media::kSampleFormatS16)   // max
    };
    auto parameters = media_stream_source->GetAudioParameters();
    if (parameters.IsValid()) {
      capabilities.channel_count = {1, parameters.channels()};
      capabilities.sample_rate = {parameters.sample_rate(),
                                  parameters.sample_rate()};
      capabilities.latency = {parameters.GetBufferDuration().InSecondsF(),
                              parameters.GetBufferDuration().InSecondsF()};
    }
    source.SetCapabilities(capabilities);
  }

  if (media_stream_source)
    media_stream_source->ConnectToTrack(track);
  else
    LOG(DFATAL) << "WebMediaStreamSource missing its MediaStreamAudioSource.";
}

// TODO(crbug.com/704136): Change this method to take the task
// runner instance, and use per thread task runner on the call site.
void MediaStreamUtils::DidCreateMediaStreamTrack(
    MediaStreamComponent* component) {
  WebMediaStreamTrack track(component);
  DCHECK(!track.IsNull() && !track.GetPlatformTrack());
  DCHECK(!track.Source().IsNull());

  switch (track.Source().GetType()) {
    case blink::WebMediaStreamSource::kTypeAudio:
      CreateNativeAudioMediaStreamTrack(track,
                                        Thread::MainThread()->GetTaskRunner());
      break;
    case blink::WebMediaStreamSource::kTypeVideo:
      CreateNativeVideoMediaStreamTrack(track);
      break;
  }
}

}  // namespace blink
