/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/platform/web_media_stream_track.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/public/platform/web_audio_source_provider.h"
#include "third_party/blink/public/platform/web_media_constraints.h"
#include "third_party/blink/public/platform/web_media_stream.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"

namespace blink {

const char WebMediaStreamTrack::kResizeModeNone[] = "none";
const char WebMediaStreamTrack::kResizeModeRescale[] = "crop-and-scale";

WebMediaStreamTrack::WebMediaStreamTrack(
    MediaStreamComponent* media_stream_component)
    : private_(media_stream_component) {}

WebMediaStreamTrack& WebMediaStreamTrack::operator=(
    MediaStreamComponent* media_stream_component) {
  private_ = media_stream_component;
  return *this;
}

void WebMediaStreamTrack::Initialize(const WebMediaStreamSource& source) {
  private_ = MakeGarbageCollected<MediaStreamComponent>(source);
}

void WebMediaStreamTrack::Initialize(const WebString& id,
                                     const WebMediaStreamSource& source) {
  private_ = MakeGarbageCollected<MediaStreamComponent>(id, source);
}

void WebMediaStreamTrack::Reset() {
  private_.Reset();
}

WebMediaStreamTrack::operator MediaStreamComponent*() const {
  return private_.Get();
}

bool WebMediaStreamTrack::IsEnabled() const {
  DCHECK(!private_.IsNull());
  return private_->Enabled();
}

bool WebMediaStreamTrack::IsMuted() const {
  DCHECK(!private_.IsNull());
  return private_->Muted();
}

WebMediaStreamTrack::ContentHintType WebMediaStreamTrack::ContentHint() const {
  DCHECK(!private_.IsNull());
  return private_->ContentHint();
}

WebMediaConstraints WebMediaStreamTrack::Constraints() const {
  DCHECK(!private_.IsNull());
  return private_->Constraints();
}

void WebMediaStreamTrack::SetConstraints(
    const WebMediaConstraints& constraints) {
  DCHECK(!private_.IsNull());
  return private_->SetConstraints(constraints);
}

WebString WebMediaStreamTrack::Id() const {
  DCHECK(!private_.IsNull());
  return private_->Id();
}

int WebMediaStreamTrack::UniqueId() const {
  DCHECK(!private_.IsNull());
  return private_->UniqueId();
}

WebMediaStreamSource WebMediaStreamTrack::Source() const {
  DCHECK(!private_.IsNull());
  return WebMediaStreamSource(private_->Source());
}

WebPlatformMediaStreamTrack* WebMediaStreamTrack::GetPlatformTrack() const {
  return private_->GetPlatformTrack();
}

void WebMediaStreamTrack::SetPlatformTrack(
    std::unique_ptr<WebPlatformMediaStreamTrack> platform_track) {
  DCHECK(!private_.IsNull());
  private_->SetPlatformTrack(std::move(platform_track));
}

void WebMediaStreamTrack::SetSourceProvider(WebAudioSourceProvider* provider) {
  DCHECK(!private_.IsNull());
  private_->SetSourceProvider(provider);
}

void WebMediaStreamTrack::Assign(const WebMediaStreamTrack& other) {
  private_ = other.private_;
}

}  // namespace blink
