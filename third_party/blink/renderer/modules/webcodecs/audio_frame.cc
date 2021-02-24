// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/audio_frame.h"

#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_frame_init.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_frame_serialization_data.h"

namespace blink {

// static
AudioFrame* AudioFrame::Create(AudioFrameInit* init,
                               ExceptionState& exception_state) {
  // FIXME : throw exception if no audio buffer.
  return MakeGarbageCollected<AudioFrame>(init);
}

AudioFrame::AudioFrame(AudioFrameInit* init)
    : timestamp_(init->timestamp()), buffer_(init->buffer()) {}

AudioFrame::AudioFrame(scoped_refptr<media::AudioBuffer> buffer)
    : timestamp_(buffer->timestamp().InMicroseconds()) {
  buffer_ = AudioBuffer::CreateUninitialized(
      buffer->channel_count(), buffer->frame_count(), buffer->sample_rate());

  // AudioBuffer::CreateUninitialized() can fail. This can be the result of
  // running out of memory, or having parameters that exceed some of WebAudio's
  // limits. Crash here to prevent accessing uninitialized data below.
  // TODO(crbug.com/1179079): Add upstream checks to prevent initializing
  // |buffer_| with parameters outside of WebAudio's limits.
  CHECK(buffer_);

  auto converted_data =
      media::AudioBus::Create(buffer->channel_count(), buffer->frame_count());

  std::vector<float*> wrapped_channels(buffer_->numberOfChannels());
  for (unsigned ch = 0; ch < buffer_->numberOfChannels(); ++ch)
    wrapped_channels[ch] = buffer_->getChannelData(ch)->Data();

  // Copy the frames, converting from |buffer|'s internal format to float.
  // TODO(chcunningham): Avoid this copy by refactoring blink::AudioBuffer to
  // ref a media::AudioBuffer and only copy for calls to copyToChannel().
  buffer->ReadAllFrames(wrapped_channels);
}

std::unique_ptr<AudioFrameSerializationData>
AudioFrame::GetSerializationData() {
  DCHECK(buffer_);

  // Copy buffer unaligned memory into media::AudioBus' aligned memory.
  // TODO(https://crbug.com/1168418): reevaluate if this copy is necessary after
  // our changes. E.g. If we can ever guarantee AudioBuffer's memory alignment,
  // we could save this copy here, by using buffer_->GetSharedAudioBuffer() and
  // wrapping it directly.
  auto data_copy =
      media::AudioBus::Create(buffer_->numberOfChannels(), buffer_->length());

  for (int i = 0; i < data_copy->channels(); ++i) {
    size_t byte_length = buffer_->getChannelData(i)->byteLength();
    DCHECK_EQ(byte_length, data_copy->frames() * sizeof(float));
    float* buffer_data_src = buffer_->getChannelData(i)->Data();
    memcpy(data_copy->channel(i), buffer_data_src, byte_length);
  }

  return AudioFrameSerializationData::Wrap(
      std::move(data_copy), buffer_->sampleRate(),
      base::TimeDelta::FromMicroseconds(timestamp_));
}

AudioFrame::AudioFrame(std::unique_ptr<AudioFrameSerializationData> data)
    : timestamp_(data->timestamp().InMicroseconds()) {
  media::AudioBus* data_bus = data->data();

  buffer_ = AudioBuffer::CreateUninitialized(
      data_bus->channels(), data_bus->frames(), data->sample_rate());

  // AudioBuffer::CreateUninitialized() can fail. This can be the result of
  // running out of memory, or having parameters that exceed some of WebAudio's
  // limits. Crash here to prevent accessing uninitialized data below.
  // TODO(crbug.com/1179079): Add upstream checks to prevent initializing
  // |buffer_| with parameters outside of WebAudio's limits.
  CHECK(buffer_);

  // Copy the frames.
  // TODO(https://crbug.com/1168418): Avoid this copy by refactoring
  // blink::AudioBuffer accept a serializable audio data backing object.
  DCHECK_EQ(static_cast<int>(buffer_->numberOfChannels()),
            data_bus->channels());
  DCHECK_EQ(static_cast<int>(buffer_->length()), data_bus->frames());

  for (int i = 0; i < data_bus->channels(); ++i) {
    size_t byte_length = buffer_->getChannelData(i)->byteLength();
    DCHECK_EQ(byte_length, data_bus->frames() * sizeof(float));
    float* buffer_data_dest = buffer_->getChannelData(i)->Data();
    memcpy(buffer_data_dest, data_bus->channel(i), byte_length);
  }
}

void AudioFrame::close() {
  buffer_.Clear();
}

uint64_t AudioFrame::timestamp() const {
  return timestamp_;
}

AudioBuffer* AudioFrame::buffer() const {
  return buffer_;
}

void AudioFrame::Trace(Visitor* visitor) const {
  visitor->Trace(buffer_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
