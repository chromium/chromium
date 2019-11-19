// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/proto_enum_utils.h"

namespace media {
namespace remoting {

#define CASE_RETURN_OTHER(x) \
  case OriginType::x:        \
    return OtherType::x

base::Optional<EncryptionScheme> ToMediaEncryptionScheme(
    pb::EncryptionScheme::CipherMode value) {
  switch (value) {
    case pb::EncryptionScheme::CIPHER_MODE_UNENCRYPTED:
      return EncryptionScheme::kUnencrypted;
    case pb::EncryptionScheme::CIPHER_MODE_AES_CTR:
      return EncryptionScheme::kCenc;
    case pb::EncryptionScheme::CIPHER_MODE_AES_CBC:
      return EncryptionScheme::kCbcs;
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<pb::EncryptionScheme::CipherMode>
ToProtoEncryptionSchemeCipherMode(EncryptionScheme value) {
  switch (value) {
    case EncryptionScheme::kUnencrypted:
      return pb::EncryptionScheme::CIPHER_MODE_UNENCRYPTED;
    case EncryptionScheme::kCenc:
      return pb::EncryptionScheme::CIPHER_MODE_AES_CTR;
    case EncryptionScheme::kCbcs:
      return pb::EncryptionScheme::CIPHER_MODE_AES_CBC;
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<AudioCodec> ToMediaAudioCodec(
    pb::AudioDecoderConfig::Codec value) {
  using OriginType = pb::AudioDecoderConfig;
  using OtherType = AudioCodec;
  switch (value) {
    CASE_RETURN_OTHER(kUnknownAudioCodec);
    CASE_RETURN_OTHER(kCodecAAC);
    CASE_RETURN_OTHER(kCodecMP3);
    CASE_RETURN_OTHER(kCodecPCM);
    CASE_RETURN_OTHER(kCodecVorbis);
    CASE_RETURN_OTHER(kCodecFLAC);
    CASE_RETURN_OTHER(kCodecAMR_NB);
    CASE_RETURN_OTHER(kCodecAMR_WB);
    CASE_RETURN_OTHER(kCodecPCM_MULAW);
    CASE_RETURN_OTHER(kCodecGSM_MS);
    CASE_RETURN_OTHER(kCodecPCM_S16BE);
    CASE_RETURN_OTHER(kCodecPCM_S24BE);
    CASE_RETURN_OTHER(kCodecOpus);
    CASE_RETURN_OTHER(kCodecEAC3);
    CASE_RETURN_OTHER(kCodecPCM_ALAW);
    CASE_RETURN_OTHER(kCodecALAC);
    CASE_RETURN_OTHER(kCodecAC3);
    CASE_RETURN_OTHER(kCodecMpegHAudio);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<pb::AudioDecoderConfig::Codec> ToProtoAudioDecoderConfigCodec(
    AudioCodec value) {
  using OriginType = AudioCodec;
  using OtherType = pb::AudioDecoderConfig;
  switch (value) {
    CASE_RETURN_OTHER(kUnknownAudioCodec);
    CASE_RETURN_OTHER(kCodecAAC);
    CASE_RETURN_OTHER(kCodecMP3);
    CASE_RETURN_OTHER(kCodecPCM);
    CASE_RETURN_OTHER(kCodecVorbis);
    CASE_RETURN_OTHER(kCodecFLAC);
    CASE_RETURN_OTHER(kCodecAMR_NB);
    CASE_RETURN_OTHER(kCodecAMR_WB);
    CASE_RETURN_OTHER(kCodecPCM_MULAW);
    CASE_RETURN_OTHER(kCodecGSM_MS);
    CASE_RETURN_OTHER(kCodecPCM_S16BE);
    CASE_RETURN_OTHER(kCodecPCM_S24BE);
    CASE_RETURN_OTHER(kCodecOpus);
    CASE_RETURN_OTHER(kCodecEAC3);
    CASE_RETURN_OTHER(kCodecPCM_ALAW);
    CASE_RETURN_OTHER(kCodecALAC);
    CASE_RETURN_OTHER(kCodecAC3);
    CASE_RETURN_OTHER(kCodecMpegHAudio);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<SampleFormat> ToMediaSampleFormat(
    pb::AudioDecoderConfig::SampleFormat value) {
  using OriginType = pb::AudioDecoderConfig;
  using OtherType = SampleFormat;
  switch (value) {
    CASE_RETURN_OTHER(kUnknownSampleFormat);
    CASE_RETURN_OTHER(kSampleFormatU8);
    CASE_RETURN_OTHER(kSampleFormatS16);
    CASE_RETURN_OTHER(kSampleFormatS32);
    CASE_RETURN_OTHER(kSampleFormatF32);
    CASE_RETURN_OTHER(kSampleFormatPlanarS16);
    CASE_RETURN_OTHER(kSampleFormatPlanarF32);
    CASE_RETURN_OTHER(kSampleFormatPlanarS32);
    CASE_RETURN_OTHER(kSampleFormatS24);
    CASE_RETURN_OTHER(kSampleFormatAc3);
    CASE_RETURN_OTHER(kSampleFormatEac3);
    CASE_RETURN_OTHER(kSampleFormatMpegHAudio);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<pb::AudioDecoderConfig::SampleFormat>
ToProtoAudioDecoderConfigSampleFormat(SampleFormat value) {
  using OriginType = SampleFormat;
  using OtherType = pb::AudioDecoderConfig;
  switch (value) {
    CASE_RETURN_OTHER(kUnknownSampleFormat);
    CASE_RETURN_OTHER(kSampleFormatU8);
    CASE_RETURN_OTHER(kSampleFormatS16);
    CASE_RETURN_OTHER(kSampleFormatS32);
    CASE_RETURN_OTHER(kSampleFormatF32);
    CASE_RETURN_OTHER(kSampleFormatPlanarS16);
    CASE_RETURN_OTHER(kSampleFormatPlanarF32);
    CASE_RETURN_OTHER(kSampleFormatPlanarS32);
    CASE_RETURN_OTHER(kSampleFormatS24);
    CASE_RETURN_OTHER(kSampleFormatAc3);
    CASE_RETURN_OTHER(kSampleFormatEac3);
    CASE_RETURN_OTHER(kSampleFormatMpegHAudio);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<ChannelLayout> ToMediaChannelLayout(
    pb::AudioDecoderConfig::ChannelLayout value) {
  using OriginType = pb::AudioDecoderConfig;
  using OtherType = ChannelLayout;
  switch (value) {
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_NONE);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_UNSUPPORTED);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_MONO);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_STEREO);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_2_1);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_SURROUND);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_4_0);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_2_2);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_QUAD);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_5_0);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_5_1);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_5_0_BACK);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_5_1_BACK);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_7_0);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_7_1);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_7_1_WIDE);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_STEREO_DOWNMIX);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_2POINT1);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_3_1);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_4_1);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_6_0);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_6_0_FRONT);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_HEXAGONAL);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_6_1);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_6_1_BACK);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_6_1_FRONT);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_7_0_FRONT);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_7_1_WIDE_BACK);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_OCTAGONAL);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_DISCRETE);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_4_1_QUAD_SIDE);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_BITSTREAM);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<pb::AudioDecoderConfig::ChannelLayout>
ToProtoAudioDecoderConfigChannelLayout(ChannelLayout value) {
  using OriginType = ChannelLayout;
  using OtherType = pb::AudioDecoderConfig;
  switch (value) {
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_NONE);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_UNSUPPORTED);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_MONO);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_STEREO);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_2_1);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_SURROUND);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_4_0);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_2_2);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_QUAD);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_5_0);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_5_1);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_5_0_BACK);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_5_1_BACK);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_7_0);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_7_1);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_7_1_WIDE);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_STEREO_DOWNMIX);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_2POINT1);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_3_1);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_4_1);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_6_0);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_6_0_FRONT);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_HEXAGONAL);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_6_1);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_6_1_BACK);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_6_1_FRONT);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_7_0_FRONT);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_7_1_WIDE_BACK);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_OCTAGONAL);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_DISCRETE);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_4_1_QUAD_SIDE);
    CASE_RETURN_OTHER(CHANNEL_LAYOUT_BITSTREAM);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<VideoCodec> ToMediaVideoCodec(
    pb::VideoDecoderConfig::Codec value) {
  using OriginType = pb::VideoDecoderConfig;
  using OtherType = VideoCodec;
  switch (value) {
    CASE_RETURN_OTHER(kUnknownVideoCodec);
    CASE_RETURN_OTHER(kCodecH264);
    CASE_RETURN_OTHER(kCodecVC1);
    CASE_RETURN_OTHER(kCodecMPEG2);
    CASE_RETURN_OTHER(kCodecMPEG4);
    CASE_RETURN_OTHER(kCodecTheora);
    CASE_RETURN_OTHER(kCodecVP8);
    CASE_RETURN_OTHER(kCodecVP9);
    CASE_RETURN_OTHER(kCodecHEVC);
    CASE_RETURN_OTHER(kCodecDolbyVision);
    CASE_RETURN_OTHER(kCodecAV1);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<pb::VideoDecoderConfig::Codec> ToProtoVideoDecoderConfigCodec(
    VideoCodec value) {
  using OriginType = VideoCodec;
  using OtherType = pb::VideoDecoderConfig;
  switch (value) {
    CASE_RETURN_OTHER(kUnknownVideoCodec);
    CASE_RETURN_OTHER(kCodecH264);
    CASE_RETURN_OTHER(kCodecVC1);
    CASE_RETURN_OTHER(kCodecMPEG2);
    CASE_RETURN_OTHER(kCodecMPEG4);
    CASE_RETURN_OTHER(kCodecTheora);
    CASE_RETURN_OTHER(kCodecVP8);
    CASE_RETURN_OTHER(kCodecVP9);
    CASE_RETURN_OTHER(kCodecHEVC);
    CASE_RETURN_OTHER(kCodecDolbyVision);
    CASE_RETURN_OTHER(kCodecAV1);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<VideoCodecProfile> ToMediaVideoCodecProfile(
    pb::VideoDecoderConfig::Profile value) {
  using OriginType = pb::VideoDecoderConfig;
  using OtherType = VideoCodecProfile;
  switch (value) {
    CASE_RETURN_OTHER(VIDEO_CODEC_PROFILE_UNKNOWN);
    CASE_RETURN_OTHER(H264PROFILE_BASELINE);
    CASE_RETURN_OTHER(H264PROFILE_MAIN);
    CASE_RETURN_OTHER(H264PROFILE_EXTENDED);
    CASE_RETURN_OTHER(H264PROFILE_HIGH);
    CASE_RETURN_OTHER(H264PROFILE_HIGH10PROFILE);
    CASE_RETURN_OTHER(H264PROFILE_HIGH422PROFILE);
    CASE_RETURN_OTHER(H264PROFILE_HIGH444PREDICTIVEPROFILE);
    CASE_RETURN_OTHER(H264PROFILE_SCALABLEBASELINE);
    CASE_RETURN_OTHER(H264PROFILE_SCALABLEHIGH);
    CASE_RETURN_OTHER(H264PROFILE_STEREOHIGH);
    CASE_RETURN_OTHER(H264PROFILE_MULTIVIEWHIGH);
    CASE_RETURN_OTHER(VP8PROFILE_ANY);
    CASE_RETURN_OTHER(VP9PROFILE_PROFILE0);
    CASE_RETURN_OTHER(VP9PROFILE_PROFILE1);
    CASE_RETURN_OTHER(VP9PROFILE_PROFILE2);
    CASE_RETURN_OTHER(VP9PROFILE_PROFILE3);
    CASE_RETURN_OTHER(HEVCPROFILE_MAIN);
    CASE_RETURN_OTHER(HEVCPROFILE_MAIN10);
    CASE_RETURN_OTHER(HEVCPROFILE_MAIN_STILL_PICTURE);
    CASE_RETURN_OTHER(DOLBYVISION_PROFILE0);
    CASE_RETURN_OTHER(DOLBYVISION_PROFILE4);
    CASE_RETURN_OTHER(DOLBYVISION_PROFILE5);
    CASE_RETURN_OTHER(DOLBYVISION_PROFILE7);
    CASE_RETURN_OTHER(DOLBYVISION_PROFILE8);
    CASE_RETURN_OTHER(DOLBYVISION_PROFILE9);
    CASE_RETURN_OTHER(THEORAPROFILE_ANY);
    CASE_RETURN_OTHER(AV1PROFILE_PROFILE_MAIN);
    CASE_RETURN_OTHER(AV1PROFILE_PROFILE_HIGH);
    CASE_RETURN_OTHER(AV1PROFILE_PROFILE_PRO);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<pb::VideoDecoderConfig::Profile>
ToProtoVideoDecoderConfigProfile(VideoCodecProfile value) {
  using OriginType = VideoCodecProfile;
  using OtherType = pb::VideoDecoderConfig;
  switch (value) {
    CASE_RETURN_OTHER(VIDEO_CODEC_PROFILE_UNKNOWN);
    CASE_RETURN_OTHER(H264PROFILE_BASELINE);
    CASE_RETURN_OTHER(H264PROFILE_MAIN);
    CASE_RETURN_OTHER(H264PROFILE_EXTENDED);
    CASE_RETURN_OTHER(H264PROFILE_HIGH);
    CASE_RETURN_OTHER(H264PROFILE_HIGH10PROFILE);
    CASE_RETURN_OTHER(H264PROFILE_HIGH422PROFILE);
    CASE_RETURN_OTHER(H264PROFILE_HIGH444PREDICTIVEPROFILE);
    CASE_RETURN_OTHER(H264PROFILE_SCALABLEBASELINE);
    CASE_RETURN_OTHER(H264PROFILE_SCALABLEHIGH);
    CASE_RETURN_OTHER(H264PROFILE_STEREOHIGH);
    CASE_RETURN_OTHER(H264PROFILE_MULTIVIEWHIGH);
    CASE_RETURN_OTHER(VP8PROFILE_ANY);
    CASE_RETURN_OTHER(VP9PROFILE_PROFILE0);
    CASE_RETURN_OTHER(VP9PROFILE_PROFILE1);
    CASE_RETURN_OTHER(VP9PROFILE_PROFILE2);
    CASE_RETURN_OTHER(VP9PROFILE_PROFILE3);
    CASE_RETURN_OTHER(HEVCPROFILE_MAIN);
    CASE_RETURN_OTHER(HEVCPROFILE_MAIN10);
    CASE_RETURN_OTHER(HEVCPROFILE_MAIN_STILL_PICTURE);
    CASE_RETURN_OTHER(DOLBYVISION_PROFILE0);
    CASE_RETURN_OTHER(DOLBYVISION_PROFILE4);
    CASE_RETURN_OTHER(DOLBYVISION_PROFILE5);
    CASE_RETURN_OTHER(DOLBYVISION_PROFILE7);
    CASE_RETURN_OTHER(DOLBYVISION_PROFILE8);
    CASE_RETURN_OTHER(DOLBYVISION_PROFILE9);
    CASE_RETURN_OTHER(THEORAPROFILE_ANY);
    CASE_RETURN_OTHER(AV1PROFILE_PROFILE_MAIN);
    CASE_RETURN_OTHER(AV1PROFILE_PROFILE_HIGH);
    CASE_RETURN_OTHER(AV1PROFILE_PROFILE_PRO);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<VideoPixelFormat> ToMediaVideoPixelFormat(
    pb::VideoDecoderConfig::Format value) {
  using OriginType = pb::VideoDecoderConfig;
  using OtherType = VideoPixelFormat;
  switch (value) {
    CASE_RETURN_OTHER(PIXEL_FORMAT_UNKNOWN);
    CASE_RETURN_OTHER(PIXEL_FORMAT_I420);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YV12);
    CASE_RETURN_OTHER(PIXEL_FORMAT_I422);
    CASE_RETURN_OTHER(PIXEL_FORMAT_I420A);
    CASE_RETURN_OTHER(PIXEL_FORMAT_I444);
    CASE_RETURN_OTHER(PIXEL_FORMAT_NV12);
    CASE_RETURN_OTHER(PIXEL_FORMAT_NV21);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YUY2);
    CASE_RETURN_OTHER(PIXEL_FORMAT_ARGB);
    CASE_RETURN_OTHER(PIXEL_FORMAT_XRGB);
    CASE_RETURN_OTHER(PIXEL_FORMAT_RGB24);
    CASE_RETURN_OTHER(PIXEL_FORMAT_MJPEG);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YUV420P9);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YUV420P10);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YUV422P9);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YUV422P10);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YUV444P9);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YUV444P10);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YUV420P12);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YUV422P12);
    CASE_RETURN_OTHER(PIXEL_FORMAT_YUV444P12);
    // PIXEL_FORMAT_UYVY, PIXEL_FORMAT_RGB32 and PIXEL_FORMAT_Y8 are deprecated.
    case pb::VideoDecoderConfig_Format_PIXEL_FORMAT_UYVY:
    case pb::VideoDecoderConfig_Format_PIXEL_FORMAT_RGB32:
    case pb::VideoDecoderConfig_Format_PIXEL_FORMAT_Y8:
      return base::nullopt;
      CASE_RETURN_OTHER(PIXEL_FORMAT_Y16);
      CASE_RETURN_OTHER(PIXEL_FORMAT_ABGR);
      CASE_RETURN_OTHER(PIXEL_FORMAT_XBGR);
      CASE_RETURN_OTHER(PIXEL_FORMAT_P016LE);
      CASE_RETURN_OTHER(PIXEL_FORMAT_XR30);
      CASE_RETURN_OTHER(PIXEL_FORMAT_XB30);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<BufferingState> ToMediaBufferingState(
    pb::RendererClientOnBufferingStateChange::State value) {
  using OriginType = pb::RendererClientOnBufferingStateChange;
  using OtherType = BufferingState;
  switch (value) {
    CASE_RETURN_OTHER(BUFFERING_HAVE_NOTHING);
    CASE_RETURN_OTHER(BUFFERING_HAVE_ENOUGH);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<pb::RendererClientOnBufferingStateChange::State>
ToProtoMediaBufferingState(BufferingState value) {
  using OriginType = BufferingState;
  using OtherType = pb::RendererClientOnBufferingStateChange;
  switch (value) {
    CASE_RETURN_OTHER(BUFFERING_HAVE_NOTHING);
    CASE_RETURN_OTHER(BUFFERING_HAVE_ENOUGH);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<CdmKeyInformation::KeyStatus> ToMediaCdmKeyInformationKeyStatus(
    pb::CdmKeyInformation::KeyStatus value) {
  using OriginType = pb::CdmKeyInformation;
  using OtherType = CdmKeyInformation;
  switch (value) {
    CASE_RETURN_OTHER(USABLE);
    CASE_RETURN_OTHER(INTERNAL_ERROR);
    CASE_RETURN_OTHER(EXPIRED);
    CASE_RETURN_OTHER(OUTPUT_RESTRICTED);
    CASE_RETURN_OTHER(OUTPUT_DOWNSCALED);
    CASE_RETURN_OTHER(KEY_STATUS_PENDING);
    CASE_RETURN_OTHER(RELEASED);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<pb::CdmKeyInformation::KeyStatus> ToProtoCdmKeyInformation(
    CdmKeyInformation::KeyStatus value) {
  using OriginType = CdmKeyInformation;
  using OtherType = pb::CdmKeyInformation;
  switch (value) {
    CASE_RETURN_OTHER(USABLE);
    CASE_RETURN_OTHER(INTERNAL_ERROR);
    CASE_RETURN_OTHER(EXPIRED);
    CASE_RETURN_OTHER(OUTPUT_RESTRICTED);
    CASE_RETURN_OTHER(OUTPUT_DOWNSCALED);
    CASE_RETURN_OTHER(KEY_STATUS_PENDING);
    CASE_RETURN_OTHER(RELEASED);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<CdmPromise::Exception> ToCdmPromiseException(
    pb::CdmException value) {
  using OriginType = pb::CdmException;
  using OtherType = CdmPromise::Exception;
  switch (value) {
    CASE_RETURN_OTHER(NOT_SUPPORTED_ERROR);
    CASE_RETURN_OTHER(INVALID_STATE_ERROR);
    CASE_RETURN_OTHER(QUOTA_EXCEEDED_ERROR);
    CASE_RETURN_OTHER(TYPE_ERROR);

    // The following were generated with previous versions of the CDM and are
    // no longer used by CdmPromise.
    case OriginType::INVALID_ACCESS_ERROR:
    case OriginType::UNKNOWN_ERROR:
    case OriginType::CLIENT_ERROR:
    case OriginType::OUTPUT_ERROR:
      return OtherType::NOT_SUPPORTED_ERROR;
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<pb::CdmException> ToProtoCdmException(
    CdmPromise::Exception value) {
  using OriginType = CdmPromise::Exception;
  using OtherType = pb::CdmException;
  switch (value) {
    CASE_RETURN_OTHER(NOT_SUPPORTED_ERROR);
    CASE_RETURN_OTHER(INVALID_STATE_ERROR);
    CASE_RETURN_OTHER(QUOTA_EXCEEDED_ERROR);
    CASE_RETURN_OTHER(TYPE_ERROR);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<CdmMessageType> ToMediaCdmMessageType(pb::CdmMessageType value) {
  using OriginType = pb::CdmMessageType;
  using OtherType = CdmMessageType;
  switch (value) {
    CASE_RETURN_OTHER(LICENSE_REQUEST);
    CASE_RETURN_OTHER(LICENSE_RENEWAL);
    CASE_RETURN_OTHER(LICENSE_RELEASE);
    CASE_RETURN_OTHER(INDIVIDUALIZATION_REQUEST);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<pb::CdmMessageType> ToProtoCdmMessageType(CdmMessageType value) {
  using OriginType = CdmMessageType;
  using OtherType = pb::CdmMessageType;
  switch (value) {
    CASE_RETURN_OTHER(LICENSE_REQUEST);
    CASE_RETURN_OTHER(LICENSE_RENEWAL);
    CASE_RETURN_OTHER(LICENSE_RELEASE);
    CASE_RETURN_OTHER(INDIVIDUALIZATION_REQUEST);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<CdmSessionType> ToCdmSessionType(pb::CdmSessionType value) {
  using OriginType = pb::CdmSessionType;
  using OtherType = CdmSessionType;
  switch (value) {
    CASE_RETURN_OTHER(kTemporary);
    CASE_RETURN_OTHER(kPersistentLicense);
    CASE_RETURN_OTHER(kPersistentUsageRecord);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<pb::CdmSessionType> ToProtoCdmSessionType(CdmSessionType value) {
  using OriginType = CdmSessionType;
  using OtherType = pb::CdmSessionType;
  switch (value) {
    CASE_RETURN_OTHER(kTemporary);
    CASE_RETURN_OTHER(kPersistentLicense);
    CASE_RETURN_OTHER(kPersistentUsageRecord);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<EmeInitDataType> ToMediaEmeInitDataType(
    pb::CdmCreateSessionAndGenerateRequest::EmeInitDataType value) {
  using OriginType = pb::CdmCreateSessionAndGenerateRequest;
  using OtherType = EmeInitDataType;
  switch (value) {
    CASE_RETURN_OTHER(UNKNOWN);
    CASE_RETURN_OTHER(WEBM);
    CASE_RETURN_OTHER(CENC);
    CASE_RETURN_OTHER(KEYIDS);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<pb::CdmCreateSessionAndGenerateRequest::EmeInitDataType>
ToProtoMediaEmeInitDataType(EmeInitDataType value) {
  using OriginType = EmeInitDataType;
  using OtherType = pb::CdmCreateSessionAndGenerateRequest;
  switch (value) {
    CASE_RETURN_OTHER(UNKNOWN);
    CASE_RETURN_OTHER(WEBM);
    CASE_RETURN_OTHER(CENC);
    CASE_RETURN_OTHER(KEYIDS);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<DemuxerStream::Status> ToDemuxerStreamStatus(
    pb::DemuxerStreamReadUntilCallback::Status value) {
  using OriginType = pb::DemuxerStreamReadUntilCallback;
  using OtherType = DemuxerStream;
  switch (value) {
    CASE_RETURN_OTHER(kOk);
    CASE_RETURN_OTHER(kAborted);
    CASE_RETURN_OTHER(kConfigChanged);
    CASE_RETURN_OTHER(kError);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<pb::DemuxerStreamReadUntilCallback::Status>
ToProtoDemuxerStreamStatus(DemuxerStream::Status value) {
  using OriginType = DemuxerStream;
  using OtherType = pb::DemuxerStreamReadUntilCallback;
  switch (value) {
    CASE_RETURN_OTHER(kOk);
    CASE_RETURN_OTHER(kAborted);
    CASE_RETURN_OTHER(kConfigChanged);
    CASE_RETURN_OTHER(kError);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<EncryptionScheme> ToMediaEncryptionScheme(
    pb::EncryptionMode value) {
  using OriginType = pb::EncryptionMode;
  using OtherType = EncryptionScheme;
  switch (value) {
    CASE_RETURN_OTHER(kUnencrypted);
    CASE_RETURN_OTHER(kCenc);
    CASE_RETURN_OTHER(kCbcs);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

base::Optional<pb::EncryptionMode> ToProtoEncryptionMode(
    EncryptionScheme value) {
  using OriginType = EncryptionScheme;
  using OtherType = pb::EncryptionMode;
  switch (value) {
    CASE_RETURN_OTHER(kUnencrypted);
    CASE_RETURN_OTHER(kCenc);
    CASE_RETURN_OTHER(kCbcs);
  }
  return base::nullopt;  // Not a 'default' to ensure compile-time checks.
}

}  // namespace remoting
}  // namespace media
