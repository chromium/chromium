// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/cdm/library_cdm/clear_key_cdm/ffmpeg_cdm_audio_decoder.h"

#include <stddef.h>

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/data_buffer.h"
#include "media/base/limits.h"
#include "media/cdm/library_cdm/cdm_host_proxy.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/ffmpeg_decoding_loop.h"

namespace media {

namespace {

// Maximum number of channels with defined layout in src/media.
static const int kMaxChannels = 8;

bool IsValidConfig(const cdm::AudioDecoderConfig_2& config) {
  // No need to check |encryption_scheme| as the buffers will always be
  // decrypted before calling DecodeBuffer().
  return config.codec != cdm::kUnknownAudioCodec && config.channel_count > 0 &&
         config.channel_count <= kMaxChannels && config.bits_per_channel > 0 &&
         config.bits_per_channel <= limits::kMaxBitsPerSample &&
         config.samples_per_second > 0 &&
         config.samples_per_second <= limits::kMaxSampleRate;
}

AVCodecID CdmAudioCodecToCodecID(cdm::AudioCodec audio_codec) {
  switch (audio_codec) {
    case cdm::kCodecVorbis:
      return AV_CODEC_ID_VORBIS;
    case cdm::kCodecAac:
      return AV_CODEC_ID_AAC;
    case cdm::kUnknownAudioCodec:
    default:
      NOTREACHED_IN_MIGRATION()
          << "Unsupported cdm::AudioCodec: " << audio_codec;
      return AV_CODEC_ID_NONE;
  }
}

void CdmAudioDecoderConfigToAVCodecContext(
    const cdm::AudioDecoderConfig_2& config,
    AVCodecContext* codec_context) {
  codec_context->codec_type = AVMEDIA_TYPE_AUDIO;
  codec_context->codec_id = CdmAudioCodecToCodecID(config.codec);

  switch (config.bits_per_channel) {
    case 8:
      codec_context->sample_fmt = AV_SAMPLE_FMT_U8;
      break;
    case 16:
      codec_context->sample_fmt = AV_SAMPLE_FMT_S16;
      break;
    case 32:
      codec_context->sample_fmt = AV_SAMPLE_FMT_S32;
      break;
    default:
      DVLOG(1) << "CdmAudioDecoderConfigToAVCodecContext() Unsupported bits "
                  "per channel: "
               << config.bits_per_channel;
      codec_context->sample_fmt = AV_SAMPLE_FMT_NONE;
  }

  codec_context->ch_layout.nb_channels = config.channel_count;
  codec_context->sample_rate = config.samples_per_second;

  if (config.extra_data) {
    codec_context->extradata_size = config.extra_data_size;
    codec_context->extradata = reinterpret_cast<uint8_t*>(
        av_malloc(config.extra_data_size + AV_INPUT_BUFFER_PADDING_SIZE));
    memcpy(codec_context->extradata, config.extra_data, config.extra_data_size);
    memset(codec_context->extradata + config.extra_data_size, '\0',
           AV_INPUT_BUFFER_PADDING_SIZE);
  } else {
    codec_context->extradata = NULL;
    codec_context->extradata_size = 0;
  }
}

cdm::AudioFormat AVSampleFormatToCdmAudioFormat(AVSampleFormat sample_format) {
  switch (sample_format) {
    case AV_SAMPLE_FMT_U8:
      return cdm::kAudioFormatU8;
    case AV_SAMPLE_FMT_S16:
      return cdm::kAudioFormatS16;
    case AV_SAMPLE_FMT_S32:
      return cdm::kAudioFormatS32;
    case AV_SAMPLE_FMT_FLT:
      return cdm::kAudioFormatF32;
    case AV_SAMPLE_FMT_S16P:
      return cdm::kAudioFormatPlanarS16;
    case AV_SAMPLE_FMT_FLTP:
      return cdm::kAudioFormatPlanarF32;
    default:
      DVLOG(1) << "Unknown AVSampleFormat: " << sample_format;
  }
  return cdm::kUnknownAudioFormat;
}

void CopySamples(cdm::AudioFormat cdm_format,
                 int decoded_audio_size,
                 const AVFrame& av_frame,
                 uint8_t* output_buffer) {
  switch (cdm_format) {
    case cdm::kAudioFormatU8:
    case cdm::kAudioFormatS16:
    case cdm::kAudioFormatS32:
    case cdm::kAudioFormatF32:
      memcpy(output_buffer, av_frame.data[0], decoded_audio_size);
      break;
    case cdm::kAudioFormatPlanarS16:
    case cdm::kAudioFormatPlanarF32: {
      const int decoded_size_per_channel =
          decoded_audio_size / av_frame.ch_layout.nb_channels;
      for (int i = 0; i < av_frame.ch_layout.nb_channels; ++i) {
        memcpy(output_buffer, av_frame.extended_data[i],
               decoded_size_per_channel);
        output_buffer += decoded_size_per_channel;
      }
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION() << "Unsupported CDM Audio Format!";
      memset(output_buffer, 0, decoded_audio_size);
  }
}

}  // namespace

FFmpegCdmAudioDecoder::FFmpegCdmAudioDecoder(CdmHostProxy* cdm_host_proxy)
    : cdm_host_proxy_(cdm_host_proxy) {}

FFmpegCdmAudioDecoder::~FFmpegCdmAudioDecoder() {
  ReleaseFFmpegResources();
}

bool FFmpegCdmAudioDecoder::Initialize(
    const cdm::AudioDecoderConfig_2& config) {
  DVLOG(1) << "Initialize()";
  if (!IsValidConfig(config)) {
    LOG(ERROR) << "Initialize(): invalid audio decoder configuration.";
    return false;
  }

  if (is_initialized_) {
    LOG(ERROR) << "Initialize(): Already initialized.";
    return false;
  }

  // Initialize AVCodecContext structure.
  codec_context_.reset(avcodec_alloc_context3(NULL));
  CdmAudioDecoderConfigToAVCodecContext(config, codec_context_.get());

  // MP3 decodes to S16P which we don't support, tell it to use S16 instead.
  if (codec_context_->sample_fmt == AV_SAMPLE_FMT_S16P)
    codec_context_->request_sample_fmt = AV_SAMPLE_FMT_S16;

  const AVCodec* codec = avcodec_find_decoder(codec_context_->codec_id);
  if (!codec || avcodec_open2(codec_context_.get(), codec, NULL) < 0) {
    DLOG(ERROR) << "Could not initialize audio decoder: "
                << codec_context_->codec_id;
    return false;
  }

  // Ensure avcodec_open2() respected our format request.
  if (codec_context_->sample_fmt == AV_SAMPLE_FMT_S16P) {
    DLOG(ERROR) << "Unable to configure a supported sample format: "
                << codec_context_->sample_fmt;
    return false;
  }

  // Success!
  decoding_loop_ = std::make_unique<FFmpegDecodingLoop>(codec_context_.get());
  samples_per_second_ = config.samples_per_second;
  bytes_per_frame_ =
      codec_context_->ch_layout.nb_channels * config.bits_per_channel / 8;
  output_timestamp_helper_ =
      std::make_unique<AudioTimestampHelper>(config.samples_per_second);
  is_initialized_ = true;

  // Store initial values to guard against midstream configuration changes.
  channels_ = codec_context_->ch_layout.nb_channels;
  av_sample_format_ = codec_context_->sample_fmt;

  return true;
}

void FFmpegCdmAudioDecoder::Deinitialize() {
  DVLOG(1) << "Deinitialize()";
  ReleaseFFmpegResources();
  is_initialized_ = false;
  ResetTimestampState();
}

void FFmpegCdmAudioDecoder::Reset() {
  DVLOG(1) << "Reset()";
  avcodec_flush_buffers(codec_context_.get());
  ResetTimestampState();
}

cdm::Status FFmpegCdmAudioDecoder::DecodeBuffer(
    const uint8_t* compressed_buffer,
    int32_t compressed_buffer_size,
    int64_t input_timestamp,
    cdm::AudioFrames* decoded_frames) {
  DVLOG(1) << "DecodeBuffer()";
  const bool is_end_of_stream = !compressed_buffer;
  base::TimeDelta timestamp = base::Microseconds(input_timestamp);

  if (!is_end_of_stream && timestamp != kNoTimestamp) {
    if (last_input_timestamp_ != kNoTimestamp &&
        timestamp < last_input_timestamp_) {
      base::TimeDelta diff = timestamp - last_input_timestamp_;
      DVLOG(1) << "Input timestamps are not monotonically increasing! "
               << " ts " << timestamp.InMicroseconds() << " us"
               << " diff " << diff.InMicroseconds() << " us";
      return cdm::kDecodeError;
    }
    last_input_timestamp_ = timestamp;
  }

  size_t total_size = 0u;
  std::vector<std::unique_ptr<AVFrame, ScopedPtrAVFreeFrame>> audio_frames;

  AVPacket* packet = av_packet_alloc();
  packet->data = const_cast<uint8_t*>(compressed_buffer);
  packet->size = compressed_buffer_size;

  FFmpegDecodingLoop::DecodeStatus decode_status = decoding_loop_->DecodePacket(
      packet,
      base::BindRepeating(&FFmpegCdmAudioDecoder::OnNewFrame,
                          base::Unretained(this), &total_size, &audio_frames));
  av_packet_free(&packet);
  switch (decode_status) {
    case FFmpegDecodingLoop::DecodeStatus::kSendPacketFailed:
      return cdm::kDecodeError;
    case FFmpegDecodingLoop::DecodeStatus::kFrameProcessingFailed:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case FFmpegDecodingLoop::DecodeStatus::kDecodeFrameFailed:
      DLOG(WARNING) << " failed to decode an audio buffer: "
                    << timestamp.InMicroseconds();
      break;
    case FFmpegDecodingLoop::DecodeStatus::kOkay:
      break;
  }

  if (!output_timestamp_helper_->base_timestamp() && !is_end_of_stream) {
    DCHECK(timestamp != kNoTimestamp);
    output_timestamp_helper_->SetBaseTimestamp(timestamp);
  }

  if (audio_frames.empty())
    return cdm::kNeedMoreData;

  const size_t allocation_size = total_size + 2 * sizeof(int64_t);
  decoded_frames->SetFrameBuffer(cdm_host_proxy_->Allocate(allocation_size));
  if (!decoded_frames->FrameBuffer()) {
    LOG(ERROR) << "DecodeBuffer() ClearKeyCdmHost::Allocate failed.";
    return cdm::kDecodeError;
  }
  decoded_frames->FrameBuffer()->SetSize(allocation_size);

  // Tell the CDM what AudioFormat we're using.
  const cdm::AudioFormat cdm_format = AVSampleFormatToCdmAudioFormat(
      static_cast<AVSampleFormat>(av_sample_format_));
  DCHECK_NE(cdm_format, cdm::kUnknownAudioFormat);
  decoded_frames->SetFormat(cdm_format);

  uint8_t* output_buffer = decoded_frames->FrameBuffer()->Data();
  SerializeInt64(output_timestamp_helper_->GetTimestamp().InMicroseconds(),
                 output_buffer);
  output_buffer += sizeof(int64_t);
  SerializeInt64(total_size, output_buffer);
  output_buffer += sizeof(int64_t);
  output_timestamp_helper_->AddFrames(total_size / bytes_per_frame_);

  for (auto& frame : audio_frames) {
    int decoded_audio_size = 0;
    if (frame->sample_rate != samples_per_second_ ||
        frame->ch_layout.nb_channels != channels_ ||
        frame->format != av_sample_format_) {
      DLOG(ERROR) << "Unsupported midstream configuration change!"
                  << " Sample Rate: " << frame->sample_rate << " vs "
                  << samples_per_second_
                  << ", Channels: " << frame->ch_layout.nb_channels << " vs "
                  << channels_ << ", Sample Format: " << frame->format << " vs "
                  << av_sample_format_;
      return cdm::kDecodeError;
    }

    decoded_audio_size = av_samples_get_buffer_size(
        nullptr, codec_context_->ch_layout.nb_channels, frame->nb_samples,
        codec_context_->sample_fmt, 1);
    if (!decoded_audio_size)
      continue;

    DCHECK_EQ(decoded_audio_size % bytes_per_frame_, 0)
        << "Decoder didn't output full frames";

    CopySamples(cdm_format, decoded_audio_size, *frame, output_buffer);
    output_buffer += decoded_audio_size;
  }

  return cdm::kSuccess;
}

bool FFmpegCdmAudioDecoder::OnNewFrame(
    size_t* total_size,
    std::vector<std::unique_ptr<AVFrame, ScopedPtrAVFreeFrame>>* audio_frames,
    AVFrame* frame) {
  *total_size += av_samples_get_buffer_size(
      nullptr, codec_context_->ch_layout.nb_channels, frame->nb_samples,
      codec_context_->sample_fmt, 1);
  audio_frames->emplace_back(av_frame_clone(frame));
  return true;
}

void FFmpegCdmAudioDecoder::ResetTimestampState() {
  output_timestamp_helper_->Reset();
  last_input_timestamp_ = kNoTimestamp;
}

void FFmpegCdmAudioDecoder::ReleaseFFmpegResources() {
  DVLOG(1) << "ReleaseFFmpegResources()";

  decoding_loop_.reset();
  codec_context_.reset();
}

void FFmpegCdmAudioDecoder::SerializeInt64(int64_t value, uint8_t* dest) {
  memcpy(dest, &value, sizeof(value));
}

}  // namespace media
