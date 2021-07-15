// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/audio_data.h"

#include "base/numerics/checked_math.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/sample_format.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_data_copy_to_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_data_init.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

absl::optional<V8AudioSampleFormat> MediaFormatToBlinkFormat(
    media::SampleFormat media_format) {
  using FormatEnum = V8AudioSampleFormat::Enum;

  DCHECK(media_format != media::SampleFormat::kUnknownSampleFormat);

  // TODO(crbug.com/1205281): Add support for "U8P" and "S24P".
  switch (media_format) {
    case media::SampleFormat::kSampleFormatU8:
      return V8AudioSampleFormat(FormatEnum::kU8);

    case media::SampleFormat::kSampleFormatS16:
      return V8AudioSampleFormat(FormatEnum::kS16);

    case media::SampleFormat::kSampleFormatS24:
      return V8AudioSampleFormat(FormatEnum::kS24);

    case media::SampleFormat::kSampleFormatS32:
      return V8AudioSampleFormat(FormatEnum::kS32);

    case media::SampleFormat::kSampleFormatF32:
      return V8AudioSampleFormat(FormatEnum::kFLT);

    case media::SampleFormat::kSampleFormatPlanarS16:
      return V8AudioSampleFormat(FormatEnum::kS16P);

    case media::SampleFormat::kSampleFormatPlanarS32:
      return V8AudioSampleFormat(FormatEnum::kS32P);

    case media::SampleFormat::kSampleFormatPlanarF32:
      return V8AudioSampleFormat(FormatEnum::kFLTP);

    case media::SampleFormat::kSampleFormatAc3:
    case media::SampleFormat::kSampleFormatEac3:
    case media::SampleFormat::kSampleFormatMpegHAudio:
    case media::SampleFormat::kUnknownSampleFormat:
      return absl::nullopt;
  }
}

media::SampleFormat BlinkFormatToMediaFormat(V8AudioSampleFormat blink_format) {
  using FormatEnum = V8AudioSampleFormat::Enum;

  DCHECK(!blink_format.IsEmpty());

  switch (blink_format.AsEnum()) {
    case FormatEnum::kU8:
      return media::SampleFormat::kSampleFormatU8;

    case FormatEnum::kS16:
      return media::SampleFormat::kSampleFormatS16;

    case FormatEnum::kS24:
      return media::SampleFormat::kSampleFormatS24;

    case FormatEnum::kS32:
      return media::SampleFormat::kSampleFormatS32;

    case FormatEnum::kFLT:
      return media::SampleFormat::kSampleFormatF32;

    case FormatEnum::kS16P:
      return media::SampleFormat::kSampleFormatPlanarS16;

    case FormatEnum::kS32P:
      return media::SampleFormat::kSampleFormatPlanarS32;

    case FormatEnum::kFLTP:
      return media::SampleFormat::kSampleFormatPlanarF32;

    // TODO(crbug.com/1205281): Add support for "U8P" and "S24P".
    case FormatEnum::kU8P:
    case FormatEnum::kS24P:
      return media::SampleFormat::kUnknownSampleFormat;
  }
}

}  // namespace

// static
AudioData* AudioData::Create(AudioDataInit* init,
                             ExceptionState& exception_state) {
  return MakeGarbageCollected<AudioData>(init, exception_state);
}

AudioData::AudioData(AudioDataInit* init, ExceptionState& exception_state)
    : format_(absl::nullopt), timestamp_(init->timestamp()) {
  media::SampleFormat media_format = BlinkFormatToMediaFormat(init->format());

  // TODO(crbug.com/1205281): Add support for "U8P" and "S24P".
  if (media_format == media::SampleFormat::kUnknownSampleFormat) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        String::Format("Format '%s' is not supported yet.",
                       init->format().AsCStr()));
    return;
  }

  uint32_t bytes_per_sample =
      media::SampleFormatToBytesPerChannel(media_format);

  uint32_t total_bytes;
  if (!base::CheckMul(bytes_per_sample, base::CheckMul(init->numberOfChannels(),
                                                       init->numberOfFrames()))
           .AssignIfValid(&total_bytes)) {
    exception_state.ThrowTypeError(
        "AudioData allocation size exceeds implementation limits.");
    return;
  }

  DOMArrayPiece source_data(init->data());
  if (total_bytes > source_data.ByteLength()) {
    exception_state.ThrowTypeError(
        String::Format("`data` is too small: needs %u bytes, received %zu.",
                       total_bytes, source_data.ByteLength()));
    return;
  }

  std::vector<const uint8_t*> wrapped_data;
  if (media::IsInterleaved(media_format)) {
    // Interleaved data can directly added.
    wrapped_data.push_back(
        reinterpret_cast<const uint8_t*>(source_data.Bytes()));
  } else {
    // Planar data needs one pointer per channel.
    wrapped_data.resize(init->numberOfChannels());

    uint32_t plane_size_in_bytes =
        init->numberOfFrames() *
        media::SampleFormatToBytesPerChannel(media_format);

    const uint8_t* plane_start =
        reinterpret_cast<const uint8_t*>(source_data.Bytes());

    for (unsigned ch = 0; ch < init->numberOfChannels(); ++ch)
      wrapped_data[ch] = plane_start + ch * plane_size_in_bytes;
  }

  format_ = init->format();

  data_ = media::AudioBuffer::CopyFrom(
      media_format, media::GuessChannelLayout(init->numberOfChannels()),
      init->numberOfChannels(), init->sampleRate(), init->numberOfFrames(),
      wrapped_data.data(), base::TimeDelta::FromMicroseconds(timestamp_));
}

AudioData::AudioData(scoped_refptr<media::AudioBuffer> buffer)
    : data_(std::move(buffer)),
      timestamp_(data_->timestamp().InMicroseconds()) {
  media::SampleFormat media_format = data_->sample_format();

  DCHECK(!media::IsBitstream(media_format));

  format_ = MediaFormatToBlinkFormat(media_format);

  if (!format_.has_value())
    close();
}

AudioData* AudioData::clone(ExceptionState& exception_state) {
  if (!data_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot clone closed AudioData.");
    return nullptr;
  }

  return MakeGarbageCollected<AudioData>(data_);
}

void AudioData::close() {
  data_.reset();
  format_ = absl::nullopt;
}

int64_t AudioData::timestamp() const {
  return timestamp_;
}

absl::optional<V8AudioSampleFormat> AudioData::format() const {
  return format_;
}

uint32_t AudioData::sampleRate() const {
  if (!data_)
    return 0;

  return data_->sample_rate();
}

uint32_t AudioData::numberOfFrames() const {
  if (!data_)
    return 0;

  return data_->frame_count();
}

uint32_t AudioData::numberOfChannels() const {
  if (!data_)
    return 0;

  return data_->channel_count();
}

uint64_t AudioData::duration() const {
  if (!data_)
    return 0;

  return data_->duration().InMicroseconds();
}

bool AudioData::IsInterleaved() {
  return media::IsInterleaved(
      static_cast<media::SampleFormat>(data_->sample_format()));
}

uint32_t AudioData::BytesPerSample() {
  return media::SampleFormatToBytesPerChannel(
      static_cast<media::SampleFormat>(data_->sample_format()));
}

uint32_t AudioData::allocationSize(AudioDataCopyToOptions* copy_to_options,
                                   ExceptionState& exception_state) {
  if (!data_)
    return 0;

  // The channel isn't used in calculating the allocationSize, but we still
  // validate it here. This prevents a failed copyTo() call following a
  // successful allocationSize() call.
  if (copy_to_options->planeIndex() >=
      static_cast<uint32_t>(data_->channel_count())) {
    exception_state.ThrowDOMException(DOMExceptionCode::kConstraintError,
                                      "Invalid planeIndex.");
    return 0;
  }

  const uint32_t offset = copy_to_options->frameOffset();
  const uint32_t total_frames = static_cast<uint32_t>(data_->frame_count());

  if (offset >= total_frames) {
    exception_state.ThrowRangeError(String::Format(
        "Frame offset exceeds total frames (%u >= %u).", offset, total_frames));
    return 0;
  }

  const uint32_t available_frames = total_frames - offset;
  const uint32_t frame_count = copy_to_options->hasFrameCount()
                                   ? copy_to_options->frameCount()
                                   : available_frames;

  if (frame_count > available_frames) {
    exception_state.ThrowRangeError(
        String::Format("Frame count exceeds available_frames frames (%u > %u).",
                       frame_count, available_frames));
    return 0;
  }

  uint32_t sample_count = frame_count;

  // For interleaved formats, frames are stored as blocks of samples. Each block
  // has 1 sample per channel, and |sample_count| needs to be adjusted.
  bool overflow = false;
  if (IsInterleaved() && !base::CheckMul(frame_count, data_->channel_count())
                              .AssignIfValid(&sample_count)) {
    overflow = true;
  }

  uint32_t allocation_size;
  if (overflow || !base::CheckMul(sample_count, BytesPerSample())
                       .AssignIfValid(&allocation_size)) {
    exception_state.ThrowTypeError(String::Format(
        "Provided options cause overflow when calculating allocation size."));
    return 0;
  }

  return allocation_size;
}

void AudioData::copyTo(const V8BufferSource* destination,
                       AudioDataCopyToOptions* copy_to_options,
                       ExceptionState& exception_state) {
  if (!data_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot read closed AudioData.");
    return;
  }

  uint32_t copy_size_in_bytes =
      allocationSize(copy_to_options, exception_state);

  if (exception_state.HadException())
    return;

  // Validate destination buffer.
  DOMArrayPiece buffer(destination);
  if (buffer.ByteLength() < static_cast<size_t>(copy_size_in_bytes)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kConstraintError,
                                      "destination is not large enough.");
    return;
  }

  // Copy data.
  uint32_t offset_in_samples = copy_to_options->frameOffset();

  bool overflow = false;
  // Interleaved frames have 1 sample per channel for each block of samples.
  if (IsInterleaved() &&
      !base::CheckMul(copy_to_options->frameOffset(), data_->channel_count())
           .AssignIfValid(&offset_in_samples)) {
    overflow = true;
  }

  uint32_t offset_in_bytes = 0;
  if (overflow || !base::CheckMul(offset_in_samples, BytesPerSample())
                       .AssignIfValid(&offset_in_bytes)) {
    exception_state.ThrowTypeError(String::Format(
        "Provided options cause overflow when calculating offset."));
    return;
  }

  const uint32_t channel = copy_to_options->planeIndex();

  uint8_t* data_start = data_->channel_data()[channel] + offset_in_bytes;
  memcpy(buffer.Bytes(), data_start, copy_size_in_bytes);
}

void AudioData::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
