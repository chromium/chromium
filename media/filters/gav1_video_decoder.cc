// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/gav1_video_decoder.h"

#include <stdint.h>
#include <numeric>

#include "base/bits.h"
#include "base/callback_helpers.h"
#include "base/numerics/safe_conversions.h"
#include "base/system/sys_info.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/limits.h"
#include "media/base/media_log.h"
#include "media/base/video_frame.h"
#include "third_party/libgav1/src/src/gav1/decoder.h"
#include "third_party/libgav1/src/src/gav1/decoder_settings.h"
#include "third_party/libgav1/src/src/gav1/frame_buffer.h"

namespace media {

namespace {

VideoPixelFormat Libgav1ImageFormatToVideoPixelFormat(
    const libgav1::ImageFormat libgav1_format,
    int bitdepth) {
  switch (libgav1_format) {
    // Single plane monochrome images will be converted to standard 3 plane ones
    // since Chromium doesn't support single Y plane images.
    case libgav1::kImageFormatMonochrome400:
    case libgav1::kImageFormatYuv420:
      switch (bitdepth) {
        case 8:
          return PIXEL_FORMAT_I420;
        case 10:
          return PIXEL_FORMAT_YUV420P10;
        case 12:
          return PIXEL_FORMAT_YUV420P12;
        default:
          DLOG(ERROR) << "Unsupported bit depth: " << bitdepth;
          return PIXEL_FORMAT_UNKNOWN;
      }
    case libgav1::kImageFormatYuv422:
      switch (bitdepth) {
        case 8:
          return PIXEL_FORMAT_I422;
        case 10:
          return PIXEL_FORMAT_YUV422P10;
        case 12:
          return PIXEL_FORMAT_YUV422P12;
        default:
          DLOG(ERROR) << "Unsupported bit depth: " << bitdepth;
          return PIXEL_FORMAT_UNKNOWN;
      }
    case libgav1::kImageFormatYuv444:
      switch (bitdepth) {
        case 8:
          return PIXEL_FORMAT_I444;
        case 10:
          return PIXEL_FORMAT_YUV444P10;
        case 12:
          return PIXEL_FORMAT_YUV444P12;
        default:
          DLOG(ERROR) << "Unsupported bit depth: " << bitdepth;
          return PIXEL_FORMAT_UNKNOWN;
      }
  }
}

int GetDecoderThreadCounts(int coded_height) {
  // Thread counts based on currently available content. We set the number of
  // threads to be equal to the general number of tiles for the given
  // resolution. As of now, YouTube content has the following tile settings:
  //   240p and below - 1 tile
  //   360p and 480p - 2 tiles
  //   720p - 4 tiles
  //   1080p - 8 tiles
  // libgav1 supports frame parallel decoding, but we do not use it (yet) since
  // the performance for this thread configuration is good enough without it.
  // Also, the memory usage is much lower in non-frame parallel mode. This can
  // be revisited as performance numbers change/evolve.
  static const int num_cores = base::SysInfo::NumberOfProcessors();
  auto threads_by_height = [](int coded_height) {
    if (coded_height >= 1000)
      return 8;
    if (coded_height >= 700)
      return 4;
    if (coded_height >= 300)
      return 2;
    return 1;
  };

  return std::min(threads_by_height(coded_height), num_cores);
}

libgav1::StatusCode GetFrameBufferImpl(void* callback_private_data,
                                       int bitdepth,
                                       libgav1::ImageFormat image_format,
                                       int width,
                                       int height,
                                       int left_border,
                                       int right_border,
                                       int top_border,
                                       int bottom_border,
                                       int stride_alignment,
                                       Libgav1FrameBuffer* frame_buffer) {
  DCHECK(callback_private_data);
  DCHECK(frame_buffer);
  DCHECK(base::bits::IsPowerOfTwo(stride_alignment));
  // VideoFramePool creates frames with a fixed alignment of
  // VideoFrame::kFrameAddressAlignment. If libgav1 requests a larger
  // alignment, it cannot be supported.
  CHECK_LE(stride_alignment, VideoFrame::kFrameAddressAlignment);

  const VideoPixelFormat format =
      Libgav1ImageFormatToVideoPixelFormat(image_format, bitdepth);
  if (format == PIXEL_FORMAT_UNKNOWN)
    return libgav1::kStatusUnimplemented;

  // VideoFramePool aligns video_frame->data(i), but libgav1 needs
  // video_frame->visible_data(i) to be aligned. To accomplish that, pad
  // left_border to be a multiple of stride_alignment.
  //
  // Here is an example:
  // width=6, height=4, left/right/top/bottom_border=2, stride_alignment=16
  //
  //     X*|TTTTTT|**pppppp
  //     **|TTTTTT|**pppppp
  //     --+------+--------
  //     LL|YFFFFF|RRpppppp
  //     LL|FFFFFF|RRpppppp
  //     LL|FFFFFF|RRpppppp
  //     LL|FFFFFF|RRpppppp
  //     --+------+--------
  //     **|BBBBBB|**pppppp
  //     **|BBBBBB|**pppppp
  //
  // F indicates the frame proper. L, R, T, B indicate the
  // left/right/top/bottom borders. Lowercase p indicates the padding at the
  // end of a row. The asterisk * indicates the borders at the four corners.
  //
  // Libgav1 requires that the callback align the first byte of the frame
  // proper, indicated by Y. VideoFramePool aligns the first byte of the
  // buffer, indicated by X. To make sure the byte indicated by Y is also
  // aligned, we need to pad left_border to be a multiple of stride_alignment.
  left_border = base::bits::Align(left_border, stride_alignment);
  gfx::Size coded_size(left_border + width + right_border,
                       top_border + height + bottom_border);
  gfx::Rect visible_rect(left_border, top_border, width, height);

  auto* decoder = static_cast<Gav1VideoDecoder*>(callback_private_data);
  auto video_frame =
      decoder->CreateVideoFrame(format, coded_size, visible_rect);
  if (!video_frame)
    return libgav1::kStatusInvalidArgument;

  for (int i = 0; i < 3; i++) {
    // frame_buffer->plane[i] points to the first byte of the frame proper,
    // not the first byte of the buffer.
    frame_buffer->plane[i] = video_frame->visible_data(i);
    frame_buffer->stride[i] = video_frame->stride(i);
  }
  if (image_format == libgav1::kImageFormatMonochrome400) {
    int uv_height = (height + 1) >> 1;
    const size_t size_needed = video_frame->stride(1) * uv_height;
    for (int i = 1; i < 3; i++) {
      frame_buffer->plane[i] = nullptr;
      frame_buffer->stride[i] = 0;
      // An AV1 monochrome (grayscale) frame has no U and V planes. Set all U
      // and V samples in video_frame to the blank value.
      if (bitdepth == 8) {
        constexpr uint8_t kBlankUV = 256 / 2;
        memset(video_frame->visible_data(i), kBlankUV, size_needed);
      } else {
        const uint16_t kBlankUV = (1 << bitdepth) / 2;
        uint16_t* data =
            reinterpret_cast<uint16_t*>(video_frame->visible_data(i));
        std::fill(data, data + size_needed / 2, kBlankUV);
      }
    }
  }
  frame_buffer->private_data = video_frame.get();
  video_frame->AddRef();

  return libgav1::kStatusOk;
}

void ReleaseFrameBufferImpl(void* callback_private_data,
                            void* buffer_private_data) {
  DCHECK(callback_private_data);
  DCHECK(buffer_private_data);
  static_cast<VideoFrame*>(buffer_private_data)->Release();
}

void ReleaseInputBufferImpl(void* callback_private_data,
                            void* buffer_private_data) {
  DCHECK(callback_private_data);
  DCHECK(buffer_private_data);
  static_cast<DecoderBuffer*>(buffer_private_data)->Release();
}

scoped_refptr<VideoFrame> FormatVideoFrame(
    const libgav1::DecoderBuffer& buffer,
    const VideoColorSpace& container_color_space) {
  scoped_refptr<VideoFrame> frame =
      static_cast<VideoFrame*>(buffer.buffer_private_data);
  frame->set_timestamp(
      base::TimeDelta::FromMicroseconds(buffer.user_private_data));

  // AV1 color space defines match ISO 23001-8:2016 via ISO/IEC 23091-4/ITU-T
  // H.273. https://aomediacodec.github.io/av1-spec/#color-config-semantics
  media::VideoColorSpace color_space(
      buffer.color_primary, buffer.transfer_characteristics,
      buffer.matrix_coefficients,
      buffer.color_range == libgav1::kColorRangeStudio
          ? gfx::ColorSpace::RangeID::LIMITED
          : gfx::ColorSpace::RangeID::FULL);

  // If the frame doesn't specify a color space, use the container's.
  if (!color_space.IsSpecified())
    color_space = container_color_space;

  frame->set_color_space(color_space.ToGfxColorSpace());
  frame->metadata().power_efficient = false;

  return frame;
}

}  // namespace

// static
SupportedVideoDecoderConfigs Gav1VideoDecoder::SupportedConfigs() {
  return {{/*profile_min=*/AV1PROFILE_PROFILE_MAIN,
           /*profile_max=*/AV1PROFILE_PROFILE_HIGH,
           /*coded_size_min=*/kDefaultSwDecodeSizeMin,
           /*coded_size_max=*/kDefaultSwDecodeSizeMax,
           /*allow_encrypted=*/false,
           /*require_encrypted=*/false}};
}

Gav1VideoDecoder::Gav1VideoDecoder(MediaLog* media_log,
                                   OffloadState offload_state)
    : media_log_(media_log),
      bind_callbacks_(offload_state == OffloadState::kNormal) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

Gav1VideoDecoder::~Gav1VideoDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CloseDecoder();
}

std::string Gav1VideoDecoder::GetDisplayName() const {
  return "Gav1VideoDecoder";
}

VideoDecoderType Gav1VideoDecoder::GetDecoderType() const {
  return VideoDecoderType::kGav1;
}

void Gav1VideoDecoder::Initialize(const VideoDecoderConfig& config,
                                  bool low_delay,
                                  CdmContext* /* cdm_context */,
                                  InitCB init_cb,
                                  const OutputCB& output_cb,
                                  const WaitingCB& /* waiting_cb */) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(config.IsValidConfig());

  InitCB bound_init_cb = bind_callbacks_ ? BindToCurrentLoop(std::move(init_cb))
                                         : std::move(init_cb);
  if (config.is_encrypted() || config.codec() != kCodecAV1) {
    std::move(bound_init_cb).Run(StatusCode::kEncryptedContentUnsupported);
    return;
  }

  // Clear any previously initialized decoder.
  CloseDecoder();

  libgav1::DecoderSettings settings;
  settings.threads = VideoDecoder::GetRecommendedThreadCount(
      GetDecoderThreadCounts(config.coded_size().height()));
  settings.get_frame_buffer = GetFrameBufferImpl;
  settings.release_frame_buffer = ReleaseFrameBufferImpl;
  settings.release_input_buffer = ReleaseInputBufferImpl;
  settings.callback_private_data = this;

  libgav1_decoder_ = std::make_unique<libgav1::Decoder>();
  libgav1::StatusCode status = libgav1_decoder_->Init(&settings);
  if (status != kLibgav1StatusOk) {
    MEDIA_LOG(ERROR, media_log_) << "libgav1::Decoder::Init() failed, "
                                 << "status=" << status;
    std::move(bound_init_cb).Run(StatusCode::kDecoderFailedInitialization);
    return;
  }

  output_cb_ = output_cb;
  state_ = DecoderState::kDecoding;
  color_space_ = config.color_space_info();
  natural_size_ = config.natural_size();
  std::move(bound_init_cb).Run(OkStatus());
}

void Gav1VideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                              DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer);
  DCHECK(decode_cb);
  DCHECK(libgav1_decoder_);
  DCHECK_NE(state_, DecoderState::kUninitialized)
      << "Called Decode() before successful Initialize()";

  DecodeCB bound_decode_cb = bind_callbacks_
                                 ? BindToCurrentLoop(std::move(decode_cb))
                                 : std::move(decode_cb);

  if (state_ == DecoderState::kError) {
    std::move(bound_decode_cb).Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  if (!DecodeBuffer(std::move(buffer))) {
    state_ = DecoderState::kError;
    std::move(bound_decode_cb).Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  // VideoDecoderShim expects |decode_cb| call after |output_cb_|.
  std::move(bound_decode_cb).Run(DecodeStatus::OK);
}

bool Gav1VideoDecoder::DecodeBuffer(scoped_refptr<DecoderBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const bool is_end_of_stream = buffer->end_of_stream();

  // Used to ensure that EnqueueFrame() actually takes the packet. If we exit
  // this function without enqueuing |buffer|, that packet will be lost. We do
  // not have anything to enqueue at the end of stream.
  bool enqueued = is_end_of_stream;

  while (is_end_of_stream || !enqueued) {
    // Try to enqueue the packet if it has not been enqueued already.
    if (!enqueued) {
      libgav1::StatusCode status = libgav1_decoder_->EnqueueFrame(
          buffer->data(), buffer->data_size(),
          /* user_private_data = */ buffer->timestamp().InMicroseconds(),
          /* buffer_private_data = */ buffer.get());
      if (status == kLibgav1StatusOk) {
        buffer->AddRef();
        enqueued = true;
      } else if (status != kLibgav1StatusTryAgain) {
        MEDIA_LOG(ERROR, media_log_)
            << "libgav1::Decoder::EnqueueFrame failed, status=" << status
            << " on " << buffer->AsHumanReadableString();
        return false;
      }
    }

    // Try to dequeue a decoded frame.
    const libgav1::DecoderBuffer* decoded_buffer;
    libgav1::StatusCode status =
        libgav1_decoder_->DequeueFrame(&decoded_buffer);
    if (status != kLibgav1StatusOk) {
      if (status != kLibgav1StatusTryAgain &&
          status != kLibgav1StatusNothingToDequeue) {
        MEDIA_LOG(ERROR, media_log_)
            << "libgav1::Decoder::DequeueFrame failed, status=" << status;
        return false;
      }

      // We've reached end of stream and no frames are remaining to be dequeued.
      if (is_end_of_stream && status == kLibgav1StatusNothingToDequeue) {
        return true;
      }

      continue;
    }

    if (!decoded_buffer) {
      // The packet did not have any displayable frames. Not an error.
      continue;
    }

    scoped_refptr<VideoFrame> frame =
        FormatVideoFrame(*decoded_buffer, color_space_);
    if (!frame) {
      MEDIA_LOG(ERROR, media_log_) << "Failed formatting VideoFrame from "
                                   << "libgav1::DecoderBuffer";
      return false;
    }

    output_cb_.Run(std::move(frame));
  }

  DCHECK(enqueued);
  return true;
}

void Gav1VideoDecoder::Reset(base::OnceClosure reset_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = DecoderState::kDecoding;

  libgav1::StatusCode status = libgav1_decoder_->SignalEOS();
  if (status != kLibgav1StatusOk) {
    MEDIA_LOG(WARNING, media_log_) << "libgav1::Decoder::SignalEOS() failed, "
                                   << "status=" << status;
  }

  if (bind_callbacks_) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     std::move(reset_cb));
  } else {
    std::move(reset_cb).Run();
  }
}

void Gav1VideoDecoder::Detach() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!bind_callbacks_);

  CloseDecoder();

  DETACH_FROM_SEQUENCE(sequence_checker_);
}

scoped_refptr<VideoFrame> Gav1VideoDecoder::CreateVideoFrame(
    VideoPixelFormat format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect) {
  // The comment for VideoFramePool::CreateFrame() says:
  //   The buffer for the new frame will be zero initialized.  Reused frames
  //   will not be zero initialized.
  // The zero initialization is necessary for FFmpeg but not for libgav1.
  return frame_pool_.CreateFrame(format, coded_size, visible_rect,
                                 natural_size_, kNoTimestamp);
}

void Gav1VideoDecoder::CloseDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  libgav1_decoder_.reset();
  state_ = DecoderState::kUninitialized;
}

}  // namespace media
