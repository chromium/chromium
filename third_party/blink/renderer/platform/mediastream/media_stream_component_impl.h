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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_COMPONENT_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_COMPONENT_IMPL_H_

#include <memory>

#include "base/synchronization/lock.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_track.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/platform/audio/audio_source_provider.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/prefinalizer.h"
#include "third_party/blink/renderer/platform/mediastream/media_constraints.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_track_platform.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

namespace blink {

class MediaStreamSource;
class WebAudioSourceProvider;
class WebLocalFrame;

class PLATFORM_EXPORT MediaStreamComponentImpl final
    : public GarbageCollected<MediaStreamComponentImpl>,
      public MediaStreamComponent {
  USING_PRE_FINALIZER(MediaStreamComponentImpl, Dispose);

 private:
  static int GenerateUniqueId();

 public:
  MediaStreamComponentImpl(MediaStreamSource*);
  MediaStreamComponentImpl(const String& id, MediaStreamSource*);
  MediaStreamComponentImpl(const String& id,
                           MediaStreamSource*,
                           std::unique_ptr<MediaStreamTrackPlatform>);
  MediaStreamComponentImpl(MediaStreamSource*,
                           std::unique_ptr<MediaStreamTrackPlatform>);

  MediaStreamComponentImpl* Clone(
      std::unique_ptr<MediaStreamTrackPlatform> cloned_platform_track =
          nullptr) const override;

  // |m_trackData| may hold pointers to GC objects indirectly, and it may touch
  // eagerly finalized objects in destruction.
  // So this class runs pre-finalizer to finalize |m_trackData| promptly.
  void Dispose();

  MediaStreamSource* Source() const override { return source_.Get(); }

  String Id() const override { return id_; }
  int UniqueId() const override { return unique_id_; }
  bool Enabled() const override { return enabled_; }
  void SetEnabled(bool enabled) override { enabled_ = enabled; }
  bool Muted() const override { return muted_; }
  void SetMuted(bool muted) override { muted_ = muted; }
  WebMediaStreamTrack::ContentHintType ContentHint() override {
    return content_hint_;
  }
  void SetContentHint(WebMediaStreamTrack::ContentHintType) override;
  const MediaConstraints& Constraints() const override { return constraints_; }
  void SetConstraints(const MediaConstraints& constraints) override {
    constraints_ = constraints;
  }
  AudioSourceProvider* GetAudioSourceProvider() override {
    return &source_provider_;
  }
  void SetSourceProvider(WebAudioSourceProvider* provider) override {
    source_provider_.Wrap(provider);
  }

  MediaStreamTrackPlatform* GetPlatformTrack() const override {
    return platform_track_.get();
  }

  [[deprecated]] void SetPlatformTrack(
      std::unique_ptr<MediaStreamTrackPlatform> platform_track) override {
    platform_track_ = std::move(platform_track);
  }
  void GetSettings(MediaStreamTrackPlatform::Settings&) override;
  MediaStreamTrackPlatform::CaptureHandle GetCaptureHandle() override;

  WebLocalFrame* CreationFrame() override { return creation_frame_; }
  void SetCreationFrame(WebLocalFrame* creation_frame) override {
    creation_frame_ = creation_frame;
  }

  String ToString() const override;

  void Trace(Visitor*) const override;

 private:
  // AudioSourceProviderImpl wraps a WebAudioSourceProvider::provideInput()
  // calls into chromium to get a rendered audio stream.

  class PLATFORM_EXPORT AudioSourceProviderImpl final
      : public AudioSourceProvider {
   public:
    AudioSourceProviderImpl() : web_audio_source_provider_(nullptr) {}

    ~AudioSourceProviderImpl() override = default;

    // Wraps the given blink::WebAudioSourceProvider to
    // blink::AudioSourceProvider.
    void Wrap(WebAudioSourceProvider*);

    // blink::AudioSourceProvider
    void ProvideInput(AudioBus*, int frames_to_process) override;

   private:
    WebAudioSourceProvider* web_audio_source_provider_;
    base::Lock provide_input_lock_;

    // Used to wrap AudioBus to be passed into |web_audio_source_provider_|.
    WebVector<float*> web_audio_data_;
  };

  AudioSourceProviderImpl source_provider_;
  Member<MediaStreamSource> source_;

  const String id_;
  const int unique_id_;
  bool enabled_ = true;
  bool muted_ = false;
  WebMediaStreamTrack::ContentHintType content_hint_ =
      WebMediaStreamTrack::ContentHintType::kNone;
  MediaConstraints constraints_;
  std::unique_ptr<MediaStreamTrackPlatform> platform_track_;
  // Frame where the referenced platform track was created, if applicable.
  WebLocalFrame* creation_frame_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_COMPONENT_IMPL_H_
