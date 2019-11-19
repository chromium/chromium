/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_SOURCE_H_

#include <memory>
#include <utility>

#include "base/optional.h"
#include "third_party/blink/public/platform/modules/mediastream/web_platform_media_stream_source.h"
#include "third_party/blink/public/platform/web_media_constraints.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/renderer/platform/audio/audio_destination_consumer.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class PLATFORM_EXPORT MediaStreamSource final
    : public GarbageCollected<MediaStreamSource> {
  USING_PRE_FINALIZER(MediaStreamSource, Dispose);

 public:
  class PLATFORM_EXPORT Observer : public GarbageCollectedMixin {
   public:
    virtual ~Observer() = default;
    virtual void SourceChangedState() = 0;
  };

  enum StreamType { kTypeAudio, kTypeVideo };

  enum ReadyState {
    kReadyStateLive = 0,
    kReadyStateMuted = 1,
    kReadyStateEnded = 2
  };

  enum class EchoCancellationMode { kDisabled, kBrowser, kAec3, kSystem };

  MediaStreamSource(const String& id,
                    StreamType,
                    const String& name,
                    bool remote,
                    ReadyState = kReadyStateLive,
                    bool requires_consumer = false);

  const String& Id() const { return id_; }
  StreamType GetType() const { return type_; }
  const String& GetName() const { return name_; }
  bool Remote() const { return remote_; }

  void SetGroupId(const String& group_id);
  const String& GroupId() { return group_id_; }

  void SetReadyState(ReadyState);
  ReadyState GetReadyState() const { return ready_state_; }

  void AddObserver(Observer*);

  WebPlatformMediaStreamSource* GetPlatformSource() const {
    return platform_source_.get();
  }
  void SetPlatformSource(
      std::unique_ptr<WebPlatformMediaStreamSource> platform_source) {
    platform_source_ = std::move(platform_source);
  }

  void SetAudioProcessingProperties(EchoCancellationMode echo_cancellation_mode,
                                    bool auto_gain_control,
                                    bool noise_supression);

  void GetSettings(WebMediaStreamTrack::Settings&);

  const WebMediaStreamSource::Capabilities& GetCapabilities() {
    return capabilities_;
  }
  void SetCapabilities(const WebMediaStreamSource::Capabilities& capabilities) {
    capabilities_ = capabilities;
  }

  void SetAudioFormat(size_t number_of_channels, float sample_rate);
  void ConsumeAudio(AudioBus*, size_t number_of_frames);

  bool RequiresAudioConsumer() const { return requires_consumer_; }
  void AddAudioConsumer(AudioDestinationConsumer*);
  bool RemoveAudioConsumer(AudioDestinationConsumer*);
  const HashSet<AudioDestinationConsumer*>& AudioConsumers() {
    return audio_consumers_;
  }

  void Trace(blink::Visitor*);

  void Dispose();

 private:
  String id_;
  StreamType type_;
  String name_;
  String group_id_;
  bool remote_;
  ReadyState ready_state_;
  bool requires_consumer_;
  HeapHashSet<WeakMember<Observer>> observers_;
  Mutex audio_consumers_lock_;
  HashSet<AudioDestinationConsumer*> audio_consumers_
      GUARDED_BY(audio_consumers_lock_);
  std::unique_ptr<WebPlatformMediaStreamSource> platform_source_;
  WebMediaConstraints constraints_;
  WebMediaStreamSource::Capabilities capabilities_;
  base::Optional<EchoCancellationMode> echo_cancellation_mode_;
  base::Optional<bool> auto_gain_control_;
  base::Optional<bool> noise_supression_;
};

typedef HeapVector<Member<MediaStreamSource>> MediaStreamSourceVector;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_SOURCE_H_
