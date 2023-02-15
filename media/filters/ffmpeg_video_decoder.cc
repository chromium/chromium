// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/ffmpeg_video_decoder.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/decoder_buffer.h"
#include "media/base/limits.h"
#include "media/base/media_log.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_aspect_ratio.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/ffmpeg_decoding_loop.h"

namespace media {

// Returns the number of threads given the FFmpeg CodecID. Also inspects the
// command line for a valid --video-threads flag.
static int GetFFmpegVideoDecoderThreadCount(const VideoDecoderConfig& config) {
  // Most codecs are so old that more threads aren't really needed.
  int desired_threads = limits::kMinVideoDecodeThreads;

  // Some ffmpeg codecs don't actually benefit from using more threads.
  // Only add more threads for those codecs that we know will benefit.
  switch (config.codec()) {
    case VideoCodec::kUnknown:
    case VideoCodec::kVC1:
    case VideoCodec::kMPEG2:
    case VideoCodec::kHEVC:
    case VideoCodec::kVP9:
    case VideoCodec::kAV1:
    case VideoCodec::kDolbyVision:
      // We do not compile ffmpeg with support for any of these codecs.
      break;

    case VideoCodec::kTheora:
    case VideoCodec::kMPEG4:
      // No extra threads for these codecs.
      break;

    case VideoCodec::kH264:
    case VideoCodec::kVP8:
      // Normalize to three threads for 1080p content, then scale linearly
      // with number of pixels.
      // Examples:
      // 4k: 12 threads
      // 1440p: 5 threads
      // 1080p: 3 threads
      // anything lower than 1080p: 2 threads
      desired_threads = config.coded_size().width() *
                        config.coded_size().height() * 3 / 1920 / 1080;
  }

  return VideoDecoder::GetRecommendedThreadCount(desired_threads);
}

static int GetVideoBufferImpl(struct AVCodecContext* s,
                              AVFrame* frame,
                              int flags) {
  FFmpegVideoDecoder* decoder = static_cast<FFmpegVideoDecoder*>(s->opaque);
  return decoder->GetVideoBuffer(s, frame, flags);
}

static void ReleaseVideoBufferImpl(void* opaque, uint8_t* data) {
  if (opaque)
    static_cast<VideoFrame*>(opaque)->Release();
}

// static
bool FFmpegVideoDecoder::IsCodecSupported(VideoCodec codec) {
  return avcodec_find_decoder(VideoCodecToCodecID(codec)) != nullptr;
}

// static
SupportedVideoDecoderConfigs FFmpegVideoDecoder::SupportedConfigsForWebRTC() {
  SupportedVideoDecoderConfigs supported_configs;

  if (IsCodecSupported(VideoCodec::kH264)) {
    supported_configs.emplace_back(/*profile_min=*/H264PROFILE_BASELINE,
                                   /*profile_max=*/H264PROFILE_HIGH,
                                   /*coded_size_min=*/kDefaultSwDecodeSizeMin,
                                   /*coded_size_max=*/kDefaultSwDecodeSizeMax,
                                   /*allow_encrypted=*/false,
                                   /*require_encrypted=*/false);
  }
  if (IsCodecSupported(VideoCodec::kVP8)) {
    supported_configs.emplace_back(/*profile_min=*/VP8PROFILE_ANY,
                                   /*profile_max=*/VP8PROFILE_ANY,
                                   /*coded_size_min=*/kDefaultSwDecodeSizeMin,
                                   /*coded_size_max=*/kDefaultSwDecodeSizeMax,
                                   /*allow_encrypted=*/false,
                                   /*require_encrypted=*/false);
  }

  return supported_configs;
}

FFmpegVideoDecoder::FFmpegVideoDecoder(MediaLog* media_log)
    : media_log_(media_log) {
  DVLOG(1) << __func__;
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

int FFmpegVideoDecoder::GetVideoBuffer(struct AVCodecContext* codec_context,
                                       AVFrame* frame,
                                       int flags) {
  // Don't use |codec_context_| here! With threaded decoding,
  // it will contain unsynchronized width/height/pix_fmt values,
  // whereas |codec_context| contains the current threads's
  // updated width/height/pix_fmt, which can change for adaptive
  // content.
  const VideoPixelFormat format =
      AVPixelFormatToVideoPixelFormat(codec_context->pix_fmt);

  if (format == PIXEL_FORMAT_UNKNOWN)
    return AVERROR(EINVAL);
  DCHECK(format == PIXEL_FORMAT_I420 || format == PIXEL_FORMAT_I422 ||
         format == PIXEL_FORMAT_I444 || format == PIXEL_FORMAT_YUV420P9 ||
         format == PIXEL_FORMAT_YUV420P10 || format == PIXEL_FORMAT_YUV422P9 ||
         format == PIXEL_FORMAT_YUV422P10 || format == PIXEL_FORMAT_YUV444P9 ||
         format == PIXEL_FORMAT_YUV444P10 || format == PIXEL_FORMAT_YUV420P12 ||
         format == PIXEL_FORMAT_YUV422P12 || format == PIXEL_FORMAT_YUV444P12);

  gfx::Size size(codec_context->width, codec_context->height);
  const int ret = av_image_check_size(size.width(), size.height(), 0, NULL);
  if (ret < 0)
    return ret;

  VideoAspectRatio aspect_ratio = config_.aspect_ratio();
  if (!aspect_ratio.IsValid() && codec_context->sample_aspect_ratio.num > 0) {
    aspect_ratio =
        VideoAspectRatio::PAR(codec_context->sample_aspect_ratio.num,
                              codec_context->sample_aspect_ratio.den);
  }
  gfx::Size natural_size = aspect_ratio.GetNaturalSize(gfx::Rect(size));

  // FFmpeg has specific requirements on the allocation size of the frame.  The
  // following logic replicates FFmpeg's allocation strategy to ensure buffers
  // are not overread / overwritten.  See ff_init_buffer_info() for details.
  //
  // When lowres is non-zero, dimensions should be divided by 2^(lowres), but
  // since we don't use this, just DCHECK that it's zero.
  DCHECK_EQ(codec_context->lowres, 0);
  gfx::Size coded_size(std::max(size.width(), codec_context->coded_width),
                       std::max(size.height(), codec_context->coded_height));

  if (force_allocation_error_)
    return AVERROR(EINVAL);

  // FFmpeg expects the initial allocation to be zero-initialized.  Failure to
  // do so can lead to uninitialized value usage.  See http://crbug.com/390941
  scoped_refptr<VideoFrame> video_frame = frame_pool_.CreateFrame(
      format, coded_size, gfx::Rect(size), natural_size, kNoTimestamp);

  if (!video_frame)
    return AVERROR(EINVAL);

  // Prefer the color space from the codec context. If it's not specified (or is
  // set to an unsupported value), fall back on the value from the config.
  VideoColorSpace color_space = AVColorSpaceToColorSpace(
      codec_context->colorspace, codec_context->color_range);
  if (!color_space.IsSpecified())
    color_space = config_.color_space_info();
  video_frame->set_color_space(color_space.ToGfxColorSpace());

  if (codec_context->codec_id == AV_CODEC_ID_VP8 &&
      codec_context->color_primaries == AVCOL_PRI_UNSPECIFIED &&
      codec_context->color_trc == AVCOL_TRC_UNSPECIFIED &&
      codec_context->colorspace == AVCOL_SPC_BT470BG) {
    // vp8 has no colorspace information, except for the color range.
    // However, because of a comment in the vp8 spec, ffmpeg sets the
    // colorspace to BT470BG. We detect this and treat it as unset.
    // If the color range is set to full range, we use the jpeg color space.
    if (codec_context->color_range == AVCOL_RANGE_JPEG) {
      video_frame->set_color_space(gfx::ColorSpace::CreateJpeg());
    }
  } else if (codec_context->codec_id == AV_CODEC_ID_H264 &&
             codec_context->colorspace == AVCOL_SPC_RGB &&
             format == PIXEL_FORMAT_I420) {
    // Some H.264 videos contain a VUI that specifies a color matrix of GBR,
    // when they are actually ordinary YUV. Only 4:2:0 formats are checked,
    // because GBR is reasonable for 4:4:4 content. See crbug.com/1067377.
    video_frame->set_color_space(gfx::ColorSpace::CreateREC709());
  } else if (codec_context->color_primaries != AVCOL_PRI_UNSPECIFIED ||
             codec_context->color_trc != AVCOL_TRC_UNSPECIFIED ||
             codec_context->colorspace != AVCOL_SPC_UNSPECIFIED) {
    media::VideoColorSpace video_color_space = media::VideoColorSpace(
        codec_context->color_primaries, codec_context->color_trc,
        codec_context->colorspace,
        codec_context->color_range != AVCOL_RANGE_MPEG
            ? gfx::ColorSpace::RangeID::FULL
            : gfx::ColorSpace::RangeID::LIMITED);
    video_frame->set_color_space(video_color_space.ToGfxColorSpace());
  }

  for (size_t i = 0; i < VideoFrame::NumPlanes(video_frame->format()); i++) {
    frame->data[i] = video_frame->writable_data(i);
    frame->linesize[i] = video_frame->stride(i);
  }

  frame->width = coded_size.width();
  frame->height = coded_size.height();
  frame->format = codec_context->pix_fmt;
  frame->reordered_opaque = codec_context->reordered_opaque;

  // Now create an AVBufferRef for the data just allocated. It will own the
  // reference to the VideoFrame object.
  VideoFrame* opaque = video_frame.get();
  opaque->AddRef();
  frame->buf[0] =
      av_buffer_create(frame->data[0],
                       VideoFrame::AllocationSize(format, coded_size),
                       ReleaseVideoBufferImpl,
                       opaque,
                       0);
  return 0;
}

VideoDecoderType FFmpegVideoDecoder::GetDecoderType() const {
  return VideoDecoderType::kFFmpeg;
}

void FFmpegVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                    bool low_delay,
                                    CdmContext* /* cdm_context */,
                                    InitCB init_cb,
                                    const OutputCB& output_cb,
                                    const WaitingCB& /* waiting_cb */) {
  DVLOG(1) << __func__ << ": " << config.AsHumanReadableString();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(config.IsValidConfig());
  DCHECK(output_cb);

  InitCB bound_init_cb = base::BindPostTaskToCurrentDefault(std::move(init_cb));

  if (config.is_encrypted()) {
    std::move(bound_init_cb)
        .Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }

  if (!ConfigureDecoder(config, low_delay)) {
    std::move(bound_init_cb).Run(DecoderStatus::Codes::kUnsupportedConfig);
    return;
  }

  // Success!
  config_ = config;
  output_cb_ = output_cb;
  state_ = DecoderState::kNormal;
  std::move(bound_init_cb).Run(DecoderStatus::Codes::kOk);
}

void FFmpegVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                DecodeCB decode_cb) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer.get());
  DCHECK(decode_cb);
  CHECK_NE(state_, DecoderState::kUninitialized);

  DecodeCB decode_cb_bound =
      base::BindPostTaskToCurrentDefault(std::move(decode_cb));

  if (state_ == DecoderState::kError) {
    std::move(decode_cb_bound).Run(DecoderStatus::Codes::kFailed);
    return;
  }

  if (state_ == DecoderState::kDecodeFinished) {
    std::move(decode_cb_bound).Run(DecoderStatus::Codes::kOk);
    return;
  }

  DCHECK_EQ(state_, DecoderState::kNormal);

  // During decode, because reads are issued asynchronously, it is possible to
  // receive multiple end of stream buffers since each decode is acked. There
  // are three states the decoder can be in:
  //
  //   DecoderState::kNormal: This is the starting state. Buffers are decoded.
  //                          Decode errors are discarded.
  //   DecoderState::kDecodeFinished: All calls return empty frames.
  //   DecoderState::kError: Unexpected error happened.
  //
  // These are the possible state transitions.
  //
  // DecoderState::kNormal -> DecoderState::kDecodeFinished:
  //     When EOS buffer is received and the codec has been flushed.
  // DecoderState::kNormal -> DecoderState::kError:
  //     A decoding error occurs and decoding needs to stop.
  // (any state) -> DecoderState::kNormal:
  //     Any time Reset() is called.

  if (!FFmpegDecode(*buffer)) {
    state_ = DecoderState::kError;
    std::move(decode_cb_bound).Run(DecoderStatus::Codes::kFailed);
    return;
  }

  if (buffer->end_of_stream())
    state_ = DecoderState::kDecodeFinished;

  // VideoDecoderShim expects that |decode_cb| is called only after
  // |output_cb_|.
  std::move(decode_cb_bound).Run(DecoderStatus::Codes::kOk);
}

void FFmpegVideoDecoder::Reset(base::OnceClosure closure) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  avcodec_flush_buffers(codec_context_.get());
  state_ = DecoderState::kNormal;
  // PostTask() to avoid calling |closure| immediately.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(closure));
}

FFmpegVideoDecoder::~FFmpegVideoDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ != DecoderState::kUninitialized)
    ReleaseFFmpegResources();
}

bool FFmpegVideoDecoder::FFmpegDecode(const DecoderBuffer& buffer) {
  // Create a packet for input data.
  // Due to FFmpeg API changes we no longer have const read-only pointers.
  // av_init_packet is deprecated and being removed, and ffmpeg clearly does
  // not want to allow on-stack allocation of AVPackets.
  AVPacket* packet = av_packet_alloc();
  if (buffer.end_of_stream()) {
    packet->data = NULL;
    packet->size = 0;
  } else {
    packet->data = const_cast<uint8_t*>(buffer.data());
    packet->size = buffer.data_size();

    DCHECK(packet->data);
    DCHECK_GT(packet->size, 0);

    // Let FFmpeg handle presentation timestamp reordering.
    codec_context_->reordered_opaque = buffer.timestamp().InMicroseconds();
  }
  FFmpegDecodingLoop::DecodeStatus decode_status = decoding_loop_->DecodePacket(
      packet, base::BindRepeating(&FFmpegVideoDecoder::OnNewFrame,
                                  base::Unretained(this)));
  av_packet_free(&packet);
  switch (decode_status) {
    case FFmpegDecodingLoop::DecodeStatus::kSendPacketFailed:
      MEDIA_LOG(ERROR, media_log_)
          << "Failed to send video packet for decoding: "
          << buffer.AsHumanReadableString();
      return false;
    case FFmpegDecodingLoop::DecodeStatus::kFrameProcessingFailed:
      // OnNewFrame() should have already issued a MEDIA_LOG for this.
      return false;
    case FFmpegDecodingLoop::DecodeStatus::kDecodeFrameFailed:
      MEDIA_LOG(DEBUG, media_log_)
          << GetDecoderType() << " failed to decode a video frame: "
          << AVErrorToString(decoding_loop_->last_averror_code()) << ", at "
          << buffer.AsHumanReadableString();
      return false;
    case FFmpegDecodingLoop::DecodeStatus::kOkay:
      break;
  }

  return true;
}

bool FFmpegVideoDecoder::OnNewFrame(AVFrame* frame) {
  // TODO(fbarchard): Work around for FFmpeg http://crbug.com/27675
  // The decoder is in a bad state and not decoding correctly.
  // Checking for NULL avoids a crash in CopyPlane().
  if (!frame->data[VideoFrame::kYPlane] || !frame->data[VideoFrame::kUPlane] ||
      !frame->data[VideoFrame::kVPlane]) {
    DLOG(ERROR) << "Video frame was produced yet has invalid frame data.";
    return false;
  }

  scoped_refptr<VideoFrame> video_frame =
      reinterpret_cast<VideoFrame*>(av_buffer_get_opaque(frame->buf[0]));
  video_frame->set_timestamp(base::Microseconds(frame->reordered_opaque));
  video_frame->metadata().power_efficient = false;
  output_cb_.Run(video_frame);
  return true;
}

void FFmpegVideoDecoder::ReleaseFFmpegResources() {
  decoding_loop_.reset();
  codec_context_.reset();
}

bool FFmpegVideoDecoder::ConfigureDecoder(const VideoDecoderConfig& config,
                                          bool low_delay) {
  DCHECK(config.IsValidConfig());
  DCHECK(!config.is_encrypted());

  // Release existing decoder resources if necessary.
  ReleaseFFmpegResources();

  // Initialize AVCodecContext structure.
  codec_context_.reset(avcodec_alloc_context3(NULL));
  VideoDecoderConfigToAVCodecContext(config, codec_context_.get());

  codec_context_->thread_count = GetFFmpegVideoDecoderThreadCount(config);
  codec_context_->thread_type =
      FF_THREAD_SLICE | (low_delay ? 0 : FF_THREAD_FRAME);
  codec_context_->opaque = this;
  codec_context_->get_buffer2 = GetVideoBufferImpl;

  if (decode_nalus_)
    codec_context_->flags2 |= AV_CODEC_FLAG2_CHUNKS;

  const AVCodec* codec = avcodec_find_decoder(codec_context_->codec_id);
  if (!codec || avcodec_open2(codec_context_.get(), codec, NULL) < 0) {
    ReleaseFFmpegResources();
    return false;
  }

  decoding_loop_ = std::make_unique<FFmpegDecodingLoop>(codec_context_.get());
  return true;
}

}  // namespace media
