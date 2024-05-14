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

#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"

#include "base/synchronization/lock.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_audio_sink.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"

namespace blink {

namespace {

static int g_unique_media_stream_component_id = 0;

void CheckSourceAndTrackSameType(
    const MediaStreamSource* source,
    const MediaStreamTrackPlatform* platform_track) {
  // Ensure the source and platform_track have the same types.
  switch (source->GetType()) {
    case MediaStreamSource::kTypeAudio:
      CHECK(platform_track->Type() ==
            MediaStreamTrackPlatform::StreamType::kAudio);
      return;
    case MediaStreamSource::kTypeVideo:
      CHECK(platform_track->Type() ==
            MediaStreamTrackPlatform::StreamType::kVideo);
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

}  // namespace

// static
int MediaStreamComponentImpl::GenerateUniqueId() {
  return ++g_unique_media_stream_component_id;
}

MediaStreamComponentImpl::MediaStreamComponentImpl(
    const String& id,
    MediaStreamSource* source,
    std::unique_ptr<MediaStreamTrackPlatform> platform_track)
    : source_(source),
      id_(id),
      unique_id_(GenerateUniqueId()),
      platform_track_(std::move(platform_track)) {
  DCHECK(platform_track_);
  CheckSourceAndTrackSameType(source, platform_track_.get());
}

MediaStreamComponentImpl::MediaStreamComponentImpl(
    MediaStreamSource* source,
    std::unique_ptr<MediaStreamTrackPlatform> platform_track)
    : MediaStreamComponentImpl(WTF::CreateCanonicalUUIDString(),
                               source,
                               std::move(platform_track)) {}

MediaStreamComponentImpl* MediaStreamComponentImpl::Clone() const {
  const String id = WTF::CreateCanonicalUUIDString();
  std::unique_ptr<MediaStreamTrackPlatform> cloned_platform_track =
      platform_track_->CreateFromComponent(this, id);
  auto* cloned_component = MakeGarbageCollected<MediaStreamComponentImpl>(
      id, Source(), std::move(cloned_platform_track));
  cloned_component->SetEnabled(enabled_);
  cloned_component->SetContentHint(content_hint_);
  return cloned_component;
}

void MediaStreamComponentImpl::Dispose() {
  platform_track_.reset();
}

void MediaStreamComponentImpl::GetSettings(
    MediaStreamTrackPlatform::Settings& settings) {
  DCHECK(platform_track_);
  source_->GetSettings(settings);
  platform_track_->GetSettings(settings);
}

MediaStreamTrackPlatform::CaptureHandle
MediaStreamComponentImpl::GetCaptureHandle() {
  DCHECK(platform_track_);
  return platform_track_->GetCaptureHandle();
}

void MediaStreamComponentImpl::SetEnabled(bool enabled) {
  enabled_ = enabled;
  // TODO(https://crbug.com/1302689): Change to a DCHECK(platform_track) once
  // the platform_track is always set in the constructor.
  if (platform_track_) {
    platform_track_->SetEnabled(enabled_);
  }
}

void MediaStreamComponentImpl::SetContentHint(
    WebMediaStreamTrack::ContentHintType hint) {
  switch (hint) {
    case WebMediaStreamTrack::ContentHintType::kNone:
      break;
    case WebMediaStreamTrack::ContentHintType::kAudioSpeech:
    case WebMediaStreamTrack::ContentHintType::kAudioMusic:
      DCHECK_EQ(MediaStreamSource::kTypeAudio, GetSourceType());
      break;
    case WebMediaStreamTrack::ContentHintType::kVideoMotion:
    case WebMediaStreamTrack::ContentHintType::kVideoDetail:
    case WebMediaStreamTrack::ContentHintType::kVideoText:
      DCHECK_EQ(MediaStreamSource::kTypeVideo, GetSourceType());
      break;
  }
  if (hint == content_hint_)
    return;
  content_hint_ = hint;

  MediaStreamTrackPlatform* native_track = GetPlatformTrack();
  if (native_track)
    native_track->SetContentHint(ContentHint());
}

void MediaStreamComponentImpl::AddSourceObserver(
    MediaStreamSource::Observer* observer) {
  Source()->AddObserver(observer);
}

void MediaStreamComponentImpl::AddSink(WebMediaStreamAudioSink* sink) {
  DCHECK(GetPlatformTrack());
  GetPlatformTrack()->AddSink(sink);
}

void MediaStreamComponentImpl::AddSink(
    WebMediaStreamSink* sink,
    const VideoCaptureDeliverFrameCB& callback,
    MediaStreamVideoSink::IsSecure is_secure,
    MediaStreamVideoSink::UsesAlpha uses_alpha) {
  DCHECK(GetPlatformTrack());
  GetPlatformTrack()->AddSink(sink, callback, is_secure, uses_alpha);
}

String MediaStreamComponentImpl::ToString() const {
  return String::Format("[id: %s, unique_id: %d, enabled: %s]",
                        Id().Utf8().c_str(), UniqueId(),
                        Enabled() ? "true" : "false");
}

void MediaStreamComponentImpl::Trace(Visitor* visitor) const {
  visitor->Trace(source_);
  MediaStreamComponent::Trace(visitor);
}

}  // namespace blink
