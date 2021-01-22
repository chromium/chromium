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

namespace {
class SharedAudioData final : public AudioFrameSerializationData {
 public:
  SharedAudioData(std::unique_ptr<SharedAudioBuffer> shared_buffer,
                  base::TimeDelta timestamp)
      : AudioFrameSerializationData(shared_buffer->sampleRate(), timestamp),
        backing_buffer_(std::move(shared_buffer)) {
    buffer_wrapper_ =
        media::AudioBus::CreateWrapper(backing_buffer_->numberOfChannels());

    for (int i = 0; i < buffer_wrapper_->channels(); ++i) {
      float* channel_data =
          static_cast<float*>(backing_buffer_->channels()[i].Data());
      buffer_wrapper_->SetChannelData(i, channel_data);
    }
    buffer_wrapper_->set_frames(backing_buffer_->length());
  }
  ~SharedAudioData() override = default;

  media::AudioBus* data() override { return buffer_wrapper_.get(); }

 private:
  std::unique_ptr<media::AudioBus> buffer_wrapper_;
  std::unique_ptr<SharedAudioBuffer> backing_buffer_;
};
}  // namespace

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

  // Wrap blink buffer with a media::AudioBus so we can interface with
  // media::AudioBuffer to copy the data out.
  auto media_bus_wrapper =
      media::AudioBus::CreateWrapper(buffer->channel_count());
  for (int i = 0; i < media_bus_wrapper->channels(); ++i) {
    DCHECK_EQ(buffer_->getChannelData(i)->byteLength(),
              buffer->frame_count() * sizeof(float));
    float* channel_data = buffer_->getChannelData(i)->Data();
    media_bus_wrapper->SetChannelData(i, channel_data);
  }
  media_bus_wrapper->set_frames(buffer->frame_count());

  // Copy the frames.
  // TODO(chcunningham): Avoid this copy by refactoring blink::AudioBuffer to
  // ref a media::AudioBuffer and only copy for calls to copyToChannel().
  buffer->ReadFrames(media_bus_wrapper->frames(), 0 /* source_frame_offset */,
                     0 /* dest_frame_offset */, media_bus_wrapper.get());
}

std::unique_ptr<AudioFrameSerializationData>
AudioFrame::GetSerializationData() {
  DCHECK(buffer_);
  return std::make_unique<SharedAudioData>(
      buffer_->CreateSharedAudioBuffer(),
      base::TimeDelta::FromMicroseconds(timestamp_));
}

AudioFrame::AudioFrame(std::unique_ptr<AudioFrameSerializationData> data)
    : timestamp_(data->timestamp().InMicroseconds()) {
  const media::AudioBus& audio_bus = *data->data();
  buffer_ = AudioBuffer::CreateUninitialized(
      audio_bus.channels(), audio_bus.frames(), data->sample_rate());

  // Wrap blink buffer with a media::AudioBus so we can interface with
  // media::AudioBuffer to copy the data out.
  auto audio_buffer_wrapper =
      media::AudioBus::CreateWrapper(audio_bus.channels());
  for (int i = 0; i < audio_buffer_wrapper->channels(); ++i) {
    DCHECK_EQ(buffer_->getChannelData(i)->byteLength(),
              audio_bus.frames() * sizeof(float));
    float* channel_data = buffer_->getChannelData(i)->Data();
    audio_buffer_wrapper->SetChannelData(i, channel_data);
  }
  audio_buffer_wrapper->set_frames(audio_bus.frames());

  // Copy the frames.
  // TODO(https://crbug.com/1168418): Avoid this copy by refactoring
  // blink::AudioBuffer accept a serializable audio data backing object.
  audio_bus.CopyTo(audio_buffer_wrapper.get());
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
