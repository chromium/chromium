// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cdm_type_conversion.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

// Note: Unexpected values must be handled explicitly since some of these
// functions may be used at either side of the CDM interface, and it's possible
// invalid values are passed in. For example, Chromium loading an older CDM, or
// the CDM is loaded by a non-Chromium browser.

namespace media {

namespace {

cdm::ColorRange ToCdmColorRange(gfx::ColorSpace::RangeID range) {
  switch (range) {
    case gfx::ColorSpace::RangeID::INVALID:
      return cdm::ColorRange::kInvalid;
    case gfx::ColorSpace::RangeID::LIMITED:
      return cdm::ColorRange::kLimited;
    case gfx::ColorSpace::RangeID::FULL:
      return cdm::ColorRange::kFull;
    case gfx::ColorSpace::RangeID::DERIVED:
      return cdm::ColorRange::kDerived;
  }

  NOTREACHED_IN_MIGRATION() << "Unexpected color range";
  return cdm::ColorRange::kInvalid;
}

gfx::ColorSpace::RangeID ToGfxColorRange(cdm::ColorRange range) {
  switch (range) {
    case cdm::ColorRange::kInvalid:
      return gfx::ColorSpace::RangeID::INVALID;
    case cdm::ColorRange::kLimited:
      return gfx::ColorSpace::RangeID::LIMITED;
    case cdm::ColorRange::kFull:
      return gfx::ColorSpace::RangeID::FULL;
    case cdm::ColorRange::kDerived:
      return gfx::ColorSpace::RangeID::DERIVED;
  }

  NOTREACHED_IN_MIGRATION() << "Unexpected color range";
  return gfx::ColorSpace::RangeID::INVALID;
}

}  // namespace

// Color Converters

cdm::ColorSpace ToCdmColorSpace(const VideoColorSpace& color_space) {
  // Cast is okay because both VideoColorSpace and cdm::ColorSpace follow the
  // standard ISO 23001-8:2016.
  return {base::checked_cast<uint8_t>(color_space.primaries),
          base::checked_cast<uint8_t>(color_space.transfer),
          base::checked_cast<uint8_t>(color_space.matrix),
          ToCdmColorRange(color_space.range)};
}

VideoColorSpace ToMediaColorSpace(const cdm::ColorSpace& color_space) {
  return VideoColorSpace(color_space.primary_id, color_space.transfer_id,
                         color_space.matrix_id,
                         ToGfxColorRange(color_space.range));
}

// CDM Converters

cdm::HdcpVersion ToCdmHdcpVersion(HdcpVersion hdcp_version) {
  switch (hdcp_version) {
    case HdcpVersion::kHdcpVersionNone:
      return cdm::kHdcpVersionNone;
    case HdcpVersion::kHdcpVersion1_0:
      return cdm::kHdcpVersion1_0;
    case HdcpVersion::kHdcpVersion1_1:
      return cdm::kHdcpVersion1_1;
    case HdcpVersion::kHdcpVersion1_2:
      return cdm::kHdcpVersion1_2;
    case HdcpVersion::kHdcpVersion1_3:
      return cdm::kHdcpVersion1_3;
    case HdcpVersion::kHdcpVersion1_4:
      return cdm::kHdcpVersion1_4;
    case HdcpVersion::kHdcpVersion2_0:
      return cdm::kHdcpVersion2_0;
    case HdcpVersion::kHdcpVersion2_1:
      return cdm::kHdcpVersion2_1;
    case HdcpVersion::kHdcpVersion2_2:
      return cdm::kHdcpVersion2_2;
    case HdcpVersion::kHdcpVersion2_3:
      return cdm::kHdcpVersion2_3;
  }

  NOTREACHED_IN_MIGRATION() << "Unexpected HdcpVersion";
  return cdm::kHdcpVersion2_3;
}

cdm::SessionType ToCdmSessionType(CdmSessionType session_type) {
  switch (session_type) {
    case CdmSessionType::kTemporary:
      return cdm::kTemporary;
    case CdmSessionType::kPersistentLicense:
      return cdm::kPersistentLicense;
  }

  NOTREACHED_IN_MIGRATION()
      << "Unexpected session type " << static_cast<int>(session_type);
  return cdm::kTemporary;
}

CdmSessionType ToMediaSessionType(cdm::SessionType session_type) {
  switch (session_type) {
    case cdm::kTemporary:
      return CdmSessionType::kTemporary;
    case cdm::kPersistentLicense:
      return CdmSessionType::kPersistentLicense;
    // TODO(crbug.com/40170205): Remove after `kPersistentUsageRecord` is
    // removed from the CDM interface.
    case cdm::kPersistentUsageRecord:
      break;
  }

  NOTREACHED_IN_MIGRATION() << "Unexpected cdm::SessionType " << session_type;
  return CdmSessionType::kTemporary;
}

cdm::InitDataType ToCdmInitDataType(EmeInitDataType init_data_type) {
  switch (init_data_type) {
    case EmeInitDataType::CENC:
      return cdm::kCenc;
    case EmeInitDataType::KEYIDS:
      return cdm::kKeyIds;
    case EmeInitDataType::WEBM:
      return cdm::kWebM;
    case EmeInitDataType::UNKNOWN:
      break;
  }

  NOTREACHED_IN_MIGRATION() << "Unexpected EmeInitDataType";
  return cdm::kKeyIds;
}

EmeInitDataType ToEmeInitDataType(cdm::InitDataType init_data_type) {
  switch (init_data_type) {
    case cdm::kCenc:
      return EmeInitDataType::CENC;
    case cdm::kKeyIds:
      return EmeInitDataType::KEYIDS;
    case cdm::kWebM:
      return EmeInitDataType::WEBM;
  }

  NOTREACHED_IN_MIGRATION()
      << "Unexpected cdm::InitDataType " << init_data_type;
  return EmeInitDataType::UNKNOWN;
}

CdmKeyInformation::KeyStatus ToMediaKeyStatus(cdm::KeyStatus status) {
  switch (status) {
    case cdm::kUsable:
      return CdmKeyInformation::USABLE;
    case cdm::kInternalError:
      return CdmKeyInformation::INTERNAL_ERROR;
    case cdm::kExpired:
      return CdmKeyInformation::EXPIRED;
    case cdm::kOutputRestricted:
      return CdmKeyInformation::OUTPUT_RESTRICTED;
    case cdm::kOutputDownscaled:
      return CdmKeyInformation::OUTPUT_DOWNSCALED;
    case cdm::kStatusPending:
      return CdmKeyInformation::KEY_STATUS_PENDING;
    case cdm::kReleased:
      return CdmKeyInformation::RELEASED;
  }

  NOTREACHED_IN_MIGRATION() << "Unexpected cdm::KeyStatus " << status;
  return CdmKeyInformation::INTERNAL_ERROR;
}

cdm::KeyStatus ToCdmKeyStatus(CdmKeyInformation::KeyStatus status) {
  switch (status) {
    case CdmKeyInformation::KeyStatus::USABLE:
      return cdm::kUsable;
    case CdmKeyInformation::KeyStatus::INTERNAL_ERROR:
      return cdm::kInternalError;
    case CdmKeyInformation::KeyStatus::EXPIRED:
      return cdm::kExpired;
    case CdmKeyInformation::KeyStatus::OUTPUT_RESTRICTED:
      return cdm::kOutputRestricted;
    case CdmKeyInformation::KeyStatus::OUTPUT_DOWNSCALED:
      return cdm::kOutputDownscaled;
    case CdmKeyInformation::KeyStatus::KEY_STATUS_PENDING:
      return cdm::kStatusPending;
    case CdmKeyInformation::KeyStatus::RELEASED:
      return cdm::kReleased;
  }

  NOTREACHED_IN_MIGRATION()
      << "Unexpected CdmKeyInformation::KeyStatus " << status;
  return cdm::kInternalError;
}

cdm::EncryptionScheme ToCdmEncryptionScheme(EncryptionScheme scheme) {
  switch (scheme) {
    case EncryptionScheme::kUnencrypted:
      return cdm::EncryptionScheme::kUnencrypted;
    case EncryptionScheme::kCenc:
      return cdm::EncryptionScheme::kCenc;
    case EncryptionScheme::kCbcs:
      return cdm::EncryptionScheme::kCbcs;
  }

  NOTREACHED_IN_MIGRATION() << "Unexpected EncryptionScheme";
  return cdm::EncryptionScheme::kUnencrypted;
}

CdmPromise::Exception ToMediaCdmPromiseException(cdm::Exception exception) {
  switch (exception) {
    case cdm::kExceptionTypeError:
      return CdmPromise::Exception::TYPE_ERROR;
    case cdm::kExceptionNotSupportedError:
      return CdmPromise::Exception::NOT_SUPPORTED_ERROR;
    case cdm::kExceptionInvalidStateError:
      return CdmPromise::Exception::INVALID_STATE_ERROR;
    case cdm::kExceptionQuotaExceededError:
      return CdmPromise::Exception::QUOTA_EXCEEDED_ERROR;
  }

  NOTREACHED_IN_MIGRATION() << "Unexpected cdm::Exception " << exception;
  return CdmPromise::Exception::INVALID_STATE_ERROR;
}

cdm::Exception ToCdmException(CdmPromise::Exception exception) {
  switch (exception) {
    case CdmPromise::Exception::NOT_SUPPORTED_ERROR:
      return cdm::kExceptionNotSupportedError;
    case CdmPromise::Exception::INVALID_STATE_ERROR:
      return cdm::kExceptionInvalidStateError;
    case CdmPromise::Exception::TYPE_ERROR:
      return cdm::kExceptionTypeError;
    case CdmPromise::Exception::QUOTA_EXCEEDED_ERROR:
      return cdm::kExceptionQuotaExceededError;
  }

  NOTREACHED_IN_MIGRATION() << "Unexpected CdmPromise::Exception";
  return cdm::kExceptionInvalidStateError;
}

CdmMessageType ToMediaMessageType(cdm::MessageType message_type) {
  switch (message_type) {
    case cdm::kLicenseRequest:
      return CdmMessageType::LICENSE_REQUEST;
    case cdm::kLicenseRenewal:
      return CdmMessageType::LICENSE_RENEWAL;
    case cdm::kLicenseRelease:
      return CdmMessageType::LICENSE_RELEASE;
    case cdm::kIndividualizationRequest:
      return CdmMessageType::INDIVIDUALIZATION_REQUEST;
  }

  NOTREACHED_IN_MIGRATION() << "Unexpected cdm::MessageType " << message_type;
  return CdmMessageType::LICENSE_REQUEST;
}

cdm::MessageType ToCdmMessageType(CdmMessageType message_type) {
  switch (message_type) {
    case CdmMessageType::LICENSE_REQUEST:
      return cdm::kLicenseRequest;
    case CdmMessageType::LICENSE_RENEWAL:
      return cdm::kLicenseRenewal;
    case CdmMessageType::LICENSE_RELEASE:
      return cdm::kLicenseRelease;
    case CdmMessageType::INDIVIDUALIZATION_REQUEST:
      return cdm::kIndividualizationRequest;
  }

  NOTREACHED_IN_MIGRATION() << "Unexpected CdmMessageType";
  return cdm::kLicenseRequest;
}

cdm::StreamType ToCdmStreamType(Decryptor::StreamType stream_type) {
  switch (stream_type) {
    case Decryptor::kAudio:
      return cdm::kStreamTypeAudio;
    case Decryptor::kVideo:
      return cdm::kStreamTypeVideo;
  }

  NOTREACHED_IN_MIGRATION()
      << "Unexpected Decryptor::StreamType " << stream_type;
  return cdm::kStreamTypeVideo;
}

Decryptor::Status ToMediaDecryptorStatus(cdm::Status status) {
  switch (status) {
    case cdm::kSuccess:
      return Decryptor::kSuccess;
    case cdm::kNoKey:
      return Decryptor::kNoKey;
    case cdm::kNeedMoreData:
      return Decryptor::kNeedMoreData;
    case cdm::kDecryptError:
      return Decryptor::kError;
    case cdm::kDecodeError:
      return Decryptor::kError;
    case cdm::kInitializationError:
    case cdm::kDeferredInitialization:
      break;
  }

  NOTREACHED_IN_MIGRATION() << "Unexpected cdm::Status " << status;
  return Decryptor::kError;
}

// Audio Converters

cdm::AudioCodec ToCdmAudioCodec(AudioCodec codec) {
  switch (codec) {
    case AudioCodec::kVorbis:
      return cdm::kCodecVorbis;
    case AudioCodec::kAAC:
      return cdm::kCodecAac;
    default:
      DVLOG(1) << "Unsupported AudioCodec " << codec;
      return cdm::kUnknownAudioCodec;
  }
}

SampleFormat ToMediaSampleFormat(cdm::AudioFormat format) {
  switch (format) {
    case cdm::kAudioFormatU8:
      return kSampleFormatU8;
    case cdm::kAudioFormatS16:
      return kSampleFormatS16;
    case cdm::kAudioFormatS32:
      return kSampleFormatS32;
    case cdm::kAudioFormatF32:
      return kSampleFormatF32;
    case cdm::kAudioFormatPlanarS16:
      return kSampleFormatPlanarS16;
    case cdm::kAudioFormatPlanarF32:
      return kSampleFormatPlanarF32;
    case cdm::kUnknownAudioFormat:
      return kUnknownSampleFormat;
  }

  NOTREACHED_IN_MIGRATION() << "Unexpected cdm::AudioFormat " << format;
  return kUnknownSampleFormat;
}

// Video Converters

cdm::VideoCodec ToCdmVideoCodec(VideoCodec codec) {
  switch (codec) {
    case VideoCodec::kVP8:
      return cdm::kCodecVp8;
    case VideoCodec::kH264:
      return cdm::kCodecH264;
    case VideoCodec::kVP9:
      return cdm::kCodecVp9;
    case VideoCodec::kAV1:
      return cdm::kCodecAv1;
    default:
      DVLOG(1) << "Unsupported VideoCodec " << codec;
      return cdm::kUnknownVideoCodec;
  }
}

VideoCodec ToMediaVideoCodec(cdm::VideoCodec codec) {
  switch (codec) {
    case cdm::kUnknownVideoCodec:
      return VideoCodec::kUnknown;
    case cdm::kCodecVp8:
      return VideoCodec::kVP8;
    case cdm::kCodecH264:
      return VideoCodec::kH264;
    case cdm::kCodecVp9:
      return VideoCodec::kVP9;
    case cdm::kCodecAv1:
      return VideoCodec::kAV1;
  }

  NOTREACHED_IN_MIGRATION() << "Unexpected cdm::VideoCodec " << codec;
  return VideoCodec::kUnknown;
}

cdm::VideoCodecProfile ToCdmVideoCodecProfile(VideoCodecProfile profile) {
  switch (profile) {
    case VP8PROFILE_ANY:
      return cdm::kProfileNotNeeded;
    case VP9PROFILE_PROFILE0:
      return cdm::kVP9Profile0;
    case VP9PROFILE_PROFILE1:
      return cdm::kVP9Profile1;
    case VP9PROFILE_PROFILE2:
      return cdm::kVP9Profile2;
    case VP9PROFILE_PROFILE3:
      return cdm::kVP9Profile3;
    case H264PROFILE_BASELINE:
      return cdm::kH264ProfileBaseline;
    case H264PROFILE_MAIN:
      return cdm::kH264ProfileMain;
    case H264PROFILE_EXTENDED:
      return cdm::kH264ProfileExtended;
    case H264PROFILE_HIGH:
      return cdm::kH264ProfileHigh;
    case H264PROFILE_HIGH10PROFILE:
      return cdm::kH264ProfileHigh10;
    case H264PROFILE_HIGH422PROFILE:
      return cdm::kH264ProfileHigh422;
    case H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return cdm::kH264ProfileHigh444Predictive;
    case AV1PROFILE_PROFILE_MAIN:
      return cdm::kAv1ProfileMain;
    case AV1PROFILE_PROFILE_HIGH:
      return cdm::kAv1ProfileHigh;
    case AV1PROFILE_PROFILE_PRO:
      return cdm::kAv1ProfilePro;
    default:
      DVLOG(1) << "Unsupported VideoCodecProfile " << profile;
      return cdm::kUnknownVideoCodecProfile;
  }
}

VideoCodecProfile ToMediaVideoCodecProfile(cdm::VideoCodecProfile profile) {
  switch (profile) {
    case cdm::kUnknownVideoCodecProfile:
      return VIDEO_CODEC_PROFILE_UNKNOWN;
    case cdm::kProfileNotNeeded:
      // There's no corresponding value for "not needed". Given CdmAdapter only
      // converts VP8PROFILE_ANY to cdm::kProfileNotNeeded, and this code is
      // only used for testing, it's okay to convert it back to VP8PROFILE_ANY.
      return VP8PROFILE_ANY;
    case cdm::kVP9Profile0:
      return VP9PROFILE_PROFILE0;
    case cdm::kVP9Profile1:
      return VP9PROFILE_PROFILE1;
    case cdm::kVP9Profile2:
      return VP9PROFILE_PROFILE2;
    case cdm::kVP9Profile3:
      return VP9PROFILE_PROFILE3;
    case cdm::kH264ProfileBaseline:
      return H264PROFILE_BASELINE;
    case cdm::kH264ProfileMain:
      return H264PROFILE_MAIN;
    case cdm::kH264ProfileExtended:
      return H264PROFILE_EXTENDED;
    case cdm::kH264ProfileHigh:
      return H264PROFILE_HIGH;
    case cdm::kH264ProfileHigh10:
      return H264PROFILE_HIGH10PROFILE;
    case cdm::kH264ProfileHigh422:
      return H264PROFILE_HIGH422PROFILE;
    case cdm::kH264ProfileHigh444Predictive:
      return H264PROFILE_HIGH444PREDICTIVEPROFILE;
    case cdm::kAv1ProfileMain:
      return AV1PROFILE_PROFILE_MAIN;
    case cdm::kAv1ProfileHigh:
      return AV1PROFILE_PROFILE_HIGH;
    case cdm::kAv1ProfilePro:
      return AV1PROFILE_PROFILE_PRO;
  }

  NOTREACHED_IN_MIGRATION() << "Unexpected cdm::VideoCodecProfile " << profile;
  return VIDEO_CODEC_PROFILE_UNKNOWN;
}

cdm::VideoFormat ToCdmVideoFormat(VideoPixelFormat format) {
  switch (format) {
    case PIXEL_FORMAT_YV12:
      return cdm::kYv12;
    case PIXEL_FORMAT_I420:
      return cdm::kI420;
    case PIXEL_FORMAT_YUV420P9:
      return cdm::kYUV420P9;
    case PIXEL_FORMAT_YUV420P10:
      return cdm::kYUV420P10;
    case PIXEL_FORMAT_YUV422P9:
      return cdm::kYUV422P9;
    case PIXEL_FORMAT_YUV422P10:
      return cdm::kYUV422P10;
    case PIXEL_FORMAT_YUV444P9:
      return cdm::kYUV444P9;
    case PIXEL_FORMAT_YUV444P10:
      return cdm::kYUV444P10;
    case PIXEL_FORMAT_YUV420P12:
      return cdm::kYUV420P12;
    case PIXEL_FORMAT_YUV422P12:
      return cdm::kYUV422P12;
    case PIXEL_FORMAT_YUV444P12:
      return cdm::kYUV444P12;
    default:
      DVLOG(1) << "Unsupported VideoPixelFormat " << format;
      return cdm::kUnknownVideoFormat;
  }
}

VideoPixelFormat ToMediaVideoFormat(cdm::VideoFormat format) {
  switch (format) {
    case cdm::kYv12:
      return PIXEL_FORMAT_YV12;
    case cdm::kI420:
      return PIXEL_FORMAT_I420;
    case cdm::kYUV420P9:
      return PIXEL_FORMAT_YUV420P9;
    case cdm::kYUV420P10:
      return PIXEL_FORMAT_YUV420P10;
    case cdm::kYUV422P9:
      return PIXEL_FORMAT_YUV422P9;
    case cdm::kYUV422P10:
      return PIXEL_FORMAT_YUV422P10;
    case cdm::kYUV444P9:
      return PIXEL_FORMAT_YUV444P9;
    case cdm::kYUV444P10:
      return PIXEL_FORMAT_YUV444P10;
    case cdm::kYUV420P12:
      return PIXEL_FORMAT_YUV420P12;
    case cdm::kYUV422P12:
      return PIXEL_FORMAT_YUV422P12;
    case cdm::kYUV444P12:
      return PIXEL_FORMAT_YUV444P12;
    default:
      DVLOG(1) << "Unsupported cdm::VideoFormat " << format;
      return PIXEL_FORMAT_UNKNOWN;
  }
}

// Aggregate Types

// Warning: The returned config contains raw pointers to the extra data in the
// input |config|. Hence, the caller must make sure the input |config| outlives
// the returned config.
cdm::AudioDecoderConfig_2 ToCdmAudioDecoderConfig(
    const AudioDecoderConfig& config) {
  cdm::AudioDecoderConfig_2 cdm_config = {};
  cdm_config.codec = ToCdmAudioCodec(config.codec());
  cdm_config.channel_count =
      ChannelLayoutToChannelCount(config.channel_layout());
  cdm_config.bits_per_channel = config.bytes_per_channel() * 8;
  cdm_config.samples_per_second = config.samples_per_second();
  cdm_config.extra_data = const_cast<uint8_t*>(config.extra_data().data());
  cdm_config.extra_data_size = config.extra_data().size();
  cdm_config.encryption_scheme =
      ToCdmEncryptionScheme(config.encryption_scheme());
  return cdm_config;
}

// Warning: The returned config contains raw pointers to the extra data in the
// input |config|. Hence, the caller must make sure the input |config| outlives
// the returned config.
cdm::VideoDecoderConfig_3 ToCdmVideoDecoderConfig(
    const VideoDecoderConfig& config) {
  cdm::VideoDecoderConfig_3 cdm_config = {};
  cdm_config.codec = ToCdmVideoCodec(config.codec());
  cdm_config.profile = ToCdmVideoCodecProfile(config.profile());

  // TODO(dalecurtis): CDM doesn't support alpha, so delete |format|.
  DCHECK_EQ(config.alpha_mode(), VideoDecoderConfig::AlphaMode::kIsOpaque);
  cdm_config.format = cdm::kI420;

  cdm_config.color_space = ToCdmColorSpace(config.color_space_info());
  cdm_config.coded_size.width = config.coded_size().width();
  cdm_config.coded_size.height = config.coded_size().height();
  cdm_config.extra_data = const_cast<uint8_t*>(config.extra_data().data());
  cdm_config.extra_data_size = config.extra_data().size();
  cdm_config.encryption_scheme =
      ToCdmEncryptionScheme(config.encryption_scheme());
  return cdm_config;
}

// Fill |input_buffer| based on the values in |encrypted|. |subsamples|
// is used to hold some of the data. |input_buffer| will contain pointers
// to data contained in |encrypted| and |subsamples|, so the lifetime of
// |input_buffer| must be <= the lifetime of |encrypted| and |subsamples|.
void ToCdmInputBuffer(const DecoderBuffer& encrypted_buffer,
                      std::vector<cdm::SubsampleEntry>* subsamples,
                      cdm::InputBuffer_2* input_buffer) {
  // End of stream buffers are represented as empty resources.
  DCHECK(!input_buffer->data);
  if (encrypted_buffer.end_of_stream())
    return;

  input_buffer->data = encrypted_buffer.data();
  input_buffer->data_size = encrypted_buffer.size();
  input_buffer->timestamp = encrypted_buffer.timestamp().InMicroseconds();

  const DecryptConfig* decrypt_config = encrypted_buffer.decrypt_config();
  if (!decrypt_config) {
    DVLOG(3) << __func__ << ": Clear buffer.";
    return;
  }

  input_buffer->key_id =
      reinterpret_cast<const uint8_t*>(decrypt_config->key_id().data());
  input_buffer->key_id_size = decrypt_config->key_id().size();
  input_buffer->iv =
      reinterpret_cast<const uint8_t*>(decrypt_config->iv().data());
  input_buffer->iv_size = decrypt_config->iv().size();

  DCHECK(subsamples->empty());
  size_t num_subsamples = decrypt_config->subsamples().size();
  if (num_subsamples > 0) {
    subsamples->reserve(num_subsamples);
    for (const auto& sample : decrypt_config->subsamples()) {
      subsamples->push_back({sample.clear_bytes, sample.cypher_bytes});
    }
  }

  input_buffer->subsamples = subsamples->data();
  input_buffer->num_subsamples = num_subsamples;

  input_buffer->encryption_scheme =
      ToCdmEncryptionScheme(decrypt_config->encryption_scheme());
  if (decrypt_config->HasPattern()) {
    input_buffer->pattern = {
        decrypt_config->encryption_pattern()->crypt_byte_block(),
        decrypt_config->encryption_pattern()->skip_byte_block()};
  }
}

}  // namespace media
