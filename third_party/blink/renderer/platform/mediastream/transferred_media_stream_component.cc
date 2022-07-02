// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/transferred_media_stream_component.h"

#include "base/synchronization/lock.h"
#include "third_party/blink/public/platform/web_audio_source_provider.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"

namespace blink {

MediaStreamComponent* TransferredMediaStreamComponent::Clone(
    std::unique_ptr<MediaStreamTrackPlatform> cloned_platform_track) const {
  if (component_) {
    return component_->Clone(std::move(cloned_platform_track));
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
  // TODO(https://crbug.com/1288839): Return the transferred value.
  return "";
}

int TransferredMediaStreamComponent::UniqueId() const {
  if (component_) {
    return component_->UniqueId();
  }
  // TODO(crbug.com/1288839): Return the transferred value
  return 0;
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

bool TransferredMediaStreamComponent::Muted() const {
  if (component_) {
    return component_->Muted();
  }
  // TODO(https://crbug.com/1288839): Return the transferred value.
  return false;
}

void TransferredMediaStreamComponent::SetMuted(bool muted) {
  if (component_) {
    component_->SetMuted(muted);
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

const MediaConstraints& TransferredMediaStreamComponent::Constraints() const {
  if (component_) {
    return component_->Constraints();
  }
  // TODO(crbug.com/1288839): Return the transferred value
  static MediaConstraints media_constraints;
  return media_constraints;
}

void TransferredMediaStreamComponent::SetConstraints(
    const MediaConstraints& constraints) {
  if (component_) {
    component_->SetConstraints(constraints);
    return;
  }
  // TODO(https://crbug.com/1288839): Save and forward to component_ once it's
  // initialized.
}

AudioSourceProvider* TransferredMediaStreamComponent::GetAudioSourceProvider() {
  if (component_) {
    return component_->GetAudioSourceProvider();
  }
  // TODO(crbug.com/1288839): Remove
  // MediaStreamComponent::GetAudioSourceProvider() and this implementation +
  // fix call sites if feasible, otherwise return a proxy for
  // the AudioSourceProvider here
  return nullptr;
}

void TransferredMediaStreamComponent::SetSourceProvider(
    WebAudioSourceProvider* provider) {
  if (component_) {
    component_->SetSourceProvider(provider);
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

[[deprecated]] void TransferredMediaStreamComponent::SetPlatformTrack(
    std::unique_ptr<MediaStreamTrackPlatform> platform_track) {
  if (component_) {
    component_->SetPlatformTrack(std::move(platform_track));
    return;
  }
  // TODO(https://crbug.com/1288839): Save and forward to component_ once it's
  // initialized.
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

void TransferredMediaStreamComponent::SetCreationFrame(
    WebLocalFrame* creation_frame) {
  if (component_) {
    component_->SetCreationFrame(creation_frame);
    return;
  }
  // TODO(https://crbug.com/1288839): Save and forward to component_ once it's
  // initialized.
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
  MediaStreamComponent::Trace(visitor);
}

}  // namespace blink
