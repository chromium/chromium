// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/filters/ffmpeg_video_decoder.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <numeric>

#include "base/bits.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/decoder_buffer.h"
#include "media/base/limits.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/supported_types.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_aspect_ratio.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/ffmpeg_decoding_loop.h"
#include "media/filters/ffmpeg_glue.h"

namespace media {

namespace {

// Dynamically allocated AVBuffer opaque data.
struct OpaqueData {
  OpaqueData(void* fb,
             scoped_refptr<FrameBufferPool> pool,
             uint8_t* d,
             size_t z,
             VideoFrameLayout l)
      : fb_priv(fb),
        frame_pool(std::move(pool)),
        data(d),
        size(z),
        layout(std::move(l)) {}

  // FrameBufferPool key that we'll free when the AVBuffer is unused.
  raw_ptr<void> fb_priv = nullptr;

  // Pool which owns `fb_priv`.
  scoped_refptr<FrameBufferPool> frame_pool;

  // Data pointer from `fb_priv`.  This is owned by `fb_priv`; do not free it.
  raw_ptr<uint8_t> data = nullptr;

  // Size of `data`.
  size_t size = 0;

  // Layout used to compute the size / stride / etc.
  VideoFrameLayout layout;
};

}  // namespace

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
    case VideoCodec::kTheora:
    case VideoCodec::kMPEG4:
    case VideoCodec::kVP8:
      // We do not compile ffmpeg with support for any of these codecs.
      NOTREACHED();

    case VideoCodec::kH264:
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
  if (!opaque) {
    return;
  }

  OpaqueData* opaque_data = static_cast<OpaqueData*>(opaque);
  opaque_data->frame_pool->ReleaseFrameBuffer(opaque_data->fb_priv);
  delete opaque_data;
}

// static
bool FFmpegVideoDecoder::IsCodecSupported(VideoCodec codec) {
  // We only build support for H.264.
  return codec == VideoCodec::kH264 && IsBuiltInVideoCodec(codec);
}

FFmpegVideoDecoder::FFmpegVideoDecoder(MediaLog* media_log)
    : media_log_(media_log), timestamp_map_(128) {
  DVLOG(1) << __func__;
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

int FFmpegVideoDecoder::GetVideoBuffer(struct AVCodecContext* codec_context,
                                       AVFrame* frame,
                                       int flags) {
  // Don't use |codec_context_| here! With threaded decoding,
  // it will contain unsynchronized width/height/pix_fmt values.  Accessing
  // `codec_context` is also somewhat unreliable, sometimes providing incorrect
  // values in a "missing memory barriers" kind of way.
  //
  // Instead, use `frame` for width / height / pix_fmt.

  // Do not trust `codec_context->pix_fmt`.
  const auto format = AVPixelFormatToVideoPixelFormat(
      static_cast<AVPixelFormat>(frame->format));

  if (format == PIXEL_FORMAT_UNKNOWN)
    return AVERROR(EINVAL);
  DCHECK(format == PIXEL_FORMAT_I420 || format == PIXEL_FORMAT_I422 ||
         format == PIXEL_FORMAT_I444 || format == PIXEL_FORMAT_YUV420P9 ||
         format == PIXEL_FORMAT_YUV420P10 || format == PIXEL_FORMAT_YUV422P9 ||
         format == PIXEL_FORMAT_YUV422P10 || format == PIXEL_FORMAT_YUV444P9 ||
         format == PIXEL_FORMAT_YUV444P10 || format == PIXEL_FORMAT_YUV420P12 ||
         format == PIXEL_FORMAT_YUV422P12 || format == PIXEL_FORMAT_YUV444P12);

  // Do not trust `codec_context` sizes either.  Use whatever `frame` requests.
  gfx::Size coded_size(frame->width, frame->height);
  const int ret =
      av_image_check_size(coded_size.width(), coded_size.height(), 0, nullptr);
  if (ret < 0)
    return ret;

  VideoAspectRatio aspect_ratio = config_.aspect_ratio();
  if (!aspect_ratio.IsValid() && codec_context->sample_aspect_ratio.num > 0) {
    aspect_ratio =
        VideoAspectRatio::PAR(codec_context->sample_aspect_ratio.num,
                              codec_context->sample_aspect_ratio.den);
  }

  // When lowres is non-zero, dimensions should be divided by 2^(lowres), but
  // since we don't use this, just DCHECK that it's zero.
  DCHECK_EQ(codec_context->lowres, 0);

  if (force_allocation_error_)
    return AVERROR(EINVAL);

  // FFmpeg has specific requirements on the allocation size of the frame.  The
  // following logic replicates FFmpeg's allocation strategy to ensure buffers
  // are not overread / overwritten.  See ff_init_buffer_info() for details.
  auto layout =
      VideoFrame::CreateFullySpecifiedLayoutWithStrides(format, coded_size);
  if (!layout) {
    return AVERROR(EINVAL);
  }

  const size_t num_planes = layout->planes().size();
  size_t allocation_size = layout->buffer_addr_align();
  for (size_t plane = 0; plane < num_planes; plane++) {
    allocation_size += layout->planes()[plane].size;
  }

  // Round up the allocation, but keep `allocation_size` as the usable
  // allocation after aligning `data`.
  void* fb_priv = nullptr;
  uint8_t* data = frame_pool_->GetFrameBuffer(allocation_size, &fb_priv);
  if (!data) {
    return AVERROR(EINVAL);
  }

  data = base::bits::AlignUp(data, layout->buffer_addr_align());

  for (size_t plane = 0; plane < num_planes; ++plane) {
    frame->data[plane] = data + layout->planes()[plane].offset;
    frame->linesize[plane] = layout->planes()[plane].stride;
  }

  // This will be freed by `ReleaseVideoBufferImpl`.
  auto* opaque = new OpaqueData(fb_priv, frame_pool_, data, allocation_size,
                                std::move(*layout));

  frame->buf[0] = av_buffer_create(
      frame->data[0], VideoFrame::AllocationSize(format, coded_size),
      ReleaseVideoBufferImpl, opaque,
      /*flags=*/0);
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

  if (!frame_pool_) {
    // FFmpeg expects the initial allocation to be zero-initialized.  Failure to
    // do so can lead to uninitialized value usage.  See http://crbug.com/390941
    frame_pool_ =
        base::MakeRefCounted<FrameBufferPool>(/*clear_allocations=*/true);
  }

  InitCB bound_init_cb = base::BindPostTaskToCurrentDefault(std::move(init_cb));
  if (config.is_encrypted()) {
    std::move(bound_init_cb)
        .Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }

  if (!IsCodecSupported(config.codec()) ||
      !ConfigureDecoder(config, low_delay)) {
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

  if (frame_pool_) {
    frame_pool_->Shutdown();
  }
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
    packet->size = buffer.size();

    DCHECK(packet->data);
    DCHECK_GT(packet->size, 0);

    const int64_t timestamp = buffer.timestamp().InMicroseconds();
    const TimestampId timestamp_id = timestamp_id_generator_.GenerateNextId();
    timestamp_map_.Put(std::make_pair(timestamp_id, timestamp));
    packet->opaque = reinterpret_cast<void*>(timestamp_id.GetUnsafeValue());
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
  if (!frame->data[VideoFrame::Plane::kY] ||
      !frame->data[VideoFrame::Plane::kU] ||
      !frame->data[VideoFrame::Plane::kV]) {
    DLOG(ERROR) << "Video frame was produced yet has invalid frame data.";
    return false;
  }

  auto* opaque = static_cast<OpaqueData*>(av_buffer_get_opaque(frame->buf[0]));
  CHECK(!!opaque);

  // `frame->width,height` may be different from what they were when we
  // allocated the buffer.  Presumably `width` is always the same, but in
  // practice `height` can be smaller.  They are advertised as the coded size,
  // though, so that's how we use them here.  `crop*` take this difference into
  // account, and are meant to be applied to `width` and `height` as they are.
  const gfx::Size coded_size(frame->width, frame->height);
  const gfx::Rect visible_rect(frame->crop_left, frame->crop_top,
                               frame->width - frame->crop_right,
                               frame->height - frame->crop_bottom);

  // Why do we prefer the container aspect ratio here?
  VideoAspectRatio aspect_ratio = config_.aspect_ratio();
  if (!aspect_ratio.IsValid() && frame->sample_aspect_ratio.num > 0) {
    aspect_ratio = VideoAspectRatio::PAR(frame->sample_aspect_ratio.num,
                                         frame->sample_aspect_ratio.den);
  }
  gfx::Size natural_size = aspect_ratio.GetNaturalSize(visible_rect);

  const auto ts_id = TimestampId(reinterpret_cast<size_t>(frame->opaque));
  const auto ts_lookup = timestamp_map_.Get(ts_id);
  if (ts_lookup == timestamp_map_.end()) {
    return false;
  }
  const auto pts = base::Microseconds(std::get<1>(*ts_lookup));
  auto video_frame = VideoFrame::WrapExternalDataWithLayout(
      opaque->layout, visible_rect, natural_size, opaque->data, opaque->size,
      pts);
  if (!video_frame) {
    return false;
  }

  auto config_cs = config_.color_space_info().ToGfxColorSpace();

  gfx::ColorSpace color_space;
  if (codec_context_->codec_id == AV_CODEC_ID_H264 &&
      frame->colorspace == AVCOL_SPC_RGB &&
      VideoPixelFormatToChromaSampling(video_frame->format()) !=
          VideoChromaSampling::k444) {
    // Some H.264 videos contain a VUI that specifies a color matrix of GBR,
    // when they are actually ordinary YUV. Default to BT.709 if the format is
    // not 4:4:4 as GBR is reasonable for 4:4:4 content. See crbug.com/1067377
    // and crbug.com/341266991.
    color_space = gfx::ColorSpace::CreateREC709();
  } else if (frame->color_primaries != AVCOL_PRI_UNSPECIFIED ||
             frame->color_trc != AVCOL_TRC_UNSPECIFIED ||
             frame->colorspace != AVCOL_SPC_UNSPECIFIED) {
    color_space = VideoColorSpace(frame->color_primaries, frame->color_trc,
                                  frame->colorspace,
                                  frame->color_range != AVCOL_RANGE_MPEG
                                      ? gfx::ColorSpace::RangeID::FULL
                                      : gfx::ColorSpace::RangeID::LIMITED)
                      .ToGfxColorSpace();
  } else if (frame->color_range == AVCOL_RANGE_JPEG) {
    // None of primaries, transfer, or colorspace are specified at this point,
    // so guess BT.709 full range for historical reasons.
    color_space = gfx::ColorSpace::CreateJpeg();
  }

  // Prefer the frame color space over what's in the config.
  video_frame->set_color_space(color_space.IsValid() ? color_space : config_cs);

  video_frame->metadata().power_efficient = false;
  video_frame->AddDestructionObserver(
      frame_pool_->CreateFrameCallback(opaque->fb_priv));
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
  codec_context_->flags |= AV_CODEC_FLAG_COPY_OPAQUE;

  if (base::FeatureList::IsEnabled(kFFmpegAllowLists)) {
    // Note: FFmpeg will try to free this string, so we must duplicate it.
    codec_context_->codec_whitelist =
        av_strdup(FFmpegGlue::GetAllowedVideoDecoders());
  }

  if (decode_nalus_) {
    codec_context_->flags2 |= AV_CODEC_FLAG2_CHUNKS;
  }

  const AVCodec* codec = avcodec_find_decoder(codec_context_->codec_id);
  if (!codec || avcodec_open2(codec_context_.get(), codec, NULL) < 0) {
    ReleaseFFmpegResources();
    return false;
  }

  decoding_loop_ = std::make_unique<FFmpegDecodingLoop>(codec_context_.get());
  return true;
}

}  // namespace media
