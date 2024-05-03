// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/transferred_media_stream_component.h"

#include "base/synchronization/lock.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_audio_sink.h"
#include "third_party/blink/public/platform/web_audio_source_provider.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"

namespace blink {

TransferredMediaStreamComponent::TransferredMediaStreamComponent(
    const TransferredValues& data)
    : data_(data) {}

void TransferredMediaStreamComponent::SetImplementation(
    MediaStreamComponent* component) {
  MediaStreamTrackPlatform::CaptureHandle old_capture_handle =
      GetCaptureHandle();
  MediaStreamSource::ReadyState old_ready_state = GetReadyState();

  component_ = component;

  // Observers may dispatch events which create and add new Observers. Such
  // observers are added directly to the implementation since component_ is
  // now set.
  bool capture_handle_changed =
      old_capture_handle.origin != GetCaptureHandle().origin ||
      old_capture_handle.handle != GetCaptureHandle().handle;
  for (MediaStreamSource::Observer* observer : observers_) {
    if (capture_handle_changed) {
      observer->SourceChangedCaptureHandle();
    }
    if (old_ready_state != GetReadyState()) {
      observer->SourceChangedState();
    }
    component->AddSourceObserver(observer);
  }
  observers_.clear();

  for (const auto& call : add_video_sink_calls_) {
    component_->AddSink(call.sink, call.callback, call.is_secure,
                        call.uses_alpha);
  }
  add_video_sink_calls_.clear();

  for (auto* call : add_audio_sink_calls_) {
    component_->AddSink(call);
  }
  add_audio_sink_calls_.clear();
}

MediaStreamComponent* TransferredMediaStreamComponent::Clone() const {
  if (component_) {
    return component_->Clone();
  }
  // TODO(crbug.com/1288839): Implement Clone() for when component_ is not set
  return nullptr;
}

MediaStreamSource* TransferredMediaStreamComponent::Source() const {
  if (component_) {
    return component_->Source();
  }
  // TODO(crbug.com/1288839): Remove MediaStreamComponent::Source() and this
  // implementation + fix call sites if feasible, otherwise return a proxy for
  // the source here
  return nullptr;
}

String TransferredMediaStreamComponent::Id() const {
  if (component_) {
    return component_->Id();
  }
  return data_.id;
}

int TransferredMediaStreamComponent::UniqueId() const {
  if (component_) {
    return component_->UniqueId();
  }
  // TODO(crbug.com/1288839): Return the transferred value
  return 0;
}

MediaStreamSource::StreamType TransferredMediaStreamComponent::GetSourceType()
    const {
  if (component_) {
    return component_->GetSourceType();
  }
  // TODO(crbug.com/1288839): Return the transferred value
  return MediaStreamSource::StreamType::kTypeVideo;
}
const String& TransferredMediaStreamComponent::GetSourceName() const {
  if (component_) {
    return component_->GetSourceName();
  }
  // TODO(crbug.com/1288839): Return the transferred value
  return g_empty_string;
}

MediaStreamSource::ReadyState TransferredMediaStreamComponent::GetReadyState()
    const {
  if (component_) {
    return component_->GetReadyState();
  }
  // TODO(crbug.com/1288839): Return the transferred value
  return MediaStreamSource::ReadyState::kReadyStateLive;
}

bool TransferredMediaStreamComponent::Remote() const {
  if (component_) {
    return component_->Remote();
  }
  // TODO(crbug.com/1288839): Return the transferred value
  return false;
}

bool TransferredMediaStreamComponent::Enabled() const {
  if (component_) {
    return component_->Enabled();
  }
  // TODO(https://crbug.com/1288839): Return the transferred value.
  return true;
}

void TransferredMediaStreamComponent::SetEnabled(bool enabled) {
  if (component_) {
    component_->SetEnabled(enabled);
    return;
  }
  // TODO(https://crbug.com/1288839): Save and forward to component_ once it's
  // initialized.
}

WebMediaStreamTrack::ContentHintType
TransferredMediaStreamComponent::ContentHint() {
  if (component_) {
    return component_->ContentHint();
  }
  // TODO(https://crbug.com/1288839): Return the transferred value.
  return WebMediaStreamTrack::ContentHintType::kNone;
}

void TransferredMediaStreamComponent::SetContentHint(
    WebMediaStreamTrack::ContentHintType hint) {
  if (component_) {
    component_->SetContentHint(hint);
    return;
  }
  // TODO(https://crbug.com/1288839): Save and forward to component_ once it's
  // initialized.
}

MediaStreamTrackPlatform* TransferredMediaStreamComponent::GetPlatformTrack()
    const {
  if (component_) {
    return component_->GetPlatformTrack();
  }
  // TODO(crbug.com/1288839): Remove MediaStreamComponent::GetPlatformTrack()
  // and this implementation if possible, otherwise return a proxy for the
  // track here
  return nullptr;
}

void TransferredMediaStreamComponent::GetSettings(
    MediaStreamTrackPlatform::Settings& settings) {
  if (component_) {
    component_->GetSettings(settings);
    return;
  }
  // TODO(crbug.com/1288839): Return the transferred value
}

MediaStreamTrackPlatform::CaptureHandle
TransferredMediaStreamComponent::GetCaptureHandle() {
  if (component_) {
    return component_->GetCaptureHandle();
  }
  // TODO(crbug.com/1288839): Return the transferred value
  return MediaStreamTrackPlatform::CaptureHandle();
}

WebLocalFrame* TransferredMediaStreamComponent::CreationFrame() {
  if (component_) {
    return component_->CreationFrame();
  }
  // TODO(crbug.com/1288839): Remove MediaStreamComponent::GetPlatformTrack()
  // and this implementation + fix call sites if feasible, otherwise return a
  // proxy for the track here
  return nullptr;
}

void TransferredMediaStreamComponent::SetCreationFrameGetter(
    base::RepeatingCallback<WebLocalFrame*()> creation_frame_getter) {
  if (component_) {
    component_->SetCreationFrameGetter(std::move(creation_frame_getter));
    return;
  }
  // TODO(https://crbug.com/1288839): Save and forward to component_ once it's
  // initialized.
}

void TransferredMediaStreamComponent::AddSourceObserver(
    MediaStreamSource::Observer* observer) {
  if (component_) {
    component_->AddSourceObserver(observer);
  } else {
    observers_.push_back(observer);
  }
}

void TransferredMediaStreamComponent::AddSink(
    WebMediaStreamSink* sink,
    const VideoCaptureDeliverFrameCB& callback,
    MediaStreamVideoSink::IsSecure is_secure,
    MediaStreamVideoSink::UsesAlpha uses_alpha) {
  DCHECK_EQ(MediaStreamSource::kTypeVideo, GetSourceType());
  if (component_) {
    component_->AddSink(sink, callback, is_secure, uses_alpha);
    return;
  }
  add_video_sink_calls_.emplace_back(
      AddSinkArgs{sink, std::move(callback), is_secure, uses_alpha});
}

void TransferredMediaStreamComponent::AddSink(WebMediaStreamAudioSink* sink) {
  DCHECK_EQ(MediaStreamSource::kTypeAudio, GetSourceType());
  if (component_) {
    component_->AddSink(sink);
    return;
  }
  add_audio_sink_calls_.emplace_back(sink);
}

String TransferredMediaStreamComponent::ToString() const {
  if (component_) {
    return component_->ToString();
  }
  // TODO(crbug.com/1288839): Return string formatted like
  // MediaStreamComponentImpl::ToString() with transferred values
  return "[]";
}

void TransferredMediaStreamComponent::Trace(Visitor* visitor) const {
  visitor->Trace(component_);
  visitor->Trace(observers_);
  MediaStreamComponent::Trace(visitor);
}

}  // namespace blink
