// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/cast/openscreen/remoting_proto_utils.h"

#include <algorithm>

#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/containers/span_writer.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "media/base/encryption_scheme.h"
#include "media/base/timestamp_constants.h"
#include "media/cast/openscreen/remoting_proto_enum_utils.h"

namespace media::cast {
namespace {

constexpr size_t kPayloadVersionFieldSize = sizeof(uint8_t);
constexpr size_t kProtoBufferHeaderSize = sizeof(uint16_t);
constexpr size_t kDataBufferHeaderSize = sizeof(uint32_t);

scoped_refptr<media::DecoderBuffer> ConvertProtoToDecoderBuffer(
    const openscreen::cast::DecoderBuffer& buffer_message,
    scoped_refptr<media::DecoderBuffer> buffer) {
  if (buffer_message.is_eos()) {
    VLOG(1) << "EOS data";
    return media::DecoderBuffer::CreateEOSBuffer();
  }

  if (buffer_message.has_timestamp_usec()) {
    buffer->set_timestamp(base::Microseconds(buffer_message.timestamp_usec()));
  }

  if (buffer_message.has_duration_usec()) {
    buffer->set_duration(base::Microseconds(buffer_message.duration_usec()));
  }
  VLOG(3) << "timestamp:" << buffer_message.timestamp_usec()
          << " duration:" << buffer_message.duration_usec();

  if (buffer_message.has_is_key_frame()) {
    buffer->set_is_key_frame(buffer_message.is_key_frame());
  }

  bool has_discard = false;
  base::TimeDelta front_discard;
  if (buffer_message.has_front_discard_usec()) {
    has_discard = true;
    front_discard = base::Microseconds(buffer_message.front_discard_usec());
  }
  base::TimeDelta back_discard;
  if (buffer_message.has_back_discard_usec()) {
    has_discard = true;
    back_discard = base::Microseconds(buffer_message.back_discard_usec());
  }

  if (has_discard) {
    buffer->set_discard_padding(
        media::DecoderBuffer::DiscardPadding(front_discard, back_discard));
  }

  if (buffer_message.has_side_data()) {
    const uint8_t* side_ptr =
        reinterpret_cast<const uint8_t*>(buffer_message.side_data().data());
    buffer->WritableSideData().alpha_data.assign(
        side_ptr, side_ptr + buffer_message.side_data().size());
  }

  return buffer;
}

void ConvertDecoderBufferToProto(
    const media::DecoderBuffer& decoder_buffer,
    openscreen::cast::DecoderBuffer* buffer_message) {
  if (decoder_buffer.end_of_stream()) {
    buffer_message->set_is_eos(true);
    return;
  }

  VLOG(3) << "timestamp:" << decoder_buffer.timestamp().InMicroseconds()
          << " duration:" << decoder_buffer.duration().InMicroseconds();
  buffer_message->set_timestamp_usec(
      decoder_buffer.timestamp().InMicroseconds());
  buffer_message->set_duration_usec(decoder_buffer.duration().InMicroseconds());
  buffer_message->set_is_key_frame(decoder_buffer.is_key_frame());

  buffer_message->set_front_discard_usec(
      decoder_buffer.discard_padding().first.InMicroseconds());
  buffer_message->set_back_discard_usec(
      decoder_buffer.discard_padding().second.InMicroseconds());

  if (decoder_buffer.has_side_data() &&
      !decoder_buffer.side_data()->alpha_data.empty()) {
    buffer_message->set_side_data(
        decoder_buffer.side_data()->alpha_data.data(),
        decoder_buffer.side_data()->alpha_data.size());
  }
}

}  // namespace

scoped_refptr<media::DecoderBuffer> ByteArrayToDecoderBuffer(
    base::span<const uint8_t> data) {
  auto reader = base::SpanReader(data);
  uint8_t payload_version = 0u;
  uint16_t proto_size = 0u;
  openscreen::cast::DecoderBuffer segment;
  base::span<const uint8_t> segment_span;
  uint32_t buffer_size = 0u;
  base::span<const uint8_t> buffer_span;
  if (reader.ReadU8BigEndian(payload_version) && payload_version == 0 &&
      reader.ReadU16BigEndian(proto_size) &&
      base::OptionalUnwrapTo(reader.Skip(proto_size), segment_span) &&
      segment.ParseFromArray(segment_span.data(), segment_span.size()) &&
      reader.ReadU32BigEndian(buffer_size) &&
      base::OptionalUnwrapTo(reader.Skip(buffer_size), buffer_span)) {
    // Deserialize proto buffer. It passes the pre allocated DecoderBuffer into
    // the function because the proto buffer may overwrite DecoderBuffer since
    // it may be EOS buffer.
    scoped_refptr<media::DecoderBuffer> decoder_buffer =
        ConvertProtoToDecoderBuffer(
            segment, media::DecoderBuffer::CopyFrom(buffer_span));
    return decoder_buffer;
  }

  return nullptr;
}

std::vector<uint8_t> DecoderBufferToByteArray(
    const media::DecoderBuffer& decoder_buffer) {
  openscreen::cast::DecoderBuffer decoder_buffer_message;
  ConvertDecoderBufferToProto(decoder_buffer, &decoder_buffer_message);

  size_t decoder_buffer_size =
      decoder_buffer.end_of_stream() ? 0 : decoder_buffer.size();
  size_t size = kPayloadVersionFieldSize + kProtoBufferHeaderSize +
                decoder_buffer_message.ByteSize() + kDataBufferHeaderSize +
                decoder_buffer_size;
  auto message_cached_size =
      // GetCachedSize() is only valid after ByteSize() is called above.
      base::checked_cast<uint16_t>(decoder_buffer_message.GetCachedSize());
  std::vector<uint8_t> buffer(size);
  auto writer = base::SpanWriter(base::span(buffer));
  if (writer.WriteU8BigEndian(0) &&
      writer.WriteU16BigEndian(message_cached_size) &&
      [&] {
        std::optional<base::span<uint8_t>> span =
            writer.Skip(message_cached_size);
        return span.has_value() && decoder_buffer_message.SerializeToArray(
                                       span->data(), span->size());
      }() &&
      writer.WriteU32BigEndian(decoder_buffer_size)) {
    if (decoder_buffer_size) {
      // DecoderBuffer frame data.
      writer.Write(base::span(decoder_buffer));
    }
    return buffer;
  }

  // Reset buffer since serialization of the data failed.
  buffer.clear();
  return buffer;
}

void ConvertAudioDecoderConfigToProto(
    const media::AudioDecoderConfig& audio_config,
    openscreen::cast::AudioDecoderConfig* audio_message) {
  DCHECK(audio_config.IsValidConfig());
  DCHECK(audio_message);

  audio_message->set_codec(
      ToProtoAudioDecoderConfigCodec(audio_config.codec()).value());
  audio_message->set_sample_format(
      ToProtoAudioDecoderConfigSampleFormat(audio_config.sample_format())
          .value());
  audio_message->set_channel_layout(
      ToProtoAudioDecoderConfigChannelLayout(audio_config.channel_layout())
          .value());

  audio_message->set_samples_per_second(audio_config.samples_per_second());
  audio_message->set_seek_preroll_usec(
      audio_config.seek_preroll().InMicroseconds());
  audio_message->set_codec_delay(audio_config.codec_delay());

  // We choose to not expose the "aac_extra_data" field to the remoting
  // protobuf, because it is due to an internal Chrome bug. Instead, use the
  // "extra_data" field as receivers should expect.
  //
  // TODO(crbug.com/40198159): Remove all references to "aac_extra_data" when it
  // is removed as part of a media/ cleanup.
#if DCHECK_IS_ON()
  if (!audio_config.extra_data().empty() &&
      !audio_config.aac_extra_data().empty() &&
      audio_config.extra_data() != audio_config.aac_extra_data()) {
    LOG(WARNING) << "mismatch between extra data and AAC extra data.";
  }
#endif
  const bool isAac = audio_config.codec() == media::AudioCodec::kAAC;
  const std::vector<uint8_t>& extra_data =
      isAac ? audio_config.aac_extra_data() : audio_config.extra_data();

  if (!extra_data.empty()) {
    audio_message->set_extra_data(extra_data.data(), extra_data.size());
  }
}

bool ConvertProtoToAudioDecoderConfig(
    const openscreen::cast::AudioDecoderConfig& audio_message,
    media::AudioDecoderConfig* audio_config) {
  DCHECK(audio_config);

  // Either "extra_data" or "aac_extra_data" should be populated but not both.
  const bool isAac =
      audio_message.codec() == openscreen::cast::AudioDecoderConfig::kCodecAAC;
  const auto extra_data = base::make_span(audio_message.extra_data().begin(),
                                          audio_message.extra_data().end());
  audio_config->Initialize(
      ToMediaAudioCodec(audio_message.codec()).value(),
      ToMediaSampleFormat(audio_message.sample_format()).value(),
      ToMediaChannelLayout(audio_message.channel_layout()).value(),
      audio_message.samples_per_second(),
      isAac ? std::vector<uint8_t>{}
            : std::vector<uint8_t>(extra_data.begin(), extra_data.end()),
      media::EncryptionScheme::kUnencrypted,
      base::Microseconds(audio_message.seek_preroll_usec()),
      audio_message.codec_delay());

  // TODO(crbug.com/40198159): Remove all references to "aac_extra_data" when it
  // is removed as part of a media/ cleanup.
  if (isAac) {
    audio_config->set_aac_extra_data(
        std::vector<uint8_t>(extra_data.begin(), extra_data.end()));
  }

  return audio_config->IsValidConfig();
}

void ConvertVideoDecoderConfigToProto(
    const media::VideoDecoderConfig& video_config,
    openscreen::cast::VideoDecoderConfig* video_message) {
  DCHECK(video_config.IsValidConfig());
  DCHECK(video_message);

  video_message->set_codec(
      ToProtoVideoDecoderConfigCodec(video_config.codec()).value());
  video_message->set_profile(
      ToProtoVideoDecoderConfigProfile(video_config.profile()).value());
  // TODO(dalecurtis): Remove |format| it's now unused.
  video_message->set_format(
      video_config.alpha_mode() ==
              media::VideoDecoderConfig::AlphaMode::kHasAlpha
          ? openscreen::cast::VideoDecoderConfig::PIXEL_FORMAT_I420A
          : openscreen::cast::VideoDecoderConfig::PIXEL_FORMAT_I420);

  // TODO(hubbe): Update proto to use color_space_info()
  if (video_config.color_space_info() == media::VideoColorSpace::JPEG()) {
    video_message->set_color_space(
        openscreen::cast::VideoDecoderConfig::COLOR_SPACE_JPEG);
  } else if (video_config.color_space_info() ==
             media::VideoColorSpace::REC709()) {
    video_message->set_color_space(
        openscreen::cast::VideoDecoderConfig::COLOR_SPACE_HD_REC709);
  } else if (video_config.color_space_info() ==
             media::VideoColorSpace::REC601()) {
    video_message->set_color_space(
        openscreen::cast::VideoDecoderConfig::COLOR_SPACE_SD_REC601);
  } else {
    video_message->set_color_space(
        openscreen::cast::VideoDecoderConfig::COLOR_SPACE_SD_REC601);
  }

  openscreen::cast::Size* coded_size_message =
      video_message->mutable_coded_size();
  coded_size_message->set_width(video_config.coded_size().width());
  coded_size_message->set_height(video_config.coded_size().height());

  openscreen::cast::Rect* visible_rect_message =
      video_message->mutable_visible_rect();
  visible_rect_message->set_x(video_config.visible_rect().x());
  visible_rect_message->set_y(video_config.visible_rect().y());
  visible_rect_message->set_width(video_config.visible_rect().width());
  visible_rect_message->set_height(video_config.visible_rect().height());

  openscreen::cast::Size* natural_size_message =
      video_message->mutable_natural_size();
  natural_size_message->set_width(video_config.natural_size().width());
  natural_size_message->set_height(video_config.natural_size().height());

  if (!video_config.extra_data().empty()) {
    video_message->set_extra_data(video_config.extra_data().data(),
                                  video_config.extra_data().size());
  }
}

bool ConvertProtoToVideoDecoderConfig(
    const openscreen::cast::VideoDecoderConfig& video_message,
    media::VideoDecoderConfig* video_config) {
  DCHECK(video_config);

  // TODO(hubbe): Update pb to use VideoColorSpace
  media::VideoColorSpace color_space;
  switch (video_message.color_space()) {
    case openscreen::cast::VideoDecoderConfig::COLOR_SPACE_UNSPECIFIED:
      break;
    case openscreen::cast::VideoDecoderConfig::COLOR_SPACE_JPEG:
      color_space = media::VideoColorSpace::JPEG();
      break;
    case openscreen::cast::VideoDecoderConfig::COLOR_SPACE_HD_REC709:
      color_space = media::VideoColorSpace::REC709();
      break;
    case openscreen::cast::VideoDecoderConfig::COLOR_SPACE_SD_REC601:
      color_space = media::VideoColorSpace::REC601();
      break;
  }
  video_config->Initialize(
      ToMediaVideoCodec(video_message.codec()).value(),
      ToMediaVideoCodecProfile(video_message.profile()).value(),
      IsOpaque(ToMediaVideoPixelFormat(video_message.format()).value())
          ? media::VideoDecoderConfig::AlphaMode::kIsOpaque
          : media::VideoDecoderConfig::AlphaMode::kHasAlpha,
      color_space, media::kNoTransformation,
      gfx::Size(video_message.coded_size().width(),
                video_message.coded_size().height()),
      gfx::Rect(video_message.visible_rect().x(),
                video_message.visible_rect().y(),
                video_message.visible_rect().width(),
                video_message.visible_rect().height()),
      gfx::Size(video_message.natural_size().width(),
                video_message.natural_size().height()),
      std::vector<uint8_t>(video_message.extra_data().begin(),
                           video_message.extra_data().end()),
      media::EncryptionScheme::kUnencrypted);
  return video_config->IsValidConfig();
}

void ConvertProtoToPipelineStatistics(
    const openscreen::cast::PipelineStatistics& stats_message,
    media::PipelineStatistics* stats) {
  stats->audio_bytes_decoded = stats_message.audio_bytes_decoded();
  stats->video_bytes_decoded = stats_message.video_bytes_decoded();
  stats->video_frames_decoded = stats_message.video_frames_decoded();
  stats->video_frames_dropped = stats_message.video_frames_dropped();
  stats->audio_memory_usage = stats_message.audio_memory_usage();
  stats->video_memory_usage = stats_message.video_memory_usage();
  // HACK: Set the following to prevent "disable video when hidden" logic in
  // media::blink::WebMediaPlayerImpl.
  stats->video_keyframe_distance_average = base::TimeDelta::Max();

  // This field is not used by the rpc field.
  stats->video_frames_decoded_power_efficient = 0;

  // The following fields were added after the initial message definition. Check
  // that sender provided the values.
  if (stats_message.has_audio_decoder_info()) {
    auto audio_info = stats_message.audio_decoder_info();
    stats->audio_pipeline_info.decoder_type =
        static_cast<media::AudioDecoderType>(audio_info.decoder_type());
    stats->audio_pipeline_info.is_platform_decoder =
        audio_info.is_platform_decoder();
    stats->audio_pipeline_info.has_decrypting_demuxer_stream = false;
    stats->audio_pipeline_info.encryption_type = media::EncryptionType::kClear;
  }
  if (stats_message.has_video_decoder_info()) {
    auto video_info = stats_message.video_decoder_info();
    stats->video_pipeline_info.decoder_type =
        static_cast<media::VideoDecoderType>(video_info.decoder_type());
    stats->video_pipeline_info.is_platform_decoder =
        video_info.is_platform_decoder();
    stats->video_pipeline_info.has_decrypting_demuxer_stream = false;
    stats->video_pipeline_info.encryption_type = media::EncryptionType::kClear;
  }
  if (stats_message.has_video_frame_duration_average_usec()) {
    stats->video_frame_duration_average =
        base::Microseconds(stats_message.video_frame_duration_average_usec());
  }
}

}  // namespace media::cast
