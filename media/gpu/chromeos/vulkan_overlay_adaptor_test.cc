// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/vulkan_overlay_adaptor.h"

#include <linux/videodev2.h>
#include <sys/mman.h>
#include <sys/poll.h>

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/bits.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/config/gpu_feature_info.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_layout.h"
#include "media/base/video_types.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/chromeos/perf_test_util.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/test/image.h"
#include "media/gpu/test/image_quality_metrics.h"
#include "media/gpu/test/video_test_environment.h"
#include "media/gpu/video_frame_mapper_factory.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"
#include "ui/ozone/public/ozone_platform.h"

// Not all versions of the V4L2 headers define these FourCCs, so we add them
// here for backwards compatibility.
#ifndef V4L2_PIX_FMT_MT2T
#define V4L2_PIX_FMT_MT2T v4l2_fourcc('M', 'T', '2', 'T')
#endif
#ifndef V4L2_PIX_FMT_ARGB2101010
#define V4L2_PIX_FMT_ARGB2101010 v4l2_fourcc('A', 'R', '3', '0')
#endif
#ifndef V4L2_PIX_FMT_I010
#define V4L2_PIX_FMT_I010 v4l2_fourcc('I', '0', '1', '0')
#endif

namespace media {
namespace {

struct FrameState {
  uint32_t fourcc;
  bool is_scaled;
  bool is_rotated;

  friend constexpr bool operator==(const FrameState& lhs,
                                   const FrameState& rhs) = default;
};

}  // namespace
}  // namespace media

template <>
struct std::hash<media::FrameState> {
  size_t operator()(const media::FrameState& f) const {
    return static_cast<size_t>(f.fourcc) |
           (static_cast<size_t>(f.is_scaled) << 15) |
           (static_cast<size_t>(f.is_rotated) << 7);
  }
};

namespace media {
namespace {

static constexpr size_t kMM21TileWidth = 16;
static constexpr size_t kMM21TileHeight = 32;
static constexpr int kRandomFrameSeed = 1000;

const char* usage_msg =
    "usage: vulkan_overlay_adaptor_test\n"
    "[--gtest_help] [--help] [-v=<level>] [--vmodule=<config>] \n";

const char* help_msg =
    "Run the vulkan image processor perf tests.\n\n"
    "The following arguments are supported:\n"
    "  --gtest_help          display the gtest help and exit.\n"
    "  --help                display this help and exit.\n"
    "  --source_directory    specify test input files location.\n"
    "  --output_directory    specify test output file location. Defaults to "
    "                        execution directory if not specified.\n"
    "   -v                   enable verbose mode, e.g. -v=2.\n"
    "  --vmodule             enable verbose mode for the specified module.\n";

// File for MM21 detile and scaling test.
const base::FilePath::CharType* kMM21Image =
    FILE_PATH_LITERAL("puppets-480x270.mm21.yuv");
// Files for MT2T Vulkan detile test.
const base::FilePath::CharType* kMT2TImage =
    FILE_PATH_LITERAL("crowd_run_1080x512.mt2t");

constexpr int kLibYUVSuccess = 0;

scoped_refptr<VideoFrame> ConvMM21ToI420(const VideoFrame& in_frame) {
  CHECK_EQ(in_frame.format(), VideoPixelFormat::PIXEL_FORMAT_NV12);

  scoped_refptr<VideoFrame> out_frame = VideoFrame::CreateFrame(
      VideoPixelFormat::PIXEL_FORMAT_I420, in_frame.coded_size(),
      in_frame.visible_rect(), in_frame.coded_size(), base::TimeDelta());
  CHECK_EQ(
      libyuv::MM21ToI420(
          in_frame.visible_data(VideoFrame::Plane::kY),
          in_frame.stride(VideoFrame::Plane::kY),
          in_frame.visible_data(VideoFrame::Plane::kUV),
          in_frame.stride(VideoFrame::Plane::kUV),
          out_frame->GetWritableVisibleData(VideoFrame::Plane::kY),
          out_frame->stride(VideoFrame::Plane::kY),
          out_frame->GetWritableVisibleData(VideoFrame::Plane::kU),
          out_frame->stride(VideoFrame::Plane::kU),
          out_frame->GetWritableVisibleData(VideoFrame::Plane::kV),
          out_frame->stride(VideoFrame::Plane::kV),
          out_frame->coded_size().width(), out_frame->coded_size().height()),
      kLibYUVSuccess);

  return out_frame;
}

scoped_refptr<VideoFrame> ConvMT2TToP010(const VideoFrame& in_frame) {
  constexpr size_t bpp_numerator = 5;
  constexpr size_t bpp_denom = 4;

  CHECK_EQ(in_frame.format(), VideoPixelFormat::PIXEL_FORMAT_NV12);

  gfx::Size out_size =
      gfx::Size(in_frame.coded_size().width(),
                in_frame.coded_size().height() / bpp_numerator * bpp_denom);

  scoped_refptr<VideoFrame> out_frame = VideoFrame::CreateFrame(
      VideoPixelFormat::PIXEL_FORMAT_P010LE, out_size, in_frame.visible_rect(),
      out_size, base::TimeDelta());
  CHECK_EQ(
      libyuv::MT2TToP010(
          in_frame.visible_data(VideoFrame::Plane::kY),
          in_frame.stride(VideoFrame::Plane::kY) * bpp_numerator / bpp_denom,
          in_frame.visible_data(VideoFrame::Plane::kUV),
          in_frame.stride(VideoFrame::Plane::kUV) * bpp_numerator / bpp_denom,
          reinterpret_cast<uint16_t*>(
              out_frame->GetWritableVisibleData(VideoFrame::Plane::kY)),
          out_frame->stride(VideoFrame::Plane::kY) / 2,
          reinterpret_cast<uint16_t*>(
              out_frame->GetWritableVisibleData(VideoFrame::Plane::kUV)),
          out_frame->stride(VideoFrame::Plane::kUV) / 2,
          out_frame->coded_size().width(), out_frame->coded_size().height()),
      kLibYUVSuccess);

  return out_frame;
}

scoped_refptr<VideoFrame> ConvP010ToI010(const VideoFrame& in_frame) {
  CHECK_EQ(in_frame.format(), VideoPixelFormat::PIXEL_FORMAT_P010LE);

  scoped_refptr<VideoFrame> out_frame = VideoFrame::CreateFrame(
      VideoPixelFormat::PIXEL_FORMAT_YUV420P10, in_frame.coded_size(),
      in_frame.visible_rect(), in_frame.coded_size(), base::TimeDelta());
  CHECK_EQ(libyuv::P010ToI010(
               reinterpret_cast<const uint16_t*>(
                   in_frame.visible_data(VideoFrame::Plane::kY)),
               in_frame.stride(VideoFrame::Plane::kY) / 2,
               reinterpret_cast<const uint16_t*>(
                   in_frame.visible_data(VideoFrame::Plane::kUV)),
               in_frame.stride(VideoFrame::Plane::kUV) / 2,
               reinterpret_cast<uint16_t*>(
                   out_frame->GetWritableVisibleData(VideoFrame::Plane::kY)),
               out_frame->stride(VideoFrame::Plane::kY) / 2,
               reinterpret_cast<uint16_t*>(
                   out_frame->GetWritableVisibleData(VideoFrame::Plane::kU)),
               out_frame->stride(VideoFrame::Plane::kU) / 2,
               reinterpret_cast<uint16_t*>(
                   out_frame->GetWritableVisibleData(VideoFrame::Plane::kV)),
               out_frame->stride(VideoFrame::Plane::kV) / 2,
               out_frame->visible_rect().width(),
               out_frame->visible_rect().height()),
           kLibYUVSuccess);

  return out_frame;
}

scoped_refptr<VideoFrame> ConvI420ToARGB(const VideoFrame& in_frame) {
  CHECK_EQ(in_frame.format(), VideoPixelFormat::PIXEL_FORMAT_I420);

  scoped_refptr<VideoFrame> out_frame = VideoFrame::CreateFrame(
      VideoPixelFormat::PIXEL_FORMAT_ARGB, in_frame.coded_size(),
      in_frame.visible_rect(), in_frame.coded_size(), base::TimeDelta());

  CHECK_EQ(libyuv::I420ToARGB(
               in_frame.visible_data(VideoFrame::Plane::kY),
               in_frame.stride(VideoFrame::Plane::kY),
               in_frame.visible_data(VideoFrame::Plane::kU),
               in_frame.stride(VideoFrame::Plane::kU),
               in_frame.visible_data(VideoFrame::Plane::kV),
               in_frame.stride(VideoFrame::Plane::kV),
               out_frame->GetWritableVisibleData(VideoFrame::Plane::kARGB),
               out_frame->stride(VideoFrame::Plane::kARGB),
               out_frame->visible_rect().width(),
               out_frame->visible_rect().height()),
           kLibYUVSuccess);

  return out_frame;
}

scoped_refptr<VideoFrame> ConvI010ToAR30(const VideoFrame& in_frame) {
  CHECK_EQ(in_frame.format(), VideoPixelFormat::PIXEL_FORMAT_YUV420P10);

  scoped_refptr<VideoFrame> out_frame = VideoFrame::CreateFrame(
      VideoPixelFormat::PIXEL_FORMAT_XR30, in_frame.coded_size(),
      in_frame.visible_rect(), in_frame.coded_size(), base::TimeDelta());

  CHECK_EQ(libyuv::I010ToAR30(
               reinterpret_cast<const uint16_t*>(
                   in_frame.visible_data(VideoFrame::Plane::kY)),
               in_frame.stride(VideoFrame::Plane::kY) / 2,
               reinterpret_cast<const uint16_t*>(
                   in_frame.visible_data(VideoFrame::Plane::kU)),
               in_frame.stride(VideoFrame::Plane::kU) / 2,
               reinterpret_cast<const uint16_t*>(
                   in_frame.visible_data(VideoFrame::Plane::kV)),
               in_frame.stride(VideoFrame::Plane::kV) / 2,
               out_frame->GetWritableVisibleData(VideoFrame::Plane::kARGB),
               out_frame->stride(VideoFrame::Plane::kARGB),
               out_frame->visible_rect().width(),
               out_frame->visible_rect().height()),
           kLibYUVSuccess);

  return out_frame;
}

scoped_refptr<VideoFrame> ScaleI420(const gfx::Size* dst_size,
                                    const VideoFrame& in_frame) {
  CHECK_EQ(in_frame.format(), VideoPixelFormat::PIXEL_FORMAT_I420);

  scoped_refptr<VideoFrame> out_frame = VideoFrame::CreateFrame(
      VideoPixelFormat::PIXEL_FORMAT_I420, *dst_size, gfx::Rect(*dst_size),
      *dst_size, base::TimeDelta());

  CHECK_EQ(
      libyuv::I420Scale(
          in_frame.visible_data(VideoFrame::Plane::kY),
          in_frame.stride(VideoFrame::Plane::kY),
          in_frame.visible_data(VideoFrame::Plane::kU),
          in_frame.stride(VideoFrame::Plane::kU),
          in_frame.visible_data(VideoFrame::Plane::kV),
          in_frame.stride(VideoFrame::Plane::kV),
          in_frame.visible_rect().width(), in_frame.visible_rect().height(),
          out_frame->GetWritableVisibleData(VideoFrame::Plane::kY),
          out_frame->stride(VideoFrame::Plane::kY),
          out_frame->GetWritableVisibleData(VideoFrame::Plane::kU),
          out_frame->stride(VideoFrame::Plane::kU),
          out_frame->GetWritableVisibleData(VideoFrame::Plane::kV),
          out_frame->stride(VideoFrame::Plane::kV),
          out_frame->visible_rect().width(), out_frame->visible_rect().height(),
          libyuv::kFilterBilinear),
      kLibYUVSuccess);

  return out_frame;
}

scoped_refptr<VideoFrame> ScaleI010(const gfx::Size* dst_size,
                                    const VideoFrame& in_frame) {
  CHECK_EQ(in_frame.format(), VideoPixelFormat::PIXEL_FORMAT_YUV420P10);

  scoped_refptr<VideoFrame> out_frame = VideoFrame::CreateFrame(
      VideoPixelFormat::PIXEL_FORMAT_YUV420P10, *dst_size, gfx::Rect(*dst_size),
      *dst_size, base::TimeDelta());

  CHECK_EQ(
      libyuv::I420Scale_16(
          reinterpret_cast<const uint16_t*>(
              in_frame.visible_data(VideoFrame::Plane::kY)),
          in_frame.stride(VideoFrame::Plane::kY) / 2,
          reinterpret_cast<const uint16_t*>(
              in_frame.visible_data(VideoFrame::Plane::kU)),
          in_frame.stride(VideoFrame::Plane::kU) / 2,
          reinterpret_cast<const uint16_t*>(
              in_frame.visible_data(VideoFrame::Plane::kV)),
          in_frame.stride(VideoFrame::Plane::kV) / 2,
          in_frame.visible_rect().width(), in_frame.visible_rect().height(),
          reinterpret_cast<uint16_t*>(
              out_frame->GetWritableVisibleData(VideoFrame::Plane::kY)),
          out_frame->stride(VideoFrame::Plane::kY) / 2,
          reinterpret_cast<uint16_t*>(
              out_frame->GetWritableVisibleData(VideoFrame::Plane::kU)),
          out_frame->stride(VideoFrame::Plane::kU) / 2,
          reinterpret_cast<uint16_t*>(
              out_frame->GetWritableVisibleData(VideoFrame::Plane::kV)),
          out_frame->stride(VideoFrame::Plane::kV) / 2,
          out_frame->visible_rect().width(), out_frame->visible_rect().height(),
          libyuv::kFilterBilinear),
      kLibYUVSuccess);

  return out_frame;
}

scoped_refptr<VideoFrame> RotateI420(libyuv::RotationMode rotation,
                                     gfx::Size* final_out_size,
                                     const VideoFrame& in_frame) {
  CHECK_EQ(in_frame.format(), VideoPixelFormat::PIXEL_FORMAT_I420);

  gfx::Size out_size = in_frame.coded_size();
  if (rotation == libyuv::kRotate90 || rotation == libyuv::kRotate270) {
    out_size.Transpose();

    // If we need to do a scale and a rotate, we may need to transpose the scale
    // operation's output dimensions if it comes before the rotation. To handle
    // this, we transpose the final output size initially, and then here, in the
    // rotation operation, we transpose it back to the original dimensions in
    // case the scale operation comes later.
    final_out_size->Transpose();
  }
  scoped_refptr<VideoFrame> out_frame =
      VideoFrame::CreateFrame(VideoPixelFormat::PIXEL_FORMAT_I420, out_size,
                              gfx::Rect(out_size), out_size, base::TimeDelta());

  CHECK_EQ(libyuv::I420Rotate(
               in_frame.visible_data(VideoFrame::Plane::kY),
               in_frame.stride(VideoFrame::Plane::kY),
               in_frame.visible_data(VideoFrame::Plane::kU),
               in_frame.stride(VideoFrame::Plane::kU),
               in_frame.visible_data(VideoFrame::Plane::kV),
               in_frame.stride(VideoFrame::Plane::kV),
               out_frame->GetWritableVisibleData(VideoFrame::Plane::kY),
               out_frame->stride(VideoFrame::Plane::kY),
               out_frame->GetWritableVisibleData(VideoFrame::Plane::kU),
               out_frame->stride(VideoFrame::Plane::kU),
               out_frame->GetWritableVisibleData(VideoFrame::Plane::kV),
               out_frame->stride(VideoFrame::Plane::kV),
               in_frame.visible_rect().width(),
               in_frame.visible_rect().height(), rotation),
           kLibYUVSuccess);

  return out_frame;
}

scoped_refptr<VideoFrame> RotateI010(libyuv::RotationMode rotation,
                                     gfx::Size* final_out_size,
                                     const VideoFrame& in_frame) {
  CHECK_EQ(in_frame.format(), VideoPixelFormat::PIXEL_FORMAT_YUV420P10);

  gfx::Size out_size = in_frame.coded_size();
  if (rotation == libyuv::kRotate90 || rotation == libyuv::kRotate270) {
    out_size.Transpose();

    final_out_size->Transpose();
  }
  scoped_refptr<VideoFrame> out_frame = VideoFrame::CreateFrame(
      VideoPixelFormat::PIXEL_FORMAT_YUV420P10, out_size, gfx::Rect(out_size),
      out_size, base::TimeDelta());

  CHECK_EQ(libyuv::I010Rotate(
               reinterpret_cast<const uint16_t*>(
                   in_frame.visible_data(VideoFrame::Plane::kY)),
               in_frame.stride(VideoFrame::Plane::kY) / 2,
               reinterpret_cast<const uint16_t*>(
                   in_frame.visible_data(VideoFrame::Plane::kU)),
               in_frame.stride(VideoFrame::Plane::kU) / 2,
               reinterpret_cast<const uint16_t*>(
                   in_frame.visible_data(VideoFrame::Plane::kV)),
               in_frame.stride(VideoFrame::Plane::kV) / 2,
               reinterpret_cast<uint16_t*>(
                   out_frame->GetWritableVisibleData(VideoFrame::Plane::kY)),
               out_frame->stride(VideoFrame::Plane::kY) / 2,
               reinterpret_cast<uint16_t*>(
                   out_frame->GetWritableVisibleData(VideoFrame::Plane::kU)),
               out_frame->stride(VideoFrame::Plane::kU) / 2,
               reinterpret_cast<uint16_t*>(
                   out_frame->GetWritableVisibleData(VideoFrame::Plane::kV)),
               out_frame->stride(VideoFrame::Plane::kV) / 2,
               in_frame.visible_rect().width(),
               in_frame.visible_rect().height(), rotation),
           kLibYUVSuccess);

  return out_frame;
}

// Convenience function for handling multi-step LibYUV conversions with pivots.
scoped_refptr<VideoFrame> ProcessFrameLibyuv(scoped_refptr<VideoFrame> in_frame,
                                             uint32_t in_fourcc,
                                             const gfx::Size& in_size,
                                             uint32_t out_fourcc,
                                             gfx::Size out_size,
                                             gfx::OverlayTransform transform) {
  libyuv::RotationMode rotation;
  switch (transform) {
    case gfx::OVERLAY_TRANSFORM_NONE:
      rotation = libyuv::kRotate0;
      break;
    case gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90:
      rotation = libyuv::kRotate90;
      break;
    case gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180:
      rotation = libyuv::kRotate180;
      break;
    case gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270:
      rotation = libyuv::kRotate270;
      break;
    default:
      NOTREACHED() << "Invalid overlay transform: " << transform;
      return nullptr;
  }

  // Assemble a graph of the available LibYUV conversion functions.
  std::unordered_multimap<
      uint32_t, std::pair<base::RepeatingCallback<scoped_refptr<VideoFrame>(
                              const VideoFrame&)>,
                          FrameState>>
      frame_process_graph = {
          {V4L2_PIX_FMT_MM21,
           std::make_pair(base::BindRepeating(&ConvMM21ToI420),
                          FrameState(V4L2_PIX_FMT_YUV420, false, false))},
          {V4L2_PIX_FMT_MT2T,
           std::make_pair(base::BindRepeating(&ConvMT2TToP010),
                          FrameState(V4L2_PIX_FMT_P010, false, false))},
          {V4L2_PIX_FMT_P010,
           std::make_pair(base::BindRepeating(&ConvP010ToI010),
                          FrameState(V4L2_PIX_FMT_I010, false, false))},
          {V4L2_PIX_FMT_YUV420,
           std::make_pair(base::BindRepeating(&ConvI420ToARGB),
                          FrameState(V4L2_PIX_FMT_ARGB32, false, false))},
          {V4L2_PIX_FMT_I010,
           std::make_pair(base::BindRepeating(&ConvI010ToAR30),
                          FrameState(V4L2_PIX_FMT_ARGB2101010, false, false))},
          {V4L2_PIX_FMT_YUV420,
           std::make_pair(base::BindRepeating(&ScaleI420, &out_size),
                          FrameState(V4L2_PIX_FMT_YUV420, true, false))},
          {V4L2_PIX_FMT_I010,
           std::make_pair(base::BindRepeating(&ScaleI010, &out_size),
                          FrameState(V4L2_PIX_FMT_I010, true, false))},
          {V4L2_PIX_FMT_YUV420,
           std::make_pair(base::BindRepeating(&RotateI420, rotation, &out_size),
                          FrameState(V4L2_PIX_FMT_YUV420, false, true))},
          {V4L2_PIX_FMT_I010,
           std::make_pair(base::BindRepeating(&RotateI010, rotation, &out_size),
                          FrameState(V4L2_PIX_FMT_I010, false, true))}};

  FrameState target_state = {out_fourcc, in_size != out_size,
                             rotation != libyuv::kRotate0};
  std::vector<
      base::RepeatingCallback<scoped_refptr<VideoFrame>(const VideoFrame&)>>
      path;
  bool found_path = false;

  // Simple BFS implementation for finding the minimal number of processing
  // steps for a given frame. Some of these conversions are not completely
  // lossless, so we want to minimize distortion.
  std::vector<FrameState> frame_states = {FrameState(in_fourcc, false)};
  std::unordered_set<FrameState> seen_states;
  std::vector<std::vector<
      base::RepeatingCallback<scoped_refptr<VideoFrame>(const VideoFrame&)>>>
      paths = {{}};
  constexpr int kMaxPivots = 10;
  int search_radius;
  for (search_radius = 0; search_radius < kMaxPivots; search_radius++) {
    if (found_path) {
      break;
    }

    std::vector<FrameState> updated_states;
    std::vector<std::vector<
        base::RepeatingCallback<scoped_refptr<VideoFrame>(const VideoFrame&)>>>
        updated_paths;

    for (size_t i = 0; i < frame_states.size(); i++) {
      if (found_path) {
        break;
      }

      const auto& curr_state = frame_states[i];
      const auto& curr_path = paths[i];
      auto range = frame_process_graph.equal_range(curr_state.fourcc);
      for (auto itr = range.first; itr != range.second; itr++) {
        FrameState candidate_state(
            itr->second.second.fourcc,
            itr->second.second.is_scaled | curr_state.is_scaled,
            itr->second.second.is_rotated | curr_state.is_rotated);
        if (seen_states.contains(candidate_state)) {
          continue;
        }

        seen_states.insert(candidate_state);
        updated_states.push_back(candidate_state);
        updated_paths.push_back(curr_path);
        updated_paths.back().push_back(itr->second.first);

        if (candidate_state == target_state) {
          path = updated_paths.back();
          found_path = true;
          break;
        }
      }
    }

    frame_states = std::move(updated_states);
    paths = std::move(updated_paths);
  }
  CHECK_NE(search_radius, kMaxPivots);

  auto frame = in_frame;
  if (rotation == libyuv::kRotate90 || rotation == libyuv::kRotate270) {
    out_size.Transpose();
  }
  for (auto& process : path) {
    frame = process.Run(*frame);
  }

  return frame;
}

void InitWithImage(const uint8_t* img_data,
                   const gfx::Size size,
                   uint8_t* y_plane,
                   size_t y_stride,
                   uint8_t* uv_plane,
                   size_t uv_stride) {
  libyuv::NV12Copy(img_data, size.width(), img_data + size.GetArea(),
                   size.width(), y_plane, y_stride, uv_plane, uv_stride,
                   size.width(), size.height());
}

void InitWithRandom(const gfx::Size size,
                    uint8_t* y_plane,
                    size_t y_stride,
                    uint8_t* uv_plane,
                    size_t uv_stride) {
  base::span<uint8_t> y_plane_span = UNSAFE_TODO(base::span(
      y_plane, y_stride * base::checked_cast<size_t>(size.height())));
  base::span<uint8_t> uv_plane_span = UNSAFE_TODO(base::span(
      uv_plane, uv_stride * base::checked_cast<size_t>(size.height()) / 2u));
  const auto width = base::checked_cast<size_t>(size.width());

  for (int row = 0; row < size.height(); row++) {
    auto [row_bytes, rem] = y_plane_span.split_at(y_stride);
    base::RandBytes(row_bytes.first(width));
    y_plane_span = rem;
  }
  for (int row = 0; row < size.height() / 2; row++) {
    auto [row_bytes, rem] = uv_plane_span.split_at(uv_stride);
    base::RandBytes(row_bytes.first(width));
    uv_plane_span = rem;
  }
}

struct VulkanOverlayAdaptorTestParam {
  TiledImageFormat tiling;
  gfx::Size size;
  gfx::OverlayTransform transform;
};

class VulkanOverlayAdaptorTest
    : public testing::Test,
      public testing::WithParamInterface<VulkanOverlayAdaptorTestParam> {
 public:
  VulkanOverlayAdaptorTest();
  ~VulkanOverlayAdaptorTest() = default;

  struct PrintToStringParamName {
    template <class ParamType>
    std::string operator()(
        const testing::TestParamInfo<ParamType>& info) const {
      std::string ret = info.param.tiling == kMM21 ? "MM21" : "MT2T";
      ret += "_" + info.param.size.ToString();
      switch (info.param.transform) {
        case gfx::OVERLAY_TRANSFORM_NONE:
          ret += "_rot0";
          break;
        case gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90:
          ret += "_rot90";
          break;
        case gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180:
          ret += "_rot180";
          break;
        case gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270:
          ret += "_rot270";
          break;
        default:
          NOTREACHED() << "Invalid transform: " << info.param.transform;
      }
      return ret;
    }
  };

  void ProcessMailboxes(gpu::Mailbox in_mailbox,
                        const gfx::Size& size,
                        gpu::Mailbox out_mailbox,
                        const gfx::RectF& display_rect,
                        const gfx::RectF& crop_rect,
                        gfx::OverlayTransform transform,
                        VulkanOverlayAdaptor& processor);

  scoped_refptr<VideoFrame> CreateVideoFrame(
      gpu::Mailbox mailbox,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      base::OnceCallback<void(gfx::Size, uint8_t*, size_t, uint8_t*, size_t)>
          frame_init_cb,
      bool is_10bit);

  scoped_refptr<VideoFrame> CreateFramebuffer(gpu::Mailbox mailbox,
                                              const gfx::Size& coded_size,
                                              bool is_10bit);

 private:
  scoped_refptr<gl::GLShareGroup> share_group_;
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gpu::SharedContextState> context_state_;
  gpu::SharedImageManager shared_image_manager_;
  gpu::GpuPreferences gpu_preferences_;
  gpu::GpuDriverBugWorkarounds gpu_workarounds_;
  gpu::GpuFeatureInfo gpu_info_;
  std::unique_ptr<gpu::SharedImageFactory> shared_image_factory_;
};

VulkanOverlayAdaptorTest::VulkanOverlayAdaptorTest()
    : share_group_(base::MakeRefCounted<gl::GLShareGroup>()),
      surface_(gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplay(),
                                                  gfx::Size())),
      context_(gl::init::CreateGLContext(share_group_.get(),
                                         surface_.get(),
                                         gl::GLContextAttribs())) {
  context_->MakeCurrent(surface_.get());
  context_state_ = base::MakeRefCounted<gpu::SharedContextState>(
      share_group_, surface_, context_, false, base::DoNothing(),
      gpu::GpuPreferences().gr_context_type);
  shared_image_factory_ = std::make_unique<gpu::SharedImageFactory>(
      gpu_preferences_, gpu_workarounds_, gpu_info_, context_state_.get(),
      &shared_image_manager_, nullptr, false);
}

void VulkanOverlayAdaptorTest::ProcessMailboxes(
    gpu::Mailbox in_mailbox,
    const gfx::Size& size,
    gpu::Mailbox out_mailbox,
    const gfx::RectF& display_rect,
    const gfx::RectF& crop_rect,
    gfx::OverlayTransform transform,
    VulkanOverlayAdaptor& processor) {
  auto in_vulkan_representation = shared_image_manager_.ProduceVulkan(
      in_mailbox, nullptr, processor.GetVulkanDeviceQueue(),
      processor.GetVulkanImplementation(), /*needs_detiling=*/true);
  auto out_vulkan_representation = shared_image_manager_.ProduceVulkan(
      out_mailbox, nullptr, processor.GetVulkanDeviceQueue(),
      processor.GetVulkanImplementation(), /*needs_detiling=*/true);
  {
    std::vector<VkSemaphore> begin_semaphores;
    std::vector<VkSemaphore> end_semaphores;
    auto in_access = in_vulkan_representation->BeginScopedAccess(
        gpu::RepresentationAccessMode::kRead, begin_semaphores, end_semaphores);
    auto out_access = out_vulkan_representation->BeginScopedAccess(
        gpu::RepresentationAccessMode::kWrite, begin_semaphores,
        end_semaphores);

    processor.Process(in_access->GetVulkanImage(), size,
                      out_access->GetVulkanImage(), display_rect, crop_rect,
                      transform, begin_semaphores, end_semaphores);
  }
}

scoped_refptr<VideoFrame> VulkanOverlayAdaptorTest::CreateVideoFrame(
    gpu::Mailbox mailbox,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    base::OnceCallback<
        void(const gfx::Size, uint8_t*, size_t, uint8_t*, size_t)>
        frame_init_cb,
    bool is_10bit) {
  constexpr base::TimeDelta kNullTimestamp;
  const size_t bpp_numerator = is_10bit ? 5 : 1;
  const size_t bpp_denom = is_10bit ? 4 : 1;
  const gfx::Size alloc_size(
      base::bits::AlignUp(base::checked_cast<size_t>(coded_size.width()),
                          kMM21TileWidth),
      base::bits::AlignUp(base::checked_cast<size_t>(coded_size.height()),
                          kMM21TileHeight) *
          bpp_numerator / bpp_denom);

  scoped_refptr<VideoFrame> frame = CreateGpuMemoryBufferVideoFrame(
      VideoPixelFormat::PIXEL_FORMAT_NV12, alloc_size, visible_rect, alloc_size,
      kNullTimestamp, gfx::BufferUsage::SCANOUT_CPU_READ_WRITE);

  std::unique_ptr<VideoFrameMapper> frame_mapper =
      VideoFrameMapperFactory::CreateMapper(
          VideoPixelFormat::PIXEL_FORMAT_NV12,
          VideoFrame::STORAGE_GPU_MEMORY_BUFFER,
          /*force_linear_buffer_mapper=*/true);
  scoped_refptr<VideoFrame> mapped_frame =
      frame_mapper->Map(frame, PROT_READ | PROT_WRITE);

  std::move(frame_init_cb)
      .Run(alloc_size,
           mapped_frame->GetWritableVisibleData(VideoFrame::Plane::kY),
           mapped_frame->stride(VideoFrame::Plane::kY),
           mapped_frame->GetWritableVisibleData(VideoFrame::Plane::kUV),
           mapped_frame->stride(VideoFrame::Plane::kUV));

  auto gmb = CreateGpuMemoryBufferHandle(frame.get());
  viz::SharedImageFormat format_nv12 = viz::SharedImageFormat::MultiPlane(
      viz::SharedImageFormat::PlaneConfig::kY_UV,
      viz::SharedImageFormat::Subsampling::k420,
      viz::SharedImageFormat::ChannelFormat::k8);
  format_nv12.SetPrefersExternalSampler();
  shared_image_factory_->CreateSharedImage(
      mailbox, format_nv12, frame->coded_size(), gfx::ColorSpace::CreateSRGB(),
      kTopLeft_GrSurfaceOrigin, kOpaque_SkAlphaType,
      gpu::SharedImageUsage::SHARED_IMAGE_USAGE_DISPLAY_READ, "TestLabel",
      std::move(gmb));

  return mapped_frame;
}

scoped_refptr<VideoFrame> VulkanOverlayAdaptorTest::CreateFramebuffer(
    gpu::Mailbox mailbox,
    const gfx::Size& coded_size,
    bool is_10bit) {
  constexpr base::TimeDelta kNullTimestamp;

  scoped_refptr<VideoFrame> frame = CreateGpuMemoryBufferVideoFrame(
      is_10bit ? VideoPixelFormat::PIXEL_FORMAT_XR30
               : VideoPixelFormat::PIXEL_FORMAT_ARGB,
      coded_size, gfx::Rect(coded_size), coded_size, kNullTimestamp,
      gfx::BufferUsage::SCANOUT_CPU_READ_WRITE);

  auto gmb = CreateGpuMemoryBufferHandle(frame.get());
  shared_image_factory_->CreateSharedImage(
      mailbox,
      is_10bit ? viz::SinglePlaneFormat::kBGRA_1010102
               : viz::SinglePlaneFormat::kBGRA_8888,
      coded_size, gfx::ColorSpace::CreateSRGB(), kTopLeft_GrSurfaceOrigin,
      kUnpremul_SkAlphaType,
      gpu::SharedImageUsage::SHARED_IMAGE_USAGE_DISPLAY_WRITE |
          gpu::SharedImageUsage::SHARED_IMAGE_USAGE_SCANOUT,
      "TestLabel", std::move(gmb));

  std::unique_ptr<VideoFrameMapper> frame_mapper =
      VideoFrameMapperFactory::CreateMapper(
          is_10bit ? VideoPixelFormat::PIXEL_FORMAT_XR30
                   : VideoPixelFormat::PIXEL_FORMAT_ARGB,
          VideoFrame::STORAGE_GPU_MEMORY_BUFFER,
          /*force_linear_buffer_mapper=*/true);
  scoped_refptr<VideoFrame> mapped_frame =
      frame_mapper->Map(frame, PROT_READ | PROT_WRITE);

  return mapped_frame;
}

TEST_P(VulkanOverlayAdaptorTest, Correctness) {
  bool is_10bit = GetParam().tiling == kMT2T;
  const gfx::Size& output_size = GetParam().size;
  const gfx::OverlayTransform transform = GetParam().transform;

  auto in_mailbox = gpu::Mailbox::Generate();
  auto out_mailbox = gpu::Mailbox::Generate();

  test::Image image(media::g_source_directory.Append(
      base::FilePath(is_10bit ? kMT2TImage : kMM21Image)));
  ASSERT_TRUE(image.Load());
  gfx::Size size(
      base::bits::AlignUp(base::checked_cast<size_t>(image.Size().width()),
                          kMM21TileWidth),
      base::bits::AlignUp(base::checked_cast<size_t>(image.Size().height()),
                          kMM21TileHeight));
  auto init_cb = base::BindOnce(&InitWithImage, image.Data());
  auto in_frame =
      CreateVideoFrame(in_mailbox, image.Size(), image.VisibleRect(),
                       std::move(init_cb), is_10bit);

  auto out_frame = CreateFramebuffer(out_mailbox, output_size, is_10bit);

  auto vulkan_overlay_adaptor =
      VulkanOverlayAdaptor::Create(/*is_protected=*/false, GetParam().tiling);

  bool performed_cleanup = false;
  auto fence_helper =
      vulkan_overlay_adaptor->GetVulkanDeviceQueue()->GetFenceHelper();
  fence_helper->EnqueueCleanupTaskForSubmittedWork(base::BindOnce(
      [](bool* cleanup_flag, gpu::VulkanDeviceQueue* device_queue,
         bool device_lost) { *cleanup_flag = true; },
      &performed_cleanup));
  auto cleanup_fence = fence_helper->GenerateCleanupFence();
  fence_helper->Wait(cleanup_fence, UINT64_MAX);

  ProcessMailboxes(in_mailbox, image.VisibleRect().size(), out_mailbox,
                   gfx::RectF(base::checked_cast<float>(output_size.width()),
                              base::checked_cast<float>(output_size.height())),
                   gfx::RectF(1.0f, 1.0f), transform, *vulkan_overlay_adaptor);
  ASSERT_TRUE(performed_cleanup);

  // This implicitly waits for all semaphores to signal.
  vulkan_overlay_adaptor->GetVulkanDeviceQueue()
      ->GetFenceHelper()
      ->PerformImmediateCleanup();

  const uint32_t in_fourcc = is_10bit ? V4L2_PIX_FMT_MT2T : V4L2_PIX_FMT_MM21;
  const uint32_t out_fourcc =
      is_10bit ? V4L2_PIX_FMT_ARGB2101010 : V4L2_PIX_FMT_ARGB32;
  // TODO(b/328227651): We have to keep the 10-bit PSNR threshold pretty low
  // because LibYUV produces inaccurate results in the 10-bit YUV->ARGB
  // conversion. We should try to fix this discrepancy though.
  const double psnr_threshold = is_10bit ? 25.0 : 35.0;
  double psnr = 0.0;
  // GPU memory buffers have some unusual stride requirements that don't play
  // nicely with the libyuv implementation of some functions. So instead of
  // using the regular input VideoFrame, we create a new one that just wraps the
  // raw, packed image data.
  auto packed_in_frame = VideoFrame::WrapExternalData(
      VideoPixelFormat::PIXEL_FORMAT_NV12, in_frame->coded_size(),
      in_frame->visible_rect(), in_frame->coded_size(), image.Data(),
      in_frame->coded_size().GetArea() * 3 / 2, base::TimeDelta());

  auto libyuv_out_frame =
      ProcessFrameLibyuv(packed_in_frame, in_fourcc, image.Size(), out_fourcc,
                         output_size, transform);
  if (is_10bit) {
    psnr = test::ComputeAR30PSNR(
        reinterpret_cast<const uint32_t*>(
            out_frame->visible_data(VideoFrame::Plane::kARGB)),
        out_frame->stride(VideoFrame::Plane::kARGB) / 4,
        reinterpret_cast<const uint32_t*>(
            libyuv_out_frame->visible_data(VideoFrame::Plane::kARGB)),
        libyuv_out_frame->stride(VideoFrame::Plane::kARGB) / 4,
        output_size.width(), output_size.height());
  } else {
    psnr = libyuv::CalcFramePsnr(
        out_frame->visible_data(VideoFrame::Plane::kARGB),
        out_frame->stride(VideoFrame::Plane::kARGB),
        libyuv_out_frame->visible_data(VideoFrame::Plane::kARGB),
        libyuv_out_frame->stride(VideoFrame::Plane::kARGB), output_size.width(),
        output_size.height());
  }
  ASSERT_TRUE(psnr >= psnr_threshold);
}

TEST_P(VulkanOverlayAdaptorTest, Performance) {
  constexpr size_t kNumberOfTestFrames = 10;
  constexpr size_t kNumberOfTestCycles = 200;

  const bool is_10bit = GetParam().tiling == kMT2T;
  const gfx::Size& test_image_size = GetParam().size;
  const gfx::OverlayTransform transform = GetParam().transform;
  gfx::Size output_size = test_image_size;
  if (transform == gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90 ||
      transform == gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270) {
    output_size.Transpose();
  }

  gfx::Size test_coded_size(
      base::bits::AlignUp(base::checked_cast<size_t>(test_image_size.width()),
                          kMM21TileWidth),
      base::bits::AlignUp(base::checked_cast<size_t>(test_image_size.height()),
                          kMM21TileHeight));
  std::array<gpu::Mailbox, kNumberOfTestFrames> in_mailboxes;
  std::array<scoped_refptr<VideoFrame>, kNumberOfTestFrames> in_frames;
  std::array<gpu::Mailbox, kNumberOfTestFrames> out_mailboxes;
  std::array<scoped_refptr<VideoFrame>, kNumberOfTestFrames> out_frames;

  for (size_t i = 0; i < kNumberOfTestFrames; i++) {
    auto init_cb = base::BindOnce(&InitWithRandom);
    in_mailboxes[i] = gpu::Mailbox::Generate();
    in_frames[i] = CreateVideoFrame(in_mailboxes[i], test_coded_size,
                                    gfx::Rect(test_coded_size),
                                    std::move(init_cb), is_10bit);

    out_mailboxes[i] = gpu::Mailbox::Generate();
    out_frames[i] = CreateFramebuffer(out_mailboxes[i], output_size, is_10bit);
  }

  auto vulkan_overlay_adaptor =
      VulkanOverlayAdaptor::Create(/*is_protected=*/false, GetParam().tiling);

  auto start_time = base::TimeTicks::Now();
  for (size_t i = 0; i < kNumberOfTestCycles; i++) {
    ProcessMailboxes(
        in_mailboxes[i % kNumberOfTestFrames], test_image_size,
        out_mailboxes[i % kNumberOfTestFrames],
        gfx::RectF(base::checked_cast<float>(output_size.width()),
                   base::checked_cast<float>(output_size.height())),
        gfx::RectF(1.0f, 1.0f), transform, *vulkan_overlay_adaptor);
  }
  auto end_time = base::TimeTicks::Now();

  base::TimeDelta delta_time = end_time - start_time;
  const double fps = (kNumberOfTestCycles / delta_time.InSecondsF());
  WriteJsonResult({{"FramesDecoded", kNumberOfTestCycles},
                   {"TotalDurationMs", delta_time.InMicrosecondsF()},
                   {"FramesPerSecond", fps}});
}

INSTANTIATE_TEST_SUITE_P(
    ,
    VulkanOverlayAdaptorTest,
    testing::Values(
        VulkanOverlayAdaptorTestParam({kMM21, gfx::Size(320, 240),
                                       gfx::OVERLAY_TRANSFORM_NONE}),
        VulkanOverlayAdaptorTestParam({kMM21, gfx::Size(1280, 720),
                                       gfx::OVERLAY_TRANSFORM_NONE}),
        VulkanOverlayAdaptorTestParam({kMM21, gfx::Size(1920, 1080),
                                       gfx::OVERLAY_TRANSFORM_NONE}),
        VulkanOverlayAdaptorTestParam(
            {kMM21, gfx::Size(240, 320),
             gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90}),
        VulkanOverlayAdaptorTestParam(
            {kMM21, gfx::Size(720, 1280),
             gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90}),
        VulkanOverlayAdaptorTestParam(
            {kMM21, gfx::Size(1080, 1920),
             gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90}),
        VulkanOverlayAdaptorTestParam(
            {kMM21, gfx::Size(320, 240),
             gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180}),
        VulkanOverlayAdaptorTestParam(
            {kMM21, gfx::Size(1280, 720),
             gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180}),
        VulkanOverlayAdaptorTestParam(
            {kMM21, gfx::Size(1920, 1080),
             gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180}),
        VulkanOverlayAdaptorTestParam(
            {kMM21, gfx::Size(240, 320),
             gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270}),
        VulkanOverlayAdaptorTestParam(
            {kMM21, gfx::Size(720, 1280),
             gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270}),
        VulkanOverlayAdaptorTestParam(
            {kMM21, gfx::Size(1080, 1920),
             gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270}),
        VulkanOverlayAdaptorTestParam({kMT2T, gfx::Size(320, 240),
                                       gfx::OVERLAY_TRANSFORM_NONE}),
        VulkanOverlayAdaptorTestParam({kMT2T, gfx::Size(1280, 720),
                                       gfx::OVERLAY_TRANSFORM_NONE}),
        VulkanOverlayAdaptorTestParam({kMT2T, gfx::Size(1920, 1080),
                                       gfx::OVERLAY_TRANSFORM_NONE}),
        VulkanOverlayAdaptorTestParam(
            {kMT2T, gfx::Size(240, 320),
             gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90}),
        VulkanOverlayAdaptorTestParam(
            {kMT2T, gfx::Size(720, 1280),
             gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90}),
        VulkanOverlayAdaptorTestParam(
            {kMT2T, gfx::Size(1080, 1920),
             gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90}),
        VulkanOverlayAdaptorTestParam(
            {kMT2T, gfx::Size(320, 240),
             gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180}),
        VulkanOverlayAdaptorTestParam(
            {kMT2T, gfx::Size(1280, 720),
             gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180}),
        VulkanOverlayAdaptorTestParam(
            {kMT2T, gfx::Size(1920, 1080),
             gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180}),
        VulkanOverlayAdaptorTestParam(
            {kMT2T, gfx::Size(240, 320),
             gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270}),
        VulkanOverlayAdaptorTestParam(
            {kMT2T, gfx::Size(720, 1280),
             gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270}),
        VulkanOverlayAdaptorTestParam(
            {kMT2T, gfx::Size(1080, 1920),
             gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270})),
    VulkanOverlayAdaptorTest::PrintToStringParamName());

}  // namespace
}  // namespace media

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

  // Print the help message if requested. This needs to be done before
  // initializing gtest, to overwrite the default gtest help message.
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  LOG_ASSERT(cmd_line);
  if (cmd_line->HasSwitch("help")) {
    std::cout << media::usage_msg << "\n" << media::help_msg;
    return 0;
  }

  base::CommandLine::SwitchMap switches = cmd_line->GetSwitches();
  for (base::CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    if (it->first.find("gtest_") == 0 ||               // Handled by GoogleTest
        it->first == "v" || it->first == "vmodule") {  // Handled by Chrome
      continue;
    }

    if (it->first == "source_directory") {
      media::g_source_directory = base::FilePath(it->second);
    } else if (it->first == "output_directory") {
      media::g_output_directory = base::FilePath(it->second);
    } else {
      std::cout << "unknown option: --" << it->first << "\n"
                << media::usage_msg;
      return EXIT_FAILURE;
    }
  }
  srand(media::kRandomFrameSeed);
  testing::InitGoogleTest(&argc, argv);

  auto* const test_environment = new media::test::VideoTestEnvironment;
  media::g_env = reinterpret_cast<media::test::VideoTestEnvironment*>(
      testing::AddGlobalTestEnvironment(test_environment));

  // TODO(b/316374371) Try to remove Ozone and replace with EGL and GL.
  ui::OzonePlatform::InitParams ozone_param;
  ozone_param.single_process = true;
  ui::OzonePlatform::InitializeForUI(ozone_param);
  ui::OzonePlatform::InitializeForGPU(ozone_param);
  gl::GLSurfaceTestSupport::InitializeOneOffImplementation(
      gl::GLImplementationParts(gl::kGLImplementationEGLGLES2));

  return RUN_ALL_TESTS();
}
