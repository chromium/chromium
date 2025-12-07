// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/ffmpeg/ffmpeg_common.h"

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/decoder_buffer.h"
#include "media/base/encryption_scheme.h"
#include "media/base/media_util.h"
#include "media/base/supported_types.h"
#include "media/base/video_aspect_ratio.h"
#include "media/base/video_color_space.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/formats/mp4/aac.h"
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
#include "media/formats/mp4/hevc.h"
#endif
#endif

namespace media {

namespace {

// TODO(crbug.com/379418979): Remove after M133 is stable.
BASE_FEATURE(kStrictFFmpegCodecs, base::FEATURE_ENABLED_BY_DEFAULT);

EncryptionScheme GetEncryptionScheme(const AVStream* stream) {
  AVDictionaryEntry* key =
      av_dict_get(stream->metadata, "enc_key_id", nullptr, 0);
  return key ? EncryptionScheme::kCenc : EncryptionScheme::kUnencrypted;
}

VideoDecoderConfig::AlphaMode GetAlphaMode(const AVStream* stream) {
  AVDictionaryEntry* alpha_mode =
      av_dict_get(stream->metadata, "alpha_mode", nullptr, 0);
  return alpha_mode && std::string_view(alpha_mode->value) == "1"
             ? VideoDecoderConfig::AlphaMode::kHasAlpha
             : VideoDecoderConfig::AlphaMode::kIsOpaque;
}

VideoColorSpace GetGuessedColorSpace(const VideoColorSpace& color_space) {
  return VideoColorSpace::FromGfxColorSpace(
      // convert to gfx color space and make a guess.
      color_space.GuessGfxColorSpace());
}

const char* GetAllowedVideoDecoders() {
  // This should match the configured lists in //third_party/ffmpeg.
#if BUILDFLAG(USE_PROPRIETARY_CODECS) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
  return "h264";
#else
  return "";
#endif
}

void ApplyCodecContextSecuritySettings(AVCodecContext* codec_context) {
  // Future versions of ffmpeg may copy the allow list from the format
  // context.
  if (!codec_context->codec_whitelist) {
    // Note: FFmpeg will try to free this string, so we must duplicate it.
    codec_context->codec_whitelist =
        av_strdup(codec_context->codec_type == AVMEDIA_TYPE_AUDIO
                      ? GetAllowedAudioDecoders()
                      : GetAllowedVideoDecoders());
  }

  // Note: This is security sensitive. FFmpeg may not always continue safely
  // in the presence of errors. See https://crbug.com/379418979
  if (base::FeatureList::IsEnabled(kStrictFFmpegCodecs)) {
    codec_context->err_recognition |= AV_EF_EXPLODE;
  }
}

template <typename T>
void CopyBufferFromConfig(const T& config, AVCodecContext* codec_context) {
  if (config.extra_data().empty()) {
    codec_context->extradata = nullptr;
    codec_context->extradata_size = 0;
    return;
  }
  codec_context->extradata_size = config.extra_data().size();
  codec_context->extradata = reinterpret_cast<uint8_t*>(
      av_malloc(config.extra_data().size() + AV_INPUT_BUFFER_PADDING_SIZE));
  // SAFETY:
  // https://ffmpeg.org/doxygen/6.0/structAVCodecContext.html#abe964316aaaa61967b012efdcced79c4
  // ffmpeg documentation: The allocated memory should be
  // `AV_INPUT_BUFFER_PADDING_SIZE` bytes larger than `extradata_size`. And the
  // memory must be allocated using `av_malloc`.
  //
  // We allocated the appropriate memory according to this rule above and
  // converted it to `base::span` here. So this is safe.
  base::span allocated_extradata = UNSAFE_BUFFERS(
      base::span(codec_context->extradata,
                 static_cast<size_t>(config.extra_data().size() +
                                     AV_INPUT_BUFFER_PADDING_SIZE)));
  auto [extradata, padding] =
      allocated_extradata.split_at(config.extra_data().size());
  extradata.copy_from_nonoverlapping(config.extra_data());
  std::ranges::fill(padding, '\0');
}

base::span<const uint32_t> GetSkipSamples(const AVPacket* packet) {
  size_t skip_samples_size = 0;
  const uint32_t* skip_samples_ptr =
      reinterpret_cast<const uint32_t*>(av_packet_get_side_data(
          packet, AV_PKT_DATA_SKIP_SAMPLES, &skip_samples_size));
  // SAFETY:
  // https://ffmpeg.org/doxygen/6.0/group__lavc__packet.html#ga68712351b8a025b464e5c854d4a9fe1f
  // ffmpeg documentation: av_packet_get_side_data() returns a pointer to
  // already allocated data with a valid size if present and sets `size`
  // to its length, or nullptr if no data is available and sets `size` to zero.
  //
  // Since we are not allocating memory, and it is considered a valid use case
  // to construct a base::span<> from nullptr with size zero, this is safe.
  return UNSAFE_BUFFERS(
      base::span<const uint32_t>(skip_samples_ptr, skip_samples_size));
}

}  // namespace

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
  return base::Microseconds(microseconds);
}

int64_t ConvertToTimeBase(const AVRational& time_base,
                          const base::TimeDelta& timestamp) {
  return av_rescale_q(timestamp.InMicroseconds(), kMicrosBase, time_base);
}

AudioCodec CodecIDToAudioCodec(AVCodecID codec_id) {
  switch (codec_id) {
    case AV_CODEC_ID_AAC:
      return AudioCodec::kAAC;
#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
    case AV_CODEC_ID_AC3:
      return AudioCodec::kAC3;
    case AV_CODEC_ID_EAC3:
      return AudioCodec::kEAC3;
#endif
    case AV_CODEC_ID_MP3:
      return AudioCodec::kMP3;
    case AV_CODEC_ID_VORBIS:
      return AudioCodec::kVorbis;
    case AV_CODEC_ID_PCM_U8:
    case AV_CODEC_ID_PCM_S16LE:
    case AV_CODEC_ID_PCM_S24LE:
    case AV_CODEC_ID_PCM_S32LE:
    case AV_CODEC_ID_PCM_F32LE:
      return AudioCodec::kPCM;
    case AV_CODEC_ID_PCM_S16BE:
      return AudioCodec::kPCM_S16BE;
    case AV_CODEC_ID_PCM_S24BE:
      return AudioCodec::kPCM_S24BE;
    case AV_CODEC_ID_FLAC:
      return AudioCodec::kFLAC;
    case AV_CODEC_ID_PCM_ALAW:
      return AudioCodec::kPCM_ALAW;
    case AV_CODEC_ID_PCM_MULAW:
      return AudioCodec::kPCM_MULAW;
    case AV_CODEC_ID_OPUS:
      return AudioCodec::kOpus;
    case AV_CODEC_ID_ALAC:
      return AudioCodec::kALAC;
#if BUILDFLAG(ENABLE_PLATFORM_MPEG_H_AUDIO)
    case AV_CODEC_ID_MPEGH_3D_AUDIO:
      return AudioCodec::kMpegHAudio;
#endif
    default:
      DVLOG(1) << "Unknown audio CodecID: " << codec_id;
  }
  return AudioCodec::kUnknown;
}

AVCodecID AudioCodecToCodecID(AudioCodec audio_codec,
                              SampleFormat sample_format) {
  switch (audio_codec) {
    case AudioCodec::kAAC:
      return AV_CODEC_ID_AAC;
    case AudioCodec::kALAC:
      return AV_CODEC_ID_ALAC;
    case AudioCodec::kMP3:
      return AV_CODEC_ID_MP3;
    case AudioCodec::kPCM:
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
    case AudioCodec::kPCM_S16BE:
      return AV_CODEC_ID_PCM_S16BE;
    case AudioCodec::kPCM_S24BE:
      return AV_CODEC_ID_PCM_S24BE;
    case AudioCodec::kVorbis:
      return AV_CODEC_ID_VORBIS;
    case AudioCodec::kFLAC:
      return AV_CODEC_ID_FLAC;
    case AudioCodec::kPCM_ALAW:
      return AV_CODEC_ID_PCM_ALAW;
    case AudioCodec::kPCM_MULAW:
      return AV_CODEC_ID_PCM_MULAW;
    case AudioCodec::kOpus:
      return AV_CODEC_ID_OPUS;
#if BUILDFLAG(ENABLE_PLATFORM_MPEG_H_AUDIO)
    case AudioCodec::kMpegHAudio:
      return AV_CODEC_ID_MPEGH_3D_AUDIO;
#endif
    default:
      DVLOG(1) << "Unknown AudioCodec: " << audio_codec;
  }
  return AV_CODEC_ID_NONE;
}

// Converts an FFmpeg video codec ID into its corresponding supported codec id.
static VideoCodec CodecIDToVideoCodec(AVCodecID codec_id) {
  switch (codec_id) {
    case AV_CODEC_ID_H264:
      return VideoCodec::kH264;
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    case AV_CODEC_ID_HEVC:
      return VideoCodec::kHEVC;
#endif
    case AV_CODEC_ID_THEORA:
      return VideoCodec::kTheora;
    case AV_CODEC_ID_MPEG4:
      return VideoCodec::kMPEG4;
    case AV_CODEC_ID_VP8:
      return VideoCodec::kVP8;
    case AV_CODEC_ID_VP9:
      return VideoCodec::kVP9;
    case AV_CODEC_ID_AV1:
      return VideoCodec::kAV1;
    default:
      DVLOG(1) << "Unknown video CodecID: " << codec_id;
  }
  return VideoCodec::kUnknown;
}

AVCodecID VideoCodecToCodecID(VideoCodec video_codec) {
  switch (video_codec) {
    case VideoCodec::kH264:
      return AV_CODEC_ID_H264;
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    case VideoCodec::kHEVC:
      return AV_CODEC_ID_HEVC;
#endif
    case VideoCodec::kTheora:
      return AV_CODEC_ID_THEORA;
    case VideoCodec::kMPEG4:
      return AV_CODEC_ID_MPEG4;
    case VideoCodec::kVP8:
      return AV_CODEC_ID_VP8;
    case VideoCodec::kVP9:
      return AV_CODEC_ID_VP9;
    case VideoCodec::kAV1:
      return AV_CODEC_ID_AV1;
    default:
      DVLOG(1) << "Unknown VideoCodec: " << video_codec;
  }
  return AV_CODEC_ID_NONE;
}

static VideoCodecProfile ProfileIDToVideoCodecProfile(int profile) {
  // Clear out the CONSTRAINED & INTRA flags which are strict subsets of the
  // corresponding profiles with which they're used.
  profile &= ~AV_PROFILE_H264_CONSTRAINED;
  profile &= ~AV_PROFILE_H264_INTRA;
  switch (profile) {
    case AV_PROFILE_H264_BASELINE:
      return H264PROFILE_BASELINE;
    case AV_PROFILE_H264_MAIN:
      return H264PROFILE_MAIN;
    case AV_PROFILE_H264_EXTENDED:
      return H264PROFILE_EXTENDED;
    case AV_PROFILE_H264_HIGH:
      return H264PROFILE_HIGH;
    case AV_PROFILE_H264_HIGH_10:
      return H264PROFILE_HIGH10PROFILE;
    case AV_PROFILE_H264_HIGH_422:
      return H264PROFILE_HIGH422PROFILE;
    case AV_PROFILE_H264_HIGH_444_PREDICTIVE:
      return H264PROFILE_HIGH444PREDICTIVEPROFILE;
    default:
      DVLOG(1) << "Unknown profile id: " << profile;
  }
  return VIDEO_CODEC_PROFILE_UNKNOWN;
}

static int VideoCodecProfileToProfileID(VideoCodecProfile profile) {
  switch (profile) {
    case H264PROFILE_BASELINE:
      return AV_PROFILE_H264_BASELINE;
    case H264PROFILE_MAIN:
      return AV_PROFILE_H264_MAIN;
    case H264PROFILE_EXTENDED:
      return AV_PROFILE_H264_EXTENDED;
    case H264PROFILE_HIGH:
      return AV_PROFILE_H264_HIGH;
    case H264PROFILE_HIGH10PROFILE:
      return AV_PROFILE_H264_HIGH_10;
    case H264PROFILE_HIGH422PROFILE:
      return AV_PROFILE_H264_HIGH_422;
    case H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return AV_PROFILE_H264_HIGH_444_PREDICTIVE;
    default:
      DVLOG(1) << "Unknown VideoCodecProfile: " << profile;
  }
  return AV_PROFILE_UNKNOWN;
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
      codec_context->ch_layout.nb_channels > 8
          ? CHANNEL_LAYOUT_DISCRETE
          : ChannelLayoutToChromeChannelLayout(
                codec_context->ch_layout.u.mask,
                codec_context->ch_layout.nb_channels);

  switch (codec) {
    // For AC3/EAC3 we enable only demuxing, but not decoding, so FFmpeg does
    // not fill |sample_fmt|.
    case AudioCodec::kAC3:
    case AudioCodec::kEAC3:
#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
      // The spec for AC3/EAC3 audio is ETSI TS 102 366. According to sections
      // F.3.1 and F.5.1 in that spec the sample_format for AC3/EAC3 must be 16.
      sample_format = kSampleFormatS16;
      break;
#else
      NOTREACHED();
#endif
#if BUILDFLAG(ENABLE_PLATFORM_MPEG_H_AUDIO)
    case AudioCodec::kMpegHAudio:
      channel_layout = CHANNEL_LAYOUT_BITSTREAM;
      sample_format = kSampleFormatMpegHAudio;
      break;
#endif

    default:
      break;
  }

  base::TimeDelta seek_preroll;
  if (codec_context->seek_preroll > 0) {
    seek_preroll = base::Microseconds(codec_context->seek_preroll * 1000000.0 /
                                      codec_context->sample_rate);
  }

  // AVStream occasionally has invalid extra data. See http://crbug.com/517163
  if ((codec_context->extradata_size == 0) !=
      (codec_context->extradata == nullptr)) {
    DLOG(ERROR) << __func__
                << (codec_context->extradata == nullptr ? " NULL" : " Non-NULL")
                << " extra data cannot have size of "
                << codec_context->extradata_size << ".";
    return false;
  }

  std::vector<uint8_t> extra_data;
  if (codec_context->extradata_size > 0) {
    extra_data.resize(codec_context->extradata_size);
    base::span(extra_data)
        .copy_from_nonoverlapping(AVCodecContextExtraDataToSpan(codec_context));
  }

  config->Initialize(codec, sample_format, channel_layout, codec_context->sample_rate,
                     extra_data, encryption_scheme, seek_preroll,
                     codec_context->delay);
  if (channel_layout == CHANNEL_LAYOUT_DISCRETE)
    config->SetChannelsForDiscrete(codec_context->ch_layout.nb_channels);

#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
  // These are bitstream formats unknown to ffmpeg, so they don't have
  // a known sample format size.
  if (codec == AudioCodec::kAC3 || codec == AudioCodec::kEAC3)
    return true;
#endif
#if BUILDFLAG(ENABLE_PLATFORM_MPEG_H_AUDIO)
  if (codec == AudioCodec::kMpegHAudio)
    return true;
#endif

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (codec == AudioCodec::kAAC) {
    // TODO(dalecurtis): Just use the profile from the codec context if ffmpeg
    // ever starts supporting xHE-AAC.
    // FFmpeg provides the (defined_profile - 1) for AVCodecContext::profile
    if (codec_context->profile == AV_PROFILE_UNKNOWN ||
        codec_context->profile == mp4::AAC::kXHeAAcType - 1) {
      // Errors aren't fatal here, so just drop any MediaLog messages.
      NullMediaLog media_log;
      mp4::AAC aac_parser;
      if (aac_parser.Parse(extra_data, &media_log))
        config->set_profile(aac_parser.GetProfile());
    }
  }
#endif

  // Verify that AudioConfig.bytes_per_channel was calculated correctly for
  // codecs that have |sample_fmt| set by FFmpeg.
  DCHECK_EQ(av_get_bytes_per_sample(codec_context->sample_fmt),
            config->bytes_per_channel());
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

  ApplyCodecContextSecuritySettings(codec_context.get());
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
  codec_context->ch_layout.nb_channels = config.channels();
  codec_context->sample_rate = config.samples_per_second();

  CopyBufferFromConfig(config, codec_context);
  ApplyCodecContextSecuritySettings(codec_context);
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
  gfx::HDRMetadata hdr_metadata;

  // In some cases a container may have a DAR but no PAR, but FFmpeg translates
  // everything to PAR. It is possible to get the render width and height, but I
  // didn't find a way to determine whether that should be preferred to the PAR.
  VideoAspectRatio aspect_ratio;
  if (stream->sample_aspect_ratio.num) {
    aspect_ratio = VideoAspectRatio::PAR(stream->sample_aspect_ratio.num,
                                         stream->sample_aspect_ratio.den);
  } else if (codec_context->sample_aspect_ratio.num) {
    aspect_ratio =
        VideoAspectRatio::PAR(codec_context->sample_aspect_ratio.num,
                              codec_context->sample_aspect_ratio.den);
  }

  // Used to guess color space and to create the config. The first use should
  // probably change to coded size, and the second should be removed as part of
  // crbug.com/1214061.
  gfx::Size natural_size = aspect_ratio.GetNaturalSize(visible_rect);

  VideoCodec codec = CodecIDToVideoCodec(codec_context->codec_id);

  // Without the ffmpeg decoder configured, libavformat is unable to get the
  // profile, format, or coded size. So choose sensible defaults and let
  // decoders fail later if the configuration is actually unsupported.
  //
  // TODO(chcunningham): We need real profiles for all of the codecs below to
  // actually handle capabilities requests correctly. http://crbug.com/784610
  VideoCodecProfile profile = VIDEO_CODEC_PROFILE_UNKNOWN;

  // Prefer the color space found by libavcodec if available
  VideoColorSpace color_space =
      VideoColorSpace(codec_context->color_primaries, codec_context->color_trc,
                      codec_context->colorspace,
                      codec_context->color_range == AVCOL_RANGE_JPEG
                          ? gfx::ColorSpace::RangeID::FULL
                          : gfx::ColorSpace::RangeID::LIMITED);
  VideoPixelFormat pixel_format =
      AVPixelFormatToVideoPixelFormat(codec_context->pix_fmt);
  VideoDecoderConfig::AlphaMode alpha_mode = GetAlphaMode(stream);
  VideoChromaSampling chroma_sampling =
      VideoPixelFormatToChromaSampling(pixel_format);

  switch (codec) {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    case VideoCodec::kH264: {
      profile = ProfileIDToVideoCodecProfile(codec_context->profile);
      // if the profile is still unknown, try to extract it from
      // the extradata using the internal parser
      if (profile == VIDEO_CODEC_PROFILE_UNKNOWN && codec_context->extradata &&
          codec_context->extradata_size) {
        mp4::AVCDecoderConfigurationRecord avc_config;
        if (avc_config.Parse(
                AVCodecContextExtraDataToSpan(codec_context.get()))) {
          profile = ProfileIDToVideoCodecProfile(avc_config.profile_indication);
        }
      }
      // All the heuristics failed, let's assign a default profile
      if (profile == VIDEO_CODEC_PROFILE_UNKNOWN)
        profile = H264PROFILE_BASELINE;
      break;
    }
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    case VideoCodec::kHEVC: {
      int hevc_profile = -1;
      // We need to parse extradata each time, because we won't add ffmpeg
      // hevc decoder & parser to chromium and codec_context->profile
      // should always be FF_PROFILE_UNKNOWN (-99) here
      if (codec_context->extradata && codec_context->extradata_size) {
        mp4::HEVCDecoderConfigurationRecord hevc_config;
        if (hevc_config.Parse(
                AVCodecContextExtraDataToSpan(codec_context.get()))) {
          hevc_profile = hevc_config.general_profile_idc;
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
          if (!color_space.IsSpecified()) {
            // We should try to parsed color space from SPS if the
            // result from libavcodec is not specified in case
            // that some encoder not write extra colorspace info to
            // the container
            color_space = hevc_config.GetColorSpace();
          }
          hdr_metadata = hevc_config.GetHDRMetadata();
          alpha_mode = hevc_config.GetAlphaMode();
          chroma_sampling = hevc_config.GetChromaSampling();
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
        }
      }
      // The values of general_profile_idc are taken from the HEVC standard, see
      // the latest https://www.itu.int/rec/T-REC-H.265/en
      switch (hevc_profile) {
        case 1:
          profile = HEVCPROFILE_MAIN;
          break;
        case 2:
          profile = HEVCPROFILE_MAIN10;
          break;
        case 3:
          profile = HEVCPROFILE_MAIN_STILL_PICTURE;
          break;
        case 4:
          profile = HEVCPROFILE_REXT;
          break;
        case 5:
          profile = HEVCPROFILE_HIGH_THROUGHPUT;
          break;
        case 6:
          profile = HEVCPROFILE_MULTIVIEW_MAIN;
          break;
        case 7:
          profile = HEVCPROFILE_SCALABLE_MAIN;
          break;
        case 8:
          profile = HEVCPROFILE_3D_MAIN;
          break;
        case 9:
          profile = HEVCPROFILE_SCREEN_EXTENDED;
          break;
        case 10:
          profile = HEVCPROFILE_SCALABLE_REXT;
          break;
        case 11:
          profile = HEVCPROFILE_HIGH_THROUGHPUT_SCREEN_EXTENDED;
          break;
        default:
          // Always assign a default if all heuristics fail.
          profile = HEVCPROFILE_MAIN;
          break;
      }
      break;
    }
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
    case VideoCodec::kVP8:
      profile = VP8PROFILE_ANY;
      break;
    case VideoCodec::kVP9:
      switch (codec_context->profile) {
        case AV_PROFILE_VP9_0:
          profile = VP9PROFILE_PROFILE0;
          break;
        case AV_PROFILE_VP9_1:
          profile = VP9PROFILE_PROFILE1;
          break;
        case AV_PROFILE_VP9_2:
          profile = VP9PROFILE_PROFILE2;
          break;
        case AV_PROFILE_VP9_3:
          profile = VP9PROFILE_PROFILE3;
          break;
        default:
          profile = VP9PROFILE_MIN;
          break;
      }
      break;
#if BUILDFLAG(ENABLE_AV1_DECODER)
    case VideoCodec::kAV1:
      profile = AV1PROFILE_PROFILE_MAIN;
      if (codec_context->extradata && codec_context->extradata_size) {
        mp4::AV1CodecConfigurationRecord av1_config;
        if (av1_config.Parse(codec_context->extradata,
                             codec_context->extradata_size)) {
          profile = av1_config.profile;
        } else {
          DLOG(WARNING) << "Failed to parse AV1 extra data for profile.";
        }
      }
      break;
#endif  // BUILDFLAG(ENABLE_AV1_DECODER)
    case VideoCodec::kTheora:
      profile = THEORAPROFILE_ANY;
      break;
    default:
      profile = ProfileIDToVideoCodecProfile(codec_context->profile);
  }

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
  } else if ((codec_context->codec_id == AV_CODEC_ID_HEVC ||
              codec_context->codec_id == AV_CODEC_ID_H264) &&
             codec_context->colorspace == AVCOL_SPC_RGB &&
             chroma_sampling != VideoChromaSampling::k444) {
    // Some H.264/H.265 videos contain a VUI that specifies a color matrix of
    // GBR, when they are actually ordinary YUV. Default to BT.709 if the format
    // is not 4:4:4 as GBR is only reasonable for 4:4:4 content. See
    // crbug.com/40682932, crbug.com/341266991, crbug.com/342003180, and
    // crbug.com/343014700.
    color_space = VideoColorSpace::REC709();
  } else if (codec_context->codec_id == AV_CODEC_ID_HEVC &&
             (color_space.primaries == VideoColorSpace::PrimaryID::INVALID ||
              color_space.transfer == VideoColorSpace::TransferID::INVALID ||
              color_space.matrix == VideoColorSpace::MatrixID::INVALID) &&
             pixel_format == PIXEL_FORMAT_I420) {
    // Some HEVC SDR content encoded by the Adobe Premiere HW HEVC encoder has
    // invalid primaries but valid transfer and matrix, and some HEVC SDR
    // content encoded by web camera has invalid primaries and transfer, this
    // will cause IsHevcProfileSupported return "false" and fail to playback.
    // make a guess can at least make these videos able to play. See
    // crbug.com/1374270.
    color_space = GetGuessedColorSpace(color_space);
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
    extra_data.resize(codec_context->extradata_size);
    base::span(extra_data)
        .copy_from_nonoverlapping(
            AVCodecContextExtraDataToSpan(codec_context.get()));
  }

  VideoTransformation video_transformation = VideoTransformation();
  for (const auto& side_data :
       AVCodecParametersCodedSideToSpan(stream->codecpar)) {
    switch (side_data.type) {
      case AV_PKT_DATA_DISPLAYMATRIX: {
        CHECK_EQ(side_data.size, sizeof(int32_t) * 3 * 3);
        video_transformation = VideoTransformation::FromFFmpegDisplayMatrix(
            reinterpret_cast<int32_t*>(side_data.data));
        break;
      }
      case AV_PKT_DATA_MASTERING_DISPLAY_METADATA: {
        AVMasteringDisplayMetadata* mdcv =
            reinterpret_cast<AVMasteringDisplayMetadata*>(side_data.data);
        gfx::HdrMetadataSmpteSt2086 smpte_st_2086;
        if (mdcv->has_primaries) {
          smpte_st_2086.primaries = {
              static_cast<float>(av_q2d(mdcv->display_primaries[0][0])),
              static_cast<float>(av_q2d(mdcv->display_primaries[0][1])),
              static_cast<float>(av_q2d(mdcv->display_primaries[1][0])),
              static_cast<float>(av_q2d(mdcv->display_primaries[1][1])),
              static_cast<float>(av_q2d(mdcv->display_primaries[2][0])),
              static_cast<float>(av_q2d(mdcv->display_primaries[2][1])),
              static_cast<float>(av_q2d(mdcv->white_point[0])),
              static_cast<float>(av_q2d(mdcv->white_point[1])),
          };
        }
        if (mdcv->has_luminance) {
          smpte_st_2086.luminance_max = av_q2d(mdcv->max_luminance);
          smpte_st_2086.luminance_min = av_q2d(mdcv->min_luminance);
        }

        // TODO(crbug.com/40268540): Consider rejecting metadata that
        // does not specify all values.
        if (mdcv->has_primaries || mdcv->has_luminance) {
          hdr_metadata.smpte_st_2086 = smpte_st_2086;
        }
        break;
      }
      case AV_PKT_DATA_CONTENT_LIGHT_LEVEL: {
        AVContentLightMetadata* clli =
            reinterpret_cast<AVContentLightMetadata*>(side_data.data);
        hdr_metadata.cta_861_3 =
            gfx::HdrMetadataCta861_3(clli->MaxCLL, clli->MaxFALL);
        break;
      }
#if BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
      case AV_PKT_DATA_DOVI_CONF: {
        AVDOVIDecoderConfigurationRecord* dovi =
            reinterpret_cast<AVDOVIDecoderConfigurationRecord*>(side_data.data);
        VideoType type;
        type.codec = VideoCodec::kDolbyVision;
        type.level = dovi->dv_level;
        type.color_space = color_space;
        type.hdr_metadata_type = gfx::HdrMetadataType::kNone;
        switch (dovi->dv_profile) {
          case 0:
            type.profile = VideoCodecProfile::DOLBYVISION_PROFILE0;
            break;
          case 5:
            type.profile = VideoCodecProfile::DOLBYVISION_PROFILE5;
            break;
          case 7:
            type.profile = VideoCodecProfile::DOLBYVISION_PROFILE7;
            break;
          case 8:
            type.profile = VideoCodecProfile::DOLBYVISION_PROFILE8;
            break;
          case 9:
            type.profile = VideoCodecProfile::DOLBYVISION_PROFILE9;
            break;
          default:
            type.profile = VideoCodecProfile::VIDEO_CODEC_PROFILE_UNKNOWN;
            break;
        }
        // Treat dolby vision contents as dolby vision codec only if the
        // device support clear DV decoding, otherwise use the original
        // HEVC or AVC codec and profile.
        if (media::IsDecoderSupportedVideoType(type)) {
          codec = type.codec;
          profile = type.profile;
        }
        break;
      }
#endif  // BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
      default:
        break;
    }
  }

  // TODO(tmathmeyer) ffmpeg can't provide us with an actual video rotation yet.
  config->Initialize(codec, profile, alpha_mode, color_space,
                     video_transformation, coded_size, visible_rect,
                     natural_size, extra_data, GetEncryptionScheme(stream));
  // Set the aspect ratio explicitly since our version hasn't been rounded.
  config->set_aspect_ratio(aspect_ratio);

  if (hdr_metadata.IsValid()) {
    config->set_hdr_metadata(hdr_metadata);
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

  CopyBufferFromConfig(config, codec_context);
  ApplyCodecContextSecuritySettings(codec_context);
}

ChannelLayout ChannelLayoutToChromeChannelLayout(int64_t layout, int channels) {
  switch (layout) {
    case AV_CH_LAYOUT_MONO:
      return CHANNEL_LAYOUT_MONO;
    case AV_CH_LAYOUT_STEREO:
      return CHANNEL_LAYOUT_STEREO;
    case AV_CH_LAYOUT_2POINT1:
      return CHANNEL_LAYOUT_2POINT1;
    case AV_CH_LAYOUT_2_1:
      return CHANNEL_LAYOUT_2_1;
    case AV_CH_LAYOUT_SURROUND:
      return CHANNEL_LAYOUT_SURROUND;
    case AV_CH_LAYOUT_3POINT1:
      return CHANNEL_LAYOUT_3_1;
    case AV_CH_LAYOUT_4POINT0:
      return CHANNEL_LAYOUT_4_0;
    case AV_CH_LAYOUT_4POINT1:
      return CHANNEL_LAYOUT_4_1;
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
    case AV_CH_LAYOUT_7POINT0:
      return CHANNEL_LAYOUT_7_0;
    case AV_CH_LAYOUT_7POINT0_FRONT:
      return CHANNEL_LAYOUT_7_0_FRONT;
    case AV_CH_LAYOUT_7POINT1:
      return CHANNEL_LAYOUT_7_1;
    case AV_CH_LAYOUT_7POINT1_WIDE:
      return CHANNEL_LAYOUT_7_1_WIDE;
    case AV_CH_LAYOUT_7POINT1_WIDE_BACK:
      return CHANNEL_LAYOUT_7_1_WIDE_BACK;
    case AV_CH_LAYOUT_OCTAGONAL:
      return CHANNEL_LAYOUT_OCTAGONAL;
    case AV_CH_LAYOUT_STEREO_DOWNMIX:
      return CHANNEL_LAYOUT_STEREO_DOWNMIX;
    case AV_CH_FRONT_CENTER | AV_CH_LOW_FREQUENCY:
      return CHANNEL_LAYOUT_1_1;
    case AV_CH_FRONT_LEFT | AV_CH_FRONT_RIGHT | AV_CH_LOW_FREQUENCY |
        AV_CH_BACK_CENTER:
      return CHANNEL_LAYOUT_3_1_BACK;
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
    case AV_PIX_FMT_GBRP:
      return PIXEL_FORMAT_I444;

    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUVJ420P:
      return PIXEL_FORMAT_I420;

    case AV_PIX_FMT_YUV422P:
    case AV_PIX_FMT_YUVJ422P:
      return PIXEL_FORMAT_I422;

    case AV_PIX_FMT_YUVA420P:
      return PIXEL_FORMAT_I420A;

    // Default to 10-bit pixel formats for 9-bits since they are non-standard
    // and were never seen in the wild.
    case AV_PIX_FMT_YUV420P9LE:
    case AV_PIX_FMT_YUV420P10LE:
      return PIXEL_FORMAT_YUV420P10;
    case AV_PIX_FMT_YUV420P12LE:
      return PIXEL_FORMAT_YUV420P12;

    case AV_PIX_FMT_YUV422P9LE:
    case AV_PIX_FMT_YUV422P10LE:
      return PIXEL_FORMAT_YUV422P10;
    case AV_PIX_FMT_YUV422P12LE:
      return PIXEL_FORMAT_YUV422P12;

    case AV_PIX_FMT_YUV444P9LE:
    case AV_PIX_FMT_GBRP9LE:
    case AV_PIX_FMT_YUV444P10LE:
    case AV_PIX_FMT_GBRP10LE:
      return PIXEL_FORMAT_YUV444P10;
    case AV_PIX_FMT_YUV444P12LE:
    case AV_PIX_FMT_GBRP12LE:
      return PIXEL_FORMAT_YUV444P12;

    default:
      // FFmpeg knows more pixel formats than Chromium cares about.
      DVLOG(1) << "Unsupported pixel format: " << pixel_format;
      return PIXEL_FORMAT_UNKNOWN;
  }
}

std::string AVErrorToString(int errnum) {
  char errbuf[AV_ERROR_MAX_STRING_SIZE] = {};
  av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE);
  return std::string(errbuf);
}

int32_t HashCodecName(const char* codec_name) {
  // Use the first 32-bits from the SHA1 hash as the identifier.
  int32_t hash;
  base::byte_span_from_ref(hash).copy_from_nonoverlapping(
      base::as_byte_span(base::SHA1HashString(codec_name)).first<4>());
  return hash;
}

const char* GetAllowedAudioDecoders() {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#define EXTRA_CODECS ",aac"
#else
#define EXTRA_CODECS
#endif

  // This should match the configured lists in //third_party/ffmpeg.
  static constexpr std::string_view kAllowedAudioCodecs =
      "vorbis,libopus,flac,pcm_u8,pcm_s16le,pcm_s24le,pcm_s32le,pcm_f32le,"
      "mp3,pcm_s16be,pcm_s24be,pcm_mulaw,pcm_alaw" EXTRA_CODECS;
#undef EXTRA_CODECS

  return kAllowedAudioCodecs.data();
}

base::TimeDelta ConvertStreamTimestamp(const AVRational& time_base,
                                       int64_t timestamp) {
  if (timestamp == kNoFFmpegTimestamp) {
    return kNoTimestamp;
  }

  return ConvertFromTimeBase(time_base, timestamp);
}

std::optional<DecoderBufferSideData::DiscardPadding>
GetDiscardPaddingFromAVPacket(const AVPacket* packet, int samples_per_second) {
  // Skip samples are only valid for audio packets.
  constexpr int kSkipSamplesValidSize = 10;
  constexpr int kSkipEndSamplesOffset = 1;
  base::span<const uint32_t> skip_samples = GetSkipSamples(packet);
  if (skip_samples.size() >= kSkipSamplesValidSize) {
    // Because FFmpeg rolls codec delay and skip samples into one we can only
    // allow front discard padding on the first packet.  Otherwise the discard
    // helper can't figure out which data to discard.  See AudioDiscardHelper.
    //
    // NOTE: Large values may end up as negative here, but negatives are
    // discarded below.
    auto discard_front_samples = static_cast<int>(skip_samples[0]);
    if (discard_front_samples < 0) {
      // See https://crbug.com/1189939 and https://trac.ffmpeg.org/ticket/9622
      DLOG(ERROR) << "Negative skip samples are not allowed.";
      discard_front_samples = 0;
    }

    // NOTE: Large values may end up as negative here, which could lead to
    // a negative timestamp. It's not clear if this is intentional.
    const auto discard_end_samples =
        static_cast<int>(skip_samples[kSkipEndSamplesOffset]);

    if (discard_front_samples || discard_end_samples) {
      const auto front_discard = AudioTimestampHelper::FramesToTime(
          discard_front_samples, samples_per_second);
      return std::make_pair(front_discard,
                            AudioTimestampHelper::FramesToTime(
                                discard_end_samples, samples_per_second));
    }
  }

  // If the packet is marked for complete discard and it doesn't already have
  // any discard padding set, mark the side data for complete discard. We don't
  // want to overwrite any existing discard padding since the discard padding
  // may refer to frames beyond this packet.
  if (packet->flags & AV_PKT_FLAG_DISCARD) {
    return std::make_pair(kInfiniteDuration, base::TimeDelta());
  }

  return std::nullopt;
}

}  // namespace media
