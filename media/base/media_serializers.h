// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_SERIALIZERS_H_
#define MEDIA_BASE_MEDIA_SERIALIZERS_H_

#include <sstream>
#include <vector>

#include "base/location.h"
#include "base/strings/stringprintf.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/buffering_state.h"
#include "media/base/decoder.h"
#include "media/base/media_serializers_base.h"
#include "media/base/status.h"
#include "media/base/status_codes.h"
#include "media/base/text_track_config.h"
#include "media/base/video_decoder_config.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/hdr_metadata.h"

namespace media {

namespace internal {

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

// Serialize vectors of things
template <typename VecType>
struct MediaSerializer<std::vector<VecType>> {
  static base::Value Serialize(const std::vector<VecType>& vec) {
    base::Value result(base::Value::Type::LIST);
    for (const VecType& value : vec)
      result.Append(MediaSerializer<VecType>::Serialize(value));
    return result;
  }
};

// serialize optional types
template <typename OptType>
struct MediaSerializer<base::Optional<OptType>> {
  static base::Value Serialize(const base::Optional<OptType>& opt) {
    return opt ? MediaSerializer<OptType>::Serialize(opt.value())
               : base::Value("unset");  // TODO(tmathmeyer) maybe empty string?
  }
};

// Sometimes raw strings wont template match to a char*.
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
    return MediaSerializer<double>::Serialize(static_cast<double>(value));
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
  result.SetKey(NAME, MediaSerialize(CONSTEXPR))

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
    return base::Value(value.ToGfxColorSpace().ToString());
  }
};

// Class (complex)
template <>
struct MediaSerializer<gfx::HDRMetadata> {
  static base::Value Serialize(const gfx::HDRMetadata& value) {
    // TODO(tmathmeyer) serialize more fields here potentially.
    base::Value result(base::Value::Type::DICTIONARY);
    FIELD_SERIALIZE("luminance range",
                    base::StringPrintf("%.2f => %.2f",
                                       value.mastering_metadata.luminance_min,
                                       value.mastering_metadata.luminance_max));
    FIELD_SERIALIZE("primaries",
                    base::StringPrintf(
                        "[r:%.4f,%.4f, g:%.4f,%.4f, b:%.4f,%.4f, wp:%.4f,%.4f]",
                        value.mastering_metadata.primary_r.x(),
                        value.mastering_metadata.primary_r.y(),
                        value.mastering_metadata.primary_g.x(),
                        value.mastering_metadata.primary_g.y(),
                        value.mastering_metadata.primary_b.x(),
                        value.mastering_metadata.primary_b.y(),
                        value.mastering_metadata.white_point.x(),
                        value.mastering_metadata.white_point.y()));
    return result;
  }
};

// Class (complex)
template <>
struct MediaSerializer<AudioDecoderConfig> {
  static base::Value Serialize(const AudioDecoderConfig& value) {
    base::Value result(base::Value::Type::DICTIONARY);
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
    result.SetStringKey("seek preroll", preroll.str());

    return result;
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
    base::Value result(base::Value::Type::DICTIONARY);
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
    return result;
  }
};

// Class (complex)
template <>
struct MediaSerializer<TextTrackConfig> {
  static base::Value Serialize(const TextTrackConfig& value) {
    base::Value result(base::Value::Type::DICTIONARY);
    FIELD_SERIALIZE("kind", value.kind());
    FIELD_SERIALIZE("language", value.language());
    if (value.label().length())
      FIELD_SERIALIZE("label", value.label());
    return result;
  }
};

// enum (simple)
template <>
struct MediaSerializer<TextKind> {
  static base::Value Serialize(const TextKind value) {
    switch (value) {
      case kTextSubtitles:
        return base::Value("Subtitles");
      case kTextCaptions:
        return base::Value("Captions");
      case kTextDescriptions:
        return base::Value("Descriptions");
      case kTextMetadata:
        return base::Value("Metadata");
      case kTextChapters:
        return base::Value("Chapters");
      case kTextNone:
        return base::Value("None");
    }
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
    base::Value result(base::Value::Type::DICTIONARY);
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
      result.SetBoolKey("for_suspended_start", value.suspended_start);

    return result;
  }
};

// enum (simple)
template <>
struct MediaSerializer<StatusCode> {
  static inline base::Value Serialize(StatusCode code) {
    return base::Value(static_cast<int>(code));
  }
};

// Class (complex)
template <>
struct MediaSerializer<Status> {
  static base::Value Serialize(const Status& status) {
    if (status.is_ok())
      return base::Value("Ok");

    base::Value result(base::Value::Type::DICTIONARY);
    FIELD_SERIALIZE("status_code", status.code());
    FIELD_SERIALIZE("status_message", status.message());
    FIELD_SERIALIZE("stack", status.data_->frames);
    FIELD_SERIALIZE("data", status.data_->data);
    FIELD_SERIALIZE("causes", status.data_->causes);
    return result;
  }
};

// Class (complex)
template <>
struct MediaSerializer<base::Location> {
  static base::Value Serialize(const base::Location& value) {
    base::Value result(base::Value::Type::DICTIONARY);
    FIELD_SERIALIZE("file", value.file_name() ? value.file_name() : "unknown");
    FIELD_SERIALIZE("line", value.line_number());
    return result;
  }
};

#undef FIELD_SERIALIZE

}  // namespace internal

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_SERIALIZERS_H_
