// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/test/video_decoder.h"

#include <linux/videodev2.h>

#include "base/bits.h"
#include "base/logging.h"
#include "media/gpu/v4l2/test/av1_pix_fmt.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/codec/png_codec.h"

namespace {
// Returns |src| in a packed buffer.
std::vector<char> CopyAndRemovePadding(const char* src,
                                       size_t stride,
                                       gfx::Size size) {
  DCHECK_GE(stride, static_cast<size_t>(size.width()));
  LOG_ASSERT(src);

  std::vector<char> dst;
  dst.reserve(size.GetArea());

  const auto* const kSrcLimit = src + stride * size.height();
  for (; src < kSrcLimit; src += stride)
    dst.insert(dst.end(), src, src + size.width());

  return dst;
}

}  // namespace
namespace media {
namespace v4l2_test {

namespace {

// Unpacks an NV12 UV plane into separate U and V planes.
void UnpackUVPlane(std::vector<char>& dest_u,
                   std::vector<char>& dest_v,
                   std::vector<char>& src_uv,
                   gfx::Size size) {
  for (int i = 0; i < size.GetArea(); i++) {
    dest_u.push_back(src_uv[2 * i]);
    dest_v.push_back(src_uv[2 * i + 1]);
  }
}

// Detiles a single MM21 plane. MM21 is an NV12-like pixel format that is stored
// in 16x32 tiles in the Y plane and 16x16 tiles in the UV plane (since it's
// 4:2:0 subsampled, but UV are interlaced). This function converts a single
// MM21 plane into its equivalent NV12 plane.
void DetilePlane(std::vector<char>& dest,
                 const gfx::Size& dest_size,
                 char* src,
                 const gfx::Size& src_size,
                 const gfx::Size& tile_size) {
  // Tile size in bytes.
  const int tile_len = tile_size.GetArea();
  // |width| rounded down to the nearest multiple of |tile_width|.
  const int aligned_dst_width =
      base::bits::AlignDown(dest_size.width(), tile_size.width());
  // number of pixels more than a full tile width
  const int last_tile_partial_width = dest_size.width() - aligned_dst_width;
  // |height| rounded up to the nearest multiple of |tile_height|.
  const int padded_dst_height =
      base::bits::AlignUp(dest_size.height(), tile_size.height());
  // Size of one row of tiles in bytes.
  const int src_row_size = src_size.width() * tile_size.height();
  // Size of the entire coded image.
  const int coded_image_num_pixels = src_size.width() * padded_dst_height;

  // Index in bytes to the start of the current tile row.
  int src_tile_row_start = 0;
  // Offset in pixels from top of the screen of the current tile row.
  int y_offset = 0;

  // Iterates over each row of tiles.
  while (src_tile_row_start < coded_image_num_pixels) {
    // Maximum relative y-axis value that we should process for the given tile
    // row. Important for cropping.
    const int max_in_tile_row_index =
        dest_size.height() - y_offset < tile_size.height()
            ? (dest_size.height() - y_offset)
            : tile_size.height();

    // Offset in bytes into the current tile row to start reading data for the
    // next pixel row.
    int src_row_start = 0;

    // Iterates over each row of pixels within the tile row.
    for (int in_tile_row_index = 0; in_tile_row_index < max_in_tile_row_index;
         in_tile_row_index++) {
      int src_index = src_tile_row_start + src_row_start;

      // Iterates over each pixel in the row of pixels.
      for (int col_index = 0; col_index < aligned_dst_width;
           col_index += tile_size.width()) {
        dest.insert(dest.end(), src + src_index,
                    src + src_index + tile_size.width());
        src_index += tile_len;
      }
      // Finish last partial tile in the row.
      dest.insert(dest.end(), src + src_index,
                  src + src_index + last_tile_partial_width);

      // Shift to the next pixel row in the tile row.
      src_row_start += tile_size.width();
    }

    src_tile_row_start += src_row_size;
    y_offset += tile_size.height();
  }
}

}  // namespace

uint32_t FileFourccToDriverFourcc(uint32_t header_fourcc) {
  if (header_fourcc == V4L2_PIX_FMT_VP9) {
    LOG(INFO) << "OUTPUT format mapped from VP90 to VP9F.";
    return V4L2_PIX_FMT_VP9_FRAME;
  } else if (header_fourcc == V4L2_PIX_FMT_AV1) {
    LOG(INFO) << "OUTPUT format mapped from AV01 to AV1F.";
    return V4L2_PIX_FMT_AV1_FRAME;
  } else if (header_fourcc == V4L2_PIX_FMT_VP8) {
    LOG(INFO) << "OUTPUT format mapped from VP80 to VP8F.";
    return V4L2_PIX_FMT_VP8_FRAME;
  }

  return header_fourcc;
}

VideoDecoder::VideoDecoder(std::unique_ptr<V4L2IoctlShim> v4l2_ioctl,
                           std::unique_ptr<V4L2Queue> OUTPUT_queue,
                           std::unique_ptr<V4L2Queue> CAPTURE_queue)
    : v4l2_ioctl_(std::move(v4l2_ioctl)),
      OUTPUT_queue_(std::move(OUTPUT_queue)),
      CAPTURE_queue_(std::move(CAPTURE_queue)) {}

VideoDecoder::~VideoDecoder() = default;

void VideoDecoder::Initialize() {
  // TODO(stevecho): remove VIDIOC_ENUM_FRAMESIZES ioctl call
  //   after b/193237015 is resolved.
  if (!v4l2_ioctl_->EnumFrameSizes(OUTPUT_queue_->fourcc()))
    LOG(INFO) << "EnumFrameSizes for OUTPUT queue failed.";

  if (!v4l2_ioctl_->SetFmt(OUTPUT_queue_))
    LOG(FATAL) << "SetFmt for OUTPUT queue failed.";

  gfx::Size coded_size;
  uint32_t num_planes;
  if (!v4l2_ioctl_->GetFmt(CAPTURE_queue_->type(), &coded_size, &num_planes))
    LOG(FATAL) << "GetFmt for CAPTURE queue failed.";

  CAPTURE_queue_->set_coded_size(coded_size);
  CAPTURE_queue_->set_num_planes(num_planes);

  // VIDIOC_TRY_FMT() ioctl is equivalent to VIDIOC_S_FMT
  // with one exception that it does not change driver state.
  // VIDIOC_TRY_FMT may or may not be needed; it's used by the stateful
  // Chromium V4L2VideoDecoder backend, see b/190733055#comment78.
  // TODO(b/190733055): try and remove it after landing all the code.
  if (!v4l2_ioctl_->TryFmt(CAPTURE_queue_))
    LOG(FATAL) << "TryFmt for CAPTURE queue failed.";

  if (!v4l2_ioctl_->SetFmt(CAPTURE_queue_))
    LOG(FATAL) << "SetFmt for CAPTURE queue failed.";

  // If there is a dynamic resolution change, the Initialization sequence will
  // be performed again, minus the allocation of OUTPUT queue buffers.
  if (IsResolutionChanged()) {
    if (!v4l2_ioctl_->ReqBufsWithCount(CAPTURE_queue_,
                                       number_of_buffers_in_capture_queue_)) {
      LOG(FATAL) << "ReqBufs for CAPTURE queue failed.";
    }
  } else {
    if (!v4l2_ioctl_->ReqBufs(OUTPUT_queue_))
      LOG(FATAL) << "ReqBufs for OUTPUT queue failed.";

    if (!v4l2_ioctl_->QueryAndMmapQueueBuffers(OUTPUT_queue_))
      LOG(FATAL) << "QueryAndMmapQueueBuffers for OUTPUT queue failed";

    if (!v4l2_ioctl_->ReqBufs(CAPTURE_queue_))
      LOG(FATAL) << "ReqBufs for CAPTURE queue failed.";
  }

  if (!v4l2_ioctl_->QueryAndMmapQueueBuffers(CAPTURE_queue_))
    LOG(FATAL) << "QueryAndMmapQueueBuffers for CAPTURE queue failed.";

  // Only 1 CAPTURE buffer is needed for 1st key frame decoding. Remaining
  // CAPTURE buffers will be queued after that.
  if (!v4l2_ioctl_->QBuf(CAPTURE_queue_, 0))
    LOG(FATAL) << "VIDIOC_QBUF failed for CAPTURE queue.";

  int media_request_fd;
  if (!v4l2_ioctl_->MediaIocRequestAlloc(&media_request_fd))
    LOG(FATAL) << "MEDIA_IOC_REQUEST_ALLOC failed";

  OUTPUT_queue_->set_media_request_fd(media_request_fd);

  if (!v4l2_ioctl_->StreamOn(OUTPUT_queue_->type()))
    LOG(FATAL) << "StreamOn for OUTPUT queue failed.";

  if (!v4l2_ioctl_->StreamOn(CAPTURE_queue_->type()))
    LOG(FATAL) << "StreamOn for CAPTURE queue failed.";
}

// Follows the dynamic resolution change sequence described in
// https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/dev-stateless-decoder.html#dynamic-resolution-change
VideoDecoder::Result VideoDecoder::HandleDynamicResolutionChange(
    const gfx::Size& new_resolution) {
  // Call VIDIOC_STREAMOFF() on both the OUTPUT and CAPTURE queues.
  if (!v4l2_ioctl_->StreamOff(OUTPUT_queue_->type()))
    LOG(FATAL) << "StreamOff for OUTPUT queue failed.";

  if (!v4l2_ioctl_->StreamOff(CAPTURE_queue_->type()))
    LOG(FATAL) << "StreamOff for CAPTURE queue failed.";

  // Free all CAPTURE buffers from the driver side by calling VIDIOC_REQBUFS()
  // on the CAPTURE queue with a buffer count of zero.
  if (!v4l2_ioctl_->ReqBufsWithCount(CAPTURE_queue_, 0))
    LOG(FATAL) << "Failed to free all buffers for CAPTURE queue";

  // Free queued CAPTURE buffer indexes that are tracked by the client side.
  CAPTURE_queue_->DequeueAllBufferIndexes();

  // Set the new resolution on OUTPUT queue. The driver will then pick up
  // the new resolution to be set on the coded size for CAPTURE queue.
  OUTPUT_queue_->set_display_size(new_resolution);
  OUTPUT_queue_->set_coded_size(new_resolution);

  CAPTURE_queue_->set_display_size(new_resolution);

  // Perform the initialization sequence again
  Initialize();
  is_resolution_changed_ = false;

  return VideoDecoder::kOk;
}

// Unpacks NV12 to I420 and optionally trims padding from source.
// This expects a contiguous NV12 buffer, as specified by
// V4L2_PIX_FMT_NV12.
void VideoDecoder::ConvertNV12ToYUV(std::vector<char>& dest_y,
                                    std::vector<char>& dest_u,
                                    std::vector<char>& dest_v,
                                    const gfx::Size& dest_size,
                                    const char* src,
                                    const gfx::Size& src_size) {
  CHECK(dest_size.width() <= src_size.width());
  CHECK(dest_size.height() <= src_size.height());

  // Copy Y plane
  dest_y.reserve(dest_size.GetArea());
  for (int i = 0; i < dest_size.height(); i++) {
    dest_y.insert(dest_y.end(), src, src + dest_size.width());
    src += src_size.width();
  }

  // Move to start of UV plane
  if (dest_size.height() < src_size.height())
    src += src_size.width() * (src_size.height() - dest_size.height());

  // Pad size for U/V plane to handle odd dimensions
  gfx::Size dest_aligned_size(base::bits::AlignUp(dest_size.width(), 2),
                              base::bits::AlignUp(dest_size.height(), 2));
  const int uv_height = dest_aligned_size.height() / 2;
  const int uv_width = dest_aligned_size.width() / 2;

  // Unpack UV plane
  dest_u.reserve(dest_aligned_size.GetArea() / 4);
  dest_v.reserve(dest_aligned_size.GetArea() / 4);

  for (int i = 0; i < uv_height; i++) {
    for (int j = 0; j < uv_width; j++) {
      dest_u.push_back(src[0]);
      dest_v.push_back(src[1]);
      src += 2;
    }

    // Skip any trailing pixels on the line in the source image
    // Skip is based on non-sub-sampled dimensions
    if (dest_aligned_size.width() < src_size.width())
      src += src_size.width() - dest_aligned_size.width();
  }
}

void VideoDecoder::ConvertMM21ToYUV(std::vector<char>& dest_y,
                                    std::vector<char>& dest_u,
                                    std::vector<char>& dest_v,
                                    const gfx::Size& dest_size,
                                    char* src_y,
                                    char* src_uv,
                                    const gfx::Size& src_size) {
  constexpr int kMM21TileWidth = 16;
  constexpr int kMM21TileHeight = 32;

  LOG_ASSERT(src_size.width() % kMM21TileWidth == 0)
      << "Source buffer width (" << src_size.width()
      << ") must be a multiple of " << kMM21TileWidth;
  constexpr gfx::Size kYTileSize(kMM21TileWidth, kMM21TileHeight);
  constexpr gfx::Size kUVTileSize(kMM21TileWidth, kMM21TileHeight / 2);

  // Detile and pad MM21's luma plane in a temporary |src_y_padded|.
  std::vector<char> src_y_padded;
  src_y_padded.reserve(src_size.GetArea());
  DetilePlane(src_y_padded, src_size, src_y, src_size, kYTileSize);
  dest_y =
      CopyAndRemovePadding(src_y_padded.data(), src_size.width(), dest_size);

  // Detile and pad MM21's chroma plane in a temporary |src_uv_padded|.
  const gfx::Size src_uv_size(base::bits::AlignUp(src_size.width(), 2),
                              base::bits::AlignUp(src_size.height(), 2) / 2);
  std::vector<char> src_uv_padded;
  src_uv_padded.reserve(src_uv_size.GetArea());
  DetilePlane(src_uv_padded, src_uv_size, src_uv, src_uv_size, kUVTileSize);

  // Round up plane dimensions for odd resolution bitstreams.
  const size_t u_plane_padded_width =
      base::bits::AlignUp(src_size.width(), 2) / 2;
  const size_t v_plane_padded_width =
      base::bits::AlignUp(src_size.width(), 2) / 2;
  const size_t u_plane_padded_height =
      base::bits::AlignUp(src_size.height(), 2) / 2;
  const gfx::Size u_plane_padded_size(u_plane_padded_width,
                                      u_plane_padded_height);

  std::vector<char> src_u_padded;
  std::vector<char> src_v_padded;
  src_u_padded.reserve(src_uv_size.GetArea() / 2);
  src_v_padded.reserve(src_uv_size.GetArea() / 2);

  // Unpack NV12's UV plane into separate U and V planes.
  UnpackUVPlane(src_u_padded, src_v_padded, src_uv_padded, u_plane_padded_size);

  const gfx::Size src_u_padded_size(
      base::bits::AlignUp(dest_size.width(), 2) / 2,
      base::bits::AlignUp(dest_size.height(), 2) / 2);
  const gfx::Size src_v_padded_size(
      base::bits::AlignUp(dest_size.width(), 2) / 2,
      base::bits::AlignUp(dest_size.height(), 2) / 2);
  dest_u = CopyAndRemovePadding(src_u_padded.data(), u_plane_padded_width,
                                src_u_padded_size);
  dest_v = CopyAndRemovePadding(src_v_padded.data(), v_plane_padded_width,
                                src_v_padded_size);
}

std::vector<unsigned char> VideoDecoder::ConvertYUVToPNG(
    char* y_plane,
    char* u_plane,
    char* v_plane,
    const gfx::Size& size) {
  const size_t argb_stride = size.width() * 4;
  auto argb_data = std::make_unique<uint8_t[]>(argb_stride * size.height());

  size_t u_plane_padded_width, v_plane_padded_width;
  u_plane_padded_width = v_plane_padded_width =
      base::bits::AlignUp(size.width(), 2) / 2;

  // Note that we use J420ToARGB instead of I420ToARGB so that the
  // kYuvJPEGConstants YUV-to-RGB conversion matrix is used.
  const int convert_to_argb_result = libyuv::J420ToARGB(
      reinterpret_cast<uint8_t*>(y_plane), size.width(),
      reinterpret_cast<uint8_t*>(u_plane), u_plane_padded_width,
      reinterpret_cast<uint8_t*>(v_plane), v_plane_padded_width,
      argb_data.get(), base::checked_cast<int>(argb_stride), size.width(),
      size.height());

  LOG_ASSERT(convert_to_argb_result == 0) << "Failed to convert to ARGB";

  std::vector<unsigned char> image_buffer;
  const bool encode_to_png_result = gfx::PNGCodec::Encode(
      argb_data.get(), gfx::PNGCodec::FORMAT_BGRA, size, argb_stride,
      true /*discard_transparency*/, std::vector<gfx::PNGCodec::Comment>(),
      &image_buffer);
  LOG_ASSERT(encode_to_png_result) << "Failed to encode to PNG";

  return image_buffer;
}
}  // namespace v4l2_test
}  // namespace media
