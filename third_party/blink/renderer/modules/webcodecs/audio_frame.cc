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

  auto converted_data =
      media::AudioBus::Create(buffer->channel_count(), buffer->frame_count());

  // Copy the frames, converting from |buffer|'s internal format to float.
  // TODO(https://crbug.com/1171840): Add a version of
  // media::AudioBus::ReadFrame that can directly read into |buffer_|'s data.
  // TODO(chcunningham): Avoid this copy by refactoring blink::AudioBuffer to
  // ref a media::AudioBuffer and only copy for calls to copyToChannel().
  buffer->ReadFrames(converted_data->frames(), 0 /* source_frame_offset */,
                     0 /* dest_frame_offset */, converted_data.get());

  CopyDataToInternalBuffer(converted_data.get());
}

void AudioFrame::CopyDataToInternalBuffer(media::AudioBus* data) {
  DCHECK_EQ(static_cast<int>(buffer_->numberOfChannels()), data->channels());
  DCHECK_EQ(static_cast<int>(buffer_->length()), data->frames());

  for (int i = 0; i < data->channels(); ++i) {
    size_t byte_length = buffer_->getChannelData(i)->byteLength();
    DCHECK_EQ(byte_length, data->frames() * sizeof(float));
    float* buffer_data_dest = buffer_->getChannelData(i)->Data();
    memcpy(data->channel(i), buffer_data_dest, byte_length);
  }
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
    memcpy(buffer_data_src, data_copy->channel(i), byte_length);
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

  // Copy the frames.
  // TODO(https://crbug.com/1168418): Avoid this copy by refactoring
  // blink::AudioBuffer accept a serializable audio data backing object.
  CopyDataToInternalBuffer(data_bus);
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
