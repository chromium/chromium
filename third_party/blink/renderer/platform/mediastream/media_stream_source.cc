/*
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
 * 3. Neither the name of Google Inc. nor the names of its contributors
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

#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"

#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

namespace {

void GetSourceSettings(const blink::WebMediaStreamSource& web_source,
                       blink::WebMediaStreamTrack::Settings& settings) {
  blink::MediaStreamAudioSource* const source =
      blink::MediaStreamAudioSource::From(web_source);
  if (!source)
    return;

  media::AudioParameters audio_parameters = source->GetAudioParameters();
  if (audio_parameters.IsValid()) {
    settings.sample_rate = audio_parameters.sample_rate();
    settings.channel_count = audio_parameters.channels();
    settings.latency = audio_parameters.GetBufferDuration().InSecondsF();
  }
  // kSampleFormatS16 is the format used for all audio input streams.
  settings.sample_size =
      media::SampleFormatToBitsPerChannel(media::kSampleFormatS16);
}

}  // namespace

MediaStreamSource::MediaStreamSource(const String& id,
                                     StreamType type,
                                     const String& name,
                                     bool remote,
                                     ReadyState ready_state,
                                     bool requires_consumer)
    : id_(id),
      type_(type),
      name_(name),
      remote_(remote),
      ready_state_(ready_state),
      requires_consumer_(requires_consumer) {}

void MediaStreamSource::SetGroupId(const String& group_id) {
  group_id_ = group_id;
}

void MediaStreamSource::SetReadyState(ReadyState ready_state) {
  if (ready_state_ != kReadyStateEnded && ready_state_ != ready_state) {
    ready_state_ = ready_state;

    // Observers may dispatch events which create and add new Observers;
    // take a snapshot so as to safely iterate.
    HeapVector<Member<Observer>> observers;
    CopyToVector(observers_, observers);
    for (auto observer : observers)
      observer->SourceChangedState();

    // setReadyState() will be invoked via the MediaStreamComponent::dispose()
    // prefinalizer, allocating |observers|. Which means that |observers| will
    // live until the next GC (but be unreferenced by other heap objects),
    // _but_ it will potentially contain references to Observers that were
    // GCed after the MediaStreamComponent prefinalizer had completed.
    //
    // So, if the next GC is a conservative one _and_ it happens to find
    // a reference to |observers| when scanning the stack, we're in trouble
    // as it contains references to now-dead objects.
    //
    // Work around this by explicitly clearing the vector backing store.
    //
    // TODO(sof): consider adding run-time checks that disallows this kind
    // of dead object revivification by default.
    for (wtf_size_t i = 0; i < observers.size(); ++i)
      observers[i] = nullptr;
  }
}

void MediaStreamSource::AddObserver(MediaStreamSource::Observer* observer) {
  observers_.insert(observer);
}

void MediaStreamSource::SetAudioProcessingProperties(
    EchoCancellationMode echo_cancellation_mode,
    bool auto_gain_control,
    bool noise_supression) {
  echo_cancellation_mode_ = echo_cancellation_mode;
  auto_gain_control_ = auto_gain_control;
  noise_supression_ = noise_supression;
}

void MediaStreamSource::AddAudioConsumer(AudioDestinationConsumer* consumer) {
  DCHECK(requires_consumer_);
  MutexLocker locker(audio_consumers_lock_);
  audio_consumers_.insert(consumer);
}

bool MediaStreamSource::RemoveAudioConsumer(
    AudioDestinationConsumer* consumer) {
  DCHECK(requires_consumer_);
  MutexLocker locker(audio_consumers_lock_);
  auto it = audio_consumers_.find(consumer);
  if (it == audio_consumers_.end())
    return false;
  audio_consumers_.erase(it);
  return true;
}

void MediaStreamSource::GetSettings(WebMediaStreamTrack::Settings& settings) {
  settings.device_id = Id();
  settings.group_id = GroupId();

  if (echo_cancellation_mode_) {
    switch (*echo_cancellation_mode_) {
      case EchoCancellationMode::kDisabled:
        settings.echo_cancellation = false;
        settings.echo_cancellation_type.Reset();
        break;
      case EchoCancellationMode::kBrowser:
        settings.echo_cancellation = true;
        settings.echo_cancellation_type =
            WebString::FromASCII(blink::kEchoCancellationTypeBrowser);
        break;
      case EchoCancellationMode::kAec3:
        settings.echo_cancellation = true;
        settings.echo_cancellation_type =
            WebString::FromASCII(blink::kEchoCancellationTypeAec3);
        break;
      case EchoCancellationMode::kSystem:
        settings.echo_cancellation = true;
        settings.echo_cancellation_type =
            WebString::FromASCII(blink::kEchoCancellationTypeSystem);
        break;
    }
  }
  if (auto_gain_control_)
    settings.auto_gain_control = *auto_gain_control_;
  if (noise_supression_)
    settings.noise_supression = *noise_supression_;

  GetSourceSettings(this, settings);
}

void MediaStreamSource::SetAudioFormat(size_t number_of_channels,
                                       float sample_rate) {
  DCHECK(requires_consumer_);
  MutexLocker locker(audio_consumers_lock_);
  for (AudioDestinationConsumer* consumer : audio_consumers_)
    consumer->SetFormat(number_of_channels, sample_rate);
}

void MediaStreamSource::ConsumeAudio(AudioBus* bus, size_t number_of_frames) {
  DCHECK(requires_consumer_);
  MutexLocker locker(audio_consumers_lock_);
  for (AudioDestinationConsumer* consumer : audio_consumers_)
    consumer->ConsumeAudio(bus, number_of_frames);
}

void MediaStreamSource::Trace(blink::Visitor* visitor) {
  visitor->Trace(observers_);
}

void MediaStreamSource::Dispose() {
  {
    MutexLocker locker(audio_consumers_lock_);
    audio_consumers_.clear();
  }
  platform_source_.reset();
  constraints_.Reset();
}

STATIC_ASSERT_ENUM(WebMediaStreamSource::kTypeAudio,
                   MediaStreamSource::kTypeAudio);
STATIC_ASSERT_ENUM(WebMediaStreamSource::kTypeVideo,
                   MediaStreamSource::kTypeVideo);
STATIC_ASSERT_ENUM(WebMediaStreamSource::kReadyStateLive,
                   MediaStreamSource::kReadyStateLive);
STATIC_ASSERT_ENUM(WebMediaStreamSource::kReadyStateMuted,
                   MediaStreamSource::kReadyStateMuted);
STATIC_ASSERT_ENUM(WebMediaStreamSource::kReadyStateEnded,
                   MediaStreamSource::kReadyStateEnded);
STATIC_ASSERT_ENUM(WebMediaStreamSource::EchoCancellationMode::kDisabled,
                   MediaStreamSource::EchoCancellationMode::kDisabled);
STATIC_ASSERT_ENUM(WebMediaStreamSource::EchoCancellationMode::kBrowser,
                   MediaStreamSource::EchoCancellationMode::kBrowser);
STATIC_ASSERT_ENUM(WebMediaStreamSource::EchoCancellationMode::kAec3,
                   MediaStreamSource::EchoCancellationMode::kAec3);
STATIC_ASSERT_ENUM(WebMediaStreamSource::EchoCancellationMode::kSystem,
                   MediaStreamSource::EchoCancellationMode::kSystem);

}  // namespace blink
