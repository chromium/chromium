// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/proto_utils.h"

#include <algorithm>

#include "base/big_endian.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/values.h"
#include "media/base/decrypt_config.h"
#include "media/base/encryption_pattern.h"
#include "media/base/encryption_scheme.h"
#include "media/base/timestamp_constants.h"
#include "media/remoting/proto_enum_utils.h"

namespace media {
namespace remoting {

namespace {

constexpr size_t kPayloadVersionFieldSize = sizeof(uint8_t);
constexpr size_t kProtoBufferHeaderSize = sizeof(uint16_t);
constexpr size_t kDataBufferHeaderSize = sizeof(uint32_t);

std::unique_ptr<DecryptConfig> ConvertProtoToDecryptConfig(
    const pb::DecryptConfig& config_message) {
  if (!config_message.has_key_id())
    return nullptr;
  if (!config_message.has_iv())
    return nullptr;

  if (!config_message.has_mode()) {
    // Assume it's unencrypted.
    return nullptr;
  }

  std::vector<SubsampleEntry> entries(config_message.sub_samples_size());
  for (int i = 0; i < config_message.sub_samples_size(); ++i) {
    entries.push_back(
        SubsampleEntry(config_message.sub_samples(i).clear_bytes(),
                       config_message.sub_samples(i).cypher_bytes()));
  }

  if (config_message.mode() == pb::EncryptionMode::kCenc) {
    return DecryptConfig::CreateCencConfig(config_message.key_id(),
                                           config_message.iv(), entries);
  }

  base::Optional<EncryptionPattern> pattern;
  if (config_message.has_crypt_byte_block()) {
    pattern = EncryptionPattern(config_message.crypt_byte_block(),
                                config_message.skip_byte_block());
  }

  if (config_message.mode() == pb::EncryptionMode::kCbcs) {
    return DecryptConfig::CreateCbcsConfig(config_message.key_id(),
                                           config_message.iv(), entries,
                                           std::move(pattern));
  }

  return nullptr;
}

scoped_refptr<DecoderBuffer> ConvertProtoToDecoderBuffer(
    const pb::DecoderBuffer& buffer_message,
    scoped_refptr<DecoderBuffer> buffer) {
  if (buffer_message.is_eos()) {
    VLOG(1) << "EOS data";
    return DecoderBuffer::CreateEOSBuffer();
  }

  if (buffer_message.has_timestamp_usec()) {
    buffer->set_timestamp(
        base::TimeDelta::FromMicroseconds(buffer_message.timestamp_usec()));
  }

  if (buffer_message.has_duration_usec()) {
    buffer->set_duration(
        base::TimeDelta::FromMicroseconds(buffer_message.duration_usec()));
  }
  VLOG(3) << "timestamp:" << buffer_message.timestamp_usec()
          << " duration:" << buffer_message.duration_usec();

  if (buffer_message.has_is_key_frame())
    buffer->set_is_key_frame(buffer_message.is_key_frame());

  if (buffer_message.has_decrypt_config()) {
    buffer->set_decrypt_config(
        ConvertProtoToDecryptConfig(buffer_message.decrypt_config()));
  }

  bool has_discard = false;
  base::TimeDelta front_discard;
  if (buffer_message.has_front_discard_usec()) {
    has_discard = true;
    front_discard =
        base::TimeDelta::FromMicroseconds(buffer_message.front_discard_usec());
  }
  base::TimeDelta back_discard;
  if (buffer_message.has_back_discard_usec()) {
    has_discard = true;
    back_discard =
        base::TimeDelta::FromMicroseconds(buffer_message.back_discard_usec());
  }

  if (has_discard) {
    buffer->set_discard_padding(
        DecoderBuffer::DiscardPadding(front_discard, back_discard));
  }

  if (buffer_message.has_side_data()) {
    buffer->CopySideDataFrom(
        reinterpret_cast<const uint8_t*>(buffer_message.side_data().data()),
        buffer_message.side_data().size());
  }

  return buffer;
}

void ConvertDecryptConfigToProto(const DecryptConfig& decrypt_config,
                                 pb::DecryptConfig* config_message) {
  DCHECK(config_message);

  config_message->set_key_id(decrypt_config.key_id());
  config_message->set_iv(decrypt_config.iv());

  for (const auto& entry : decrypt_config.subsamples()) {
    pb::DecryptConfig::SubSample* sub_sample =
        config_message->add_sub_samples();
    sub_sample->set_clear_bytes(entry.clear_bytes);
    sub_sample->set_cypher_bytes(entry.cypher_bytes);
  }

  config_message->set_mode(
      ToProtoEncryptionMode(decrypt_config.encryption_scheme()).value());
  if (decrypt_config.HasPattern()) {
    config_message->set_crypt_byte_block(
        decrypt_config.encryption_pattern()->crypt_byte_block());
    config_message->set_skip_byte_block(
        decrypt_config.encryption_pattern()->skip_byte_block());
  }
}

void ConvertDecoderBufferToProto(const DecoderBuffer& decoder_buffer,
                                 pb::DecoderBuffer* buffer_message) {
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

  if (decoder_buffer.decrypt_config()) {
    ConvertDecryptConfigToProto(*decoder_buffer.decrypt_config(),
                                buffer_message->mutable_decrypt_config());
  }

  buffer_message->set_front_discard_usec(
      decoder_buffer.discard_padding().first.InMicroseconds());
  buffer_message->set_back_discard_usec(
      decoder_buffer.discard_padding().second.InMicroseconds());

  if (decoder_buffer.side_data_size()) {
    buffer_message->set_side_data(decoder_buffer.side_data(),
                                  decoder_buffer.side_data_size());
  }
}

}  // namespace

scoped_refptr<DecoderBuffer> ByteArrayToDecoderBuffer(const uint8_t* data,
                                                      uint32_t size) {
  base::BigEndianReader reader(reinterpret_cast<const char*>(data), size);
  uint8_t payload_version = 0;
  uint16_t proto_size = 0;
  pb::DecoderBuffer segment;
  uint32_t buffer_size = 0;
  if (reader.ReadU8(&payload_version) && payload_version == 0 &&
      reader.ReadU16(&proto_size) && proto_size < reader.remaining() &&
      segment.ParseFromArray(reader.ptr(), proto_size) &&
      reader.Skip(proto_size) && reader.ReadU32(&buffer_size) &&
      buffer_size <= reader.remaining()) {
    // Deserialize proto buffer. It passes the pre allocated DecoderBuffer into
    // the function because the proto buffer may overwrite DecoderBuffer since
    // it may be EOS buffer.
    scoped_refptr<DecoderBuffer> decoder_buffer = ConvertProtoToDecoderBuffer(
        segment,
        DecoderBuffer::CopyFrom(reinterpret_cast<const uint8_t*>(reader.ptr()),
                                buffer_size));
    return decoder_buffer;
  }

  return nullptr;
}

std::vector<uint8_t> DecoderBufferToByteArray(
    const DecoderBuffer& decoder_buffer) {
  pb::DecoderBuffer decoder_buffer_message;
  ConvertDecoderBufferToProto(decoder_buffer, &decoder_buffer_message);

  size_t decoder_buffer_size =
      decoder_buffer.end_of_stream() ? 0 : decoder_buffer.data_size();
  size_t size = kPayloadVersionFieldSize + kProtoBufferHeaderSize +
                decoder_buffer_message.ByteSize() + kDataBufferHeaderSize +
                decoder_buffer_size;
  std::vector<uint8_t> buffer(size);
  base::BigEndianWriter writer(reinterpret_cast<char*>(buffer.data()),
                               buffer.size());
  if (writer.WriteU8(0) &&
      writer.WriteU16(
          static_cast<uint16_t>(decoder_buffer_message.GetCachedSize())) &&
      decoder_buffer_message.SerializeToArray(
          writer.ptr(), decoder_buffer_message.GetCachedSize()) &&
      writer.Skip(decoder_buffer_message.GetCachedSize()) &&
      writer.WriteU32(decoder_buffer_size)) {
    if (decoder_buffer_size) {
      // DecoderBuffer frame data.
      writer.WriteBytes(reinterpret_cast<const void*>(decoder_buffer.data()),
                        decoder_buffer.data_size());
    }
    return buffer;
  }

  NOTREACHED();
  // Reset buffer since serialization of the data failed.
  buffer.clear();
  return buffer;
}

void ConvertEncryptionSchemeToProto(EncryptionScheme encryption_scheme,
                                    pb::EncryptionScheme* message) {
  DCHECK(message);

  // The remote side only cares about the cipher mode. Setting EncryptionPattern
  // to (0, 0) is fine.
  // TODO(crbug.com/1018923): Upgrade proto to remove EncryptionPattern from
  // Audio/VideoDecoderConfig.
  message->set_mode(
      ToProtoEncryptionSchemeCipherMode(encryption_scheme).value());
  message->set_encrypt_blocks(0);
  message->set_skip_blocks(0);
}

EncryptionScheme ConvertProtoToEncryptionScheme(
    const pb::EncryptionScheme& message) {
  return ToMediaEncryptionScheme(message.mode()).value();
}

void ConvertAudioDecoderConfigToProto(const AudioDecoderConfig& audio_config,
                                      pb::AudioDecoderConfig* audio_message) {
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

  if (!audio_config.extra_data().empty()) {
    audio_message->set_extra_data(audio_config.extra_data().data(),
                                  audio_config.extra_data().size());
  }

  if (audio_config.is_encrypted()) {
    pb::EncryptionScheme* encryption_scheme_message =
        audio_message->mutable_encryption_scheme();
    ConvertEncryptionSchemeToProto(audio_config.encryption_scheme(),
                                   encryption_scheme_message);
  }
}

bool ConvertProtoToAudioDecoderConfig(
    const pb::AudioDecoderConfig& audio_message,
    AudioDecoderConfig* audio_config) {
  DCHECK(audio_config);
  audio_config->Initialize(
      ToMediaAudioCodec(audio_message.codec()).value(),
      ToMediaSampleFormat(audio_message.sample_format()).value(),
      ToMediaChannelLayout(audio_message.channel_layout()).value(),
      audio_message.samples_per_second(),
      std::vector<uint8_t>(audio_message.extra_data().begin(),
                           audio_message.extra_data().end()),
      ConvertProtoToEncryptionScheme(audio_message.encryption_scheme()),
      base::TimeDelta::FromMicroseconds(audio_message.seek_preroll_usec()),
      audio_message.codec_delay());
  return audio_config->IsValidConfig();
}

void ConvertVideoDecoderConfigToProto(const VideoDecoderConfig& video_config,
                                      pb::VideoDecoderConfig* video_message) {
  DCHECK(video_config.IsValidConfig());
  DCHECK(video_message);

  video_message->set_codec(
      ToProtoVideoDecoderConfigCodec(video_config.codec()).value());
  video_message->set_profile(
      ToProtoVideoDecoderConfigProfile(video_config.profile()).value());
  // TODO(dalecurtis): Remove |format| it's now unused.
  video_message->set_format(video_config.alpha_mode() ==
                                    VideoDecoderConfig::AlphaMode::kHasAlpha
                                ? pb::VideoDecoderConfig::PIXEL_FORMAT_I420A
                                : pb::VideoDecoderConfig::PIXEL_FORMAT_I420);

  // TODO(hubbe): Update proto to use color_space_info()
  if (video_config.color_space_info() == VideoColorSpace::JPEG()) {
    video_message->set_color_space(pb::VideoDecoderConfig::COLOR_SPACE_JPEG);
  } else if (video_config.color_space_info() == VideoColorSpace::REC709()) {
    video_message->set_color_space(
        pb::VideoDecoderConfig::COLOR_SPACE_HD_REC709);
  } else if (video_config.color_space_info() == VideoColorSpace::REC601()) {
    video_message->set_color_space(
        pb::VideoDecoderConfig::COLOR_SPACE_SD_REC601);
  } else {
    video_message->set_color_space(
        pb::VideoDecoderConfig::COLOR_SPACE_SD_REC601);
  }

  pb::Size* coded_size_message = video_message->mutable_coded_size();
  coded_size_message->set_width(video_config.coded_size().width());
  coded_size_message->set_height(video_config.coded_size().height());

  pb::Rect* visible_rect_message = video_message->mutable_visible_rect();
  visible_rect_message->set_x(video_config.visible_rect().x());
  visible_rect_message->set_y(video_config.visible_rect().y());
  visible_rect_message->set_width(video_config.visible_rect().width());
  visible_rect_message->set_height(video_config.visible_rect().height());

  pb::Size* natural_size_message = video_message->mutable_natural_size();
  natural_size_message->set_width(video_config.natural_size().width());
  natural_size_message->set_height(video_config.natural_size().height());

  if (!video_config.extra_data().empty()) {
    video_message->set_extra_data(video_config.extra_data().data(),
                                  video_config.extra_data().size());
  }

  if (video_config.is_encrypted()) {
    pb::EncryptionScheme* encryption_scheme_message =
        video_message->mutable_encryption_scheme();
    ConvertEncryptionSchemeToProto(video_config.encryption_scheme(),
                                   encryption_scheme_message);
  }
}

bool ConvertProtoToVideoDecoderConfig(
    const pb::VideoDecoderConfig& video_message,
    VideoDecoderConfig* video_config) {
  DCHECK(video_config);

  // TODO(hubbe): Update pb to use VideoColorSpace
  VideoColorSpace color_space;
  switch (video_message.color_space()) {
    case pb::VideoDecoderConfig::COLOR_SPACE_UNSPECIFIED:
      break;
    case pb::VideoDecoderConfig::COLOR_SPACE_JPEG:
      color_space = VideoColorSpace::JPEG();
      break;
    case pb::VideoDecoderConfig::COLOR_SPACE_HD_REC709:
      color_space = VideoColorSpace::REC709();
      break;
    case pb::VideoDecoderConfig::COLOR_SPACE_SD_REC601:
      color_space = VideoColorSpace::REC601();
      break;
  }
  video_config->Initialize(
      ToMediaVideoCodec(video_message.codec()).value(),
      ToMediaVideoCodecProfile(video_message.profile()).value(),
      IsOpaque(ToMediaVideoPixelFormat(video_message.format()).value())
          ? VideoDecoderConfig::AlphaMode::kIsOpaque
          : VideoDecoderConfig::AlphaMode::kHasAlpha,
      color_space, kNoTransformation,
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
      ConvertProtoToEncryptionScheme(video_message.encryption_scheme()));
  return video_config->IsValidConfig();
}

void ConvertProtoToPipelineStatistics(
    const pb::PipelineStatistics& stats_message,
    PipelineStatistics* stats) {
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
    stats->audio_decoder_info.decoder_name = audio_info.decoder_name();
    stats->audio_decoder_info.is_platform_decoder =
        audio_info.is_platform_decoder();
    stats->audio_decoder_info.has_decrypting_demuxer_stream =
        audio_info.has_decrypting_demuxer_stream();
  }
  if (stats_message.has_video_decoder_info()) {
    auto video_info = stats_message.video_decoder_info();
    stats->video_decoder_info.decoder_name = video_info.decoder_name();
    stats->video_decoder_info.is_platform_decoder =
        video_info.is_platform_decoder();
    stats->video_decoder_info.has_decrypting_demuxer_stream =
        video_info.has_decrypting_demuxer_stream();
  }
  if (stats_message.has_video_frame_duration_average_usec()) {
    stats->video_frame_duration_average = base::TimeDelta::FromMicroseconds(
        stats_message.video_frame_duration_average_usec());
  }
}

void ConvertCdmKeyInfoToProto(
    const CdmKeysInfo& keys_information,
    pb::CdmClientOnSessionKeysChange* key_change_message) {
  for (auto& info : keys_information) {
    pb::CdmKeyInformation* key = key_change_message->add_key_information();
    key->set_key_id(info->key_id.data(), info->key_id.size());
    key->set_status(ToProtoCdmKeyInformation(info->status).value());
    key->set_system_code(info->system_code);
  }
}

void ConvertProtoToCdmKeyInfo(
    const pb::CdmClientOnSessionKeysChange keychange_message,
    CdmKeysInfo* key_information) {
  DCHECK(key_information);
  key_information->reserve(keychange_message.key_information_size());
  for (int i = 0; i < keychange_message.key_information_size(); ++i) {
    const pb::CdmKeyInformation key_info_msg =
        keychange_message.key_information(i);

    std::unique_ptr<CdmKeyInformation> key(new CdmKeyInformation(
        key_info_msg.key_id(),
        ToMediaCdmKeyInformationKeyStatus(key_info_msg.status()).value(),
        key_info_msg.system_code()));
    key_information->push_back(std::move(key));
  }
}

void ConvertCdmPromiseToProto(const CdmPromiseResult& result,
                              pb::CdmPromise* promise_message) {
  promise_message->set_success(result.success());
  if (!result.success()) {
    promise_message->set_exception(
        ToProtoCdmException(result.exception()).value());
    promise_message->set_system_code(result.system_code());
    promise_message->set_error_message(result.error_message());
  }
}

void ConvertCdmPromiseWithSessionIdToProto(const CdmPromiseResult& result,
                                           const std::string& session_id,
                                           pb::CdmPromise* promise_message) {
  ConvertCdmPromiseToProto(result, promise_message);
  promise_message->set_session_id(session_id);
}

void ConvertCdmPromiseWithCdmIdToProto(const CdmPromiseResult& result,
                                       int cdm_id,
                                       pb::CdmPromise* promise_message) {
  ConvertCdmPromiseToProto(result, promise_message);
  promise_message->set_cdm_id(cdm_id);
}

bool ConvertProtoToCdmPromise(const pb::CdmPromise& promise_message,
                              CdmPromiseResult* result) {
  if (!promise_message.has_success())
    return false;

  bool success = promise_message.success();
  if (success) {
    *result = CdmPromiseResult::SuccessResult();
    return true;
  }

  CdmPromise::Exception exception = CdmPromise::Exception::NOT_SUPPORTED_ERROR;
  uint32_t system_code = 0;
  std::string error_message;

  exception = ToCdmPromiseException(promise_message.exception()).value();
  system_code = promise_message.system_code();
  error_message = promise_message.error_message();
  *result = CdmPromiseResult(exception, system_code, error_message);
  return true;
}

bool ConvertProtoToCdmPromiseWithCdmIdSessionId(const pb::RpcMessage& message,
                                                CdmPromiseResult* result,
                                                int* cdm_id,
                                                std::string* session_id) {
  if (!message.has_cdm_promise_rpc())
    return false;

  const auto& promise_message = message.cdm_promise_rpc();
  if (!ConvertProtoToCdmPromise(promise_message, result))
    return false;

  if (cdm_id)
    *cdm_id = promise_message.cdm_id();
  if (session_id)
    *session_id = promise_message.session_id();

  return true;
}

//==============================================================================
CdmPromiseResult::CdmPromiseResult()
    : CdmPromiseResult(CdmPromise::Exception::NOT_SUPPORTED_ERROR, 0, "") {}

CdmPromiseResult::CdmPromiseResult(CdmPromise::Exception exception,
                                   uint32_t system_code,
                                   std::string error_message)
    : success_(false),
      exception_(exception),
      system_code_(system_code),
      error_message_(error_message) {}

CdmPromiseResult::CdmPromiseResult(const CdmPromiseResult& other) = default;

CdmPromiseResult::~CdmPromiseResult() = default;

CdmPromiseResult CdmPromiseResult::SuccessResult() {
  CdmPromiseResult result(static_cast<CdmPromise::Exception>(0), 0, "");
  result.success_ = true;
  return result;
}

}  // namespace remoting
}  // namespace media
