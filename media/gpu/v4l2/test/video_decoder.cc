// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/test/video_decoder.h"

#include <linux/videodev2.h>

#include "base/bits.h"
#include "base/logging.h"
#include "media/gpu/v4l2/test/av1_pix_fmt.h"

namespace media {
namespace v4l2_test {

namespace {

// Unpacks an NV12 UV plane into separate U and V planes.
void UnpackUVPlane(std::vector<char>& dest_u,
                   std::vector<char>& dest_v,
                   std::vector<char>& src_uv,
                   gfx::Size size) {
  dest_u.reserve(size.GetArea() / 4);
  dest_v.reserve(size.GetArea() / 4);
  for (int i = 0; i < size.GetArea() / 4; i++) {
    dest_u.push_back(src_uv[2 * i]);
    dest_v.push_back(src_uv[2 * i + 1]);
  }
}

// Detiles a single MM21 plane. MM21 is an NV12-like pixel format that is stored
// in 16x32 tiles in the Y plane and 16x16 tiles in the UV plane (since it's
// 4:2:0 subsampled, but UV are interlaced). This function converts a single
// MM21 plane into its equivalent NV12 plane.
void DetilePlane(std::vector<char>& dest,
                 gfx::Size dest_size,
                 char* src,
                 gfx::Size src_size,
                 gfx::Size tile_size) {
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
    LOG(FATAL) << "EnumFrameSizes for OUTPUT queue failed.";

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

  if (!v4l2_ioctl_->ReqBufs(OUTPUT_queue_))
    LOG(FATAL) << "ReqBufs for OUTPUT queue failed.";

  if (!v4l2_ioctl_->QueryAndMmapQueueBuffers(OUTPUT_queue_))
    LOG(FATAL) << "QueryAndMmapQueueBuffers for OUTPUT queue failed";

  if (!v4l2_ioctl_->ReqBufs(CAPTURE_queue_))
    LOG(FATAL) << "ReqBufs for CAPTURE queue failed.";

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

void VideoDecoder::ConvertMM21ToYUV(std::vector<char>& dest_y,
                                    std::vector<char>& dest_u,
                                    std::vector<char>& dest_v,
                                    gfx::Size dest_size,
                                    char* src_y,
                                    char* src_uv,
                                    gfx::Size src_size) {
  // Detile MM21's luma plane.
  constexpr int kMM21TileWidth = 16;
  constexpr int kMM21TileHeight = 32;

  LOG_ASSERT(src_size.width() % kMM21TileWidth == 0)
      << "Source buffer width (" << src_size.width()
      << ") must be a multiple of " << kMM21TileWidth;
  constexpr gfx::Size kYTileSize(kMM21TileWidth, kMM21TileHeight);
  dest_y.reserve(dest_size.GetArea());
  DetilePlane(dest_y, dest_size, src_y, src_size, kYTileSize);

  // Detile MM21's chroma plane in a temporary |detiled_uv|.
  std::vector<char> detiled_uv;
  const gfx::Size dest_uv_size(dest_size.width(), dest_size.height() / 2);
  const gfx::Size src_uv_size(src_size.width(), src_size.height() / 2);
  constexpr gfx::Size kUVTileSize(kMM21TileWidth, kMM21TileHeight / 2);
  detiled_uv.reserve(dest_size.GetArea() / 2);
  DetilePlane(detiled_uv, dest_uv_size, src_uv, src_uv_size, kUVTileSize);

  // Unpack NV12's UV plane into separate U and V planes.
  UnpackUVPlane(dest_u, dest_v, detiled_uv, dest_size);
}

}  // namespace v4l2_test
}  // namespace media
