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

#include "third_party/blink/public/platform/web_media_stream.h"

#include <memory>
#include "third_party/blink/public/platform/web_media_stream_source.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

WebMediaStream::WebMediaStream(MediaStreamDescriptor* media_stream_descriptor)
    : private_(media_stream_descriptor) {}

void WebMediaStream::Reset() {
  private_.Reset();
}

WebString WebMediaStream::Id() const {
  return private_->Id();
}

int WebMediaStream::UniqueId() const {
  return private_->UniqueId();
}

WebVector<WebMediaStreamTrack> WebMediaStream::AudioTracks() const {
  size_t number_of_tracks = private_->NumberOfAudioComponents();
  WebVector<WebMediaStreamTrack> result(number_of_tracks);
  for (size_t i = 0; i < number_of_tracks; ++i)
    result[i] = private_->AudioComponent(i);
  return result;
}

WebVector<WebMediaStreamTrack> WebMediaStream::VideoTracks() const {
  size_t number_of_tracks = private_->NumberOfVideoComponents();
  WebVector<WebMediaStreamTrack> result(number_of_tracks);
  for (size_t i = 0; i < number_of_tracks; ++i)
    result[i] = private_->VideoComponent(i);
  return result;
}

WebMediaStreamTrack WebMediaStream::GetAudioTrack(
    const WebString& track_id) const {
  size_t number_of_tracks = private_->NumberOfAudioComponents();
  String id = track_id;
  for (size_t i = 0; i < number_of_tracks; ++i) {
    MediaStreamComponent* audio_component = private_->AudioComponent(i);
    DCHECK(audio_component);
    if (audio_component->Id() == id)
      return private_->AudioComponent(i);
  }
  return nullptr;
}

WebMediaStreamTrack WebMediaStream::GetVideoTrack(
    const WebString& track_id) const {
  size_t number_of_tracks = private_->NumberOfVideoComponents();
  String id = track_id;
  for (size_t i = 0; i < number_of_tracks; ++i) {
    MediaStreamComponent* video_component = private_->VideoComponent(i);
    DCHECK(video_component);
    if (video_component->Id() == id)
      return private_->VideoComponent(i);
  }
  return nullptr;
}

void WebMediaStream::AddTrack(const WebMediaStreamTrack& track) {
  DCHECK(!IsNull());
  private_->AddRemoteTrack(track);
}

void WebMediaStream::RemoveTrack(const WebMediaStreamTrack& track) {
  DCHECK(!IsNull());
  private_->RemoveRemoteTrack(track);
}

void WebMediaStream::AddObserver(WebMediaStreamObserver* observer) {
  DCHECK(!IsNull());
  private_->AddObserver(observer);
}

void WebMediaStream::RemoveObserver(WebMediaStreamObserver* observer) {
  DCHECK(!IsNull());
  private_->RemoveObserver(observer);
}

WebMediaStream& WebMediaStream::operator=(
    MediaStreamDescriptor* media_stream_descriptor) {
  private_ = media_stream_descriptor;
  return *this;
}

WebMediaStream::operator MediaStreamDescriptor*() const {
  return private_.Get();
}

void WebMediaStream::Initialize(
    const WebVector<WebMediaStreamTrack>& audio_tracks,
    const WebVector<WebMediaStreamTrack>& video_tracks) {
  Initialize(WTF::CreateCanonicalUUIDString(), audio_tracks, video_tracks);
}

void WebMediaStream::Initialize(
    const WebString& label,
    const WebVector<WebMediaStreamTrack>& audio_tracks,
    const WebVector<WebMediaStreamTrack>& video_tracks) {
  MediaStreamComponentVector audio, video;
  for (size_t i = 0; i < audio_tracks.size(); ++i) {
    MediaStreamComponent* component = audio_tracks[i];
    audio.push_back(component);
  }
  for (size_t i = 0; i < video_tracks.size(); ++i) {
    MediaStreamComponent* component = video_tracks[i];
    video.push_back(component);
  }
  private_ = MakeGarbageCollected<MediaStreamDescriptor>(label, audio, video);
}

void WebMediaStream::Assign(const WebMediaStream& other) {
  private_ = other.private_;
}

}  // namespace blink
