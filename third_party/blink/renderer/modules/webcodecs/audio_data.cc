// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webcodecs/audio_data.h"

#include "base/compiler_specific.h"
#include "base/notreached.h"
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
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

namespace {

std::optional<V8AudioSampleFormat> MediaFormatToBlinkFormat(
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
      return std::nullopt;
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

template <typename SampleType>
void CopyToInterleaved(uint8_t* dest_data,
                       const std::vector<uint8_t*>& src_channels_data,
                       const int frame_offset,
                       const int frames_to_copy) {
  const int channels = static_cast<int>(src_channels_data.size());

  SampleType* dest = reinterpret_cast<SampleType*>(dest_data);
  for (int ch = 0; ch < channels; ++ch) {
    const SampleType* src_start =
        reinterpret_cast<SampleType*>(src_channels_data[ch]) + frame_offset;
    for (int i = 0; i < frames_to_copy; ++i) {
      dest[i * channels + ch] = src_start[i];
    }
  }
}

template <typename SampleType>
void CopyToPlanar(uint8_t* dest_data,
                  const uint8_t* src_data,
                  const int src_channel_count,
                  const int dest_channel_index,
                  const int frame_offset,
                  const int frames_to_copy,
                  ExceptionState& exception_state) {
  SampleType* dest = reinterpret_cast<SampleType*>(dest_data);

  uint32_t offset_in_samples = 0;
  bool overflow = false;
  if (!base::CheckMul(frame_offset, src_channel_count)
           .AssignIfValid(&offset_in_samples)) {
    overflow = true;
    return;
  }

  // Check for potential overflows in the index calculation of the for-loop
  // below.
  if (overflow ||
      !base::CheckMul(frames_to_copy, src_channel_count).IsValid()) {
    exception_state.ThrowTypeError(String::Format(
        "Provided options cause overflow when calculating offset."));
    return;
  }

  const SampleType* src_start =
      reinterpret_cast<const SampleType*>(src_data) + offset_in_samples;
  for (int i = 0; i < frames_to_copy; ++i) {
    dest[i] = src_start[i * src_channel_count + dest_channel_index];
  }
}

media::SampleFormat RemovePlanar(media::SampleFormat format) {
  switch (format) {
    case media::kSampleFormatPlanarU8:
    case media::kSampleFormatU8:
      return media::kSampleFormatU8;

    case media::kSampleFormatPlanarS16:
    case media::kSampleFormatS16:
      return media::kSampleFormatS16;

    case media::kSampleFormatPlanarS32:
    case media::kSampleFormatS32:
      return media::kSampleFormatS32;

    case media::kSampleFormatPlanarF32:
    case media::kSampleFormatF32:
      return media::kSampleFormatF32;

    default:
      NOTREACHED();
  }
}

class ArrayBufferContentsAsAudioExternalMemory
    : public media::AudioBuffer::ExternalMemory {
 public:
  explicit ArrayBufferContentsAsAudioExternalMemory(
      ArrayBufferContents contents,
      base::span<uint8_t> span)
      : media::AudioBuffer::ExternalMemory(span),
        contents_(std::move(contents)) {
    // Check that `span` refers to the memory inside `contents`.
    auto* contents_data = static_cast<uint8_t*>(contents_.Data());
    CHECK_GE(span.data(), contents_data);
    CHECK_LE(span.data() + span.size(), contents_data + contents_.DataLength());
  }

 private:
  ArrayBufferContents contents_;
};

}  // namespace

// static
AudioData* AudioData::Create(ScriptState* script_state,
                             AudioDataInit* init,
                             ExceptionState& exception_state) {
  return MakeGarbageCollected<AudioData>(script_state, init, exception_state);
}

AudioData::AudioData(ScriptState* script_state,
                     AudioDataInit* init,
                     ExceptionState& exception_state)
    : format_(std::nullopt), timestamp_(init->timestamp()) {
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

  auto array_span = AsSpan<uint8_t>(init->data());
  if (!array_span.data()) {
    exception_state.ThrowTypeError("data is detached.");
    return;
  }
  if (total_bytes > array_span.size()) {
    exception_state.ThrowTypeError(
        String::Format("data is too small: needs %u bytes, received %zu.",
                       total_bytes, array_span.size()));
    return;
  }

  // Try if we can transfer `init.data` into `buffer_contents`.
  // We do to make the ctor behave in a spec compliant way regarding transfers,
  // even though we copy the span contents later anyway.
  // TODO(crbug.com/1446808) Modify `media::AudioBuffer` to allow moving
  // `buffer_contents` into it without copying.
  auto* isolate = script_state->GetIsolate();
  auto buffer_contents = TransferArrayBufferForSpan(
      init->transfer(), array_span, exception_state, isolate);
  if (exception_state.HadException()) {
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

  format_ = init->format();
  auto channel_layout =
      init->numberOfChannels() > 8
          // GuesschannelLayout() doesn't know how to guess above 8 channels.
          ? media::CHANNEL_LAYOUT_DISCRETE
          : media::GuessChannelLayout(init->numberOfChannels());

  bool sample_aligned = base::IsAligned(array_span.data(), bytes_per_sample);
  if (buffer_contents.IsValid() && sample_aligned) {
    // The buffer is properly aligned and allowed to be transferred,
    // wrap it as external-memory object and move without a copy.
    auto external_memory =
        std::make_unique<ArrayBufferContentsAsAudioExternalMemory>(
            std::move(buffer_contents), array_span);
    data_ = media::AudioBuffer::CreateFromExternalMemory(
        media_format, channel_layout, init->numberOfChannels(), sample_rate,
        init->numberOfFrames(), base::Microseconds(timestamp_),
        std::move(external_memory));
    CHECK(data_);
    return;
  }

  std::vector<const uint8_t*> channel_ptrs;
  if (media::IsInterleaved(media_format)) {
    // Interleaved data can directly added.
    channel_ptrs.push_back(array_span.data());
  } else {
    // Planar data needs one pointer per channel.
    channel_ptrs.resize(init->numberOfChannels());

    uint32_t plane_size_in_bytes =
        init->numberOfFrames() *
        media::SampleFormatToBytesPerChannel(media_format);

    const uint8_t* plane_start =
        reinterpret_cast<const uint8_t*>(array_span.data());

    for (unsigned ch = 0; ch < init->numberOfChannels(); ++ch) {
      channel_ptrs[ch] = plane_start + ch * plane_size_in_bytes;
    }
  }

  data_ = media::AudioBuffer::CopyFrom(
      media_format, channel_layout, init->numberOfChannels(), sample_rate,
      init->numberOfFrames(), channel_ptrs.data(),
      base::Microseconds(timestamp_));
  CHECK(data_);
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
  data_as_f32_bus_.reset();
  format_ = std::nullopt;
}

int64_t AudioData::timestamp() const {
  return timestamp_;
}

std::optional<V8AudioSampleFormat> AudioData::format() const {
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

  auto format = copy_to_options->hasFormat()
                    ? BlinkFormatToMediaFormat(copy_to_options->format())
                    : data_->sample_format();

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

  const auto dest_format =
      copy_to_options->hasFormat()
          ? BlinkFormatToMediaFormat(copy_to_options->format())
          : data_->sample_format();

  const auto src_format = data_->sample_format();

  // Ignore (de)interleaving, and verify whether we need to convert between
  // sample types.
  const bool needs_conversion =
      RemovePlanar(src_format) != RemovePlanar(dest_format);

  if (needs_conversion) {
    CopyConvert(dest_wrapper, copy_to_options);
    return;
  }

  // Interleave data.
  if (!media::IsInterleaved(src_format) && media::IsInterleaved(dest_format)) {
    CopyToInterleaved(dest_wrapper, copy_to_options);
    return;
  }

  // Deinterleave data.
  if (media::IsInterleaved(src_format) && !media::IsInterleaved(dest_format)) {
    CopyToPlanar(dest_wrapper, copy_to_options, exception_state);
    return;
  }

  // Simple copy, without conversion or (de)interleaving.
  CHECK_LE(copy_to_options->planeIndex(),
           static_cast<uint32_t>(data_->channel_count()));

  size_t src_data_size = 0;
  if (media::IsInterleaved(src_format)) {
    src_data_size = data_->data_size();
  } else {
    src_data_size =
        media::SampleFormatToBytesPerChannel(src_format) * data_->frame_count();
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

  const uint8_t* src_data =
      data_->channel_data()[copy_to_options->planeIndex()];
  const uint8_t* data_start = src_data + offset_in_bytes;
  CHECK_LE(data_start + copy_size_in_bytes, src_data + src_data_size);
  memcpy(dest_wrapper.data(), data_start, copy_size_in_bytes);
}

void AudioData::CopyConvert(base::span<uint8_t> dest,
                            AudioDataCopyToOptions* copy_to_options) {
  const media::SampleFormat dest_format =
      BlinkFormatToMediaFormat(copy_to_options->format());

  // We should only convert when needed.
  CHECK_NE(data_->sample_format(), dest_format);

  // Convert the entire AudioBuffer at once and save it for future copy calls.
  if (!data_as_f32_bus_) {
    data_as_f32_bus_ = media::AudioBuffer::WrapOrCopyToAudioBus(data_);
  }

  // An invalid offset or too many requested frames should have already been
  // caught in allocationSize().
  const uint32_t offset = copy_to_options->frameOffset();
  const uint32_t total_frames =
      static_cast<uint32_t>(data_as_f32_bus_->frames());
  CHECK_LT(offset, total_frames);

  const uint32_t available_frames = total_frames - offset;
  const uint32_t frame_count = copy_to_options->hasFrameCount()
                                   ? copy_to_options->frameCount()
                                   : available_frames;
  CHECK_LE(frame_count, available_frames);

  if (media::IsInterleaved(dest_format)) {
    CHECK_EQ(0u, copy_to_options->planeIndex());

    switch (dest_format) {
      case media::kSampleFormatU8: {
        data_as_f32_bus_
            ->ToInterleavedPartial<media::UnsignedInt8SampleTypeTraits>(
                offset, frame_count, dest.data());
        return;
      }

      case media::kSampleFormatS16: {
        int16_t* dest_data = reinterpret_cast<int16_t*>(dest.data());

        data_as_f32_bus_
            ->ToInterleavedPartial<media::SignedInt16SampleTypeTraits>(
                offset, frame_count, dest_data);
        return;
      }

      case media::kSampleFormatS32: {
        int32_t* dest_data = reinterpret_cast<int32_t*>(dest.data());

        data_as_f32_bus_
            ->ToInterleavedPartial<media::SignedInt32SampleTypeTraits>(
                offset, frame_count, dest_data);
        return;
      }

      case media::kSampleFormatF32: {
        float* dest_data = reinterpret_cast<float*>(dest.data());

        data_as_f32_bus_->ToInterleavedPartial<media::Float32SampleTypeTraits>(
            offset, frame_count, dest_data);
        return;
      }

      default:
        NOTREACHED();
    }
  }

  // Planar conversion.
  const int channel = copy_to_options->planeIndex();

  CHECK_LT(channel, data_as_f32_bus_->channels());
  float* src_data = data_as_f32_bus_->channel(channel);
  float* offset_src_data = src_data + offset;
  CHECK_LE(offset_src_data + frame_count,
           src_data + data_as_f32_bus_->frames());
  switch (dest_format) {
    case media::kSampleFormatPlanarU8: {
      uint8_t* dest_data = dest.data();
      for (uint32_t i = 0; i < frame_count; ++i) {
        dest_data[i] =
            media::UnsignedInt8SampleTypeTraits::FromFloat(offset_src_data[i]);
      }
      return;
    }
    case media::kSampleFormatPlanarS16: {
      int16_t* dest_data = reinterpret_cast<int16_t*>(dest.data());
      for (uint32_t i = 0; i < frame_count; ++i) {
        dest_data[i] =
            media::SignedInt16SampleTypeTraits::FromFloat(offset_src_data[i]);
      }
      return;
    }
    case media::kSampleFormatPlanarS32: {
      int32_t* dest_data = reinterpret_cast<int32_t*>(dest.data());
      for (uint32_t i = 0; i < frame_count; ++i) {
        dest_data[i] =
            media::SignedInt32SampleTypeTraits::FromFloat(offset_src_data[i]);
      }
      return;
    }
    case media::kSampleFormatPlanarF32: {
      int32_t* dest_data = reinterpret_cast<int32_t*>(dest.data());
      CHECK_LE(offset_src_data + frame_count,
               src_data + data_as_f32_bus_->frames());
      memcpy(dest_data, offset_src_data, sizeof(float) * frame_count);
      return;
    }
    default:
      NOTREACHED();
  }
}

void AudioData::CopyToInterleaved(base::span<uint8_t> dest,
                                  AudioDataCopyToOptions* copy_to_options) {
  const media::SampleFormat src_format = data_->sample_format();
  const media::SampleFormat dest_format =
      BlinkFormatToMediaFormat(copy_to_options->format());
  CHECK_EQ(RemovePlanar(src_format), RemovePlanar(dest_format));
  CHECK(!media::IsInterleaved(src_format));
  CHECK(media::IsInterleaved(dest_format));
  CHECK_EQ(0u, copy_to_options->planeIndex());

  const int frame_offset = copy_to_options->frameOffset();
  const int available_frames = data_->frame_count() - frame_offset;
  const int frames_to_copy = copy_to_options->hasFrameCount()
                                 ? copy_to_options->frameCount()
                                 : available_frames;
  const auto& channel_data = data_->channel_data();

  CHECK_LE(frame_offset + frames_to_copy, data_->frame_count());

  switch (dest_format) {
    case media::kSampleFormatU8:
      ::blink::CopyToInterleaved<uint8_t>(dest.data(), channel_data,
                                          frame_offset, frames_to_copy);
      return;
    case media::kSampleFormatS16:
      ::blink::CopyToInterleaved<int16_t>(dest.data(), channel_data,
                                          frame_offset, frames_to_copy);
      return;
    case media::kSampleFormatS32:
      ::blink::CopyToInterleaved<int32_t>(dest.data(), channel_data,
                                          frame_offset, frames_to_copy);
      return;
    case media::kSampleFormatF32:
      ::blink::CopyToInterleaved<float>(dest.data(), channel_data, frame_offset,
                                        frames_to_copy);
      return;
    default:
      NOTREACHED();
  }
}

void AudioData::CopyToPlanar(base::span<uint8_t> dest,
                             AudioDataCopyToOptions* copy_to_options,
                             ExceptionState& exception_state) {
  const media::SampleFormat src_format = data_->sample_format();
  const media::SampleFormat dest_format =
      BlinkFormatToMediaFormat(copy_to_options->format());
  CHECK_EQ(RemovePlanar(src_format), RemovePlanar(dest_format));
  CHECK(media::IsInterleaved(src_format));
  CHECK(!media::IsInterleaved(dest_format));

  const int frame_offset = copy_to_options->frameOffset();
  const int available_frames = data_->frame_count() - frame_offset;
  const int frames_to_copy = copy_to_options->hasFrameCount()
                                 ? copy_to_options->frameCount()
                                 : available_frames;
  const int channels = data_->channel_count();
  const int channel_index = copy_to_options->planeIndex();

  // Interleaved data could all be in the same plane.
  CHECK_EQ(1u, data_->channel_data().size());
  const uint8_t* src_data = data_->channel_data()[0];

  CHECK_LE(frame_offset + frames_to_copy, data_->frame_count());

  switch (dest_format) {
    case media::kSampleFormatPlanarU8:
      ::blink::CopyToPlanar<uint8_t>(dest.data(), src_data, channels,
                                     channel_index, frame_offset,
                                     frames_to_copy, exception_state);
      return;
    case media::kSampleFormatPlanarS16:
      ::blink::CopyToPlanar<int16_t>(dest.data(), src_data, channels,
                                     channel_index, frame_offset,
                                     frames_to_copy, exception_state);
      return;
    case media::kSampleFormatPlanarS32:
      ::blink::CopyToPlanar<int32_t>(dest.data(), src_data, channels,
                                     channel_index, frame_offset,
                                     frames_to_copy, exception_state);
      return;
    case media::kSampleFormatPlanarF32:
      ::blink::CopyToPlanar<float>(dest.data(), src_data, channels,
                                   channel_index, frame_offset, frames_to_copy,
                                   exception_state);
      return;
    default:
      NOTREACHED();
  }
}

void AudioData::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
