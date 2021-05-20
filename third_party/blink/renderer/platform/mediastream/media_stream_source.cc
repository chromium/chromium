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

#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/webaudio_destination_consumer.h"

namespace blink {

namespace {

void SendLogMessage(const std::string& message) {
  blink::WebRtcLogMessage("MSS::" + message);
}

const char* StreamTypeToString(MediaStreamSource::StreamType type) {
  switch (type) {
    case MediaStreamSource::kTypeAudio:
      return "Audio";
    case MediaStreamSource::kTypeVideo:
      return "Video";
    default:
      NOTREACHED();
  }
  return "Invalid";
}

const char* ReadyStateToString(MediaStreamSource::ReadyState state) {
  switch (state) {
    case MediaStreamSource::kReadyStateLive:
      return "Live";
    case MediaStreamSource::kReadyStateMuted:
      return "Muted";
    case MediaStreamSource::kReadyStateEnded:
      return "Ended";
    default:
      NOTREACHED();
  }
  return "Invalid";
}

const char* EchoCancellationModeToString(
    MediaStreamSource::EchoCancellationMode mode) {
  switch (mode) {
    case MediaStreamSource::EchoCancellationMode::kDisabled:
      return "disabled";
    case MediaStreamSource::EchoCancellationMode::kBrowser:
      return "browser";
    case MediaStreamSource::EchoCancellationMode::kAec3:
      return "AEC3";
    case MediaStreamSource::EchoCancellationMode::kSystem:
      return "system";
  }
}

void GetSourceSettings(const blink::WebMediaStreamSource& web_source,
                       MediaStreamTrackPlatform::Settings& settings) {
  auto* const source = blink::MediaStreamAudioSource::From(web_source);
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

class ConsumerWrapper final : public AudioDestinationConsumer {
  USING_FAST_MALLOC(ConsumerWrapper);

 public:
  static ConsumerWrapper* Create(WebAudioDestinationConsumer* consumer) {
    return new ConsumerWrapper(consumer);
  }

  void SetFormat(size_t number_of_channels, float sample_rate) override;
  void ConsumeAudio(AudioBus*, size_t number_of_frames) override;

  WebAudioDestinationConsumer* Consumer() { return consumer_; }

 private:
  explicit ConsumerWrapper(WebAudioDestinationConsumer* consumer)
      : consumer_(consumer) {
    // To avoid reallocation in ConsumeAudio, reserve initial capacity for most
    // common known layouts.
    bus_vector_.ReserveInitialCapacity(8);
  }

  // m_consumer is not owned by this class.
  WebAudioDestinationConsumer* consumer_;
  // bus_vector_ must only be used in ConsumeAudio. The only reason it's a
  // member variable is to not have to reallocate it for each call.
  Vector<const float*> bus_vector_;
};

void ConsumerWrapper::SetFormat(size_t number_of_channels, float sample_rate) {
  consumer_->SetFormat(number_of_channels, sample_rate);
}

void ConsumerWrapper::ConsumeAudio(AudioBus* bus, size_t number_of_frames) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("mediastream"),
               "ConsumerWrapper::ConsumeAudio");

  if (!bus)
    return;

  // Wrap AudioBus.
  size_t number_of_channels = bus->NumberOfChannels();
  if (bus_vector_.size() != number_of_channels) {
    bus_vector_.resize(number_of_channels);
  }
  for (size_t i = 0; i < number_of_channels; ++i)
    bus_vector_[i] = bus->Channel(i)->Data();

  consumer_->ConsumeAudio(bus_vector_, number_of_frames);
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
      requires_consumer_(requires_consumer) {
  SendLogMessage(
      String::Format(
          "MediaStreamSource({id=%s}, {type=%s}, {name=%s}, {remote=%d}, "
          "{ready_state=%s})",
          id.Utf8().c_str(), StreamTypeToString(type), name.Utf8().c_str(),
          remote, ReadyStateToString(ready_state))
          .Utf8());
}

void MediaStreamSource::SetGroupId(const String& group_id) {
  SendLogMessage(
      String::Format("SetGroupId({group_id=%s})", group_id.Utf8().c_str())
          .Utf8());
  group_id_ = group_id;
}

void MediaStreamSource::SetReadyState(ReadyState ready_state) {
  SendLogMessage(String::Format("SetReadyState({id=%s}, {ready_state=%s})",
                                Id().Utf8().c_str(),
                                ReadyStateToString(ready_state))
                     .Utf8());
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

void MediaStreamSource::SetPlatformSource(
    std::unique_ptr<WebPlatformMediaStreamSource> platform_source) {
  if (platform_source)
    platform_source->SetOwner(this);

  platform_source_ = std::move(platform_source);
}

void MediaStreamSource::SetAudioProcessingProperties(
    EchoCancellationMode echo_cancellation_mode,
    bool auto_gain_control,
    bool noise_supression) {
  SendLogMessage(
      String::Format("%s({echo_cancellation_mode=%s}, {auto_gain_control=%d}, "
                     "{noise_supression=%d})",
                     __func__,
                     EchoCancellationModeToString(echo_cancellation_mode),
                     auto_gain_control, noise_supression)
          .Utf8());
  echo_cancellation_mode_ = echo_cancellation_mode;
  auto_gain_control_ = auto_gain_control;
  noise_supression_ = noise_supression;
}

void MediaStreamSource::AddAudioConsumer(
    WebAudioDestinationConsumer* consumer) {
  DCHECK(requires_consumer_);
  auto* consumer_wrapper = ConsumerWrapper::Create(consumer);

  MutexLocker locker(audio_consumers_lock_);
  audio_consumers_.insert(consumer_wrapper);
}

bool MediaStreamSource::RemoveAudioConsumer(
    WebAudioDestinationConsumer* consumer) {
  DCHECK(requires_consumer_);
  auto* consumer_wrapper = ConsumerWrapper::Create(consumer);

  MutexLocker locker(audio_consumers_lock_);
  auto it = audio_consumers_.find(consumer_wrapper);
  if (it == audio_consumers_.end())
    return false;
  audio_consumers_.erase(it);
  return true;
}

void MediaStreamSource::GetSettings(
    MediaStreamTrackPlatform::Settings& settings) {
  settings.device_id = Id();
  settings.group_id = GroupId();

  if (echo_cancellation_mode_) {
    switch (*echo_cancellation_mode_) {
      case EchoCancellationMode::kDisabled:
        settings.echo_cancellation = false;
        settings.echo_cancellation_type = String();
        break;
      case EchoCancellationMode::kBrowser:
        settings.echo_cancellation = true;
        settings.echo_cancellation_type =
            String::FromUTF8(blink::kEchoCancellationTypeBrowser);
        break;
      case EchoCancellationMode::kAec3:
        settings.echo_cancellation = true;
        settings.echo_cancellation_type =
            String::FromUTF8(blink::kEchoCancellationTypeAec3);
        break;
      case EchoCancellationMode::kSystem:
        settings.echo_cancellation = true;
        settings.echo_cancellation_type =
            String::FromUTF8(blink::kEchoCancellationTypeSystem);
        break;
    }
  }
  if (auto_gain_control_)
    settings.auto_gain_control = *auto_gain_control_;
  if (noise_supression_)
    settings.noise_supression = *noise_supression_;

  GetSourceSettings(WebMediaStreamSource(this), settings);
}

void MediaStreamSource::SetAudioFormat(size_t number_of_channels,
                                       float sample_rate) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("mediastream"),
               "MediaStreamSource::SetAudioFormat");

  SendLogMessage(String::Format("SetAudioFormat({id=%s}, "
                                "{number_of_channels=%d}, {sample_rate=%.0f})",
                                Id().Utf8().c_str(),
                                static_cast<int>(number_of_channels),
                                sample_rate)
                     .Utf8());
  DCHECK(requires_consumer_);
  MutexLocker locker(audio_consumers_lock_);
  for (AudioDestinationConsumer* consumer : audio_consumers_)
    consumer->SetFormat(number_of_channels, sample_rate);
}

void MediaStreamSource::ConsumeAudio(AudioBus* bus, size_t number_of_frames) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("mediastream"),
               "MediaStreamSource::ConsumeAudio");

  DCHECK(requires_consumer_);
  MutexLocker locker(audio_consumers_lock_);
  for (AudioDestinationConsumer* consumer : audio_consumers_)
    consumer->ConsumeAudio(bus, number_of_frames);
}

void MediaStreamSource::OnDeviceCaptureHandleChange(
    const MediaStreamDevice& device) {
  if (!platform_source_) {
    return;
  }

  auto capture_handle = media::mojom::CaptureHandle::New();
  if (device.display_media_info.has_value()) {
    capture_handle = device.display_media_info.value()->capture_handle.Clone();
  }

  platform_source_->SetCaptureHandle(capture_handle.Clone());

  // Observers may dispatch events which create and add new Observers;
  // take a snapshot so as to safely iterate.
  HeapVector<Member<Observer>> observers;
  CopyToVector(observers_, observers);
  for (auto observer : observers) {
    observer->SourceChangedCaptureHandle(capture_handle.Clone());
  }
}

void MediaStreamSource::Trace(Visitor* visitor) const {
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

}  // namespace blink
