// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/ffmpeg_video_decoder.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/limits.h"
#include "media/base/media_log.h"
#include "media/base/timestamp_constants.h"
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
    case kUnknownVideoCodec:
    case kCodecVC1:
    case kCodecMPEG2:
    case kCodecHEVC:
    case kCodecVP9:
    case kCodecAV1:
    case kCodecDolbyVision:
      // We do not compile ffmpeg with support for any of these codecs.
      break;

    case kCodecTheora:
    case kCodecMPEG4:
      // No extra threads for these codecs.
      break;

    case kCodecH264:
    case kCodecVP8:
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

FFmpegVideoDecoder::FFmpegVideoDecoder(MediaLog* media_log)
    : media_log_(media_log), state_(kUninitialized), decode_nalus_(false) {
  DVLOG(1) << __func__;
  thread_checker_.DetachFromThread();
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

  gfx::Size natural_size;
  if (codec_context->sample_aspect_ratio.num > 0) {
    natural_size = GetNaturalSize(size,
                                  codec_context->sample_aspect_ratio.num,
                                  codec_context->sample_aspect_ratio.den);
  } else {
    natural_size =
        GetNaturalSize(gfx::Rect(size), config_.GetPixelAspectRatio());
  }

  // FFmpeg has specific requirements on the allocation size of the frame.  The
  // following logic replicates FFmpeg's allocation strategy to ensure buffers
  // are not overread / overwritten.  See ff_init_buffer_info() for details.
  //
  // When lowres is non-zero, dimensions should be divided by 2^(lowres), but
  // since we don't use this, just DCHECK that it's zero.
  DCHECK_EQ(codec_context->lowres, 0);
  gfx::Size coded_size(std::max(size.width(), codec_context->coded_width),
                       std::max(size.height(), codec_context->coded_height));

  if (!VideoFrame::IsValidConfig(format, VideoFrame::STORAGE_UNKNOWN,
                                 coded_size, gfx::Rect(size), natural_size)) {
    return AVERROR(EINVAL);
  }

  // FFmpeg expects the initialize allocation to be zero-initialized.  Failure
  // to do so can lead to unitialized value usage.  See http://crbug.com/390941
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
    frame->data[i] = video_frame->data(i);
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

std::string FFmpegVideoDecoder::GetDisplayName() const {
  return "FFmpegVideoDecoder";
}

void FFmpegVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                    bool low_delay,
                                    CdmContext* /* cdm_context */,
                                    InitCB init_cb,
                                    const OutputCB& output_cb,
                                    const WaitingCB& /* waiting_cb */) {
  DVLOG(1) << __func__ << ": " << config.AsHumanReadableString();
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(config.IsValidConfig());
  DCHECK(output_cb);

  InitCB bound_init_cb = BindToCurrentLoop(std::move(init_cb));

  if (config.is_encrypted()) {
    std::move(bound_init_cb).Run(false);
    return;
  }

  if (!ConfigureDecoder(config, low_delay)) {
    std::move(bound_init_cb).Run(false);
    return;
  }

  // Success!
  config_ = config;
  output_cb_ = output_cb;
  state_ = kNormal;
  std::move(bound_init_cb).Run(true);
}

void FFmpegVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                DecodeCB decode_cb) {
  DVLOG(3) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(buffer.get());
  DCHECK(decode_cb);
  CHECK_NE(state_, kUninitialized);

  DecodeCB decode_cb_bound = BindToCurrentLoop(std::move(decode_cb));

  if (state_ == kError) {
    std::move(decode_cb_bound).Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  if (state_ == kDecodeFinished) {
    std::move(decode_cb_bound).Run(DecodeStatus::OK);
    return;
  }

  DCHECK_EQ(state_, kNormal);

  // During decode, because reads are issued asynchronously, it is possible to
  // receive multiple end of stream buffers since each decode is acked. There
  // are three states the decoder can be in:
  //
  //   kNormal: This is the starting state. Buffers are decoded. Decode errors
  //            are discarded.
  //   kDecodeFinished: All calls return empty frames.
  //   kError: Unexpected error happened.
  //
  // These are the possible state transitions.
  //
  // kNormal -> kDecodeFinished:
  //     When EOS buffer is received and the codec has been flushed.
  // kNormal -> kError:
  //     A decoding error occurs and decoding needs to stop.
  // (any state) -> kNormal:
  //     Any time Reset() is called.

  if (!FFmpegDecode(*buffer)) {
    state_ = kError;
    std::move(decode_cb_bound).Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  if (buffer->end_of_stream())
    state_ = kDecodeFinished;

  // VideoDecoderShim expects that |decode_cb| is called only after
  // |output_cb_|.
  std::move(decode_cb_bound).Run(DecodeStatus::OK);
}

void FFmpegVideoDecoder::Reset(base::OnceClosure closure) {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  avcodec_flush_buffers(codec_context_.get());
  state_ = kNormal;
  // PostTask() to avoid calling |closure| inmediately.
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(closure));
}

FFmpegVideoDecoder::~FFmpegVideoDecoder() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ != kUninitialized)
    ReleaseFFmpegResources();
}

bool FFmpegVideoDecoder::FFmpegDecode(const DecoderBuffer& buffer) {
  // Create a packet for input data.
  // Due to FFmpeg API changes we no longer have const read-only pointers.
  AVPacket packet;
  av_init_packet(&packet);
  if (buffer.end_of_stream()) {
    packet.data = NULL;
    packet.size = 0;
  } else {
    packet.data = const_cast<uint8_t*>(buffer.data());
    packet.size = buffer.data_size();

    DCHECK(packet.data);
    DCHECK_GT(packet.size, 0);

    // Let FFmpeg handle presentation timestamp reordering.
    codec_context_->reordered_opaque = buffer.timestamp().InMicroseconds();
  }

  switch (decoding_loop_->DecodePacket(
      &packet, base::BindRepeating(&FFmpegVideoDecoder::OnNewFrame,
                                   base::Unretained(this)))) {
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
          << GetDisplayName() << " failed to decode a video frame: "
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
  video_frame->set_timestamp(
      base::TimeDelta::FromMicroseconds(frame->reordered_opaque));
  video_frame->metadata()->SetBoolean(VideoFrameMetadata::POWER_EFFICIENT,
                                      false);
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

  AVCodec* codec = avcodec_find_decoder(codec_context_->codec_id);
  if (!codec || avcodec_open2(codec_context_.get(), codec, NULL) < 0) {
    ReleaseFFmpegResources();
    return false;
  }

  decoding_loop_.reset(new FFmpegDecodingLoop(codec_context_.get()));
  return true;
}

}  // namespace media
