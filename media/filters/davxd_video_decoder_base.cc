// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/davxd_video_decoder_base.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bits.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/dcheck_is_on.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_log.h"
#include "media/base/video_aspect_ratio.h"
#include "media/base/video_frame.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

namespace {

void GetDecoderThreadCounts(const int coded_height,
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

VideoPixelFormat ToVideoPixelFormat(VideoChromaSampling chroma_sampling,
                                    int bit_depth) {
  switch (chroma_sampling) {
    // Single plane monochrome images will be converted to standard 3 plane ones
    // since Chromium doesn't support single Y plane images.
    case VideoChromaSampling::k400:
    case VideoChromaSampling::k420:
      switch (bit_depth) {
        case 8:
          return PIXEL_FORMAT_I420;
        case 10:
          return PIXEL_FORMAT_YUV420P10;
        case 12:
          return PIXEL_FORMAT_YUV420P12;
        default:
          DLOG(ERROR) << "Unsupported bit depth: " << bit_depth;
          return PIXEL_FORMAT_UNKNOWN;
      }
    case VideoChromaSampling::k422:
      switch (bit_depth) {
        case 8:
          return PIXEL_FORMAT_I422;
        case 10:
          return PIXEL_FORMAT_YUV422P10;
        case 12:
          return PIXEL_FORMAT_YUV422P12;
        default:
          DLOG(ERROR) << "Unsupported bit depth: " << bit_depth;
          return PIXEL_FORMAT_UNKNOWN;
      }
    case VideoChromaSampling::k444:
      switch (bit_depth) {
        case 8:
          return PIXEL_FORMAT_I444;
        case 10:
          return PIXEL_FORMAT_YUV444P10;
        case 12:
          return PIXEL_FORMAT_YUV444P12;
        default:
          DLOG(ERROR) << "Unsupported bit depth: " << bit_depth;
          return PIXEL_FORMAT_UNKNOWN;
      }
    case VideoChromaSampling::kUnknown:
      return PIXEL_FORMAT_UNKNOWN;
  }
}

// Dynamically allocated picture opaque data.
struct FrameBufferData {
  FrameBufferData(void* fb,
                  scoped_refptr<FrameBufferPool> pool,
                  VideoChromaSampling sampling,
                  size_t y_stride,
                  size_t uv_stride,
                  base::span<const uint8_t> y,
                  base::span<const uint8_t> u,
                  base::span<const uint8_t> v)
      : fb_priv(fb),
        frame_pool(std::move(pool)),
        chroma_sampling(sampling),
        y_stride(y_stride),
        uv_stride(uv_stride),
        y_span(y),
        u_span(u),
        v_span(v) {
    CHECK(fb_priv);
    CHECK(frame_pool);
  }

  FrameBufferData(const FrameBufferData&) = delete;
  FrameBufferData& operator=(const FrameBufferData&) = delete;

  ~FrameBufferData() {
    y_span = {};
    u_span = {};
    v_span = {};
    frame_pool->ReleaseFrameBuffer(fb_priv);
  }

  // FrameBufferPool key that we'll free when the picture is unused.
  const raw_ptr<void> fb_priv;

  // Pool which owns `fb_priv`.
  const scoped_refptr<FrameBufferPool> frame_pool;

  // Chroma subsampling, strides, and plane spans sliced from the
  // FrameBufferPool buffer at allocation time.
  const VideoChromaSampling chroma_sampling;
  const size_t y_stride;
  const size_t uv_stride;
  base::raw_span<const uint8_t> y_span;
  base::raw_span<const uint8_t> u_span;
  base::raw_span<const uint8_t> v_span;
};

// Helper structure for storing generated 10-16bit UV planes for I400 content.
class RefCountedUV16Data : public base::RefCountedMemory {
 public:
  RefCountedUV16Data(size_t count, uint16_t fill_value)
      : uv_data_(count, fill_value) {}

 private:
  ~RefCountedUV16Data() override = default;

  base::span<const uint8_t> AsSpan() const LIFETIME_BOUND override {
    return base::as_byte_span(uv_data_);
  }

  const std::vector<uint16_t> uv_data_;
};

}  // namespace

// static
void DavXdVideoDecoderBase::ReleaseDecoderBuffer(const uint8_t* buffer,
                                                 void* opaque) {
  if (opaque) {
    static_cast<DecoderBuffer*>(opaque)->Release();
  }
}

// static
void DavXdVideoDecoderBase::LogDecoderMessage(void* cookie,
                                              const char* format,
                                              va_list ap) {
#if DCHECK_IS_ON()
  if (!DLOG_IS_ON(ERROR)) {
    return;
  }

  // SAFETY: `LogDecoderMessage` is only registered as the C logger callback
  // for libdav1d / libdav2d, which pass a null-terminated printf format string
  // literal matching `ap`.
  std::string log = UNSAFE_BUFFERS(base::StringPrintV(format, ap));
  if (log.empty()) {
    return;
  }

  if (log.back() == '\n') {
    log.pop_back();
  }

  DLOG(ERROR) << log;
#endif
}

// static
int DavXdVideoDecoderBase::AllocatePictureBuffer(
    int width,
    int height,
    int bit_depth,
    VideoChromaSampling chroma_sampling,
    size_t alignment,
    void* frame_pool_opaque,
    base::span<ptrdiff_t, 2> strides,
    base::span<void*, 3> planes,
    void** allocator_data) {
  if (chroma_sampling == VideoChromaSampling::kUnknown || width <= 0 ||
      height <= 0) {
    return -1;
  }

  // Copy of dav1d_default_picture_alloc() dav1d 1.5.1, adapted to use
  // FrameBufferPool.
  const int hbd = bit_depth > 8;
  const int aligned_w = (width + 127) & ~127;
  const int aligned_h = (height + 127) & ~127;
  const bool has_chroma = chroma_sampling != VideoChromaSampling::k400;
  const int ss_ver = chroma_sampling == VideoChromaSampling::k420;
  const int ss_hor = chroma_sampling != VideoChromaSampling::k444;
  ptrdiff_t y_stride = aligned_w << hbd;
  ptrdiff_t uv_stride = has_chroma ? y_stride >> ss_hor : 0;
  /* Due to how mapping of addresses to sets works in most L1 and L2 cache
   * implementations, strides of multiples of certain power-of-two numbers
   * may cause multiple rows of the same superblock to map to the same set,
   * causing evictions of previous rows resulting in a reduction in cache
   * hit rate. Avoid that by slightly padding the stride when necessary. */
  if (!(y_stride & 1023)) {
    y_stride += alignment;
  }
  if (!(uv_stride & 1023) && has_chroma) {
    uv_stride += alignment;
  }
  strides[0] = y_stride;
  strides[1] = uv_stride;
  const size_t y_stride_sz = base::checked_cast<size_t>(y_stride);
  const size_t uv_stride_sz = base::checked_cast<size_t>(uv_stride);
  const size_t y_sz = y_stride_sz * base::checked_cast<size_t>(aligned_h);
  const size_t uv_sz =
      uv_stride_sz * base::checked_cast<size_t>(aligned_h >> ss_ver);
  const size_t pic_size = y_sz + 2 * uv_sz;

  // Note: Subsequent code diverges from dav1d_default_picture_alloc().
  auto frame_pool =
      base::WrapRefCounted(static_cast<FrameBufferPool*>(frame_pool_opaque));

  // FrameBufferPool doesn't provide alignment and it's not easy to add, so we
  // over-allocate by the alignment and adjust accordingly. Over-allocating by
  // 2 * alignment ensures this is safe to do and satisfies the requirement
  // that data is padded _AND_ aligned by `alignment`.
  const size_t alloc_size = pic_size + alignment * 2;
  void* fb_priv = nullptr;
  auto span = frame_pool->GetFrameBuffer(alloc_size, &fb_priv);
  if (span.empty()) {
    return -1;
  }

  // Safe due to over-allocation by alignment * 2 above.
  auto aligned_span = span.subspan(
      alignment - reinterpret_cast<uintptr_t>(span.data()) % alignment);
  auto y_span = aligned_span.first(y_sz);
  auto u_span =
      has_chroma ? aligned_span.subspan(y_sz, uv_sz) : base::span<uint8_t>();
  auto v_span = has_chroma ? aligned_span.subspan(y_sz + uv_sz, uv_sz)
                           : base::span<uint8_t>();

  planes[0] = y_span.data();
  planes[1] = has_chroma ? u_span.data() : nullptr;
  planes[2] = has_chroma ? v_span.data() : nullptr;
  *allocator_data =
      new FrameBufferData(fb_priv, std::move(frame_pool), chroma_sampling,
                          y_stride_sz, uv_stride_sz, y_span, u_span, v_span);
  return 0;
}

// static
void DavXdVideoDecoderBase::ReleasePictureBuffer(void* allocator_data) {
  delete static_cast<FrameBufferData*>(allocator_data);
}

DavXdVideoDecoderBase::DavXdVideoDecoderBase(
    std::unique_ptr<MediaLog> media_log,
    OffloadState offload_state)
    : media_log_(std::move(media_log)),
      bind_callbacks_(offload_state == OffloadState::kNormal) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

DavXdVideoDecoderBase::~DavXdVideoDecoderBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, DecoderState::kUninitialized);
  if (frame_pool_) {
    frame_pool_->Shutdown();
  }
}

void DavXdVideoDecoderBase::Initialize(const VideoDecoderConfig& config,
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

  if (config.codec() != GetSupportedCodec()) {
    std::move(bound_init_cb)
        .Run(DecoderStatus(DecoderStatus::Codes::kUnsupportedCodec)
                 .WithData("codec", config.codec()));
    return;
  }

  if (!frame_pool_) {
    frame_pool_ =
        base::MakeRefCounted<FrameBufferPool>(/*zero_initialize_memory=*/true);
  }

  // Clear any previously initialized decoder.
  CloseDecoder();

  // Compute the ideal thread count values. We'll then clamp these based on the
  // maximum number of recommended threads (using number of processors, etc).
  int tile_threads, frame_threads;
  GetDecoderThreadCounts(config.coded_size().height(), &tile_threads,
                         &frame_threads);

  // While dav1d uses a thread pool, preserve the same thread counts we used
  // when tile and frame threads were configured distinctly. It may be possible
  // to lower this after some performance analysis of the new system.
  const int n_threads = VideoDecoder::GetRecommendedThreadCount(
      frame_threads * (tile_threads + 1));

  DecoderStatus status = OpenDecoder(low_delay, n_threads);
  if (!status.is_ok()) {
    std::move(bound_init_cb).Run(std::move(status));
    return;
  }

  config_ = config;
  state_ = DecoderState::kNormal;
  output_cb_ = output_cb;
  std::move(bound_init_cb).Run(DecoderStatus::Codes::kOk);
}

void DavXdVideoDecoderBase::Decode(scoped_refptr<DecoderBuffer> buffer,
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
    std::move(bound_decode_cb).Run(error_status_);
    return;
  }

  if (!DecodeBuffer(std::move(buffer))) {
    state_ = DecoderState::kError;
    std::move(bound_decode_cb).Run(error_status_);
    return;
  }

  // VideoDecoderShim expects |decode_cb| call after |output_cb_|.
  std::move(bound_decode_cb).Run(DecoderStatus::Codes::kOk);
}

void DavXdVideoDecoderBase::Reset(base::OnceClosure reset_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(state_, DecoderState::kUninitialized);
  FlushCodec();
  state_ = DecoderState::kNormal;
  hdr_metadata_reordering_map_.Clear();
  error_status_ = DecoderStatus::Codes::kFailed;

  if (bind_callbacks_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(reset_cb));
  } else {
    std::move(reset_cb).Run();
  }
}

void DavXdVideoDecoderBase::Detach() {
  // Even though we offload all resolutions, this may be called in a transition
  // from clear to encrypted content, which will subsequently fail Initialize()
  // since encrypted content isn't supported by this decoder.

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!bind_callbacks_);

  CloseDecoder();
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void DavXdVideoDecoderBase::CloseDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CloseCodec();
  hdr_metadata_reordering_map_.Clear();
  state_ = DecoderState::kUninitialized;
}

void DavXdVideoDecoderBase::RecordBufferMetadata(const DecoderBuffer& buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  hdr_metadata_reordering_map_.Insert(buffer);
}

bool DavXdVideoDecoderBase::OutputPicture(const gfx::Size& visible_size,
                                          int bit_depth,
                                          base::TimeDelta timestamp,
                                          const VideoColorSpace& color_space,
                                          gfx::HDRMetadata hdr_metadata,
                                          const void* allocator_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(allocator_data);

  hdr_metadata_reordering_map_.MergeAndEraseMetadataForTimestamp(timestamp,
                                                                 hdr_metadata);

  auto frame =
      BindImageToVideoFrame(visible_size, bit_depth, timestamp, allocator_data);
  if (!frame) {
    MEDIA_LOG(DEBUG, media_log_)
        << "Failed to produce video frame from decoded picture.";
    return false;
  }

  // If the frame doesn't specify a color space, use the container's.
  auto gfx_cs = color_space.ToGfxColorSpace();
  if (!gfx_cs.IsValid()) {
    gfx_cs = config_.color_space_info().ToGfxColorSpace();
  }

  frame->set_color_space(gfx_cs);
  frame->metadata().power_efficient = false;
  frame->set_hdr_metadata(hdr_metadata);

  const auto* opaque_data = static_cast<const FrameBufferData*>(allocator_data);
  frame->AddDestructionObserver(
      frame_pool_->CreateFrameCallback(opaque_data->fb_priv));

  output_cb_.Run(std::move(frame));
  return true;
}

scoped_refptr<VideoFrame> DavXdVideoDecoderBase::BindImageToVideoFrame(
    const gfx::Size& visible_size,
    int bit_depth,
    base::TimeDelta timestamp,
    const void* allocator_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(allocator_data);
  const auto& fb_data = *static_cast<const FrameBufferData*>(allocator_data);

  const VideoPixelFormat pixel_format =
      ToVideoPixelFormat(fb_data.chroma_sampling, bit_depth);
  if (pixel_format == PIXEL_FORMAT_UNKNOWN) {
    return nullptr;
  }

  const size_t y_plane_height =
      base::checked_cast<size_t>(visible_size.height());
  const size_t uv_plane_height = base::checked_cast<size_t>(
      VideoFrame::PlaneSizeInSamples(pixel_format, VideoFrame::Plane::kU,
                                     visible_size)
          .height());

  const auto y_plane =
      base::span(fb_data.y_span).first(y_plane_height * fb_data.y_stride);

  size_t uv_plane_stride = fb_data.uv_stride;
  base::span<const uint8_t> u_plane;
  base::span<const uint8_t> v_plane;

  const bool needs_fake_uv_planes =
      fb_data.chroma_sampling == VideoChromaSampling::k400;
  if (needs_fake_uv_planes) {
    // UV planes are half the size of the Y plane.
    uv_plane_stride = base::bits::AlignUp(fb_data.y_stride / 2, size_t{2});
    const size_t size_needed = uv_plane_stride * uv_plane_height;

    if (!fake_uv_data_ || fake_uv_data_->size() != size_needed) {
      if (bit_depth == 8) {
        // Avoid having base::RefCountedBytes zero initialize the memory just to
        // fill it with a different value. When we resize, existing frames will
        // keep their refs on the old data.
        constexpr uint8_t kBlankUV = 256 / 2;
        fake_uv_data_ = base::MakeRefCounted<base::RefCountedBytes>(
            std::vector<uint8_t>(size_needed, kBlankUV));
      } else {
        DCHECK(bit_depth == 10 || bit_depth == 12);
        const uint16_t kBlankUV = (1 << bit_depth) / 2;
        fake_uv_data_ =
            base::MakeRefCounted<RefCountedUV16Data>(size_needed / 2, kBlankUV);
      }
    }

    u_plane = v_plane = base::span<const uint8_t>(*fake_uv_data_);
  } else {
    const size_t uv_plane_bytes = uv_plane_height * uv_plane_stride;
    u_plane = base::span(fb_data.u_span).first(uv_plane_bytes);
    v_plane = base::span(fb_data.v_span).first(uv_plane_bytes);
  }

  const gfx::Rect visible_rect(visible_size);
  auto frame = VideoFrame::WrapExternalYuvData(
      pixel_format, visible_size, visible_rect,
      config_.aspect_ratio().GetNaturalSize(visible_rect), fb_data.y_stride,
      uv_plane_stride, uv_plane_stride, y_plane, u_plane, v_plane, timestamp);
  if (!frame) {
    return nullptr;
  }

  // Each frame needs a ref on the fake UV data to keep it alive until done.
  if (needs_fake_uv_planes) {
    frame->AddDestructionObserver(base::BindOnce(
        [](scoped_refptr<base::RefCountedMemory>) {}, fake_uv_data_));
  }

  return frame;
}

}  // namespace media
