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

#include "base/synchronization/lock.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mediastream/media_constraints_consts.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/webaudio_destination_consumer.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/display/types/display_constants.h"

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
      NOTREACHED_IN_MIGRATION();
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
      NOTREACHED_IN_MIGRATION();
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

}  // namespace

MediaStreamSource::ConsumerWrapper::ConsumerWrapper(
    WebAudioDestinationConsumer* consumer)
    : consumer_(consumer) {
  // To avoid reallocation in ConsumeAudio, reserve initial capacity for most
  // common known layouts.
  bus_vector_.ReserveInitialCapacity(8);
}

void MediaStreamSource::ConsumerWrapper::SetFormat(int number_of_channels,
                                                   float sample_rate) {
  consumer_->SetFormat(number_of_channels, sample_rate);
}

void MediaStreamSource::ConsumerWrapper::ConsumeAudio(AudioBus* bus,
                                                      int number_of_frames) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("mediastream"),
               "ConsumerWrapper::ConsumeAudio");

  if (!bus)
    return;

  // Wrap AudioBus.
  unsigned number_of_channels = bus->NumberOfChannels();
  if (bus_vector_.size() != number_of_channels) {
    bus_vector_.resize(number_of_channels);
  }
  for (unsigned i = 0; i < number_of_channels; ++i)
    bus_vector_[i] = bus->Channel(i)->Data();

  consumer_->ConsumeAudio(bus_vector_, number_of_frames);
}

MediaStreamSource::MediaStreamSource(
    const String& id,
    StreamType type,
    const String& name,
    bool remote,
    std::unique_ptr<WebPlatformMediaStreamSource> platform_source,
    ReadyState ready_state,
    bool requires_consumer)
    : MediaStreamSource(id,
                        display::kInvalidDisplayId,
                        type,
                        name,
                        remote,
                        std::move(platform_source),
                        ready_state,
                        requires_consumer) {}

MediaStreamSource::MediaStreamSource(
    const String& id,
    int64_t display_id,
    StreamType type,
    const String& name,
    bool remote,
    std::unique_ptr<WebPlatformMediaStreamSource> platform_source,
    ReadyState ready_state,
    bool requires_consumer)
    : id_(id),
      display_id_(display_id),
      type_(type),
      name_(name),
      remote_(remote),
      ready_state_(ready_state),
      requires_consumer_(requires_consumer),
      platform_source_(std::move(platform_source)) {
  SendLogMessage(
      String::Format(
          "MediaStreamSource({id=%s}, {type=%s}, {name=%s}, {remote=%d}, "
          "{ready_state=%s})",
          id.Utf8().c_str(), StreamTypeToString(type), name.Utf8().c_str(),
          remote, ReadyStateToString(ready_state))
          .Utf8());
  if (platform_source_)
    platform_source_->SetOwner(this);
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
    // take a snapshot so as to safely iterate. Wrap the observers in
    // weak persistents to allow cancelling callbacks in case they are reclaimed
    // until the callback is executed.
    Vector<base::OnceClosure> observer_callbacks;
    for (const auto& it : observers_) {
      observer_callbacks.push_back(WTF::BindOnce(&Observer::SourceChangedState,
                                                 WrapWeakPersistent(it.Get())));
    }
    for (auto& observer_callback : observer_callbacks) {
      std::move(observer_callback).Run();
    }
  }
}

void MediaStreamSource::AddObserver(MediaStreamSource::Observer* observer) {
  observers_.insert(observer);
}

void MediaStreamSource::SetAudioProcessingProperties(
    EchoCancellationMode echo_cancellation_mode,
    bool auto_gain_control,
    bool noise_supression,
    bool voice_isolation) {
  SendLogMessage(
      String::Format("%s({echo_cancellation_mode=%s}, {auto_gain_control=%d}, "
                     "{noise_supression=%d}, {voice_isolation=%d})",
                     __func__,
                     EchoCancellationModeToString(echo_cancellation_mode),
                     auto_gain_control, noise_supression, voice_isolation)
          .Utf8());
  echo_cancellation_mode_ = echo_cancellation_mode;
  auto_gain_control_ = auto_gain_control;
  noise_supression_ = noise_supression;
  voice_isolation_ = voice_isolation;
}

void MediaStreamSource::SetAudioConsumer(
    WebAudioDestinationConsumer* consumer) {
  DCHECK(requires_consumer_);
  base::AutoLock locker(audio_consumer_lock_);
  // audio_consumer_ should only be set once.
  DCHECK(!audio_consumer_);
  audio_consumer_ = std::make_unique<ConsumerWrapper>(consumer);
}

bool MediaStreamSource::RemoveAudioConsumer() {
  DCHECK(requires_consumer_);

  base::AutoLock locker(audio_consumer_lock_);
  if (!audio_consumer_)
    return false;
  audio_consumer_.reset();
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
  if (voice_isolation_) {
    settings.voice_isolation = *voice_isolation_;
  }

  GetSourceSettings(WebMediaStreamSource(this), settings);
}

void MediaStreamSource::SetAudioFormat(int number_of_channels,
                                       float sample_rate) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("mediastream"),
               "MediaStreamSource::SetAudioFormat");

  SendLogMessage(String::Format("SetAudioFormat({id=%s}, "
                                "{number_of_channels=%d}, {sample_rate=%.0f})",
                                Id().Utf8().c_str(), number_of_channels,
                                sample_rate)
                     .Utf8());
  DCHECK(requires_consumer_);
  base::AutoLock locker(audio_consumer_lock_);
  if (!audio_consumer_)
    return;
  audio_consumer_->SetFormat(number_of_channels, sample_rate);
}

void MediaStreamSource::ConsumeAudio(AudioBus* bus, int number_of_frames) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("mediastream"),
               "MediaStreamSource::ConsumeAudio");

  DCHECK(requires_consumer_);

  base::AutoLock locker(audio_consumer_lock_);
  if (!audio_consumer_)
    return;
  audio_consumer_->ConsumeAudio(bus, number_of_frames);
}

void MediaStreamSource::OnDeviceCaptureConfigurationChange(
    const MediaStreamDevice& device) {
  if (!platform_source_) {
    return;
  }

  // Observers may dispatch events which create and add new Observers;
  // take a snapshot so as to safely iterate.
  HeapVector<Member<Observer>> observers(observers_);
  for (auto observer : observers) {
    observer->SourceChangedCaptureConfiguration();
  }
}

void MediaStreamSource::OnDeviceCaptureHandleChange(
    const MediaStreamDevice& device) {
  if (!platform_source_) {
    return;
  }

  auto capture_handle = media::mojom::CaptureHandle::New();
  if (device.display_media_info) {
    capture_handle = device.display_media_info->capture_handle.Clone();
  }

  platform_source_->SetCaptureHandle(std::move(capture_handle));

  // Observers may dispatch events which create and add new Observers;
  // take a snapshot so as to safely iterate.
  HeapVector<Member<Observer>> observers(observers_);
  for (auto observer : observers) {
    observer->SourceChangedCaptureHandle();
  }
}

void MediaStreamSource::OnZoomLevelChange(const MediaStreamDevice& device,
                                          int zoom_level) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  if (!platform_source_) {
    return;
  }

  // Observers may dispatch events which create and add new Observers;
  // take a snapshot so as to safely iterate.
  HeapVector<Member<Observer>> observers(observers_);
  for (auto observer : observers) {
    observer->SourceChangedZoomLevel(zoom_level);
  }
#endif
}

void MediaStreamSource::Trace(Visitor* visitor) const {
  visitor->Trace(observers_);
}

void MediaStreamSource::Dispose() {
  {
    base::AutoLock locker(audio_consumer_lock_);
    audio_consumer_.reset();
  }
  platform_source_.reset();
}

}  // namespace blink
