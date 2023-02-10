// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/audio_data.h"

#include "base/compiler_specific.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/limits.h"
#include "media/base/sample_format.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_data_copy_to_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_data_init.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

absl::optional<V8AudioSampleFormat> MediaFormatToBlinkFormat(
    media::SampleFormat media_format) {
  using FormatEnum = V8AudioSampleFormat::Enum;

  DCHECK(media_format != media::SampleFormat::kUnknownSampleFormat);

  switch (media_format) {
    case media::SampleFormat::kSampleFormatU8:
      return V8AudioSampleFormat(FormatEnum::kU8);

    case media::SampleFormat::kSampleFormatS16:
      return V8AudioSampleFormat(FormatEnum::kS16);

    case media::SampleFormat::kSampleFormatS24:
      // TODO(crbug.com/1231633): ffmpeg automatically converts kSampleFormatS24
      // to kSampleFormatS32, but we do not update our labelling. It's ok to
      // treat the kSampleFormatS24 as kSampleFormatS32 until we update the
      // labelling, since our code already treats S24 as S32.
      [[fallthrough]];
    case media::SampleFormat::kSampleFormatS32:
      return V8AudioSampleFormat(FormatEnum::kS32);

    case media::SampleFormat::kSampleFormatF32:
      return V8AudioSampleFormat(FormatEnum::kF32);

    case media::SampleFormat::kSampleFormatPlanarU8:
      return V8AudioSampleFormat(FormatEnum::kU8Planar);

    case media::SampleFormat::kSampleFormatPlanarS16:
      return V8AudioSampleFormat(FormatEnum::kS16Planar);

    case media::SampleFormat::kSampleFormatPlanarS32:
      return V8AudioSampleFormat(FormatEnum::kS32Planar);

    case media::SampleFormat::kSampleFormatPlanarF32:
      return V8AudioSampleFormat(FormatEnum::kF32Planar);

    case media::SampleFormat::kSampleFormatAc3:
    case media::SampleFormat::kSampleFormatEac3:
    case media::SampleFormat::kSampleFormatMpegHAudio:
    case media::SampleFormat::kUnknownSampleFormat:
    case media::SampleFormat::kSampleFormatDts:
    case media::SampleFormat::kSampleFormatDtsxP2:
    case media::SampleFormat::kSampleFormatIECDts:
    case media::SampleFormat::kSampleFormatDtse:
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

    case FormatEnum::kS32:
      return media::SampleFormat::kSampleFormatS32;

    case FormatEnum::kF32:
      return media::SampleFormat::kSampleFormatF32;

    case FormatEnum::kU8Planar:
      return media::SampleFormat::kSampleFormatPlanarU8;

    case FormatEnum::kS16Planar:
      return media::SampleFormat::kSampleFormatPlanarS16;

    case FormatEnum::kS32Planar:
      return media::SampleFormat::kSampleFormatPlanarS32;

    case FormatEnum::kF32Planar:
      return media::SampleFormat::kSampleFormatPlanarF32;
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

  if (init->numberOfChannels() == 0) {
    exception_state.ThrowTypeError("numberOfChannels must be greater than 0.");
    return;
  }

  if (init->numberOfChannels() > media::limits::kMaxChannels) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        String::Format("numberOfChannels exceeds supported implementation "
                       "limits: %u vs %u.",
                       init->numberOfChannels(), media::limits::kMaxChannels));
    return;
  }

  if (init->numberOfFrames() == 0) {
    exception_state.ThrowTypeError("numberOfFrames must be greater than 0.");
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

  auto data_wrapper = AsSpan<const uint8_t>(init->data());
  if (!data_wrapper.data()) {
    exception_state.ThrowTypeError("data is detached.");
    return;
  }
  if (total_bytes > data_wrapper.size()) {
    exception_state.ThrowTypeError(
        String::Format("data is too small: needs %u bytes, received %zu.",
                       total_bytes, data_wrapper.size()));
    return;
  }

  int sample_rate = base::saturated_cast<int>(init->sampleRate());
  if (sample_rate < media::limits::kMinSampleRate ||
      sample_rate > media::limits::kMaxSampleRate) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        String::Format("sampleRate is outside of supported implementation "
                       "limits: need between %u and %u, received %d.",
                       media::limits::kMinSampleRate,
                       media::limits::kMaxSampleRate, sample_rate));
    return;
  }

  std::vector<const uint8_t*> wrapped_data;
  if (media::IsInterleaved(media_format)) {
    // Interleaved data can directly added.
    wrapped_data.push_back(data_wrapper.data());
  } else {
    // Planar data needs one pointer per channel.
    wrapped_data.resize(init->numberOfChannels());

    uint32_t plane_size_in_bytes =
        init->numberOfFrames() *
        media::SampleFormatToBytesPerChannel(media_format);

    const uint8_t* plane_start =
        reinterpret_cast<const uint8_t*>(data_wrapper.data());

    for (unsigned ch = 0; ch < init->numberOfChannels(); ++ch)
      wrapped_data[ch] = plane_start + ch * plane_size_in_bytes;
  }

  format_ = init->format();

  auto channel_layout =
      init->numberOfChannels() > 8
          // GuesschannelLayout() doesn't know how to guess above 8 channels.
          ? media::CHANNEL_LAYOUT_DISCRETE
          : media::GuessChannelLayout(init->numberOfChannels());

  data_ = media::AudioBuffer::CopyFrom(
      media_format, channel_layout, init->numberOfChannels(), sample_rate,
      init->numberOfFrames(), wrapped_data.data(),
      base::Microseconds(timestamp_));
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

AudioData::~AudioData() = default;

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
  temp_bus_.reset();
  format_ = absl::nullopt;
}

int64_t AudioData::timestamp() const {
  return timestamp_;
}

absl::optional<V8AudioSampleFormat> AudioData::format() const {
  return format_;
}

float AudioData::sampleRate() const {
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

uint32_t AudioData::allocationSize(AudioDataCopyToOptions* copy_to_options,
                                   ExceptionState& exception_state) {
  if (!data_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "AudioData is closed.");
    return 0;
  }

  auto format = data_->sample_format();
  if (copy_to_options->hasFormat()) {
    auto dest_format = BlinkFormatToMediaFormat(copy_to_options->format());
    if (dest_format != data_->sample_format() &&
        dest_format != media::SampleFormat::kSampleFormatPlanarF32) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotSupportedError,
          "AudioData currently only supports copy conversion to f32-planar.");
      return 0;
    }

    format = dest_format;
  }

  // Interleaved formats only have one plane, despite many channels.
  uint32_t max_plane_index =
      media::IsInterleaved(format) ? 0 : data_->channel_count() - 1;

  // The channel isn't used in calculating the allocationSize, but we still
  // validate it here. This prevents a failed copyTo() call following a
  // successful allocationSize() call.
  if (copy_to_options->planeIndex() > max_plane_index) {
    exception_state.ThrowRangeError("Invalid planeIndex.");
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
  if (media::IsInterleaved(format) &&
      !base::CheckMul(frame_count, data_->channel_count())
           .AssignIfValid(&sample_count)) {
    overflow = true;
  }

  uint32_t allocation_size;
  if (overflow || !base::CheckMul(sample_count,
                                  media::SampleFormatToBytesPerChannel(format))
                       .AssignIfValid(&allocation_size)) {
    exception_state.ThrowTypeError(String::Format(
        "Provided options cause overflow when calculating allocation size."));
    return 0;
  }

  return allocation_size;
}

void AudioData::copyTo(const AllowSharedBufferSource* destination,
                       AudioDataCopyToOptions* copy_to_options,
                       ExceptionState& exception_state) {
  if (!data_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot copy closed AudioData.");
    return;
  }

  uint32_t copy_size_in_bytes =
      allocationSize(copy_to_options, exception_state);

  if (exception_state.HadException())
    return;

  // Validate destination buffer.
  auto dest_wrapper = AsSpan<uint8_t>(destination);
  if (!dest_wrapper.data()) {
    exception_state.ThrowRangeError("destination is detached.");
    return;
  }
  if (dest_wrapper.size() < static_cast<size_t>(copy_size_in_bytes)) {
    exception_state.ThrowRangeError("destination is not large enough.");
    return;
  }

  auto dest_format = copy_to_options->hasFormat()
                         ? BlinkFormatToMediaFormat(copy_to_options->format())
                         : data_->sample_format();

  const uint8_t* src_data = nullptr;
  size_t src_data_size = 0;
  if (dest_format != data_->sample_format()) {
    // NOTE: The call to allocationSize() above ensures only passthrough or
    // f32-planar are possible by this point.
    DCHECK_EQ(dest_format, media::SampleFormat::kSampleFormatPlanarF32);

    // In case of format conversion to float32, convert the entire AudioBuffer
    // at once and save it for future copy calls.
    if (!temp_bus_)
      temp_bus_ = media::AudioBuffer::WrapOrCopyToAudioBus(data_);

    CHECK_LE(copy_to_options->planeIndex(),
             static_cast<uint32_t>(temp_bus_->channels()));

    src_data = reinterpret_cast<const uint8_t*>(
        temp_bus_->channel(copy_to_options->planeIndex()));

    src_data_size = sizeof(float) * temp_bus_->frames();
  } else {
    CHECK_LE(copy_to_options->planeIndex(),
             static_cast<uint32_t>(data_->channel_count()));

    src_data = data_->channel_data()[copy_to_options->planeIndex()];

    if (media::IsInterleaved(data_->sample_format())) {
      src_data_size = data_->data_size();
    } else {
      src_data_size =
          media::SampleFormatToBytesPerChannel(data_->sample_format()) *
          data_->frame_count();
    }
  }

  // Copy data.
  uint32_t offset_in_samples = copy_to_options->frameOffset();

  bool overflow = false;
  // Interleaved frames have 1 sample per channel for each block of samples.
  if (media::IsInterleaved(dest_format) &&
      !base::CheckMul(copy_to_options->frameOffset(), data_->channel_count())
           .AssignIfValid(&offset_in_samples)) {
    overflow = true;
  }

  uint32_t offset_in_bytes = 0;
  if (overflow ||
      !base::CheckMul(offset_in_samples,
                      media::SampleFormatToBytesPerChannel(dest_format))
           .AssignIfValid(&offset_in_bytes)) {
    exception_state.ThrowTypeError(String::Format(
        "Provided options cause overflow when calculating offset."));
    return;
  }

  const uint8_t* data_start = src_data + offset_in_bytes;
  CHECK_LE(data_start + copy_size_in_bytes, src_data + src_data_size);
  memcpy(dest_wrapper.data(), data_start, copy_size_in_bytes);
}

void AudioData::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
