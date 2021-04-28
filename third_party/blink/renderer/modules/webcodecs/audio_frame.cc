// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/audio_frame.h"

#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_frame_init.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"

namespace blink {

// static
AudioFrame* AudioFrame::Create(AudioFrameInit* init,
                               ExceptionState& exception_state) {
  return MakeGarbageCollected<AudioFrame>(init);
}

AudioFrame::AudioFrame(AudioFrameInit* init)
    : timestamp_(init->timestamp()), buffer_(init->buffer()) {
  std::vector<const uint8_t*> wrapped_channels(buffer_->numberOfChannels());
  for (unsigned ch = 0; ch < buffer_->numberOfChannels(); ++ch) {
    wrapped_channels[ch] =
        reinterpret_cast<const uint8_t*>(buffer_->getChannelData(ch)->Data());
  }

  data_ = media::AudioBuffer::CopyFrom(
      media::SampleFormat::kSampleFormatPlanarF32,
      media::GuessChannelLayout(buffer_->numberOfChannels()),
      buffer_->numberOfChannels(), buffer_->sampleRate(), buffer_->length(),
      wrapped_channels.data(), base::TimeDelta::FromMicroseconds(timestamp_));
}

AudioFrame::AudioFrame(scoped_refptr<media::AudioBuffer> buffer)
    : data_(std::move(buffer)),
      timestamp_(data_->timestamp().InMicroseconds()) {}

AudioFrame* AudioFrame::clone(ExceptionState& exception_state) {
  if (!data_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot clone closed AudioFrame.");
    return nullptr;
  }

  return MakeGarbageCollected<AudioFrame>(data_);
}

void AudioFrame::close() {
  data_.reset();
  buffer_.Clear();
}

int64_t AudioFrame::timestamp() const {
  return timestamp_;
}

void AudioFrame::CopyDataToBuffer() {
  DCHECK(!buffer_);

  // |this| might have been closed already.
  if (!data_)
    return;

  buffer_ = AudioBuffer::CreateUninitialized(
      data_->channel_count(), data_->frame_count(), data_->sample_rate());

  // AudioBuffer::CreateUninitialized() can fail if we run out of memory.
  // Crash here to prevent accessing uninitialized data below.
  CHECK(buffer_);

  std::vector<float*> wrapped_channels(buffer_->numberOfChannels());
  for (unsigned ch = 0; ch < buffer_->numberOfChannels(); ++ch)
    wrapped_channels[ch] = buffer_->getChannelData(ch)->Data();

  // Copy the frames, converting from |buffer|'s internal format to float.
  data_->ReadAllFrames(wrapped_channels);
}

AudioBuffer* AudioFrame::buffer() {
  if (!buffer_)
    CopyDataToBuffer();

  return buffer_;
}

void AudioFrame::Trace(Visitor* visitor) const {
  visitor->Trace(buffer_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
