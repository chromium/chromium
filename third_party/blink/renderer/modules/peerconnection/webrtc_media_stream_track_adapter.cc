// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/webrtc_media_stream_track_adapter.h"

#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/renderer/modules/mediastream/processed_local_audio_source.h"
#include "third_party/blink/renderer/modules/peerconnection/media_stream_video_webrtc_sink.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace {

void SendLogMessage(const std::string& message) {
  blink::WebRtcLogMessage("WRMSTA::" + message);
}

}  // namespace

namespace blink {

// static
scoped_refptr<WebRtcMediaStreamTrackAdapter>
WebRtcMediaStreamTrackAdapter::CreateLocalTrackAdapter(
    blink::PeerConnectionDependencyFactory* factory,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
    MediaStreamComponent* component) {
  DCHECK(factory);
  DCHECK(main_thread->BelongsToCurrentThread());
  DCHECK(component);
  scoped_refptr<WebRtcMediaStreamTrackAdapter> local_track_adapter(
      base::AdoptRef(new WebRtcMediaStreamTrackAdapter(factory, main_thread)));
  if (component->GetSourceType() == MediaStreamSource::kTypeAudio) {
    local_track_adapter->InitializeLocalAudioTrack(component);
  } else {
    DCHECK_EQ(component->GetSourceType(), MediaStreamSource::kTypeVideo);
    local_track_adapter->InitializeLocalVideoTrack(component);
  }
  return local_track_adapter;
}

// static
scoped_refptr<WebRtcMediaStreamTrackAdapter>
WebRtcMediaStreamTrackAdapter::CreateRemoteTrackAdapter(
    blink::PeerConnectionDependencyFactory* factory,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
    const scoped_refptr<webrtc::MediaStreamTrackInterface>& webrtc_track) {
  DCHECK(factory);
  DCHECK(!main_thread->BelongsToCurrentThread());
  DCHECK(webrtc_track);
  scoped_refptr<WebRtcMediaStreamTrackAdapter> remote_track_adapter(
      base::AdoptRef(new WebRtcMediaStreamTrackAdapter(factory, main_thread)));
  if (webrtc_track->kind() == webrtc::MediaStreamTrackInterface::kAudioKind) {
    remote_track_adapter->InitializeRemoteAudioTrack(
        base::WrapRefCounted(
            static_cast<webrtc::AudioTrackInterface*>(webrtc_track.get())),
        factory->GetSupplementable());
  } else {
    DCHECK_EQ(webrtc_track->kind(),
              webrtc::MediaStreamTrackInterface::kVideoKind);
    remote_track_adapter->InitializeRemoteVideoTrack(
        base::WrapRefCounted(
            static_cast<webrtc::VideoTrackInterface*>(webrtc_track.get())),
        factory->GetSupplementable());
  }
  return remote_track_adapter;
}

WebRtcMediaStreamTrackAdapter::WebRtcMediaStreamTrackAdapter(
    blink::PeerConnectionDependencyFactory* factory,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread)
    : factory_(factory),
      webrtc_signaling_task_runner_(nullptr),
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
    PostCrossThreadTask(
        *adapter->main_thread_.get(), FROM_HERE,
        CrossThreadBindOnce(&WebRtcMediaStreamTrackAdapterTraits::Destruct,
                            CrossThreadUnretained(adapter)));
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
  if (component_->GetSourceType() == MediaStreamSource::kTypeAudio) {
    if (local_track_audio_sink_)
      DisposeLocalAudioTrack();
    else
      DisposeRemoteAudioTrack();
  } else {
    DCHECK_EQ(component_->GetSourceType(), MediaStreamSource::kTypeVideo);
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

MediaStreamComponent* WebRtcMediaStreamTrackAdapter::track() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  EnsureTrackIsInitialized();
  DCHECK(component_);
  return component_.Get();
}

rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>
WebRtcMediaStreamTrackAdapter::webrtc_track() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  DCHECK(webrtc_track_);
  EnsureTrackIsInitialized();
  return rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>(
      webrtc_track_.get());
}

bool WebRtcMediaStreamTrackAdapter::IsEqual(MediaStreamComponent* component) {
  DCHECK(main_thread_->BelongsToCurrentThread());
  EnsureTrackIsInitialized();
  return component_->GetPlatformTrack() == component->GetPlatformTrack();
}

void WebRtcMediaStreamTrackAdapter::InitializeLocalAudioTrack(
    MediaStreamComponent* component) {
  DCHECK(main_thread_->BelongsToCurrentThread());
  DCHECK(!is_initialized_);
  DCHECK(component);
  DCHECK_EQ(component->GetSourceType(), MediaStreamSource::kTypeAudio);
  SendLogMessage(base::StringPrintf("InitializeLocalAudioTrack({id=%s})",
                                    component->Id().Utf8().c_str()));
  component_ = component;

  // Non-WebRtc remote sources and local sources do not provide an instance of
  // the webrtc::AudioSourceInterface, and also do not need references to the
  // audio level calculator or audio processor passed to the sink.
  webrtc::AudioSourceInterface* source_interface = nullptr;

  // Initialize `webrtc_signaling_task_runner_` here instead of the ctor since
  // `GetWebRtcSignalingTaskRunner()` must be called on the main thread.
  auto factory = factory_.Lock();
  DCHECK(factory);
  webrtc_signaling_task_runner_ = factory->GetWebRtcSignalingTaskRunner();

  local_track_audio_sink_ = std::make_unique<blink::WebRtcAudioSink>(
      component_->Id().Utf8(), source_interface, webrtc_signaling_task_runner_,
      main_thread_);

  if (auto* media_stream_source = blink::ProcessedLocalAudioSource::From(
          blink::MediaStreamAudioSource::From(component_->Source()))) {
    local_track_audio_sink_->SetLevel(media_stream_source->audio_level());
    local_track_audio_sink_->SetAudioProcessor(
        media_stream_source->GetAudioProcessor());
  }
  component_->AddSink(local_track_audio_sink_.get());
  webrtc_track_ = local_track_audio_sink_->webrtc_audio_track();
  DCHECK(webrtc_track_);
  is_initialized_ = true;
}

void WebRtcMediaStreamTrackAdapter::InitializeLocalVideoTrack(
    MediaStreamComponent* component) {
  DCHECK(main_thread_->BelongsToCurrentThread());
  DCHECK(!is_initialized_);
  DCHECK(component);
  DCHECK_EQ(component->GetSourceType(), MediaStreamSource::kTypeVideo);
  component_ = component;
  auto factory = factory_.Lock();
  DCHECK(factory);
  local_track_video_sink_ = std::make_unique<blink::MediaStreamVideoWebRtcSink>(
      component_, factory, main_thread_);
  webrtc_track_ = local_track_video_sink_->webrtc_video_track();
  DCHECK(webrtc_track_);

  // Initialize `webrtc_signaling_task_runner_` here instead of the ctor since
  // `GetWebRtcSignalingTaskRunner()` must be called on the main thread.
  webrtc_signaling_task_runner_ = factory->GetWebRtcSignalingTaskRunner();

  is_initialized_ = true;
}

void WebRtcMediaStreamTrackAdapter::InitializeRemoteAudioTrack(
    const scoped_refptr<webrtc::AudioTrackInterface>& webrtc_audio_track,
    ExecutionContext* track_execution_context) {
  DCHECK(!main_thread_->BelongsToCurrentThread());
  DCHECK(!is_initialized_);
  DCHECK(!remote_track_can_complete_initialization_.IsSignaled());
  DCHECK(webrtc_audio_track);
  DCHECK_EQ(webrtc_audio_track->kind(),
            webrtc::MediaStreamTrackInterface::kAudioKind);
  SendLogMessage(
      base::StringPrintf("InitializeRemoteAudioTrack([this=%p])", this));
  remote_audio_track_adapter_ =
      base::MakeRefCounted<blink::RemoteAudioTrackAdapter>(
          main_thread_, webrtc_audio_track.get(), track_execution_context);
  webrtc_track_ = webrtc_audio_track;
  // Set the initial volume to zero. When the track is put in an audio tag for
  // playout, its volume is set to that of the tag. Without this, we could end
  // up playing out audio that's not attached to any tag, see:
  // http://crbug.com/810848
  webrtc_audio_track->GetSource()->SetVolume(0);
  remote_track_can_complete_initialization_.Signal();
  PostCrossThreadTask(
      *main_thread_.get(), FROM_HERE,
      CrossThreadBindOnce(&WebRtcMediaStreamTrackAdapter::
                              FinalizeRemoteTrackInitializationOnMainThread,
                          WrapRefCounted(this)));
}

void WebRtcMediaStreamTrackAdapter::InitializeRemoteVideoTrack(
    const scoped_refptr<webrtc::VideoTrackInterface>& webrtc_video_track,
    ExecutionContext* track_execution_context) {
  DCHECK(!main_thread_->BelongsToCurrentThread());
  DCHECK(webrtc_video_track);
  DCHECK_EQ(webrtc_video_track->kind(),
            webrtc::MediaStreamTrackInterface::kVideoKind);
  remote_video_track_adapter_ =
      base::MakeRefCounted<blink::RemoteVideoTrackAdapter>(
          main_thread_, webrtc_video_track.get(), track_execution_context);
  webrtc_track_ = webrtc_video_track;
  remote_track_can_complete_initialization_.Signal();
  PostCrossThreadTask(
      *main_thread_.get(), FROM_HERE,
      CrossThreadBindOnce(&WebRtcMediaStreamTrackAdapter::
                              FinalizeRemoteTrackInitializationOnMainThread,
                          WrapRefCounted(this)));
}

void WebRtcMediaStreamTrackAdapter::
    FinalizeRemoteTrackInitializationOnMainThread() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  DCHECK(remote_audio_track_adapter_ || remote_video_track_adapter_);
  if (is_initialized_)
    return;

  if (remote_audio_track_adapter_) {
    remote_audio_track_adapter_->Initialize();
    component_ = remote_audio_track_adapter_->track();
  } else {
    remote_video_track_adapter_->Initialize();
    component_ = remote_video_track_adapter_->track();
  }

  // Initialize `webrtc_signaling_task_runner_` here instead of the ctor since
  // `GetWebRtcSignalingTaskRunner()` must be called on the main thread.
  auto factory = factory_.Lock();
  DCHECK(factory);
  webrtc_signaling_task_runner_ = factory->GetWebRtcSignalingTaskRunner();

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
  DCHECK_EQ(component_->GetSourceType(), MediaStreamSource::kTypeAudio);
  auto* audio_track = MediaStreamAudioTrack::From(component_);
  DCHECK(audio_track);
  audio_track->RemoveSink(local_track_audio_sink_.get());
  local_track_audio_sink_.reset();
  webrtc_track_ = nullptr;
  component_ = nullptr;
}

void WebRtcMediaStreamTrackAdapter::DisposeLocalVideoTrack() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  DCHECK(local_track_video_sink_);
  DCHECK_EQ(component_->GetSourceType(), MediaStreamSource::kTypeVideo);
  local_track_video_sink_.reset();
  webrtc_track_ = nullptr;
  component_ = nullptr;
}

void WebRtcMediaStreamTrackAdapter::DisposeRemoteAudioTrack() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  DCHECK(remote_audio_track_adapter_);
  DCHECK_EQ(component_->GetSourceType(), MediaStreamSource::kTypeAudio);

  DCHECK(webrtc_signaling_task_runner_);
  PostCrossThreadTask(
      *webrtc_signaling_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &WebRtcMediaStreamTrackAdapter::
              UnregisterRemoteAudioTrackAdapterOnSignalingThread,
          WrapRefCounted(this)));
}

void WebRtcMediaStreamTrackAdapter::DisposeRemoteVideoTrack() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  DCHECK(remote_video_track_adapter_);
  DCHECK_EQ(component_->GetSourceType(), MediaStreamSource::kTypeVideo);
  FinalizeRemoteTrackDisposingOnMainThread();
}

void WebRtcMediaStreamTrackAdapter::
    UnregisterRemoteAudioTrackAdapterOnSignalingThread() {
  DCHECK(!main_thread_->BelongsToCurrentThread());
  DCHECK(remote_audio_track_adapter_);
  remote_audio_track_adapter_->Unregister();
  PostCrossThreadTask(
      *main_thread_.get(), FROM_HERE,
      CrossThreadBindOnce(&WebRtcMediaStreamTrackAdapter::
                              FinalizeRemoteTrackDisposingOnMainThread,
                          WrapRefCounted(this)));
}

void WebRtcMediaStreamTrackAdapter::FinalizeRemoteTrackDisposingOnMainThread() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  DCHECK(is_disposed_);
  remote_audio_track_adapter_ = nullptr;
  remote_video_track_adapter_ = nullptr;
  webrtc_track_ = nullptr;
  component_ = nullptr;
}

}  // namespace blink
