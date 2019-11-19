/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"

#include "third_party/blink/public/platform/web_audio_source_provider.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"

namespace blink {

namespace {

static int g_unique_media_stream_component_id = 0;

}  // namespace

// static
int MediaStreamComponent::GenerateUniqueId() {
  return ++g_unique_media_stream_component_id;
}

MediaStreamComponent::MediaStreamComponent(MediaStreamSource* source)
    : MediaStreamComponent(WTF::CreateCanonicalUUIDString(), source) {}
MediaStreamComponent::MediaStreamComponent(const String& id,
                                           MediaStreamSource* source)
    : source_(source), id_(id), unique_id_(GenerateUniqueId()) {
  DCHECK(id_.length());
  DCHECK(source_);
}

MediaStreamComponent* MediaStreamComponent::Clone() const {
  auto* cloned_component = MakeGarbageCollected<MediaStreamComponent>(Source());
  cloned_component->SetEnabled(enabled_);
  cloned_component->SetMuted(muted_);
  cloned_component->SetContentHint(content_hint_);
  cloned_component->SetConstraints(constraints_);
  return cloned_component;
}

void MediaStreamComponent::Dispose() {
  platform_track_.reset();
}

void MediaStreamComponent::AudioSourceProviderImpl::Wrap(
    WebAudioSourceProvider* provider) {
  MutexLocker locker(provide_input_lock_);
  web_audio_source_provider_ = provider;
}

void MediaStreamComponent::GetSettings(
    WebMediaStreamTrack::Settings& settings) {
  DCHECK(platform_track_);
  source_->GetSettings(settings);
  platform_track_->GetSettings(settings);
}

void MediaStreamComponent::SetContentHint(
    WebMediaStreamTrack::ContentHintType hint) {
  switch (hint) {
    case WebMediaStreamTrack::ContentHintType::kNone:
      break;
    case WebMediaStreamTrack::ContentHintType::kAudioSpeech:
    case WebMediaStreamTrack::ContentHintType::kAudioMusic:
      DCHECK_EQ(MediaStreamSource::kTypeAudio, Source()->GetType());
      break;
    case WebMediaStreamTrack::ContentHintType::kVideoMotion:
    case WebMediaStreamTrack::ContentHintType::kVideoDetail:
    case WebMediaStreamTrack::ContentHintType::kVideoText:
      DCHECK_EQ(MediaStreamSource::kTypeVideo, Source()->GetType());
      break;
  }
  if (hint == content_hint_)
    return;
  content_hint_ = hint;

  WebPlatformMediaStreamTrack* native_track = GetPlatformTrack();
  if (native_track)
    native_track->SetContentHint(ContentHint());
}

void MediaStreamComponent::AudioSourceProviderImpl::ProvideInput(
    AudioBus* bus,
    uint32_t frames_to_process) {
  DCHECK(bus);
  if (!bus)
    return;

  MutexTryLocker try_locker(provide_input_lock_);
  if (!try_locker.Locked() || !web_audio_source_provider_) {
    bus->Zero();
    return;
  }

  // Wrap the AudioBus channel data using WebVector.
  uint32_t n = bus->NumberOfChannels();
  WebVector<float*> web_audio_data(n);
  for (uint32_t i = 0; i < n; ++i)
    web_audio_data[i] = bus->Channel(i)->MutableData();

  web_audio_source_provider_->ProvideInput(web_audio_data, frames_to_process);
}

void MediaStreamComponent::Trace(blink::Visitor* visitor) {
  visitor->Trace(source_);
}

}  // namespace blink
