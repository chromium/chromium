// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_media_stream_track_adapter.h"

#include "content/renderer/media/stream/media_stream_audio_track.h"
#include "content/renderer/media/stream/processed_local_audio_source.h"
#include "content/renderer/media/webrtc/media_stream_video_webrtc_sink.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"

namespace content {

// static
scoped_refptr<WebRtcMediaStreamTrackAdapter>
WebRtcMediaStreamTrackAdapter::CreateLocalTrackAdapter(
    PeerConnectionDependencyFactory* factory,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
    const blink::WebMediaStreamTrack& web_track) {
  DCHECK(factory);
  DCHECK(main_thread->BelongsToCurrentThread());
  DCHECK(!web_track.IsNull());
  scoped_refptr<WebRtcMediaStreamTrackAdapter> local_track_adapter(
      new WebRtcMediaStreamTrackAdapter(factory, main_thread));
  if (web_track.Source().GetType() == blink::WebMediaStreamSource::kTypeAudio) {
    local_track_adapter->InitializeLocalAudioTrack(web_track);
  } else {
    DCHECK_EQ(web_track.Source().GetType(),
              blink::WebMediaStreamSource::kTypeVideo);
    local_track_adapter->InitializeLocalVideoTrack(web_track);
  }
  return local_track_adapter;
}

// static
scoped_refptr<WebRtcMediaStreamTrackAdapter>
WebRtcMediaStreamTrackAdapter::CreateRemoteTrackAdapter(
    PeerConnectionDependencyFactory* factory,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
    const scoped_refptr<webrtc::MediaStreamTrackInterface>& webrtc_track) {
  DCHECK(factory);
  DCHECK(!main_thread->BelongsToCurrentThread());
  DCHECK(webrtc_track);
  scoped_refptr<WebRtcMediaStreamTrackAdapter> remote_track_adapter(
      new WebRtcMediaStreamTrackAdapter(factory, main_thread));
  if (webrtc_track->kind() == webrtc::MediaStreamTrackInterface::kAudioKind) {
    remote_track_adapter->InitializeRemoteAudioTrack(base::WrapRefCounted(
        static_cast<webrtc::AudioTrackInterface*>(webrtc_track.get())));
  } else {
    DCHECK_EQ(webrtc_track->kind(),
              webrtc::MediaStreamTrackInterface::kVideoKind);
    remote_track_adapter->InitializeRemoteVideoTrack(base::WrapRefCounted(
        static_cast<webrtc::VideoTrackInterface*>(webrtc_track.get())));
  }
  return remote_track_adapter;
}

WebRtcMediaStreamTrackAdapter::WebRtcMediaStreamTrackAdapter(
    PeerConnectionDependencyFactory* factory,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread)
    : factory_(factory),
      main_thread_(main_thread),
      remote_track_can_complete_initialization_(
          base::WaitableEvent::ResetPolicy::MANUAL,
          base::WaitableEvent::InitialState::NOT_SIGNALED),
      is_initialized_(false),
      is_disposed_(false) {
  DCHECK(factory_);
  DCHECK(main_thread_);
}

WebRtcMediaStreamTrackAdapter::~WebRtcMediaStreamTrackAdapter() {
  DCHECK(!remote_track_can_complete_initialization_.IsSignaled());
  DCHECK(is_disposed_);
  // Ensured by destructor traits.
  DCHECK(main_thread_->BelongsToCurrentThread());
}

// static
void WebRtcMediaStreamTrackAdapterTraits::Destruct(
    const WebRtcMediaStreamTrackAdapter* adapter) {
  if (!adapter->main_thread_->BelongsToCurrentThread()) {
    adapter->main_thread_->PostTask(
        FROM_HERE,
        base::BindOnce(&WebRtcMediaStreamTrackAdapterTraits::Destruct,
                       base::Unretained(adapter)));
    return;
  }
  delete adapter;
}

void WebRtcMediaStreamTrackAdapter::Dispose() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  DCHECK(is_initialized_);
  if (is_disposed_)
    return;
  remote_track_can_complete_initialization_.Reset();
  is_disposed_ = true;
  if (web_track_.Source().GetType() ==
      blink::WebMediaStreamSource::kTypeAudio) {
    if (local_track_audio_sink_)
      DisposeLocalAudioTrack();
    else
      DisposeRemoteAudioTrack();
  } else {
    DCHECK_EQ(web_track_.Source().GetType(),
              blink::WebMediaStreamSource::kTypeVideo);
    if (local_track_video_sink_)
      DisposeLocalVideoTrack();
    else
      DisposeRemoteVideoTrack();
  }
}

bool WebRtcMediaStreamTrackAdapter::is_initialized() const {
  return is_initialized_;
}

void WebRtcMediaStreamTrackAdapter::InitializeOnMainThread() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  if (is_initialized_)
    return;
  // TODO(hbos): Only ever initialize explicitly,
  // remove EnsureTrackIsInitialized(). https://crbug.com/857458
  EnsureTrackIsInitialized();
}

const blink::WebMediaStreamTrack& WebRtcMediaStreamTrackAdapter::web_track() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  EnsureTrackIsInitialized();
  DCHECK(!web_track_.IsNull());
  return web_track_;
}

webrtc::MediaStreamTrackInterface*
WebRtcMediaStreamTrackAdapter::webrtc_track() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  DCHECK(webrtc_track_);
  EnsureTrackIsInitialized();
  return webrtc_track_.get();
}

bool WebRtcMediaStreamTrackAdapter::IsEqual(
    const blink::WebMediaStreamTrack& web_track) {
  DCHECK(main_thread_->BelongsToCurrentThread());
  EnsureTrackIsInitialized();
  return web_track_.GetTrackData() == web_track.GetTrackData();
}

void WebRtcMediaStreamTrackAdapter::InitializeLocalAudioTrack(
    const blink::WebMediaStreamTrack& web_track) {
  DCHECK(main_thread_->BelongsToCurrentThread());
  DCHECK(!is_initialized_);
  DCHECK(!web_track.IsNull());
  DCHECK_EQ(web_track.Source().GetType(),
            blink::WebMediaStreamSource::kTypeAudio);
  web_track_ = web_track;
  MediaStreamAudioTrack* native_track = MediaStreamAudioTrack::From(web_track_);
  DCHECK(native_track);

  // Non-WebRtc remote sources and local sources do not provide an instance of
  // the webrtc::AudioSourceInterface, and also do not need references to the
  // audio level calculator or audio processor passed to the sink.
  webrtc::AudioSourceInterface* source_interface = nullptr;
  local_track_audio_sink_.reset(
      new WebRtcAudioSink(web_track_.Id().Utf8(), source_interface,
                          factory_->GetWebRtcSignalingThread(), main_thread_));

  if (auto* media_stream_source = ProcessedLocalAudioSource::From(
          MediaStreamAudioSource::From(web_track_.Source()))) {
    local_track_audio_sink_->SetLevel(media_stream_source->audio_level());
    // The sink only grabs stats from the audio processor. Stats are only
    // available if audio processing is turned on. Therefore, only provide the
    // sink a reference to the processor if audio processing is turned on.
    if (media_stream_source->has_audio_processing()) {
      local_track_audio_sink_->SetAudioProcessor(
          media_stream_source->audio_processor());
    }
  }
  native_track->AddSink(local_track_audio_sink_.get());
  webrtc_track_ = local_track_audio_sink_->webrtc_audio_track();
  DCHECK(webrtc_track_);
  is_initialized_ = true;
}

void WebRtcMediaStreamTrackAdapter::InitializeLocalVideoTrack(
    const blink::WebMediaStreamTrack& web_track) {
  DCHECK(main_thread_->BelongsToCurrentThread());
  DCHECK(!is_initialized_);
  DCHECK(!web_track.IsNull());
  DCHECK_EQ(web_track.Source().GetType(),
            blink::WebMediaStreamSource::kTypeVideo);
  web_track_ = web_track;
  local_track_video_sink_.reset(
      new MediaStreamVideoWebRtcSink(web_track_, factory_, main_thread_));
  webrtc_track_ = local_track_video_sink_->webrtc_video_track();
  DCHECK(webrtc_track_);
  is_initialized_ = true;
}

void WebRtcMediaStreamTrackAdapter::InitializeRemoteAudioTrack(
    const scoped_refptr<webrtc::AudioTrackInterface>& webrtc_audio_track) {
  DCHECK(!main_thread_->BelongsToCurrentThread());
  DCHECK(!is_initialized_);
  DCHECK(!remote_track_can_complete_initialization_.IsSignaled());
  DCHECK(webrtc_audio_track);
  DCHECK_EQ(webrtc_audio_track->kind(),
            webrtc::MediaStreamTrackInterface::kAudioKind);
  remote_audio_track_adapter_ =
      new RemoteAudioTrackAdapter(main_thread_, webrtc_audio_track.get());
  webrtc_track_ = webrtc_audio_track;
  // Set the initial volume to zero. When the track is put in an audio tag for
  // playout, its volume is set to that of the tag. Without this, we could end
  // up playing out audio that's not attached to any tag, see:
  // http://crbug.com/810848
  webrtc_audio_track->GetSource()->SetVolume(0);
  remote_track_can_complete_initialization_.Signal();
  main_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcMediaStreamTrackAdapter::
                         FinalizeRemoteTrackInitializationOnMainThread,
                     this));
}

void WebRtcMediaStreamTrackAdapter::InitializeRemoteVideoTrack(
    const scoped_refptr<webrtc::VideoTrackInterface>& webrtc_video_track) {
  DCHECK(!main_thread_->BelongsToCurrentThread());
  DCHECK(webrtc_video_track);
  DCHECK_EQ(webrtc_video_track->kind(),
            webrtc::MediaStreamTrackInterface::kVideoKind);
  remote_video_track_adapter_ =
      new RemoteVideoTrackAdapter(main_thread_, webrtc_video_track.get());
  webrtc_track_ = webrtc_video_track;
  remote_track_can_complete_initialization_.Signal();
  main_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcMediaStreamTrackAdapter::
                         FinalizeRemoteTrackInitializationOnMainThread,
                     this));
}

void WebRtcMediaStreamTrackAdapter::
    FinalizeRemoteTrackInitializationOnMainThread() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  DCHECK(remote_audio_track_adapter_ || remote_video_track_adapter_);
  if (is_initialized_)
    return;

  if (remote_audio_track_adapter_) {
    remote_audio_track_adapter_->Initialize();
    web_track_ = *remote_audio_track_adapter_->web_track();
  } else {
    remote_video_track_adapter_->Initialize();
    web_track_ = *remote_video_track_adapter_->web_track();
  }
  is_initialized_ = true;
}

void WebRtcMediaStreamTrackAdapter::EnsureTrackIsInitialized() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  if (is_initialized_)
    return;

  // Remote tracks may not be fully initialized yet, since they are partly
  // initialized on the signaling thread.
  remote_track_can_complete_initialization_.Wait();
  FinalizeRemoteTrackInitializationOnMainThread();
}

void WebRtcMediaStreamTrackAdapter::DisposeLocalAudioTrack() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  DCHECK(local_track_audio_sink_);
  DCHECK_EQ(web_track_.Source().GetType(),
            blink::WebMediaStreamSource::kTypeAudio);
  MediaStreamAudioTrack* audio_track = MediaStreamAudioTrack::From(web_track_);
  DCHECK(audio_track);
  audio_track->RemoveSink(local_track_audio_sink_.get());
  local_track_audio_sink_.reset();
  webrtc_track_ = nullptr;
  web_track_.Reset();
}

void WebRtcMediaStreamTrackAdapter::DisposeLocalVideoTrack() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  DCHECK(local_track_video_sink_);
  DCHECK_EQ(web_track_.Source().GetType(),
            blink::WebMediaStreamSource::kTypeVideo);
  local_track_video_sink_.reset();
  webrtc_track_ = nullptr;
  web_track_.Reset();
}

void WebRtcMediaStreamTrackAdapter::DisposeRemoteAudioTrack() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  DCHECK(remote_audio_track_adapter_);
  DCHECK_EQ(web_track_.Source().GetType(),
            blink::WebMediaStreamSource::kTypeAudio);
  factory_->GetWebRtcSignalingThread()->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcMediaStreamTrackAdapter::
                         UnregisterRemoteAudioTrackAdapterOnSignalingThread,
                     this));
}

void WebRtcMediaStreamTrackAdapter::DisposeRemoteVideoTrack() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  DCHECK(remote_video_track_adapter_);
  DCHECK_EQ(web_track_.Source().GetType(),
            blink::WebMediaStreamSource::kTypeVideo);
  FinalizeRemoteTrackDisposingOnMainThread();
}

void WebRtcMediaStreamTrackAdapter::
    UnregisterRemoteAudioTrackAdapterOnSignalingThread() {
  DCHECK(!main_thread_->BelongsToCurrentThread());
  DCHECK(remote_audio_track_adapter_);
  remote_audio_track_adapter_->Unregister();
  main_thread_->PostTask(
      FROM_HERE, base::BindOnce(&WebRtcMediaStreamTrackAdapter::
                                    FinalizeRemoteTrackDisposingOnMainThread,
                                this));
}

void WebRtcMediaStreamTrackAdapter::FinalizeRemoteTrackDisposingOnMainThread() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  DCHECK(is_disposed_);
  remote_audio_track_adapter_ = nullptr;
  remote_video_track_adapter_ = nullptr;
  webrtc_track_ = nullptr;
  web_track_.Reset();
}

}  // namespace content
