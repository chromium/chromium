// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_SERIALIZERS_H_
#define MEDIA_BASE_MEDIA_SERIALIZERS_H_

#include <optional>
#include <sstream>
#include <vector>

#include "base/location.h"
#include "base/strings/stringprintf.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/buffering_state.h"
#include "media/base/cdm_config.h"
#include "media/base/decoder.h"
#include "media/base/media_serializers_base.h"
#include "media/base/media_track.h"
#include "media/base/renderer.h"
#include "media/base/status.h"
#include "media/base/video_decoder_config.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/hdr_metadata.h"

namespace media::internal {

// Serializing any const or reference combination.
template <typename T>
struct MediaSerializer<const T> {
  static base::Value Serialize(const T& value) {
    return MediaSerializer<T>::Serialize(value);
  }
};

template <typename T>
struct MediaSerializer<T&> {
  static base::Value Serialize(const T& value) {
    return MediaSerializer<T>::Serialize(value);
  }
};

// Serialize default value.
template <>
struct MediaSerializer<base::Value> {
  static base::Value Serialize(const base::Value& value) {
    return value.Clone();
  }
};

// Serialize list value.
template <>
struct MediaSerializer<base::Value::List> {
  static base::Value Serialize(const base::Value::List& value) {
    return base::Value(value.Clone());
  }
};

// Serialize vectors of things
template <typename VecType>
struct MediaSerializer<std::vector<VecType>> {
  static base::Value Serialize(const std::vector<VecType>& vec) {
    base::Value::List result;
    for (const VecType& value : vec)
      result.Append(MediaSerializer<VecType>::Serialize(value));
    return base::Value(std::move(result));
  }
};

// Serialize unique pointers
template <typename T>
struct MediaSerializer<std::unique_ptr<T>> {
  static base::Value Serialize(const std::unique_ptr<T>& ptr) {
    if (!ptr)
      return base::Value("nullptr");
    return MediaSerializer<T>::Serialize(*ptr);
  }
};

// serialize optional types
template <typename OptType>
struct MediaSerializer<std::optional<OptType>> {
  static base::Value Serialize(const std::optional<OptType>& opt) {
    return opt ? MediaSerializer<OptType>::Serialize(opt.value())
               : base::Value("unset");  // TODO(tmathmeyer) maybe empty string?
  }
};

// Sometimes raw strings won't template match to a char*.
template <int len>
struct MediaSerializer<char[len]> {
  static inline base::Value Serialize(const char* code) {
    return base::Value(code);
  }
};

// Can't send non-finite double values to a base::Value.
template <>
struct MediaSerializer<double> {
  static inline base::Value Serialize(double value) {
    return std::isfinite(value) ? base::Value(value) : base::Value("unknown");
  }
};

template <>
struct MediaSerializer<int64_t> {
  static inline base::Value Serialize(int64_t value) {
    std::stringstream stream;
    stream << "0x" << std::hex << value;
    return MediaSerializer<std::string>::Serialize(stream.str());
  }
};

template <>
struct MediaSerializer<uint32_t> {
  static inline base::Value Serialize(uint32_t value) {
    std::stringstream stream;
    stream << "0x" << std::hex << value;
    return MediaSerializer<std::string>::Serialize(stream.str());
  }
};

// Just upcast this to get the NaN check.
template <>
struct MediaSerializer<float> {
  static inline base::Value Serialize(float value) {
    return MediaSerializer<double>::Serialize(value);
  }
};

// Serialization for chromium-specific types.
// Each serializer should be commented like:
// Class/Enum (simple/complex)
// where Classes should take constref arguments, and "simple" methods should
// be declared inline.

// the FIELD_SERIALIZE method can be used whenever the result is a dict named
// |result|.
#define FIELD_SERIALIZE(NAME, CONSTEXPR) \
  result.Set(NAME, MediaSerialize(CONSTEXPR))

// Class (simple)
template <>
struct MediaSerializer<gfx::Size> {
  static inline base::Value Serialize(const gfx::Size& value) {
    return base::Value(value.ToString());
  }
};

// Class (simple)
template <>
struct MediaSerializer<gfx::Rect> {
  static inline base::Value Serialize(const gfx::Rect& value) {
    return base::Value(value.ToString());
  }
};

// enum (simple)
template <>
struct MediaSerializer<base::TimeDelta> {
  static inline base::Value Serialize(const base::TimeDelta value) {
    return MediaSerializer<double>::Serialize(value.InSecondsF());
  }
};

// enum (simple)
template <>
struct MediaSerializer<base::Time> {
  static inline base::Value Serialize(const base::Time value) {
    std::stringstream formatted;
    formatted << value;
    return MediaSerializer<std::string>::Serialize(formatted.str());
  }
};

// Enum (simple)
template <>
struct MediaSerializer<RendererType> {
  static inline base::Value Serialize(RendererType value) {
    return base::Value(GetRendererName(value));
  }
};

// Enum (simple)
template <>
struct MediaSerializer<VideoDecoderType> {
  static inline base::Value Serialize(VideoDecoderType value) {
    return base::Value(GetDecoderName(value));
  }
};

// Enum (simple)
template <>
struct MediaSerializer<AudioDecoderType> {
  static inline base::Value Serialize(AudioDecoderType value) {
    return base::Value(GetDecoderName(value));
  }
};

// Enum (simple)
template <>
struct MediaSerializer<AudioCodec> {
  static inline base::Value Serialize(AudioCodec value) {
    return base::Value(GetCodecName(value));
  }
};

// Enum (simple)
template <>
struct MediaSerializer<AudioCodecProfile> {
  static inline base::Value Serialize(AudioCodecProfile value) {
    return base::Value(GetProfileName(value));
  }
};

// Enum (simple)
template <>
struct MediaSerializer<VideoCodec> {
  static inline base::Value Serialize(VideoCodec value) {
    return base::Value(GetCodecName(value));
  }
};

// Enum (simple)
template <>
struct MediaSerializer<VideoCodecProfile> {
  static inline base::Value Serialize(VideoCodecProfile value) {
    return base::Value(GetProfileName(value));
  }
};

// Enum (simple)
template <>
struct MediaSerializer<ChannelLayout> {
  static inline base::Value Serialize(ChannelLayout value) {
    return base::Value(ChannelLayoutToString(value));
  }
};

// Enum (simple)
template <>
struct MediaSerializer<SampleFormat> {
  static inline base::Value Serialize(SampleFormat value) {
    return base::Value(SampleFormatToString(value));
  }
};

// Class (complex)
template <>
struct MediaSerializer<CdmConfig> {
  static base::Value Serialize(const CdmConfig& value) {
    base::Value::Dict result;
    FIELD_SERIALIZE("key_system", value.key_system);
    FIELD_SERIALIZE("allow_distinctive_identifier",
                    value.allow_distinctive_identifier);
    FIELD_SERIALIZE("allow_persistent_state", value.allow_persistent_state);
    FIELD_SERIALIZE("use_hw_secure_codecs", value.use_hw_secure_codecs);
    return base::Value(std::move(result));
  }
};

// Enum (complex)
template <>
struct MediaSerializer<EncryptionScheme> {
  static base::Value Serialize(const EncryptionScheme& value) {
    std::ostringstream encryptionSchemeString;
    encryptionSchemeString << value;
    return base::Value(encryptionSchemeString.str());
  }
};

// Class (complex)
template <>
struct MediaSerializer<VideoTransformation> {
  static base::Value Serialize(const VideoTransformation& value) {
    std::string rotation = VideoRotationToString(value.rotation);
    if (value.mirrored)
      rotation += ", mirrored";
    return base::Value(rotation);
  }
};

// Class (simple)
template <>
struct MediaSerializer<VideoColorSpace> {
  static inline base::Value Serialize(const VideoColorSpace& value) {
    base::Value::Dict result;
    FIELD_SERIALIZE("primaries", value.primaries);
    FIELD_SERIALIZE("transfer", value.transfer);
    FIELD_SERIALIZE("matrix", value.matrix);
    FIELD_SERIALIZE("range", value.range);
    return base::Value(std::move(result));
  }
};

// Class (complex)
template <>
struct MediaSerializer<gfx::HDRMetadata> {
  static base::Value Serialize(const gfx::HDRMetadata& value) {
    base::Value::Dict result;
    if (value.smpte_st_2086.has_value()) {
      FIELD_SERIALIZE("smpte_st_2086", value.smpte_st_2086->ToString());
    }
    if (value.cta_861_3.has_value()) {
      FIELD_SERIALIZE("cta_861_3", value.cta_861_3->ToString());
    }
    if (value.ndwl.has_value()) {
      FIELD_SERIALIZE("ndwl", value.ndwl->ToString());
    }
    if (value.extended_range.has_value()) {
      FIELD_SERIALIZE("extended_range", value.extended_range->ToString());
    }
    return base::Value(std::move(result));
  }
};

// Class (complex)
template <>
struct MediaSerializer<AudioDecoderConfig> {
  static base::Value Serialize(const AudioDecoderConfig& value) {
    base::Value::Dict result;
    FIELD_SERIALIZE("codec", value.codec());
    FIELD_SERIALIZE("profile", value.profile());
    FIELD_SERIALIZE("bytes per channel", value.bytes_per_channel());
    FIELD_SERIALIZE("channel layout", value.channel_layout());
    FIELD_SERIALIZE("channels", value.channels());
    FIELD_SERIALIZE("samples per second", value.samples_per_second());
    FIELD_SERIALIZE("sample format", value.sample_format());
    FIELD_SERIALIZE("bytes per frame", value.bytes_per_frame());
    FIELD_SERIALIZE("codec delay", value.codec_delay());
    FIELD_SERIALIZE("has extra data", !value.extra_data().empty());
    FIELD_SERIALIZE("encryption scheme", value.encryption_scheme());
    FIELD_SERIALIZE("discard decoder delay",
                    value.should_discard_decoder_delay());

    // TODO(tmathmeyer) drop the units, let the frontend handle it.
    // use ostringstreams because windows & linux have _different types_
    // defined for int64_t, (long vs long long) so format specifiers dont work.
    std::ostringstream preroll;
    preroll << value.seek_preroll().InMicroseconds() << "us";
    result.Set("seek preroll", preroll.str());

    return base::Value(std::move(result));
  }
};

// Enum (simple)
template <>
struct MediaSerializer<VideoDecoderConfig::AlphaMode> {
  static inline base::Value Serialize(VideoDecoderConfig::AlphaMode value) {
    return base::Value(value == VideoDecoderConfig::AlphaMode::kHasAlpha
                           ? "has_alpha"
                           : "is_opaque");
  }
};

// Class (complex)
template <>
struct MediaSerializer<VideoDecoderConfig> {
  static base::Value Serialize(const VideoDecoderConfig& value) {
    base::Value::Dict result;
    FIELD_SERIALIZE("codec", value.codec());
    FIELD_SERIALIZE("profile", value.profile());
    FIELD_SERIALIZE("alpha mode", value.alpha_mode());
    FIELD_SERIALIZE("coded size", value.coded_size());
    FIELD_SERIALIZE("visible rect", value.visible_rect());
    FIELD_SERIALIZE("natural size", value.natural_size());
    FIELD_SERIALIZE("has extra data", !value.extra_data().empty());
    FIELD_SERIALIZE("encryption scheme", value.encryption_scheme());
    FIELD_SERIALIZE("orientation", value.video_transformation());
    FIELD_SERIALIZE("color space", value.color_space_info());
    FIELD_SERIALIZE("hdr metadata", value.hdr_metadata());
    return base::Value(std::move(result));
  }
};

// enum (simple)
template <>
struct MediaSerializer<BufferingState> {
  static inline base::Value Serialize(const BufferingState value) {
    return base::Value(value == BufferingState::BUFFERING_HAVE_ENOUGH
                           ? "BUFFERING_HAVE_ENOUGH"
                           : "BUFFERING_HAVE_NOTHING");
  }
};

// enum (complex)
template <>
struct MediaSerializer<BufferingStateChangeReason> {
  static base::Value Serialize(const BufferingStateChangeReason value) {
    switch (value) {
      case DEMUXER_UNDERFLOW:
        return base::Value("DEMUXER_UNDERFLOW");
      case DECODER_UNDERFLOW:
        return base::Value("DECODER_UNDERFLOW");
      case REMOTING_NETWORK_CONGESTION:
        return base::Value("REMOTING_NETWORK_CONGESTION");
      case BUFFERING_CHANGE_REASON_UNKNOWN:
        return base::Value("BUFFERING_CHANGE_REASON_UNKNOWN");
    }
  }
};

// Class (complex)
template <SerializableBufferingStateType T>
struct MediaSerializer<SerializableBufferingState<T>> {
  static base::Value Serialize(const SerializableBufferingState<T>& value) {
    base::Value::Dict result;
    FIELD_SERIALIZE("state", value.state);

    switch (value.reason) {
      case DEMUXER_UNDERFLOW:
      case DECODER_UNDERFLOW:
      case REMOTING_NETWORK_CONGESTION:
        FIELD_SERIALIZE("reason", value.reason);
        break;

      // Don't write anything here if the reason is unknown.
      case BUFFERING_CHANGE_REASON_UNKNOWN:
        break;
    }

    if (T == SerializableBufferingStateType::kPipeline)
      result.Set("for_suspended_start", value.suspended_start);

    return base::Value(std::move(result));
  }
};

// Class (complex)
template <typename T>
struct MediaSerializer<TypedStatus<T>> {
  static base::Value Serialize(const TypedStatus<T>& status) {
    // TODO: replace this with some kind of static "description"
    // of the default type, instead of "Ok".
    if (status.is_ok())
      return base::Value("Ok");
    return MediaSerialize(status.data_);
  }
};

// Class (complex)
template <>
struct MediaSerializer<StatusData> {
  static base::Value Serialize(const StatusData& status) {
    base::Value::Dict result;
    // TODO: replace code with a stringified version, since
    // this representation will only go to medialog anyway.
    FIELD_SERIALIZE(StatusConstants::kCodeKey, status.code);
    FIELD_SERIALIZE(StatusConstants::kGroupKey, status.group);
    FIELD_SERIALIZE(StatusConstants::kMsgKey, status.message);
    FIELD_SERIALIZE(StatusConstants::kStackKey, status.frames);
    FIELD_SERIALIZE(StatusConstants::kDataKey, status.data);
    if (status.cause)
      FIELD_SERIALIZE(StatusConstants::kCauseKey, *status.cause);
    return base::Value(std::move(result));
  }
};

// Class (complex)
template <>
struct MediaSerializer<base::Location> {
  static base::Value Serialize(const base::Location& value) {
    base::Value::Dict result;
    FIELD_SERIALIZE(StatusConstants::kFileKey,
                    value.file_name() ? value.file_name() : "unknown");
    FIELD_SERIALIZE(StatusConstants::kLineKey, value.line_number());
    return base::Value(std::move(result));
  }
};

#define ENUM_CASE_TO_STRING(ENUM_NAME) \
  case ENUM_NAME:                      \
    return base::Value(##ENUM_NAME);

#define ENUM_CLASS_CASE_TO_STRING(ENUM_CLASS, ENUM_NAME) \
  case ENUM_CLASS::ENUM_NAME:                            \
    return base::Value(#ENUM_NAME);

// Enum (simple)
template <>
struct MediaSerializer<VideoColorSpace::PrimaryID> {
  static inline base::Value Serialize(VideoColorSpace::PrimaryID value) {
    switch (value) {
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::PrimaryID, INVALID);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::PrimaryID, BT709);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::PrimaryID, UNSPECIFIED);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::PrimaryID, BT470M);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::PrimaryID, BT470BG);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::PrimaryID, SMPTE170M);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::PrimaryID, SMPTE240M);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::PrimaryID, FILM);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::PrimaryID, BT2020);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::PrimaryID, SMPTEST428_1);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::PrimaryID, SMPTEST431_2);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::PrimaryID, SMPTEST432_1);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::PrimaryID, EBU_3213_E);
    }
  }
};

// Enum (simple)
template <>
struct MediaSerializer<VideoColorSpace::TransferID> {
  static inline base::Value Serialize(VideoColorSpace::TransferID value) {
    switch (value) {
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::TransferID, INVALID);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::TransferID, BT709);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::TransferID, UNSPECIFIED);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::TransferID, GAMMA22);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::TransferID, GAMMA28);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::TransferID, SMPTE170M);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::TransferID, SMPTE240M);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::TransferID, LINEAR);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::TransferID, LOG);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::TransferID, LOG_SQRT);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::TransferID, IEC61966_2_4);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::TransferID, BT1361_ECG);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::TransferID, IEC61966_2_1);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::TransferID, BT2020_10);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::TransferID, BT2020_12);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::TransferID, SMPTEST2084);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::TransferID, SMPTEST428_1);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::TransferID, ARIB_STD_B67);
    }
  }
};

// Enum (simple)
template <>
struct MediaSerializer<VideoColorSpace::MatrixID> {
  static inline base::Value Serialize(VideoColorSpace::MatrixID value) {
    switch (value) {
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::MatrixID, RGB);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::MatrixID, BT709);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::MatrixID, UNSPECIFIED);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::MatrixID, FCC);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::MatrixID, BT470BG);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::MatrixID, SMPTE170M);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::MatrixID, SMPTE240M);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::MatrixID, YCOCG);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::MatrixID, BT2020_NCL);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::MatrixID, BT2020_CL);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::MatrixID, YDZDX);
      ENUM_CLASS_CASE_TO_STRING(VideoColorSpace::MatrixID, INVALID);
    }
  }
};

// Enum (simple)
template <>
struct MediaSerializer<gfx::ColorSpace::RangeID> {
  static inline base::Value Serialize(gfx::ColorSpace::RangeID value) {
    switch (value) {
      ENUM_CLASS_CASE_TO_STRING(gfx::ColorSpace::RangeID, INVALID);
      ENUM_CLASS_CASE_TO_STRING(gfx::ColorSpace::RangeID, LIMITED);
      ENUM_CLASS_CASE_TO_STRING(gfx::ColorSpace::RangeID, FULL);
      ENUM_CLASS_CASE_TO_STRING(gfx::ColorSpace::RangeID, DERIVED);
    }
  }
};

template <>
struct MediaSerializer<MediaTrack::Id> {
  static inline base::Value Serialize(MediaTrack::Id id) {
    return base::Value(id.value());
  }
};

#undef FIELD_SERIALIZE

}  // namespace media::internal

#endif  // MEDIA_BASE_MEDIA_SERIALIZERS_H_
