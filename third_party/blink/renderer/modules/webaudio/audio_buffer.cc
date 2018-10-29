/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"

#include "third_party/blink/renderer/modules/webaudio/audio_buffer_options.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/audio_file_reader.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/typed_arrays/float32_array.h"

namespace blink {

AudioBuffer* AudioBuffer::Create(unsigned number_of_channels,
                                 size_t number_of_frames,
                                 float sample_rate) {
  if (!audio_utilities::IsValidAudioBufferSampleRate(sample_rate) ||
      number_of_channels > BaseAudioContext::MaxNumberOfChannels() ||
      !number_of_channels || !number_of_frames)
    return nullptr;

  AudioBuffer* buffer =
      new AudioBuffer(number_of_channels, number_of_frames, sample_rate);

  if (!buffer->CreatedSuccessfully(number_of_channels))
    return nullptr;
  return buffer;
}

AudioBuffer* AudioBuffer::Create(unsigned number_of_channels,
                                 size_t number_of_frames,
                                 float sample_rate,
                                 ExceptionState& exception_state) {
  if (!number_of_channels ||
      number_of_channels > BaseAudioContext::MaxNumberOfChannels()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        ExceptionMessages::IndexOutsideRange(
            "number of channels", number_of_channels, 1u,
            ExceptionMessages::kInclusiveBound,
            BaseAudioContext::MaxNumberOfChannels(),
            ExceptionMessages::kInclusiveBound));
    return nullptr;
  }

  if (!audio_utilities::IsValidAudioBufferSampleRate(sample_rate)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        ExceptionMessages::IndexOutsideRange(
            "sample rate", sample_rate,
            audio_utilities::MinAudioBufferSampleRate(),
            ExceptionMessages::kInclusiveBound,
            audio_utilities::MaxAudioBufferSampleRate(),
            ExceptionMessages::kInclusiveBound));
    return nullptr;
  }

  if (!number_of_frames) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        ExceptionMessages::IndexExceedsMinimumBound(
            "number of frames", number_of_frames, static_cast<size_t>(0)));
    return nullptr;
  }

  AudioBuffer* audio_buffer =
      Create(number_of_channels, number_of_frames, sample_rate);

  if (!audio_buffer) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "createBuffer(" + String::Number(number_of_channels) + ", " +
            String::Number(number_of_frames) + ", " +
            String::Number(sample_rate) + ") failed.");
  }

  return audio_buffer;
}

AudioBuffer* AudioBuffer::Create(const AudioBufferOptions& options,
                                 ExceptionState& exception_state) {
  return Create(options.numberOfChannels(), options.length(),
                options.sampleRate(), exception_state);
}

AudioBuffer* AudioBuffer::CreateUninitialized(unsigned number_of_channels,
                                              size_t number_of_frames,
                                              float sample_rate) {
  if (!audio_utilities::IsValidAudioBufferSampleRate(sample_rate) ||
      number_of_channels > BaseAudioContext::MaxNumberOfChannels() ||
      !number_of_channels || !number_of_frames)
    return nullptr;

  AudioBuffer* buffer = new AudioBuffer(number_of_channels, number_of_frames,
                                        sample_rate, kDontInitialize);

  if (!buffer->CreatedSuccessfully(number_of_channels))
    return nullptr;
  return buffer;
}

AudioBuffer* AudioBuffer::CreateFromAudioFileData(const void* data,
                                                  size_t data_size,
                                                  bool mix_to_mono,
                                                  float sample_rate) {
  scoped_refptr<AudioBus> bus =
      CreateBusFromInMemoryAudioFile(data, data_size, mix_to_mono, sample_rate);
  if (bus) {
    AudioBuffer* buffer = new AudioBuffer(bus.get());
    if (buffer->CreatedSuccessfully(bus->NumberOfChannels()))
      return buffer;
  }

  return nullptr;
}

AudioBuffer* AudioBuffer::CreateFromAudioBus(AudioBus* bus) {
  if (!bus)
    return nullptr;
  AudioBuffer* buffer = new AudioBuffer(bus);
  if (buffer->CreatedSuccessfully(bus->NumberOfChannels()))
    return buffer;
  return nullptr;
}

bool AudioBuffer::CreatedSuccessfully(
    unsigned desired_number_of_channels) const {
  return numberOfChannels() == desired_number_of_channels;
}

DOMFloat32Array* AudioBuffer::CreateFloat32ArrayOrNull(
    size_t length,
    InitializationPolicy policy) {
  scoped_refptr<WTF::Float32Array> buffer;

  switch (policy) {
    case kZeroInitialize:
      buffer = WTF::Float32Array::CreateOrNull(length);
      break;
    case kDontInitialize:
      buffer = WTF::Float32Array::CreateUninitializedOrNull(length);
      break;
    default:
      NOTREACHED();
      break;
  }

  if (!buffer) {
    return nullptr;
  }
  return DOMFloat32Array::Create(std::move(buffer));
}

AudioBuffer::AudioBuffer(unsigned number_of_channels,
                         size_t number_of_frames,
                         float sample_rate,
                         InitializationPolicy policy)
    : sample_rate_(sample_rate), length_(number_of_frames) {
  channels_.ReserveCapacity(number_of_channels);

  for (unsigned i = 0; i < number_of_channels; ++i) {
    DOMFloat32Array* channel_data_array =
        CreateFloat32ArrayOrNull(length_, policy);
    // If the channel data array could not be created, just return. The caller
    // will need to check that the desired number of channels were created.
    if (!channel_data_array)
      return;

    channel_data_array->SetNeuterable(false);
    channels_.push_back(channel_data_array);
  }
}

AudioBuffer::AudioBuffer(AudioBus* bus)
    : sample_rate_(bus->SampleRate()), length_(bus->length()) {
  // Copy audio data from the bus to the Float32Arrays we manage.
  unsigned number_of_channels = bus->NumberOfChannels();
  channels_.ReserveCapacity(number_of_channels);
  for (unsigned i = 0; i < number_of_channels; ++i) {
    DOMFloat32Array* channel_data_array =
        CreateFloat32ArrayOrNull(length_, kDontInitialize);
    // If the channel data array could not be created, just return. The caller
    // will need to check that the desired number of channels were created.
    if (!channel_data_array)
      return;

    channel_data_array->SetNeuterable(false);
    const float* src = bus->Channel(i)->Data();
    float* dst = channel_data_array->Data();
    memmove(dst, src, length_ * sizeof(*dst));
    channels_.push_back(channel_data_array);
  }
}

NotShared<DOMFloat32Array> AudioBuffer::getChannelData(
    unsigned channel_index,
    ExceptionState& exception_state) {
  if (channel_index >= channels_.size()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "channel index (" + String::Number(channel_index) +
            ") exceeds number of channels (" +
            String::Number(channels_.size()) + ")");
    return NotShared<DOMFloat32Array>(nullptr);
  }

  return getChannelData(channel_index);
}

NotShared<DOMFloat32Array> AudioBuffer::getChannelData(unsigned channel_index) {
  if (channel_index >= channels_.size())
    return NotShared<DOMFloat32Array>(nullptr);

  return NotShared<DOMFloat32Array>(channels_[channel_index].Get());
}

void AudioBuffer::copyFromChannel(NotShared<DOMFloat32Array> destination,
                                  long channel_number,
                                  ExceptionState& exception_state) {
  return copyFromChannel(destination, channel_number, 0, exception_state);
}

void AudioBuffer::copyFromChannel(NotShared<DOMFloat32Array> destination,
                                  long channel_number,
                                  unsigned long start_in_channel,
                                  ExceptionState& exception_state) {
  if (channel_number < 0 ||
      channel_number >= static_cast<long>(channels_.size())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexOutsideRange(
            "channelNumber", channel_number, 0L,
            ExceptionMessages::kInclusiveBound,
            static_cast<long>(channels_.size() - 1),
            ExceptionMessages::kInclusiveBound));

    return;
  }

  DOMFloat32Array* channel_data = channels_[channel_number].Get();

  if (start_in_channel >= channel_data->length()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexOutsideRange(
            "startInChannel", start_in_channel, 0UL,
            ExceptionMessages::kInclusiveBound,
            static_cast<unsigned long>(channel_data->length()),
            ExceptionMessages::kExclusiveBound));

    return;
  }

  unsigned count = channel_data->length() - start_in_channel;
  count = std::min(destination.View()->length(), count);

  const float* src = channel_data->Data();
  float* dst = destination.View()->Data();

  DCHECK(src);
  DCHECK(dst);

  memcpy(dst, src + start_in_channel, count * sizeof(*src));
}

void AudioBuffer::copyToChannel(NotShared<DOMFloat32Array> source,
                                long channel_number,
                                ExceptionState& exception_state) {
  return copyToChannel(source, channel_number, 0, exception_state);
}

void AudioBuffer::copyToChannel(NotShared<DOMFloat32Array> source,
                                long channel_number,
                                unsigned long start_in_channel,
                                ExceptionState& exception_state) {
  if (channel_number < 0 ||
      channel_number >= static_cast<long>(channels_.size())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexOutsideRange(
            "channelNumber", channel_number, 0L,
            ExceptionMessages::kInclusiveBound,
            static_cast<long>(channels_.size() - 1),
            ExceptionMessages::kInclusiveBound));
    return;
  }

  DOMFloat32Array* channel_data = channels_[channel_number].Get();

  if (start_in_channel >= channel_data->length()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexOutsideRange(
            "startInChannel", start_in_channel, 0UL,
            ExceptionMessages::kInclusiveBound,
            static_cast<unsigned long>(channel_data->length()),
            ExceptionMessages::kExclusiveBound));

    return;
  }

  unsigned count = channel_data->length() - start_in_channel;
  count = std::min(source.View()->length(), count);

  const float* src = source.View()->Data();
  float* dst = channel_data->Data();

  DCHECK(src);
  DCHECK(dst);

  memcpy(dst + start_in_channel, src, count * sizeof(*dst));
}

void AudioBuffer::Zero() {
  for (unsigned i = 0; i < channels_.size(); ++i) {
    if (NotShared<DOMFloat32Array> array = getChannelData(i)) {
      float* data = array.View()->Data();
      memset(data, 0, length() * sizeof(*data));
    }
  }
}

}  // namespace blink
