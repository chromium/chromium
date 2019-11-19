// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/ffmpeg/ffmpeg_common.h"

#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/encryption_scheme.h"
#include "media/base/media_util.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_util.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/media_buildflags.h"

namespace media {

namespace {

EncryptionScheme GetEncryptionScheme(const AVStream* stream) {
  AVDictionaryEntry* key =
      av_dict_get(stream->metadata, "enc_key_id", nullptr, 0);
  return key ? EncryptionScheme::kCenc : EncryptionScheme::kUnencrypted;
}

}  // namespace

// Why AV_INPUT_BUFFER_PADDING_SIZE? FFmpeg assumes all input buffers are
// padded. Check here to ensure FFmpeg only receives data padded to its
// specifications.
static_assert(DecoderBuffer::kPaddingSize >= AV_INPUT_BUFFER_PADDING_SIZE,
              "DecoderBuffer padding size does not fit ffmpeg requirement");

// Alignment requirement by FFmpeg for input and output buffers. This need to
// be updated to match FFmpeg when it changes.
#if defined(ARCH_CPU_ARM_FAMILY)
static const int kFFmpegBufferAddressAlignment = 16;
#else
static const int kFFmpegBufferAddressAlignment = 32;
#endif

// Check here to ensure FFmpeg only receives data aligned to its specifications.
static_assert(
    DecoderBuffer::kAlignmentSize >= kFFmpegBufferAddressAlignment &&
    DecoderBuffer::kAlignmentSize % kFFmpegBufferAddressAlignment == 0,
    "DecoderBuffer alignment size does not fit ffmpeg requirement");

// Allows faster SIMD YUV convert. Also, FFmpeg overreads/-writes occasionally.
// See video_get_buffer() in libavcodec/utils.c.
static const int kFFmpegOutputBufferPaddingSize = 16;

static_assert(VideoFrame::kFrameSizePadding >= kFFmpegOutputBufferPaddingSize,
              "VideoFrame padding size does not fit ffmpeg requirement");

static_assert(
    VideoFrame::kFrameAddressAlignment >= kFFmpegBufferAddressAlignment &&
    VideoFrame::kFrameAddressAlignment % kFFmpegBufferAddressAlignment == 0,
    "VideoFrame frame address alignment does not fit ffmpeg requirement");

static const AVRational kMicrosBase = { 1, base::Time::kMicrosecondsPerSecond };

base::TimeDelta ConvertFromTimeBase(const AVRational& time_base,
                                    int64_t timestamp) {
  int64_t microseconds = av_rescale_q(timestamp, time_base, kMicrosBase);
  return base::TimeDelta::FromMicroseconds(microseconds);
}

int64_t ConvertToTimeBase(const AVRational& time_base,
                          const base::TimeDelta& timestamp) {
  return av_rescale_q(timestamp.InMicroseconds(), kMicrosBase, time_base);
}

AudioCodec CodecIDToAudioCodec(AVCodecID codec_id) {
  switch (codec_id) {
    case AV_CODEC_ID_AAC:
      return kCodecAAC;
#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
    case AV_CODEC_ID_AC3:
      return kCodecAC3;
    case AV_CODEC_ID_EAC3:
      return kCodecEAC3;
#endif
    case AV_CODEC_ID_MP3:
      return kCodecMP3;
    case AV_CODEC_ID_VORBIS:
      return kCodecVorbis;
    case AV_CODEC_ID_PCM_U8:
    case AV_CODEC_ID_PCM_S16LE:
    case AV_CODEC_ID_PCM_S24LE:
    case AV_CODEC_ID_PCM_S32LE:
    case AV_CODEC_ID_PCM_F32LE:
      return kCodecPCM;
    case AV_CODEC_ID_PCM_S16BE:
      return kCodecPCM_S16BE;
    case AV_CODEC_ID_PCM_S24BE:
      return kCodecPCM_S24BE;
    case AV_CODEC_ID_FLAC:
      return kCodecFLAC;
    case AV_CODEC_ID_AMR_NB:
      return kCodecAMR_NB;
    case AV_CODEC_ID_AMR_WB:
      return kCodecAMR_WB;
    case AV_CODEC_ID_GSM_MS:
      return kCodecGSM_MS;
    case AV_CODEC_ID_PCM_ALAW:
      return kCodecPCM_ALAW;
    case AV_CODEC_ID_PCM_MULAW:
      return kCodecPCM_MULAW;
    case AV_CODEC_ID_OPUS:
      return kCodecOpus;
    case AV_CODEC_ID_ALAC:
      return kCodecALAC;
    default:
      DVLOG(1) << "Unknown audio CodecID: " << codec_id;
  }
  return kUnknownAudioCodec;
}

AVCodecID AudioCodecToCodecID(AudioCodec audio_codec,
                              SampleFormat sample_format) {
  switch (audio_codec) {
    case kCodecAAC:
      return AV_CODEC_ID_AAC;
    case kCodecALAC:
      return AV_CODEC_ID_ALAC;
    case kCodecMP3:
      return AV_CODEC_ID_MP3;
    case kCodecPCM:
      switch (sample_format) {
        case kSampleFormatU8:
          return AV_CODEC_ID_PCM_U8;
        case kSampleFormatS16:
          return AV_CODEC_ID_PCM_S16LE;
        case kSampleFormatS24:
          return AV_CODEC_ID_PCM_S24LE;
        case kSampleFormatS32:
          return AV_CODEC_ID_PCM_S32LE;
        case kSampleFormatF32:
          return AV_CODEC_ID_PCM_F32LE;
        default:
          DVLOG(1) << "Unsupported sample format: " << sample_format;
      }
      break;
    case kCodecPCM_S16BE:
      return AV_CODEC_ID_PCM_S16BE;
    case kCodecPCM_S24BE:
      return AV_CODEC_ID_PCM_S24BE;
    case kCodecVorbis:
      return AV_CODEC_ID_VORBIS;
    case kCodecFLAC:
      return AV_CODEC_ID_FLAC;
    case kCodecAMR_NB:
      return AV_CODEC_ID_AMR_NB;
    case kCodecAMR_WB:
      return AV_CODEC_ID_AMR_WB;
    case kCodecGSM_MS:
      return AV_CODEC_ID_GSM_MS;
    case kCodecPCM_ALAW:
      return AV_CODEC_ID_PCM_ALAW;
    case kCodecPCM_MULAW:
      return AV_CODEC_ID_PCM_MULAW;
    case kCodecOpus:
      return AV_CODEC_ID_OPUS;
    default:
      DVLOG(1) << "Unknown AudioCodec: " << audio_codec;
  }
  return AV_CODEC_ID_NONE;
}

// Converts an FFmpeg video codec ID into its corresponding supported codec id.
static VideoCodec CodecIDToVideoCodec(AVCodecID codec_id) {
  switch (codec_id) {
    case AV_CODEC_ID_H264:
      return kCodecH264;
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    case AV_CODEC_ID_HEVC:
      return kCodecHEVC;
#endif
    case AV_CODEC_ID_THEORA:
      return kCodecTheora;
    case AV_CODEC_ID_MPEG4:
      return kCodecMPEG4;
    case AV_CODEC_ID_VP8:
      return kCodecVP8;
    case AV_CODEC_ID_VP9:
      return kCodecVP9;
    case AV_CODEC_ID_AV1:
      return kCodecAV1;
    default:
      DVLOG(1) << "Unknown video CodecID: " << codec_id;
  }
  return kUnknownVideoCodec;
}

AVCodecID VideoCodecToCodecID(VideoCodec video_codec) {
  switch (video_codec) {
    case kCodecH264:
      return AV_CODEC_ID_H264;
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    case kCodecHEVC:
      return AV_CODEC_ID_HEVC;
#endif
    case kCodecTheora:
      return AV_CODEC_ID_THEORA;
    case kCodecMPEG4:
      return AV_CODEC_ID_MPEG4;
    case kCodecVP8:
      return AV_CODEC_ID_VP8;
    case kCodecVP9:
      return AV_CODEC_ID_VP9;
    case kCodecAV1:
      return AV_CODEC_ID_AV1;
    default:
      DVLOG(1) << "Unknown VideoCodec: " << video_codec;
  }
  return AV_CODEC_ID_NONE;
}

static VideoCodecProfile ProfileIDToVideoCodecProfile(int profile) {
  // Clear out the CONSTRAINED & INTRA flags which are strict subsets of the
  // corresponding profiles with which they're used.
  profile &= ~FF_PROFILE_H264_CONSTRAINED;
  profile &= ~FF_PROFILE_H264_INTRA;
  switch (profile) {
    case FF_PROFILE_H264_BASELINE:
      return H264PROFILE_BASELINE;
    case FF_PROFILE_H264_MAIN:
      return H264PROFILE_MAIN;
    case FF_PROFILE_H264_EXTENDED:
      return H264PROFILE_EXTENDED;
    case FF_PROFILE_H264_HIGH:
      return H264PROFILE_HIGH;
    case FF_PROFILE_H264_HIGH_10:
      return H264PROFILE_HIGH10PROFILE;
    case FF_PROFILE_H264_HIGH_422:
      return H264PROFILE_HIGH422PROFILE;
    case FF_PROFILE_H264_HIGH_444_PREDICTIVE:
      return H264PROFILE_HIGH444PREDICTIVEPROFILE;
    default:
      DVLOG(1) << "Unknown profile id: " << profile;
  }
  return VIDEO_CODEC_PROFILE_UNKNOWN;
}

static int VideoCodecProfileToProfileID(VideoCodecProfile profile) {
  switch (profile) {
    case H264PROFILE_BASELINE:
      return FF_PROFILE_H264_BASELINE;
    case H264PROFILE_MAIN:
      return FF_PROFILE_H264_MAIN;
    case H264PROFILE_EXTENDED:
      return FF_PROFILE_H264_EXTENDED;
    case H264PROFILE_HIGH:
      return FF_PROFILE_H264_HIGH;
    case H264PROFILE_HIGH10PROFILE:
      return FF_PROFILE_H264_HIGH_10;
    case H264PROFILE_HIGH422PROFILE:
      return FF_PROFILE_H264_HIGH_422;
    case H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return FF_PROFILE_H264_HIGH_444_PREDICTIVE;
    default:
      DVLOG(1) << "Unknown VideoCodecProfile: " << profile;
  }
  return FF_PROFILE_UNKNOWN;
}

SampleFormat AVSampleFormatToSampleFormat(AVSampleFormat sample_format,
                                          AVCodecID codec_id) {
  switch (sample_format) {
    case AV_SAMPLE_FMT_U8:
      return kSampleFormatU8;
    case AV_SAMPLE_FMT_S16:
      return kSampleFormatS16;
    case AV_SAMPLE_FMT_S32:
      if (codec_id == AV_CODEC_ID_PCM_S24LE)
        return kSampleFormatS24;
      else
        return kSampleFormatS32;
    case AV_SAMPLE_FMT_FLT:
      return kSampleFormatF32;
    case AV_SAMPLE_FMT_S16P:
      return kSampleFormatPlanarS16;
    case  AV_SAMPLE_FMT_S32P:
      return kSampleFormatPlanarS32;
    case AV_SAMPLE_FMT_FLTP:
      return kSampleFormatPlanarF32;
    default:
      DVLOG(1) << "Unknown AVSampleFormat: " << sample_format;
  }
  return kUnknownSampleFormat;
}

static AVSampleFormat SampleFormatToAVSampleFormat(SampleFormat sample_format) {
  switch (sample_format) {
    case kSampleFormatU8:
      return AV_SAMPLE_FMT_U8;
    case kSampleFormatS16:
      return AV_SAMPLE_FMT_S16;
    // pcm_s24le is treated as a codec with sample format s32 in ffmpeg
    case kSampleFormatS24:
    case kSampleFormatS32:
      return AV_SAMPLE_FMT_S32;
    case kSampleFormatF32:
      return AV_SAMPLE_FMT_FLT;
    case kSampleFormatPlanarS16:
      return AV_SAMPLE_FMT_S16P;
    case kSampleFormatPlanarF32:
      return AV_SAMPLE_FMT_FLTP;
    default:
      DVLOG(1) << "Unknown SampleFormat: " << sample_format;
  }
  return AV_SAMPLE_FMT_NONE;
}

bool AVCodecContextToAudioDecoderConfig(const AVCodecContext* codec_context,
                                        EncryptionScheme encryption_scheme,
                                        AudioDecoderConfig* config) {
  DCHECK_EQ(codec_context->codec_type, AVMEDIA_TYPE_AUDIO);

  AudioCodec codec = CodecIDToAudioCodec(codec_context->codec_id);

  SampleFormat sample_format = AVSampleFormatToSampleFormat(
      codec_context->sample_fmt, codec_context->codec_id);

  ChannelLayout channel_layout =
      codec_context->channels > 8
          ? CHANNEL_LAYOUT_DISCRETE
          : ChannelLayoutToChromeChannelLayout(codec_context->channel_layout,
                                               codec_context->channels);

  int sample_rate = codec_context->sample_rate;
  switch (codec) {
    // For AC3/EAC3 we enable only demuxing, but not decoding, so FFmpeg does
    // not fill |sample_fmt|.
    case kCodecAC3:
    case kCodecEAC3:
#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
      // The spec for AC3/EAC3 audio is ETSI TS 102 366. According to sections
      // F.3.1 and F.5.1 in that spec the sample_format for AC3/EAC3 must be 16.
      sample_format = kSampleFormatS16;
#else
      NOTREACHED();
#endif
      break;

    default:
      break;
  }

  base::TimeDelta seek_preroll;
  if (codec_context->seek_preroll > 0) {
    seek_preroll = base::TimeDelta::FromMicroseconds(
        codec_context->seek_preroll * 1000000.0 / codec_context->sample_rate);
  }

  // AVStream occasionally has invalid extra data. See http://crbug.com/517163
  if ((codec_context->extradata_size == 0) !=
      (codec_context->extradata == nullptr)) {
    LOG(ERROR) << __func__
               << (codec_context->extradata == nullptr ? " NULL" : " Non-NULL")
               << " extra data cannot have size of "
               << codec_context->extradata_size << ".";
    return false;
  }

  std::vector<uint8_t> extra_data;
  if (codec_context->extradata_size > 0) {
    extra_data.assign(codec_context->extradata,
                      codec_context->extradata + codec_context->extradata_size);
  }

  config->Initialize(codec, sample_format, channel_layout, sample_rate,
                     extra_data, encryption_scheme, seek_preroll,
                     codec_context->delay);
  if (channel_layout == CHANNEL_LAYOUT_DISCRETE)
    config->SetChannelsForDiscrete(codec_context->channels);

#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
  // These are bitstream formats unknown to ffmpeg, so they don't have
  // a known sample format size.
  if (codec == kCodecAC3 || codec == kCodecEAC3)
    return true;
#endif

  // Verify that AudioConfig.bits_per_channel was calculated correctly for
  // codecs that have |sample_fmt| set by FFmpeg.
  DCHECK_EQ(av_get_bytes_per_sample(codec_context->sample_fmt) * 8,
            config->bits_per_channel());
  return true;
}

std::unique_ptr<AVCodecContext, ScopedPtrAVFreeContext>
AVStreamToAVCodecContext(const AVStream* stream) {
  std::unique_ptr<AVCodecContext, ScopedPtrAVFreeContext> codec_context(
      avcodec_alloc_context3(nullptr));
  if (avcodec_parameters_to_context(codec_context.get(), stream->codecpar) <
      0) {
    return nullptr;
  }

  return codec_context;
}

bool AVStreamToAudioDecoderConfig(const AVStream* stream,
                                  AudioDecoderConfig* config) {
  std::unique_ptr<AVCodecContext, ScopedPtrAVFreeContext> codec_context(
      AVStreamToAVCodecContext(stream));
  if (!codec_context)
    return false;

  return AVCodecContextToAudioDecoderConfig(
      codec_context.get(), GetEncryptionScheme(stream), config);
}

void AudioDecoderConfigToAVCodecContext(const AudioDecoderConfig& config,
                                        AVCodecContext* codec_context) {
  codec_context->codec_type = AVMEDIA_TYPE_AUDIO;
  codec_context->codec_id = AudioCodecToCodecID(config.codec(),
                                                config.sample_format());
  codec_context->sample_fmt = SampleFormatToAVSampleFormat(
      config.sample_format());

  // TODO(scherkus): should we set |channel_layout|? I'm not sure if FFmpeg uses
  // said information to decode.
  codec_context->channels = config.channels();
  codec_context->sample_rate = config.samples_per_second();

  if (config.extra_data().empty()) {
    codec_context->extradata = nullptr;
    codec_context->extradata_size = 0;
  } else {
    codec_context->extradata_size = config.extra_data().size();
    codec_context->extradata = reinterpret_cast<uint8_t*>(
        av_malloc(config.extra_data().size() + AV_INPUT_BUFFER_PADDING_SIZE));
    memcpy(codec_context->extradata, &config.extra_data()[0],
           config.extra_data().size());
    memset(codec_context->extradata + config.extra_data().size(), '\0',
           AV_INPUT_BUFFER_PADDING_SIZE);
  }
}

bool AVStreamToVideoDecoderConfig(const AVStream* stream,
                                  VideoDecoderConfig* config) {
  std::unique_ptr<AVCodecContext, ScopedPtrAVFreeContext> codec_context(
      AVStreamToAVCodecContext(stream));
  if (!codec_context)
    return false;

  // TODO(vrk): This assumes decoded frame data starts at (0, 0), which is true
  // for now, but may not always be true forever. Fix this in the future.
  gfx::Rect visible_rect(codec_context->width, codec_context->height);
  gfx::Size coded_size = visible_rect.size();

  AVRational aspect_ratio = {1, 1};
  if (stream->sample_aspect_ratio.num)
    aspect_ratio = stream->sample_aspect_ratio;
  else if (codec_context->sample_aspect_ratio.num)
    aspect_ratio = codec_context->sample_aspect_ratio;

  VideoCodec codec = CodecIDToVideoCodec(codec_context->codec_id);

  gfx::Size natural_size =
      GetNaturalSize(visible_rect.size(), aspect_ratio.num, aspect_ratio.den);

  // Without the ffmpeg decoder configured, libavformat is unable to get the
  // profile, format, or coded size. So choose sensible defaults and let
  // decoders fail later if the configuration is actually unsupported.
  //
  // TODO(chcunningham): We need real profiles for all of the codecs below to
  // actually handle capabilities requests correctly. http://crbug.com/784610
  VideoCodecProfile profile = VIDEO_CODEC_PROFILE_UNKNOWN;
  switch (codec) {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    case kCodecH264: {
      profile = ProfileIDToVideoCodecProfile(codec_context->profile);
      // if the profile is still unknown, try to extract it from
      // the extradata using the internal parser
      if (profile == VIDEO_CODEC_PROFILE_UNKNOWN && codec_context->extradata &&
          codec_context->extradata_size) {
        mp4::AVCDecoderConfigurationRecord avc_config;
        if (avc_config.Parse(codec_context->extradata,
                             codec_context->extradata_size)) {
          profile = ProfileIDToVideoCodecProfile(avc_config.profile_indication);
        }
      }
      // All the heuristics failed, let's assign a default profile
      if (profile == VIDEO_CODEC_PROFILE_UNKNOWN)
        profile = H264PROFILE_BASELINE;
      break;
    }
#endif
    case kCodecVP8:
      profile = VP8PROFILE_ANY;
      break;
    case kCodecVP9:
      switch (codec_context->profile) {
        case FF_PROFILE_VP9_0:
          profile = VP9PROFILE_PROFILE0;
          break;
        case FF_PROFILE_VP9_1:
          profile = VP9PROFILE_PROFILE1;
          break;
        case FF_PROFILE_VP9_2:
          profile = VP9PROFILE_PROFILE2;
          break;
        case FF_PROFILE_VP9_3:
          profile = VP9PROFILE_PROFILE3;
          break;
        default:
          profile = VP9PROFILE_MIN;
          break;
      }
      break;
    case kCodecAV1:
      profile = AV1PROFILE_PROFILE_MAIN;
      break;
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    case kCodecHEVC:
      profile = HEVCPROFILE_MAIN;
      break;
#endif
    case kCodecTheora:
      profile = THEORAPROFILE_ANY;
      break;
    default:
      profile = ProfileIDToVideoCodecProfile(codec_context->profile);
  }

  auto* alpha_mode = av_dict_get(stream->metadata, "alpha_mode", nullptr, 0);
  const bool has_alpha = alpha_mode && !strcmp(alpha_mode->value, "1");

  VideoRotation video_rotation = VIDEO_ROTATION_0;
  int rotation = 0;
  AVDictionaryEntry* rotation_entry = NULL;
  rotation_entry = av_dict_get(stream->metadata, "rotate", nullptr, 0);
  if (rotation_entry && rotation_entry->value && rotation_entry->value[0])
    base::StringToInt(rotation_entry->value, &rotation);

  switch (rotation) {
    case 0:
      break;
    case 90:
      video_rotation = VIDEO_ROTATION_90;
      break;
    case 180:
      video_rotation = VIDEO_ROTATION_180;
      break;
    case 270:
      video_rotation = VIDEO_ROTATION_270;
      break;
    default:
      DLOG(ERROR) << "Unsupported video rotation metadata: " << rotation;
      break;
  }

  // Prefer the color space found by libavcodec if available.
  VideoColorSpace color_space =
      VideoColorSpace(codec_context->color_primaries, codec_context->color_trc,
                      codec_context->colorspace,
                      codec_context->color_range == AVCOL_RANGE_JPEG
                          ? gfx::ColorSpace::RangeID::FULL
                          : gfx::ColorSpace::RangeID::LIMITED);
  if (!color_space.IsSpecified()) {
    // VP9 frames may have color information, but that information cannot
    // express new color spaces, like HDR. For that reason, color space
    // information from the container should take precedence over color space
    // information from the VP9 stream. However, if we infer the color space
    // based on resolution here, it looks as if it came from the container.
    // Since this inference causes color shifts and is slated to go away
    // we just skip it for VP9 and leave the color space undefined, which
    // will make the VP9 decoder behave correctly..
    // We also ignore the resolution for AV1, since it's new and it's easy
    // to make it behave correctly from the get-go.
    // TODO(hubbe): Skip this inference for all codecs.
    if (codec_context->codec_id != AV_CODEC_ID_VP9 &&
        codec_context->codec_id != AV_CODEC_ID_AV1) {
      // Otherwise, assume that SD video is usually Rec.601, and HD is usually
      // Rec.709.
      color_space = (natural_size.height() < 720) ? VideoColorSpace::REC601()
                                                  : VideoColorSpace::REC709();
    }
  }

  // AVCodecContext occasionally has invalid extra data. See
  // http://crbug.com/517163
  if (codec_context->extradata != nullptr &&
      codec_context->extradata_size == 0) {
    DLOG(ERROR) << __func__ << " Non-Null extra data cannot have size of 0.";
    return false;
  }

  std::vector<uint8_t> extra_data;
  if (codec_context->extradata_size > 0) {
    extra_data.assign(codec_context->extradata,
                      codec_context->extradata + codec_context->extradata_size);
  }
  // TODO(tmathmeyer) ffmpeg can't provide us with an actual video rotation yet.
  config->Initialize(codec, profile,
                     has_alpha ? VideoDecoderConfig::AlphaMode::kHasAlpha
                               : VideoDecoderConfig::AlphaMode::kIsOpaque,
                     color_space, VideoTransformation(video_rotation),
                     coded_size, visible_rect, natural_size, extra_data,
                     GetEncryptionScheme(stream));

  if (stream->nb_side_data) {
    for (int i = 0; i < stream->nb_side_data; ++i) {
      AVPacketSideData side_data = stream->side_data[i];
      if (side_data.type != AV_PKT_DATA_MASTERING_DISPLAY_METADATA)
        continue;

      HDRMetadata hdr_metadata{};
      AVMasteringDisplayMetadata* metadata =
          reinterpret_cast<AVMasteringDisplayMetadata*>(side_data.data);
      if (metadata->has_primaries) {
        hdr_metadata.mastering_metadata.primary_r =
            gfx::PointF(av_q2d(metadata->display_primaries[0][0]),
                        av_q2d(metadata->display_primaries[0][1]));
        hdr_metadata.mastering_metadata.primary_g =
            gfx::PointF(av_q2d(metadata->display_primaries[1][0]),
                        av_q2d(metadata->display_primaries[1][1]));
        hdr_metadata.mastering_metadata.primary_b =
            gfx::PointF(av_q2d(metadata->display_primaries[2][0]),
                        av_q2d(metadata->display_primaries[2][1]));
        hdr_metadata.mastering_metadata.white_point = gfx::PointF(
            av_q2d(metadata->white_point[0]), av_q2d(metadata->white_point[1]));
      }
      if (metadata->has_luminance) {
        hdr_metadata.mastering_metadata.luminance_max =
            av_q2d(metadata->max_luminance);
        hdr_metadata.mastering_metadata.luminance_min =
            av_q2d(metadata->min_luminance);
      }
      config->set_hdr_metadata(hdr_metadata);
    }
  }

  return true;
}

void VideoDecoderConfigToAVCodecContext(
    const VideoDecoderConfig& config,
    AVCodecContext* codec_context) {
  codec_context->codec_type = AVMEDIA_TYPE_VIDEO;
  codec_context->codec_id = VideoCodecToCodecID(config.codec());
  codec_context->profile = VideoCodecProfileToProfileID(config.profile());
  codec_context->coded_width = config.coded_size().width();
  codec_context->coded_height = config.coded_size().height();
  if (config.color_space_info().range == gfx::ColorSpace::RangeID::FULL)
    codec_context->color_range = AVCOL_RANGE_JPEG;

  if (config.extra_data().empty()) {
    codec_context->extradata = nullptr;
    codec_context->extradata_size = 0;
  } else {
    codec_context->extradata_size = config.extra_data().size();
    codec_context->extradata = reinterpret_cast<uint8_t*>(
        av_malloc(config.extra_data().size() + AV_INPUT_BUFFER_PADDING_SIZE));
    memcpy(codec_context->extradata, &config.extra_data()[0],
           config.extra_data().size());
    memset(codec_context->extradata + config.extra_data().size(), '\0',
           AV_INPUT_BUFFER_PADDING_SIZE);
  }
}

ChannelLayout ChannelLayoutToChromeChannelLayout(int64_t layout, int channels) {
  switch (layout) {
    case AV_CH_LAYOUT_MONO:
      return CHANNEL_LAYOUT_MONO;
    case AV_CH_LAYOUT_STEREO:
      return CHANNEL_LAYOUT_STEREO;
    case AV_CH_LAYOUT_2_1:
      return CHANNEL_LAYOUT_2_1;
    case AV_CH_LAYOUT_SURROUND:
      return CHANNEL_LAYOUT_SURROUND;
    case AV_CH_LAYOUT_4POINT0:
      return CHANNEL_LAYOUT_4_0;
    case AV_CH_LAYOUT_2_2:
      return CHANNEL_LAYOUT_2_2;
    case AV_CH_LAYOUT_QUAD:
      return CHANNEL_LAYOUT_QUAD;
    case AV_CH_LAYOUT_5POINT0:
      return CHANNEL_LAYOUT_5_0;
    case AV_CH_LAYOUT_5POINT1:
      return CHANNEL_LAYOUT_5_1;
    case AV_CH_LAYOUT_5POINT0_BACK:
      return CHANNEL_LAYOUT_5_0_BACK;
    case AV_CH_LAYOUT_5POINT1_BACK:
      return CHANNEL_LAYOUT_5_1_BACK;
    case AV_CH_LAYOUT_7POINT0:
      return CHANNEL_LAYOUT_7_0;
    case AV_CH_LAYOUT_7POINT1:
      return CHANNEL_LAYOUT_7_1;
    case AV_CH_LAYOUT_7POINT1_WIDE:
      return CHANNEL_LAYOUT_7_1_WIDE;
    case AV_CH_LAYOUT_STEREO_DOWNMIX:
      return CHANNEL_LAYOUT_STEREO_DOWNMIX;
    case AV_CH_LAYOUT_2POINT1:
      return CHANNEL_LAYOUT_2POINT1;
    case AV_CH_LAYOUT_3POINT1:
      return CHANNEL_LAYOUT_3_1;
    case AV_CH_LAYOUT_4POINT1:
      return CHANNEL_LAYOUT_4_1;
    case AV_CH_LAYOUT_6POINT0:
      return CHANNEL_LAYOUT_6_0;
    case AV_CH_LAYOUT_6POINT0_FRONT:
      return CHANNEL_LAYOUT_6_0_FRONT;
    case AV_CH_LAYOUT_HEXAGONAL:
      return CHANNEL_LAYOUT_HEXAGONAL;
    case AV_CH_LAYOUT_6POINT1:
      return CHANNEL_LAYOUT_6_1;
    case AV_CH_LAYOUT_6POINT1_BACK:
      return CHANNEL_LAYOUT_6_1_BACK;
    case AV_CH_LAYOUT_6POINT1_FRONT:
      return CHANNEL_LAYOUT_6_1_FRONT;
    case AV_CH_LAYOUT_7POINT0_FRONT:
      return CHANNEL_LAYOUT_7_0_FRONT;
#ifdef AV_CH_LAYOUT_7POINT1_WIDE_BACK
    case AV_CH_LAYOUT_7POINT1_WIDE_BACK:
      return CHANNEL_LAYOUT_7_1_WIDE_BACK;
#endif
    case AV_CH_LAYOUT_OCTAGONAL:
      return CHANNEL_LAYOUT_OCTAGONAL;
    default:
      // FFmpeg channel_layout is 0 for .wav and .mp3.  Attempt to guess layout
      // based on the channel count.
      return GuessChannelLayout(channels);
  }
}

#if !defined(ARCH_CPU_LITTLE_ENDIAN)
#error The code below assumes little-endianness.
#endif

VideoPixelFormat AVPixelFormatToVideoPixelFormat(AVPixelFormat pixel_format) {
  // The YUVJ alternatives are FFmpeg's (deprecated, but still in use) way to
  // specify a pixel format and full range color combination.
  switch (pixel_format) {
    case AV_PIX_FMT_YUV444P:
    case AV_PIX_FMT_YUVJ444P:
      return PIXEL_FORMAT_I444;

    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUVJ420P:
      return PIXEL_FORMAT_I420;

    case AV_PIX_FMT_YUV422P:
    case AV_PIX_FMT_YUVJ422P:
      return PIXEL_FORMAT_I422;

    case AV_PIX_FMT_YUVA420P:
      return PIXEL_FORMAT_I420A;

    case AV_PIX_FMT_YUV420P9LE:
      return PIXEL_FORMAT_YUV420P9;
    case AV_PIX_FMT_YUV420P10LE:
      return PIXEL_FORMAT_YUV420P10;
    case AV_PIX_FMT_YUV420P12LE:
      return PIXEL_FORMAT_YUV420P12;

    case AV_PIX_FMT_YUV422P9LE:
      return PIXEL_FORMAT_YUV422P9;
    case AV_PIX_FMT_YUV422P10LE:
      return PIXEL_FORMAT_YUV422P10;
    case AV_PIX_FMT_YUV422P12LE:
      return PIXEL_FORMAT_YUV422P12;

    case AV_PIX_FMT_YUV444P9LE:
      return PIXEL_FORMAT_YUV444P9;
    case AV_PIX_FMT_YUV444P10LE:
      return PIXEL_FORMAT_YUV444P10;
    case AV_PIX_FMT_YUV444P12LE:
      return PIXEL_FORMAT_YUV444P12;

    case AV_PIX_FMT_P016LE:
      return PIXEL_FORMAT_P016LE;

    default:
      DVLOG(1) << "Unsupported AVPixelFormat: " << pixel_format;
  }
  return PIXEL_FORMAT_UNKNOWN;
}

VideoColorSpace AVColorSpaceToColorSpace(AVColorSpace color_space,
                                         AVColorRange color_range) {
  // TODO(hubbe): make this better
  if (color_range == AVCOL_RANGE_JPEG)
    return VideoColorSpace::JPEG();

  switch (color_space) {
    case AVCOL_SPC_UNSPECIFIED:
      break;
    case AVCOL_SPC_BT709:
      return VideoColorSpace::REC709();
    case AVCOL_SPC_SMPTE170M:
    case AVCOL_SPC_BT470BG:
      return VideoColorSpace::REC601();
    default:
      DVLOG(1) << "Unknown AVColorSpace: " << color_space;
  }
  return VideoColorSpace();
}

std::string AVErrorToString(int errnum) {
  char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
  av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE);
  return std::string(errbuf);
}

int32_t HashCodecName(const char* codec_name) {
  // Use the first 32-bits from the SHA1 hash as the identifier.
  int32_t hash;
  memcpy(&hash, base::SHA1HashString(codec_name).substr(0, 4).c_str(), 4);
  return hash;
}

}  // namespace media
