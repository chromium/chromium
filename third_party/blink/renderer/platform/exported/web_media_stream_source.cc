/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "third_party/blink/public/platform/web_media_stream_source.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/public/platform/web_audio_destination_consumer.h"
#include "third_party/blink/public/platform/web_media_constraints.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

WebMediaStreamSource::WebMediaStreamSource(
    MediaStreamSource* media_stream_source)
    : private_(media_stream_source) {}

WebMediaStreamSource& WebMediaStreamSource::operator=(
    MediaStreamSource* media_stream_source) {
  private_ = media_stream_source;
  return *this;
}

void WebMediaStreamSource::Assign(const WebMediaStreamSource& other) {
  private_ = other.private_;
}

void WebMediaStreamSource::Reset() {
  private_.Reset();
}

WebMediaStreamSource::operator MediaStreamSource*() const {
  return private_.Get();
}

void WebMediaStreamSource::Initialize(const WebString& id,
                                      Type type,
                                      const WebString& name,
                                      bool remote) {
  private_ = MakeGarbageCollected<MediaStreamSource>(
      id, static_cast<MediaStreamSource::StreamType>(type), name, remote);
}

WebString WebMediaStreamSource::Id() const {
  DCHECK(!private_.IsNull());
  return private_.Get()->Id();
}

WebMediaStreamSource::Type WebMediaStreamSource::GetType() const {
  DCHECK(!private_.IsNull());
  return static_cast<Type>(private_.Get()->GetType());
}

WebString WebMediaStreamSource::GetName() const {
  DCHECK(!private_.IsNull());
  return private_.Get()->GetName();
}

bool WebMediaStreamSource::Remote() const {
  DCHECK(!private_.IsNull());
  return private_.Get()->Remote();
}

void WebMediaStreamSource::SetGroupId(const blink::WebString& group_id) {
  DCHECK(!private_.IsNull());
  private_->SetGroupId(group_id);
}

WebString WebMediaStreamSource::GroupId() const {
  DCHECK(!private_.IsNull());
  return private_->GroupId();
}

void WebMediaStreamSource::SetReadyState(ReadyState state) {
  DCHECK(!private_.IsNull());
  private_->SetReadyState(static_cast<MediaStreamSource::ReadyState>(state));
}

WebMediaStreamSource::ReadyState WebMediaStreamSource::GetReadyState() const {
  DCHECK(!private_.IsNull());
  return static_cast<ReadyState>(private_->GetReadyState());
}

WebPlatformMediaStreamSource* WebMediaStreamSource::GetPlatformSource() const {
  DCHECK(!private_.IsNull());
  return private_->GetPlatformSource();
}

void WebMediaStreamSource::SetPlatformSource(
    std::unique_ptr<WebPlatformMediaStreamSource> platform_source) {
  DCHECK(!private_.IsNull());

  if (platform_source)
    platform_source->SetOwner(private_.Get());

  private_->SetPlatformSource(std::move(platform_source));
}

void WebMediaStreamSource::SetAudioProcessingProperties(
    EchoCancellationMode echo_cancellation_mode,
    bool auto_gain_control,
    bool noise_supression) {
  DCHECK(!private_.IsNull());
  private_->SetAudioProcessingProperties(
      static_cast<MediaStreamSource::EchoCancellationMode>(
          echo_cancellation_mode),
      auto_gain_control, noise_supression);
}

void WebMediaStreamSource::SetCapabilities(
    const WebMediaStreamSource::Capabilities& capabilities) {
  DCHECK(!private_.IsNull());
  private_->SetCapabilities(capabilities);
}

bool WebMediaStreamSource::RequiresAudioConsumer() const {
  DCHECK(!private_.IsNull());
  return private_->RequiresAudioConsumer();
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
      : consumer_(consumer) {}

  // m_consumer is not owned by this class.
  WebAudioDestinationConsumer* consumer_;
};

void ConsumerWrapper::SetFormat(size_t number_of_channels, float sample_rate) {
  consumer_->SetFormat(number_of_channels, sample_rate);
}

void ConsumerWrapper::ConsumeAudio(AudioBus* bus, size_t number_of_frames) {
  if (!bus)
    return;

  // Wrap AudioBus.
  size_t number_of_channels = bus->NumberOfChannels();
  WebVector<const float*> bus_vector(number_of_channels);
  for (size_t i = 0; i < number_of_channels; ++i)
    bus_vector[i] = bus->Channel(i)->Data();

  consumer_->ConsumeAudio(bus_vector, number_of_frames);
}

void WebMediaStreamSource::AddAudioConsumer(
    WebAudioDestinationConsumer* consumer) {
  DCHECK(IsMainThread());
  DCHECK(!private_.IsNull() && consumer);

  private_->AddAudioConsumer(ConsumerWrapper::Create(consumer));
}

bool WebMediaStreamSource::RemoveAudioConsumer(
    WebAudioDestinationConsumer* consumer) {
  DCHECK(IsMainThread());
  DCHECK(!private_.IsNull() && consumer);

  const HashSet<AudioDestinationConsumer*>& consumers =
      private_->AudioConsumers();
  for (AudioDestinationConsumer* it : consumers) {
    ConsumerWrapper* wrapper = static_cast<ConsumerWrapper*>(it);
    if (wrapper->Consumer() == consumer) {
      private_->RemoveAudioConsumer(wrapper);
      return true;
    }
  }
  return false;
}

}  // namespace blink
