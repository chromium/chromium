// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/filters/dav1d_video_decoder.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bits.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/decoder_buffer.h"
#include "media/base/limits.h"
#include "media/base/media_log.h"
#include "media/base/video_aspect_ratio.h"
#include "media/base/video_util.h"

extern "C" {
#include "third_party/dav1d/libdav1d/include/dav1d/dav1d.h"
}

namespace media {

static void GetDecoderThreadCounts(const int coded_height,
                                   int* tile_threads,
                                   int* frame_threads) {
  // Tile thread counts based on currently available content. Recommended by
  // YouTube, while frame thread values fit within
  // limits::kMaxVideoDecodeThreads.
  if (coded_height >= 700) {
    *tile_threads =
        4;  // Current 720p content is encoded in 5 tiles and 1080p content with
            // 8 tiles, but we'll exceed limits::kMaxVideoDecodeThreads with 5+
            // tile threads with 3 frame threads (5 * 3 + 3 = 18 threads vs 16
            // max).
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
    // Single plane monochrome images will be converted to standard 3 plane ones
    // since Chromium doesn't support single Y plane images.
    case DAV1D_PIXEL_LAYOUT_I400:
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

// static
SupportedVideoDecoderConfigs Dav1dVideoDecoder::SupportedConfigs() {
  return {{/*profile_min=*/AV1PROFILE_PROFILE_MAIN,
           /*profile_max=*/AV1PROFILE_PROFILE_HIGH,
           /*coded_size_min=*/kDefaultSwDecodeSizeMin,
           /*coded_size_max=*/kDefaultSwDecodeSizeMax,
           /*allow_encrypted=*/false,
           /*require_encrypted=*/false}};
}

Dav1dVideoDecoder::Dav1dVideoDecoder(std::unique_ptr<MediaLog> media_log,
                                     OffloadState offload_state)
    : media_log_(std::move(media_log)),
      bind_callbacks_(offload_state == OffloadState::kNormal) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

Dav1dVideoDecoder::~Dav1dVideoDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CloseDecoder();
}

VideoDecoderType Dav1dVideoDecoder::GetDecoderType() const {
  return VideoDecoderType::kDav1d;
}

void Dav1dVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                   bool low_delay,
                                   CdmContext* /* cdm_context */,
                                   InitCB init_cb,
                                   const OutputCB& output_cb,
                                   const WaitingCB& /* waiting_cb */) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(config.IsValidConfig());

  InitCB bound_init_cb =
      bind_callbacks_ ? base::BindPostTaskToCurrentDefault(std::move(init_cb))
                      : std::move(init_cb);
  if (config.is_encrypted()) {
    std::move(bound_init_cb)
        .Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }

  if (config.codec() != VideoCodec::kAV1) {
    std::move(bound_init_cb)
        .Run(DecoderStatus(DecoderStatus::Codes::kUnsupportedCodec)
                 .WithData("codec", config.codec()));
    return;
  }

  // Clear any previously initialized decoder.
  CloseDecoder();

  Dav1dSettings s;
  dav1d_default_settings(&s);

  // Compute the ideal thread count values. We'll then clamp these based on the
  // maximum number of recommended threads (using number of processors, etc).
  int tile_threads, frame_threads;
  GetDecoderThreadCounts(config.coded_size().height(), &tile_threads,
                         &frame_threads);

  // While dav1d has switched to a thread pool, preserve the same thread counts
  // we used when tile and frame threads were configured distinctly. It may be
  // possible to lower this after some performance analysis of the new system.
  s.n_threads = VideoDecoder::GetRecommendedThreadCount(frame_threads *
                                                        (tile_threads + 1));

  // We only want 1 frame thread in low delay mode, since otherwise we'll
  // require at least two buffers before the first frame can be output.
  if (low_delay) {
    s.max_frame_delay = 1;
  }

  // Only output the highest spatial layer.
  s.all_layers = 0;

  // Route dav1d internal logs through Chrome's DLOG system.
  s.logger = {nullptr, &LogDav1dMessage};

  // Set a maximum frame size limit to avoid OOM'ing fuzzers.
  s.frame_size_limit = limits::kMaxCanvas;

  {
    Dav1dContext* decoder = nullptr;
    const int res = dav1d_open(&decoder, &s);
    if (res < 0) {
      std::move(bound_init_cb)
          .Run(DecoderStatus(DecoderStatus::Codes::kFailedToCreateDecoder,
                             "dav1d_open() failed")
                   .WithData("error_code", res));
      return;
    }
    dav1d_decoder_.reset(decoder);
  }

  config_ = config;
  state_ = DecoderState::kNormal;
  output_cb_ = output_cb;
  std::move(bound_init_cb).Run(DecoderStatus::Codes::kOk);
}

void Dav1dVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                               DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer);
  DCHECK(decode_cb);
  DCHECK_NE(state_, DecoderState::kUninitialized)
      << "Called Decode() before successful Initialize()";

  DecodeCB bound_decode_cb =
      bind_callbacks_ ? base::BindPostTaskToCurrentDefault(std::move(decode_cb))
                      : std::move(decode_cb);

  if (state_ == DecoderState::kError) {
    std::move(bound_decode_cb).Run(DecoderStatus::Codes::kFailed);
    return;
  }

  if (!DecodeBuffer(std::move(buffer))) {
    state_ = DecoderState::kError;
    std::move(bound_decode_cb).Run(DecoderStatus::Codes::kFailed);
    return;
  }

  // VideoDecoderShim expects |decode_cb| call after |output_cb_|.
  std::move(bound_decode_cb).Run(DecoderStatus::Codes::kOk);
}

void Dav1dVideoDecoder::Reset(base::OnceClosure reset_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = DecoderState::kNormal;
  dav1d_flush(dav1d_decoder_.get());

  if (bind_callbacks_)
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(reset_cb));
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

void Dav1dVideoDecoder::Dav1dContextDeleter::operator()(Dav1dContext* ptr) {
  dav1d_close(&ptr);
}

void Dav1dVideoDecoder::CloseDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  dav1d_decoder_.reset();
}

bool Dav1dVideoDecoder::DecodeBuffer(scoped_refptr<DecoderBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  using ScopedPtrDav1dData = std::unique_ptr<Dav1dData, ScopedDav1dDataFree>;
  ScopedPtrDav1dData input_buffer;

  if (!buffer->end_of_stream()) {
    input_buffer.reset(new Dav1dData{});
    const int res =
        dav1d_data_wrap(input_buffer.get(), buffer->data(), buffer->size(),
                        &ReleaseDecoderBuffer, buffer.get());
    if (res < 0) {
      MEDIA_LOG(ERROR, media_log_)
          << "dav1d_data_wrap() failed with error " << res;
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
      const int res = dav1d_send_data(dav1d_decoder_.get(), input_buffer.get());
      if (res < 0 && res != -EAGAIN) {
        MEDIA_LOG(ERROR, media_log_)
            << "dav1d_send_data() failed with error " << res << " on "
            << buffer->AsHumanReadableString();
        return false;
      }

      if (res != -EAGAIN)
        send_data_completed = true;

      // Even if dav1d_send_data() returned EAGAIN, try dav1d_get_picture().
    }

    using ScopedPtrDav1dPicture =
        std::unique_ptr<Dav1dPicture, ScopedDav1dPictureFree>;
    ScopedPtrDav1dPicture p(new Dav1dPicture{});

    const int res = dav1d_get_picture(dav1d_decoder_.get(), p.get());
    if (res < 0) {
      if (res != -EAGAIN) {
        MEDIA_LOG(ERROR, media_log_)
            << "dav1d_get_picture() failed with error " << res << " on "
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

    auto frame = BindImageToVideoFrame(p.get());
    if (!frame) {
      MEDIA_LOG(DEBUG, media_log_)
          << "Failed to produce video frame from Dav1dPicture.";
      return false;
    }

    // AV1 color space defines match ISO 23001-8:2016 via ISO/IEC 23091-4/ITU-T
    // H.273. https://aomediacodec.github.io/av1-spec/#color-config-semantics
    VideoColorSpace color_space(
        p->seq_hdr->pri, p->seq_hdr->trc, p->seq_hdr->mtrx,
        p->seq_hdr->color_range ? gfx::ColorSpace::RangeID::FULL
                                : gfx::ColorSpace::RangeID::LIMITED);

    // If the frame doesn't specify a color space, use the container's.
    auto gfx_cs = color_space.ToGfxColorSpace();
    if (!gfx_cs.IsValid()) {
      gfx_cs = config_.color_space_info().ToGfxColorSpace();
    }

    frame->set_color_space(gfx_cs);
    frame->metadata().power_efficient = false;
    frame->set_hdr_metadata(config_.hdr_metadata());

    // When we use bind mode, our image data is dependent on the Dav1dPicture,
    // so we must ensure it stays alive along enough.
    frame->AddDestructionObserver(base::DoNothingWithBoundArgs(std::move(p)));
    output_cb_.Run(std::move(frame));
  }

  DCHECK(send_data_completed);
  return true;
}

scoped_refptr<VideoFrame> Dav1dVideoDecoder::BindImageToVideoFrame(
    const Dav1dPicture* pic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const gfx::Size visible_size(pic->p.w, pic->p.h);

  VideoPixelFormat pixel_format = Dav1dImgFmtToVideoPixelFormat(&pic->p);
  if (pixel_format == PIXEL_FORMAT_UNKNOWN)
    return nullptr;

  auto uv_plane_stride = pic->stride[1];
  auto* u_plane = static_cast<uint8_t*>(pic->data[1]);
  auto* v_plane = static_cast<uint8_t*>(pic->data[2]);

  const bool needs_fake_uv_planes = pic->p.layout == DAV1D_PIXEL_LAYOUT_I400;
  if (needs_fake_uv_planes) {
    // UV planes are half the size of the Y plane.
    uv_plane_stride =
        base::bits::AlignUpDeprecatedDoNotUse(pic->stride[0] / 2, ptrdiff_t{2});
    const auto uv_plane_height = (pic->p.h + 1) / 2;
    const size_t size_needed = uv_plane_stride * uv_plane_height;

    if (!fake_uv_data_ || fake_uv_data_->size() != size_needed) {
      if (pic->p.bpc == 8) {
        // Avoid having base::RefCountedBytes zero initialize the memory just to
        // fill it with a different value.
        constexpr uint8_t kBlankUV = 256 / 2;
        std::vector<unsigned char> empty_data(size_needed, kBlankUV);

        // When we resize, existing frames will keep their refs on the old data.
        fake_uv_data_ = base::RefCountedBytes::TakeVector(&empty_data);
      } else {
        DCHECK(pic->p.bpc == 10 || pic->p.bpc == 12);
        const uint16_t kBlankUV = (1 << pic->p.bpc) / 2;
        fake_uv_data_ =
            base::MakeRefCounted<base::RefCountedBytes>(size_needed);

        uint16_t* data =
            reinterpret_cast<uint16_t*>(fake_uv_data_->as_vector().data());
        std::fill(data, data + size_needed / 2, kBlankUV);
      }
    }

    u_plane = v_plane =
        reinterpret_cast<uint8_t*>(fake_uv_data_->as_vector().data());
  }

  auto frame = VideoFrame::WrapExternalYuvData(
      pixel_format, visible_size, gfx::Rect(visible_size),
      config_.aspect_ratio().GetNaturalSize(gfx::Rect(visible_size)),
      pic->stride[0], uv_plane_stride, uv_plane_stride,
      static_cast<uint8_t*>(pic->data[0]), u_plane, v_plane,
      base::Microseconds(pic->m.timestamp));
  if (!frame)
    return nullptr;

  // Each frame needs a ref on the fake UV data to keep it alive until done.
  if (needs_fake_uv_planes) {
    frame->AddDestructionObserver(base::BindOnce(
        [](scoped_refptr<base::RefCountedBytes>) {}, fake_uv_data_));
  }

  return frame;
}

}  // namespace media
