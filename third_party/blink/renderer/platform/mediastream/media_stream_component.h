/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_COMPONENT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_COMPONENT_H_

#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_track.h"
#include "third_party/blink/renderer/platform/audio/audio_source_provider.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/prefinalizer.h"
#include "third_party/blink/renderer/platform/mediastream/media_constraints.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_track_platform.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class MediaStreamSource;
class WebAudioSourceProvider;
class WebLocalFrame;

// A MediaStreamComponent is a MediaStreamTrack.
// TODO(hta): Consider merging the two classes.

class PLATFORM_EXPORT MediaStreamComponent : public GarbageCollectedMixin {
 public:
  virtual MediaStreamComponent* Clone(
      std::unique_ptr<MediaStreamTrackPlatform> cloned_platform_track =
          nullptr) const = 0;

  virtual MediaStreamSource* Source() const = 0;

  // This is the same as the id of the |MediaStreamTrack|. It is unique in most
  // contexts but collisions can occur e.g. if tracks are created by different
  // |RTCPeerConnection|s or a remote track ID is signaled to be added, removed
  // and then re-added resulting in a new track object the second time around.
  virtual String Id() const = 0;
  // Uniquely identifies this component.
  virtual int UniqueId() const = 0;
  virtual bool Enabled() const = 0;
  virtual void SetEnabled(bool enabled) = 0;
  virtual bool Muted() const = 0;
  virtual void SetMuted(bool muted) = 0;
  virtual WebMediaStreamTrack::ContentHintType ContentHint() = 0;
  virtual void SetContentHint(WebMediaStreamTrack::ContentHintType) = 0;
  virtual const MediaConstraints& Constraints() const = 0;
  virtual void SetConstraints(const MediaConstraints& constraints) = 0;
  virtual AudioSourceProvider* GetAudioSourceProvider() = 0;
  virtual void SetSourceProvider(WebAudioSourceProvider* provider) = 0;

  virtual MediaStreamTrackPlatform* GetPlatformTrack() const = 0;

  // Deprecated - use the MediaStreamComponentImpl constructor which takes a
  // MediaStreamTrackPlatform instead.
  // TODO(crbug.com/1302689): Remove once all callers have been migrated.
  [[deprecated]] virtual void SetPlatformTrack(
      std::unique_ptr<MediaStreamTrackPlatform> platform_track) = 0;
  virtual void GetSettings(MediaStreamTrackPlatform::Settings&) = 0;
  virtual MediaStreamTrackPlatform::CaptureHandle GetCaptureHandle() = 0;

  virtual WebLocalFrame* CreationFrame() = 0;
  virtual void SetCreationFrame(WebLocalFrame* creation_frame) = 0;

  virtual String ToString() const = 0;
};

class PLATFORM_EXPORT MediaStreamComponents final
    : public GarbageCollected<MediaStreamComponents> {
 public:
  // At least one of audio_track / video_track must be non-null.
  MediaStreamComponents(MediaStreamComponent* audio_track,
                        MediaStreamComponent* video_track);

  void Trace(Visitor*) const;

  Member<MediaStreamComponent> audio_track_;
  Member<MediaStreamComponent> video_track_;
};

using MediaStreamComponentVector = HeapVector<Member<MediaStreamComponent>>;
using MediaStreamsComponentsVector = HeapVector<Member<MediaStreamComponents>>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_COMPONENT_H_
