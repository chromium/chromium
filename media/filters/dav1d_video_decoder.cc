// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/dav1d_video_decoder.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/limits.h"
#include "media/base/media_log.h"
#include "media/base/video_util.h"

extern "C" {
#include "third_party/dav1d/libdav1d/include/dav1d/dav1d.h"
}

namespace media {

static void GetDecoderThreadCounts(const int coded_height,
                                   int* tile_threads,
                                   int* frame_threads) {
  // Tile thread counts based on currently available content. Recommended by
  // YouTube, while frame thread values fit within limits::kMaxVideoThreads.
  if (coded_height >= 700) {
    *tile_threads =
        4;  // Current 720p content is encoded in 5 tiles and 1080p content with
            // 8 tiles, but we'll exceed limits::kMaxVideoThreads with 5+ tile
            // threads with 3 frame threads (5 * 3 + 3 = 18 threads vs 16 max).
            //
            // Since 720p playback isn't smooth without 3 frame threads, we've
            // chosen a slightly lower tile thread count.
    *frame_threads = 3;
  } else if (coded_height >= 300) {
    *tile_threads = 3;
    *frame_threads = 2;
  } else {
    *tile_threads = 2;
    *frame_threads = 2;
  }
}

static VideoPixelFormat Dav1dImgFmtToVideoPixelFormat(
    const Dav1dPictureParameters* pic) {
  switch (pic->layout) {
    case DAV1D_PIXEL_LAYOUT_I420:
      switch (pic->bpc) {
        case 8:
          return PIXEL_FORMAT_I420;
        case 10:
          return PIXEL_FORMAT_YUV420P10;
        case 12:
          return PIXEL_FORMAT_YUV420P12;
        default:
          DLOG(ERROR) << "Unsupported bit depth: " << pic->bpc;
          return PIXEL_FORMAT_UNKNOWN;
      }
    case DAV1D_PIXEL_LAYOUT_I422:
      switch (pic->bpc) {
        case 8:
          return PIXEL_FORMAT_I422;
        case 10:
          return PIXEL_FORMAT_YUV422P10;
        case 12:
          return PIXEL_FORMAT_YUV422P12;
        default:
          DLOG(ERROR) << "Unsupported bit depth: " << pic->bpc;
          return PIXEL_FORMAT_UNKNOWN;
      }
    case DAV1D_PIXEL_LAYOUT_I444:
      switch (pic->bpc) {
        case 8:
          return PIXEL_FORMAT_I444;
        case 10:
          return PIXEL_FORMAT_YUV444P10;
        case 12:
          return PIXEL_FORMAT_YUV444P12;
        default:
          DLOG(ERROR) << "Unsupported bit depth: " << pic->bpc;
          return PIXEL_FORMAT_UNKNOWN;
      }
    default:
      DLOG(ERROR) << "Unsupported pixel format: " << pic->layout;
      return PIXEL_FORMAT_UNKNOWN;
  }
}

static void ReleaseDecoderBuffer(const uint8_t* buffer, void* opaque) {
  if (opaque)
    static_cast<DecoderBuffer*>(opaque)->Release();
}

static void LogDav1dMessage(void* cookie, const char* format, va_list ap) {
  auto log = base::StringPrintV(format, ap);
  if (log.empty())
    return;

  if (log.back() == '\n')
    log.pop_back();

  DLOG(ERROR) << log;
}

// std::unique_ptr release helpers. We need to release both the containing
// structs as well as refs held within the structures.
struct ScopedDav1dDataFree {
  void operator()(void* x) const {
    auto* data = static_cast<Dav1dData*>(x);
    dav1d_data_unref(data);
    delete data;
  }
};

struct ScopedDav1dPictureFree {
  void operator()(void* x) const {
    auto* pic = static_cast<Dav1dPicture*>(x);
    dav1d_picture_unref(pic);
    delete pic;
  }
};

Dav1dVideoDecoder::Dav1dVideoDecoder(MediaLog* media_log,
                                     OffloadState offload_state)
    : media_log_(media_log),
      bind_callbacks_(offload_state == OffloadState::kNormal) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

Dav1dVideoDecoder::~Dav1dVideoDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CloseDecoder();
}

std::string Dav1dVideoDecoder::GetDisplayName() const {
  return "Dav1dVideoDecoder";
}

void Dav1dVideoDecoder::Initialize(const VideoDecoderConfig& config,
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
    std::move(bound_init_cb).Run(false);
    return;
  }

  // Clear any previously initialized decoder.
  CloseDecoder();

  Dav1dSettings s;
  dav1d_default_settings(&s);

  // Compute the ideal thread count values. We'll then clamp these based on the
  // maximum number of recommended threads (using number of processors, etc).
  //
  // dav1d will spawn |n_tile_threads| per frame thread.
  GetDecoderThreadCounts(config.coded_size().height(), &s.n_tile_threads,
                         &s.n_frame_threads);

  const int max_threads = VideoDecoder::GetRecommendedThreadCount(
      s.n_frame_threads * (s.n_tile_threads + 1));

  // First clamp tile threads to the allowed maximum. We prefer tile threads
  // over frame threads since dav1d folk indicate they are more efficient. In an
  // ideal world this would be auto-detected by dav1d from the content.
  //
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1536783#c0
  s.n_tile_threads = std::min(max_threads, s.n_tile_threads);

  // Now clamp frame threads based on the number of total threads that would be
  // created with the given |n_tile_threads| value. Note: A thread count of 1
  // generates no additional threads since the calling thread (this thread) is
  // counted as a thread.
  //
  // We only want 1 frame thread in low delay mode, since otherwise we'll
  // require at least two buffers before the first frame can be output.
  //
  // If a system has the cores for it, we'll end up using the following:
  // <300p: 2 tile threads, 2 frame threads = 2 * 2 + 2 = 6 total threads.
  // <700p: 3 tile threads, 2 frame threads = 3 * 2 + 2 = 8 total threads.
  //
  // For higher resolutions we hit limits::kMaxVideoThreads (16):
  // >700p: 4 tile threads, 3 frame threads = 4 * 3 + 3  = 15 total threads.
  //
  // Due to the (surprising) performance issues which occurred when setting
  // |n_frame_threads|=1 (https://crbug.com/957511) the minimum total number of
  // threads is 6 (two tile and two frame) regardless of core count. The maximum
  // is min(2 * base::SysInfo::NumberOfProcessors(), limits::kMaxVideoThreads).
  if (low_delay)
    s.n_frame_threads = 1;
  else if (s.n_frame_threads * (s.n_tile_threads + 1) > max_threads)
    s.n_frame_threads = std::max(2, max_threads / (s.n_tile_threads + 1));

  // Route dav1d internal logs through Chrome's DLOG system.
  s.logger = {nullptr, &LogDav1dMessage};

  // Set a maximum frame size limit to avoid OOM'ing fuzzers.
  s.frame_size_limit = limits::kMaxCanvas;

  if (dav1d_open(&dav1d_decoder_, &s) < 0) {
    std::move(bound_init_cb).Run(false);
    return;
  }

  config_ = config;
  state_ = DecoderState::kNormal;
  output_cb_ = output_cb;
  std::move(bound_init_cb).Run(true);
}

void Dav1dVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                               DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer);
  DCHECK(decode_cb);
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

void Dav1dVideoDecoder::Reset(base::OnceClosure reset_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = DecoderState::kNormal;
  dav1d_flush(dav1d_decoder_);

  if (bind_callbacks_)
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     std::move(reset_cb));
  else
    std::move(reset_cb).Run();
}

void Dav1dVideoDecoder::Detach() {
  // Even though we offload all resolutions of AV1, this may be called in a
  // transition from clear to encrypted content. Which will subsequently fail
  // Initialize() since encrypted content isn't supported by this decoder.

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!bind_callbacks_);

  CloseDecoder();
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void Dav1dVideoDecoder::CloseDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!dav1d_decoder_)
    return;
  dav1d_close(&dav1d_decoder_);
  DCHECK(!dav1d_decoder_);
}

bool Dav1dVideoDecoder::DecodeBuffer(scoped_refptr<DecoderBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  using ScopedPtrDav1dData = std::unique_ptr<Dav1dData, ScopedDav1dDataFree>;
  ScopedPtrDav1dData input_buffer;

  if (!buffer->end_of_stream()) {
    input_buffer.reset(new Dav1dData{0});
    if (dav1d_data_wrap(input_buffer.get(), buffer->data(), buffer->data_size(),
                        &ReleaseDecoderBuffer, buffer.get()) < 0) {
      return false;
    }
    input_buffer->m.timestamp = buffer->timestamp().InMicroseconds();
    buffer->AddRef();
  }

  // Used to DCHECK that dav1d_send_data() actually takes the packet. If we exit
  // this function without sending |input_buffer| that packet will be lost. We
  // have no packet to send at end of stream.
  bool send_data_completed = buffer->end_of_stream();

  while (!input_buffer || input_buffer->sz) {
    if (input_buffer) {
      const int res = dav1d_send_data(dav1d_decoder_, input_buffer.get());
      if (res < 0 && res != -EAGAIN) {
        MEDIA_LOG(ERROR, media_log_) << "dav1d_send_data() failed on "
                                     << buffer->AsHumanReadableString();
        return false;
      }

      if (res != -EAGAIN)
        send_data_completed = true;

      // Even if dav1d_send_data() returned EAGAIN, try dav1d_get_picture().
    }

    using ScopedPtrDav1dPicture =
        std::unique_ptr<Dav1dPicture, ScopedDav1dPictureFree>;
    ScopedPtrDav1dPicture p(new Dav1dPicture{0});

    const int res = dav1d_get_picture(dav1d_decoder_, p.get());
    if (res < 0) {
      if (res != -EAGAIN) {
        MEDIA_LOG(ERROR, media_log_) << "dav1d_get_picture() failed on "
                                     << buffer->AsHumanReadableString();
        return false;
      }

      // We've reached end of stream and no frames remain to drain.
      if (!input_buffer) {
        DCHECK(send_data_completed);
        return true;
      }

      continue;
    }

    auto frame = CopyImageToVideoFrame(p.get());
    if (!frame) {
      MEDIA_LOG(DEBUG, media_log_)
          << "Failed to produce video frame from Dav1dPicture.";
      return false;
    }

    // AV1 color space defines match ISO 23001-8:2016 via ISO/IEC 23091-4/ITU-T
    // H.273. https://aomediacodec.github.io/av1-spec/#color-config-semantics
    media::VideoColorSpace color_space(
        p->seq_hdr->pri, p->seq_hdr->trc, p->seq_hdr->mtrx,
        p->seq_hdr->color_range ? gfx::ColorSpace::RangeID::FULL
                                : gfx::ColorSpace::RangeID::LIMITED);

    // If the frame doesn't specify a color space, use the container's.
    if (!color_space.IsSpecified())
      color_space = config_.color_space_info();

    frame->set_color_space(color_space.ToGfxColorSpace());
    frame->metadata()->SetBoolean(VideoFrameMetadata::POWER_EFFICIENT, false);
    frame->AddDestructionObserver(base::BindOnce(
        base::DoNothing::Once<ScopedPtrDav1dPicture>(), std::move(p)));

    output_cb_.Run(std::move(frame));
  }

  DCHECK(send_data_completed);
  return true;
}

scoped_refptr<VideoFrame> Dav1dVideoDecoder::CopyImageToVideoFrame(
    const Dav1dPicture* pic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VideoPixelFormat pixel_format = Dav1dImgFmtToVideoPixelFormat(&pic->p);
  if (pixel_format == PIXEL_FORMAT_UNKNOWN)
    return nullptr;

  // Since we're making a copy, only copy the visible area.
  const gfx::Size visible_size(pic->p.w, pic->p.h);
  return VideoFrame::WrapExternalYuvData(
      pixel_format, visible_size, gfx::Rect(visible_size),
      config_.natural_size(), pic->stride[0], pic->stride[1], pic->stride[1],
      static_cast<uint8_t*>(pic->data[0]), static_cast<uint8_t*>(pic->data[1]),
      static_cast<uint8_t*>(pic->data[2]),
      base::TimeDelta::FromMicroseconds(pic->m.timestamp));
}

}  // namespace media
