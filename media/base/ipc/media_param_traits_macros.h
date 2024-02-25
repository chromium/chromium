// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_IPC_MEDIA_PARAM_TRAITS_MACROS_H_
#define MEDIA_BASE_IPC_MEDIA_PARAM_TRAITS_MACROS_H_

#include "build/build_config.h"
#include "ipc/ipc_message_macros.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_parameters.h"
#include "media/base/buffering_state.h"
#include "media/base/cdm_config.h"
#include "media/base/cdm_key_information.h"
#include "media/base/cdm_promise.h"
#include "media/base/channel_layout.h"
#include "media/base/container_names.h"
#include "media/base/content_decryption_module.h"
#include "media/base/decoder.h"
#include "media/base/decoder_status.h"
#include "media/base/decrypt_config.h"
#include "media/base/decryptor.h"
#include "media/base/demuxer_stream.h"
#include "media/base/eme_constants.h"
#include "media/base/encryption_scheme.h"
#include "media/base/media_content_type.h"
#include "media/base/media_log_record.h"
#include "media/base/media_status.h"
#include "media/base/output_device_info.h"
#include "media/base/overlay_info.h"
#include "media/base/pipeline_status.h"
#include "media/base/sample_format.h"
#include "media/base/subsample_entry.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "media/base/video_transformation.h"
#include "media/base/video_types.h"
#include "media/base/waiting.h"
#include "media/base/watch_time_keys.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/platform/web_fullscreen_video_status.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/ipc/color/gfx_param_traits_macros.h"

#if BUILDFLAG(ENABLE_MEDIA_DRM_STORAGE)
#include "media/base/media_drm_key_type.h"
#endif  // BUILDFLAG(ENABLE_MEDIA_DRM_STORAGE)

// Enum traits.

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebFullscreenVideoStatus,
                          blink::WebFullscreenVideoStatus::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(media::AudioCodec, media::AudioCodec::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(media::AudioCodecProfile,
                          media::AudioCodecProfile::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(media::AudioLatency::Type,
                          media::AudioLatency::Type::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(media::AudioParameters::Format,
                          media::AudioParameters::AUDIO_FORMAT_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(media::BufferingState,
                          media::BufferingState::BUFFERING_STATE_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(
    media::BufferingStateChangeReason,
    media::BufferingStateChangeReason::BUFFERING_STATE_CHANGE_REASON_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media::CdmMessageType,
                          media::CdmMessageType::MESSAGE_TYPE_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media::CdmPromise::Exception,
                          media::CdmPromise::Exception::EXCEPTION_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media::CdmSessionType,
                          media::CdmSessionType::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(media::ChannelLayout, media::CHANNEL_LAYOUT_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media::Decryptor::Status,
                          media::Decryptor::Status::kStatusMax)

IPC_ENUM_TRAITS_MAX_VALUE(media::Decryptor::StreamType,
                          media::Decryptor::StreamType::kStreamTypeMax)

IPC_ENUM_TRAITS_MAX_VALUE(media::DemuxerStream::Status,
                          media::DemuxerStream::kStatusMax)

IPC_ENUM_TRAITS_MAX_VALUE(media::DemuxerStream::Type,
                          media::DemuxerStream::TYPE_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media::EmeInitDataType, media::EmeInitDataType::MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media::EncryptionScheme,
                          media::EncryptionScheme::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(media::HdcpVersion,
                          media::HdcpVersion::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(media::MediaContentType, media::MediaContentType::kMax)

IPC_ENUM_TRAITS_MAX_VALUE(media::MediaLogRecord::Type,
                          media::MediaLogRecord::Type::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(media::MediaStatus::State,
                          media::MediaStatus::State::kStateMax)

IPC_ENUM_TRAITS_MAX_VALUE(media::OutputDeviceStatus,
                          media::OUTPUT_DEVICE_STATUS_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media::PipelineStatusCodes,
                          media::PipelineStatusCodes::PIPELINE_STATUS_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media::SampleFormat, media::kSampleFormatMax)

IPC_ENUM_TRAITS_MAX_VALUE(media::VideoCodec, media::VideoCodec::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(media::WaitingReason, media::WaitingReason::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(media::WatchTimeKey,
                          media::WatchTimeKey::kWatchTimeKeyMax)

IPC_ENUM_TRAITS_MIN_MAX_VALUE(media::VideoCodecProfile,
                              media::VIDEO_CODEC_PROFILE_MIN,
                              media::VIDEO_CODEC_PROFILE_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media::VideoDecoderType,
                          media::VideoDecoderType::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(media::AudioDecoderType,
                          media::AudioDecoderType::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(media::VideoPixelFormat, media::PIXEL_FORMAT_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media::VideoRotation, media::VIDEO_ROTATION_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(
    media::container_names::MediaContainerName,
    media::container_names::MediaContainerName::kMaxValue)

#if BUILDFLAG(ENABLE_MEDIA_DRM_STORAGE)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(media::MediaDrmKeyType,
                              media::MediaDrmKeyType::MIN,
                              media::MediaDrmKeyType::MAX)
#endif  // BUILDFLAG(ENABLE_MEDIA_DRM_STORAGE)

IPC_ENUM_TRAITS_VALIDATE(
    media::VideoColorSpace::PrimaryID,
    static_cast<int>(value) ==
        static_cast<int>(
            media::VideoColorSpace::GetPrimaryID(static_cast<int>(value))))

IPC_ENUM_TRAITS_VALIDATE(
    media::VideoColorSpace::TransferID,
    static_cast<int>(value) ==
        static_cast<int>(
            media::VideoColorSpace::GetTransferID(static_cast<int>(value))))

IPC_ENUM_TRAITS_VALIDATE(
    media::VideoColorSpace::MatrixID,
    static_cast<int>(value) ==
        static_cast<int>(
            media::VideoColorSpace::GetMatrixID(static_cast<int>(value))))

// Struct traits.

IPC_STRUCT_TRAITS_BEGIN(media::CdmConfig)
  IPC_STRUCT_TRAITS_MEMBER(key_system)
  IPC_STRUCT_TRAITS_MEMBER(allow_distinctive_identifier)
  IPC_STRUCT_TRAITS_MEMBER(allow_persistent_state)
  IPC_STRUCT_TRAITS_MEMBER(use_hw_secure_codecs)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::MediaLogRecord)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(params)
  IPC_STRUCT_TRAITS_MEMBER(time)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::SubsampleEntry)
  IPC_STRUCT_TRAITS_MEMBER(clear_bytes)
  IPC_STRUCT_TRAITS_MEMBER(cypher_bytes)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::OverlayInfo)
  IPC_STRUCT_TRAITS_MEMBER(routing_token)
  IPC_STRUCT_TRAITS_MEMBER(is_fullscreen)
  IPC_STRUCT_TRAITS_MEMBER(is_persistent_video)
IPC_STRUCT_TRAITS_END()

#endif  // MEDIA_BASE_IPC_MEDIA_PARAM_TRAITS_MACROS_H_
