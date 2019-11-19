// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/remote_media_stream_track_adapter.h"

#include "base/single_thread_task_runner.h"
#include "media/base/limits.h"
#include "third_party/blink/public/platform/modules/webrtc/track_observer.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/peerconnection/media_stream_remote_video_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/webrtc/peer_connection_remote_audio_source.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

RemoteVideoTrackAdapter::RemoteVideoTrackAdapter(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
    webrtc::VideoTrackInterface* webrtc_track)
    : RemoteMediaStreamTrackAdapter(main_thread, webrtc_track) {
  std::unique_ptr<TrackObserver> observer(
      new TrackObserver(main_thread, observed_track().get()));
  // Here, we use CrossThreadUnretained() to avoid a circular reference.
  //
  // TODO(crbug.com/963574): Remove the use of ConvertToBaseOnceCallback here
  // once the file that includes remote_media_stream_track_adapter.h (namely
  // webrtc_media_stream_track_adapter.h) is Onion souped.
  web_initialize_ = ConvertToBaseOnceCallback(
      CrossThreadBindOnce(&RemoteVideoTrackAdapter::InitializeWebVideoTrack,
                          CrossThreadUnretained(this), std::move(observer),
                          observed_track()->enabled()));
}

RemoteVideoTrackAdapter::~RemoteVideoTrackAdapter() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  if (initialized()) {
    // TODO(crbug.com/704136): When moving RemoteVideoTrackAdapter out of the
    // public API, make this managed by Oilpan. Note that, the destructor will
    // not allowed to touch other on-heap objects like web_track().
    static_cast<MediaStreamRemoteVideoSource*>(
        web_track()->Source().GetPlatformSource())
        ->OnSourceTerminated();
  }
}

void RemoteVideoTrackAdapter::InitializeWebVideoTrack(
    std::unique_ptr<TrackObserver> observer,
    bool enabled) {
  DCHECK(main_thread_->BelongsToCurrentThread());
  auto video_source_ptr =
      std::make_unique<MediaStreamRemoteVideoSource>(std::move(observer));
  MediaStreamRemoteVideoSource* video_source = video_source_ptr.get();
  InitializeWebTrack(WebMediaStreamSource::kTypeVideo);
  web_track()->Source().SetPlatformSource(std::move(video_source_ptr));

  WebMediaStreamSource::Capabilities capabilities;
  capabilities.device_id = id();
  web_track()->Source().SetCapabilities(capabilities);

  web_track()->SetPlatformTrack(std::make_unique<MediaStreamVideoTrack>(
      video_source, MediaStreamVideoSource::ConstraintsCallback(), enabled));
}

RemoteAudioTrackAdapter::RemoteAudioTrackAdapter(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
    webrtc::AudioTrackInterface* webrtc_track)
    : RemoteMediaStreamTrackAdapter(main_thread, webrtc_track),
#if DCHECK_IS_ON()
      unregistered_(false),
#endif
      state_(observed_track()->state()) {
  // TODO(tommi): Use TrackObserver instead.
  observed_track()->RegisterObserver(this);
  // Here, we use CrossThreadUnretained() to avoid a circular reference.
  //
  // TODO(crbug.com/963574): Remove the use of ConvertToBaseOnceCallback here
  // once the file that includes remote_media_stream_track_adapter.h (namely
  // webrtc_media_stream_track_adapter.h) is Onion souped.
  web_initialize_ = ConvertToBaseOnceCallback(
      CrossThreadBindOnce(&RemoteAudioTrackAdapter::InitializeWebAudioTrack,
                          CrossThreadUnretained(this), main_thread));
}

RemoteAudioTrackAdapter::~RemoteAudioTrackAdapter() {
#if DCHECK_IS_ON()
  DCHECK(unregistered_);
#endif
}

void RemoteAudioTrackAdapter::Unregister() {
#if DCHECK_IS_ON()
  DCHECK(!unregistered_);
  unregistered_ = true;
#endif
  observed_track()->UnregisterObserver(this);
}

void RemoteAudioTrackAdapter::InitializeWebAudioTrack(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread) {
  InitializeWebTrack(WebMediaStreamSource::kTypeAudio);

  auto source = std::make_unique<PeerConnectionRemoteAudioSource>(
      observed_track().get(), main_thread);
  auto* source_ptr = source.get();
  web_track()->Source().SetPlatformSource(
      std::move(source));  // Takes ownership.

  WebMediaStreamSource::Capabilities capabilities;
  capabilities.device_id = id();
  bool values[] = {false};
  capabilities.echo_cancellation = WebVector<bool>(values, 1u);
  capabilities.auto_gain_control = WebVector<bool>(values, 1u);
  capabilities.noise_suppression = WebVector<bool>(values, 1u);
  capabilities.sample_size = {
      media::SampleFormatToBitsPerChannel(media::kSampleFormatS16),  // min
      media::SampleFormatToBitsPerChannel(media::kSampleFormatS16)   // max
  };
  web_track()->Source().SetCapabilities(capabilities);

  source_ptr->ConnectToTrack(*(web_track()));
}

void RemoteAudioTrackAdapter::OnChanged() {
  PostCrossThreadTask(
      *main_thread_, FROM_HERE,
      CrossThreadBindOnce(&RemoteAudioTrackAdapter::OnChangedOnMainThread,
                          WrapRefCounted(this), observed_track()->state()));
}

void RemoteAudioTrackAdapter::OnChangedOnMainThread(
    webrtc::MediaStreamTrackInterface::TrackState state) {
  DCHECK(main_thread_->BelongsToCurrentThread());

  if (state == state_ || !initialized())
    return;

  state_ = state;

  switch (state) {
    case webrtc::MediaStreamTrackInterface::kLive:
      web_track()->Source().SetReadyState(
          WebMediaStreamSource::kReadyStateLive);
      break;
    case webrtc::MediaStreamTrackInterface::kEnded:
      web_track()->Source().SetReadyState(
          WebMediaStreamSource::kReadyStateEnded);
      break;
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace blink
