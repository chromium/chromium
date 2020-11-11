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
  // FIXME : throw exception if no audio buffer.
  return MakeGarbageCollected<AudioFrame>(init);
}

AudioFrame::AudioFrame(AudioFrameInit* init)
    : timestamp_(init->timestamp()), buffer_(init->buffer()) {}

AudioFrame::AudioFrame(scoped_refptr<media::AudioBuffer> buffer)
    : timestamp_(buffer->timestamp().InMicroseconds()) {
  buffer_ = AudioBuffer::CreateUninitialized(
      buffer->channel_count(), buffer->frame_count(), buffer->sample_rate());

  // Wrap blink buffer a media::AudioBus so we can interface with
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
