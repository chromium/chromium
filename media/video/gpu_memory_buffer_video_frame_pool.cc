// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/video/gpu_memory_buffer_video_frame_pool.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <stddef.h>
#include <stdint.h>

#include <atomic>
#include <list>
#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bits.h"
#include "base/command_line.h"
#include "base/containers/circular_deque.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_switches.h"
#include "media/base/media_switches.h"
#include "media/base/video_types.h"
#include "media/base/video_util.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gl/trace_util.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "media/base/mac/video_frame_mac.h"
#endif

namespace media {

namespace {

}  // namespace

// Implementation of a pool of mappable shared images(MappableSI) used to back
// VideoFrames.
class GpuMemoryBufferVideoFramePool::PoolImpl
    : public base::RefCountedThreadSafe<
          GpuMemoryBufferVideoFramePool::PoolImpl>,
      public base::trace_event::MemoryDumpProvider {
 public:
  // |media_task_runner| is the media task runner associated with the
  // GL context provided by |gpu_factories|
  // |worker_task_runner| is a task runner used to asynchronously copy
  // video frame's planes.
  // |gpu_factories| is an interface to GPU related operation and can be
  // null if a GL context is not available.
  PoolImpl(const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
           const scoped_refptr<base::TaskRunner>& worker_task_runner,
           GpuVideoAcceleratorFactories* const gpu_factories)
      : media_task_runner_(media_task_runner),
        worker_task_runner_(worker_task_runner),
        gpu_factories_(gpu_factories),
        output_format_(GpuVideoAcceleratorFactories::OutputFormat::UNDEFINED),
        tick_clock_(base::DefaultTickClock::GetInstance()) {
    DCHECK(media_task_runner_);
    DCHECK(worker_task_runner_);

    // Using a static atomic id generator to generate a unique id for each
    // GpuMemoryBufferVideoFramePool in a thread safe manner.
    static std::atomic_uint32_t id = 0;
    pool_id_ = ++id;
  }

  PoolImpl(const PoolImpl&) = delete;
  PoolImpl& operator=(const PoolImpl&) = delete;

  // Takes a software VideoFrame and calls |frame_ready_cb| with a VideoFrame
  // backed by native textures if possible.
  // The data contained in |video_frame| is copied into the returned frame
  // asynchronously posting tasks to |worker_task_runner_|, while
  // |frame_ready_cb| will be called on |media_task_runner_| once all the data
  // has been copied.
  void CreateHardwareFrame(scoped_refptr<VideoFrame> video_frame,
                           FrameReadyCB cb);

  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  // Aborts any pending copies.
  void Abort();

  // Shuts down the frame pool and releases all frames in |frames_|.
  // Once this is called frames will no longer be inserted back into
  // |frames_|.
  void Shutdown();

  void SetTickClockForTesting(const base::TickClock* tick_clock);

 private:
  friend class base::RefCountedThreadSafe<
      GpuMemoryBufferVideoFramePool::PoolImpl>;
  ~PoolImpl() override;

  // Resource needed to compose a frame.
  // TODO(dalecurtis): The method of use marking used is very brittle
  // and prone to leakage. Switch this to pass around std::unique_ptr
  // such that callers own resource explicitly.
  struct FrameResource {
    explicit FrameResource(const gfx::Size& size, gfx::BufferUsage usage)
        : size(size), usage(usage) {}
    void MarkUsed() {
      is_used_ = true;
      last_use_time_ = base::TimeTicks();
    }
    void MarkUnused(base::TimeTicks last_use_time) {
      is_used_ = false;
      last_use_time_ = last_use_time;
    }
    bool is_used() const { return is_used_; }
    base::TimeTicks last_use_time() const { return last_use_time_; }

    const gfx::Size size;
    const gfx::BufferUsage usage;

    int32_t buffer_id = -1;
    scoped_refptr<gpu::ClientSharedImage> shared_image;

    // Currently when MappableSI is used to represent the resource,
    // Map() and UnMap() happens in different methods and are not scoped in the
    // same method. With MappableSI, keeping the |scoped_mapping| will allow to
    // Map() and reset(UnMap()) it as needed to achieve same behavior.
    std::unique_ptr<gpu::ClientSharedImage::ScopedMapping> scoped_mapping;

    // The sync token used to recycle or destroy the resource. It is set when
    // resource is returned from the VideoFrame (via MailboxHolderReleased).
    gpu::SyncToken sync_token;

   private:
    bool is_used_ = true;
    base::TimeTicks last_use_time_;
  };

  // Struct to keep track of requested VideoFrame copies.
  struct VideoFrameCopyRequest {
    VideoFrameCopyRequest(scoped_refptr<VideoFrame> video_frame,
                          FrameReadyCB frame_ready_cb,
                          bool passthrough)
        : video_frame(std::move(video_frame)),
          frame_ready_cb(std::move(frame_ready_cb)),
          passthrough(passthrough) {}
    scoped_refptr<VideoFrame> video_frame;
    FrameReadyCB frame_ready_cb;
    bool passthrough;
  };

  // Start the copy of a video_frame on the worker_task_runner_.
  // It assumes there are currently no in-flight copies and works on the request
  // in the front of |frame_copy_requests_| queue.
  void StartCopy();

  // Copy |video_frame| data into |frame_resource| and calls |frame_ready_cb|
  // when done.
  void CopyVideoFrameToGpuMemoryBuffer(scoped_refptr<VideoFrame> video_frame,
                                       FrameResource* frame_resource);

  // Called when all the data has been copied.
  void OnCopiesDone(bool copy_failed,
                    scoped_refptr<VideoFrame> video_frame,
                    FrameResource* frame_resource);

  // Called on the media thread when all data has been copied.
  void OnCopiesDoneOnMediaThread(bool copy_failed,
                                 scoped_refptr<VideoFrame> video_frame,
                                 FrameResource* frame_resource);

  static void CopyRowsToBuffer(
      GpuVideoAcceleratorFactories::OutputFormat output_format,
      const size_t row,
      const size_t rows_to_copy,
      const gfx::Size coded_size,
      const VideoFrame* video_frame,
      FrameResource* frame_resource,
      base::OnceClosure done);
  // Prepares a shared image mailbox and allocates the new VideoFrame. This has
  // to be run on `media_task_runner_`. On failure, this will release
  // `frame_resource` and return nullptr.
  scoped_refptr<VideoFrame> BindAndCreateMailboxHardwareFrameResource(
      FrameResource* frame_resource,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      const gfx::ColorSpace& color_space,
      base::TimeDelta timestamp,
      bool video_frame_allow_overlay,
      const std::optional<gpu::VulkanYCbCrInfo>& ycbcr_info);

  // Return true if |resource| can be used to represent a frame for
  // specific |format| and |size|.
  static bool IsFrameResourceCompatible(const FrameResource* resource,
                                        const gfx::Size& size,
                                        gfx::BufferUsage usage) {
    return size == resource->size && usage == resource->usage;
  }

  // Get the resource needed for a frame out of the pool, or create it if
  // necessary.
  // This also drops the LRU resource that can't be reuse for this frame.
  FrameResource* GetOrCreateFrameResource(const gfx::Size& size,
                                          gfx::BufferUsage usage,
                                          const gfx::ColorSpace& color_space);

  // Calls the FrameReadyCB of the first entry in |frame_copy_requests_|, with
  // the provided |video_frame|, then deletes the entry from
  // |frame_copy_requests_| and attempts to start another copy if there are
  // other |frame_copy_requests_| elements.
  void CompleteCopyRequestAndMaybeStartNextCopy(
      scoped_refptr<VideoFrame> video_frame);

  // Callback called when a VideoFrame generated with GetOrCreateFrameResource
  // is no longer referenced.
  void MailboxHolderReleased(FrameResource* frame_resource,
                             const gpu::SyncToken& sync_token);

  // Delete resource. This has to be called on the thread where |task_runner|
  // is current.
  static void DeleteFrameResource(
      GpuVideoAcceleratorFactories* const gpu_factories,
      FrameResource* frame_resource);

  // Task runner associated to the GL context provided by |gpu_factories_|.
  const scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
  // Task runner used to asynchronously copy planes.
  const scoped_refptr<base::TaskRunner> worker_task_runner_;

  // Interface to GPU related operations.
  const raw_ptr<GpuVideoAcceleratorFactories> gpu_factories_;

  // Pool of resources.
  std::list<raw_ptr<FrameResource, CtnExperimental>> resources_pool_;

  GpuVideoAcceleratorFactories::OutputFormat output_format_;

  // |tick_clock_| is always a DefaultTickClock outside of testing.
  raw_ptr<const base::TickClock> tick_clock_;

  // Queued up video frames for copies. The front is the currently
  // in-flight copy, new copies are added at the end.
  base::circular_deque<VideoFrameCopyRequest> frame_copy_requests_;
  bool in_shutdown_ = false;

  // Id used in ::OnMemoryDump to identify the GpuMemoryBufferVideoFramePool.
  uint32_t pool_id_ = 0;

  // Unique Id generated each time a MappableSI is created. This is
  // used to identify the shared image.
  uint32_t buffer_id_ = 0;
};

namespace {

// VideoFrame copies to MappableSI will be split in copies where the
// output size is |kBytesPerCopyTarget| bytes and run in parallel.
constexpr size_t kBytesPerCopyTarget = 1024 * 1024;  // 1MB

// Return the SharedImageFormat format to use for a specific VideoPixelFormat.
viz::SharedImageFormat OutputFormatToSharedImageFormat(
    GpuVideoAcceleratorFactories::OutputFormat format) {
  switch (format) {
    case GpuVideoAcceleratorFactories::OutputFormat::YV12:
      return viz::MultiPlaneFormat::kYV12;
    case GpuVideoAcceleratorFactories::OutputFormat::P010:
      return viz::MultiPlaneFormat::kP010;
    case GpuVideoAcceleratorFactories::OutputFormat::NV12:
      return viz::MultiPlaneFormat::kNV12;
    case GpuVideoAcceleratorFactories::OutputFormat::XR30:
      return viz::SinglePlaneFormat::kBGRA_1010102;
    case GpuVideoAcceleratorFactories::OutputFormat::XB30:
      return viz::SinglePlaneFormat::kRGBA_1010102;
    case GpuVideoAcceleratorFactories::OutputFormat::UNDEFINED:
      NOTREACHED();
  }
}

VideoPixelFormat VideoFormat(
    GpuVideoAcceleratorFactories::OutputFormat format) {
  switch (format) {
    case GpuVideoAcceleratorFactories::OutputFormat::YV12:
      return PIXEL_FORMAT_YV12;
    case GpuVideoAcceleratorFactories::OutputFormat::NV12:
      return PIXEL_FORMAT_NV12;
    case GpuVideoAcceleratorFactories::OutputFormat::P010:
      return PIXEL_FORMAT_P010LE;
    case GpuVideoAcceleratorFactories::OutputFormat::XR30:
      return PIXEL_FORMAT_XR30;
    case GpuVideoAcceleratorFactories::OutputFormat::XB30:
      return PIXEL_FORMAT_XB30;
    case GpuVideoAcceleratorFactories::OutputFormat::UNDEFINED:
      NOTREACHED();
  }
}

// The number of output rows to be copied in each iteration.
int RowsPerCopy(VideoPixelFormat format, int width) {
  int bytes_per_row = VideoFrame::RowBytes(/*plane=*/0, format, width);
  if (format == PIXEL_FORMAT_NV12) {
    bytes_per_row += VideoFrame::RowBytes(1, format, width);
  }
  // Copy an even number of lines, and at least one.
  return std::max<size_t>((kBytesPerCopyTarget / bytes_per_row) & ~1, 1);
}

void CopyRowsToI420Buffer(int first_row,
                          int rows,
                          int bytes_per_row,
                          size_t bit_depth,
                          const uint8_t* source,
                          int source_stride,
                          uint8_t* output,
                          int dest_stride) {
  TRACE_EVENT2("media", "CopyRowsToI420Buffer", "bytes_per_row", bytes_per_row,
               "rows", rows);

  if (!output)
    return;

  DCHECK_NE(dest_stride, 0);
  DCHECK_LE(bytes_per_row, std::abs(dest_stride));
  DCHECK_LE(bytes_per_row, source_stride);
  DCHECK_GE(bit_depth, 8u);

  if (bit_depth == 8) {
    libyuv::CopyPlane(source + source_stride * first_row, source_stride,
                      output + dest_stride * first_row, dest_stride,
                      bytes_per_row, rows);
  } else {
    const int scale = 0x10000 >> (bit_depth - 8);
    libyuv::Convert16To8Plane(
        reinterpret_cast<const uint16_t*>(source + source_stride * first_row),
        source_stride / 2, output + dest_stride * first_row, dest_stride, scale,
        bytes_per_row, rows);
  }
}

void CopyRowsToP010Buffer(int first_row,
                          int rows,
                          int width,
                          const VideoFrame* source_frame,
                          uint8_t* dest_y,
                          int dest_stride_y,
                          uint8_t* dest_uv,
                          int dest_stride_uv) {
  TRACE_EVENT2("media", "CopyRowsToP010Buffer", "width", width, "rows", rows);

  if (!dest_y || !dest_uv)
    return;

  DCHECK_NE(dest_stride_y, 0);
  DCHECK_NE(dest_stride_uv, 0);
  DCHECK_EQ(0, first_row % 2);
  DCHECK_EQ(source_frame->format(), PIXEL_FORMAT_YUV420P10);
  DCHECK_LE(width * 2, source_frame->stride(VideoFrame::Plane::kY));

  const uint16_t* y_plane = reinterpret_cast<const uint16_t*>(
      source_frame->visible_data(VideoFrame::Plane::kY) +
      first_row * source_frame->stride(VideoFrame::Plane::kY));
  const size_t y_plane_stride = source_frame->stride(VideoFrame::Plane::kY) / 2;
  const uint16_t* u_plane = reinterpret_cast<const uint16_t*>(
      source_frame->visible_data(VideoFrame::Plane::kU) +
      (first_row / 2) * source_frame->stride(VideoFrame::Plane::kU));
  const size_t u_plane_stride = source_frame->stride(VideoFrame::Plane::kU) / 2;
  const uint16_t* v_plane = reinterpret_cast<const uint16_t*>(
      source_frame->visible_data(VideoFrame::Plane::kV) +
      (first_row / 2) * source_frame->stride(VideoFrame::Plane::kV));
  const size_t v_plane_stride = source_frame->stride(VideoFrame::Plane::kV) / 2;

  libyuv::I010ToP010(
      y_plane, y_plane_stride, u_plane, u_plane_stride, v_plane, v_plane_stride,
      reinterpret_cast<uint16_t*>(dest_y + first_row * dest_stride_y),
      dest_stride_y / 2,
      reinterpret_cast<uint16_t*>(dest_uv + (first_row / 2) * dest_stride_uv),
      dest_stride_uv / 2, width, rows);
}

void CopyRowsToNV12Buffer(int first_row,
                          int rows,
                          int width,
                          size_t bit_depth,
                          const VideoFrame* source_frame,
                          uint8_t* dest_y,
                          int dest_stride_y,
                          uint8_t* dest_uv,
                          int dest_stride_uv) {
  TRACE_EVENT2("media", "CopyRowsToNV12Buffer", "width", width, "rows", rows);

  if (!dest_y || !dest_uv)
    return;

  DCHECK_NE(dest_stride_y, 0);
  DCHECK_NE(dest_stride_uv, 0);
  DCHECK_EQ(0, first_row % 2);
  DCHECK(source_frame->format() == PIXEL_FORMAT_I420 ||
         source_frame->format() == PIXEL_FORMAT_YV12 ||
         source_frame->format() == PIXEL_FORMAT_NV12 ||
         source_frame->format() == PIXEL_FORMAT_YUV420P10);

  if (bit_depth == 8) {
    const int rows_y =
        VideoFrame::Rows(VideoFrame::Plane::kY, PIXEL_FORMAT_NV12, rows);
    const int rows_uv =
        VideoFrame::Rows(VideoFrame::Plane::kUV, PIXEL_FORMAT_NV12, rows);
    const int bytes_per_row_y =
        VideoFrame::RowBytes(VideoFrame::Plane::kY, PIXEL_FORMAT_NV12, width);
    const int bytes_per_row_uv =
        VideoFrame::RowBytes(VideoFrame::Plane::kUV, PIXEL_FORMAT_NV12, width);
    DCHECK_LE(bytes_per_row_y, std::abs(dest_stride_y));
    DCHECK_LE(bytes_per_row_uv, std::abs(dest_stride_uv));

    if (source_frame->format() == PIXEL_FORMAT_NV12) {
      libyuv::CopyPlane(
          source_frame->visible_data(VideoFrame::Plane::kY) +
              first_row * source_frame->stride(VideoFrame::Plane::kY),
          source_frame->stride(VideoFrame::Plane::kY),
          dest_y + first_row * dest_stride_y, dest_stride_y, bytes_per_row_y,
          rows_y);
      libyuv::CopyPlane(
          source_frame->visible_data(VideoFrame::Plane::kUV) +
              first_row / 2 * source_frame->stride(VideoFrame::Plane::kUV),
          source_frame->stride(VideoFrame::Plane::kUV),
          dest_uv + first_row / 2 * dest_stride_uv, dest_stride_uv,
          bytes_per_row_uv, rows_uv);

      return;
    }

    libyuv::I420ToNV12(
        source_frame->visible_data(VideoFrame::Plane::kY) +
            first_row * source_frame->stride(VideoFrame::Plane::kY),
        source_frame->stride(VideoFrame::Plane::kY),
        source_frame->visible_data(VideoFrame::Plane::kU) +
            first_row / 2 * source_frame->stride(VideoFrame::Plane::kU),
        source_frame->stride(VideoFrame::Plane::kU),
        source_frame->visible_data(VideoFrame::Plane::kV) +
            first_row / 2 * source_frame->stride(VideoFrame::Plane::kV),
        source_frame->stride(VideoFrame::Plane::kV),
        dest_y + first_row * dest_stride_y, dest_stride_y,
        dest_uv + first_row / 2 * dest_stride_uv, dest_stride_uv,
        bytes_per_row_y, rows_y);
  } else {
    DCHECK_LE(width * 2, source_frame->stride(VideoFrame::Plane::kY));

    const uint16_t* y_plane = reinterpret_cast<const uint16_t*>(
        source_frame->visible_data(VideoFrame::Plane::kY) +
        first_row * source_frame->stride(VideoFrame::Plane::kY));
    const size_t y_plane_stride =
        source_frame->stride(VideoFrame::Plane::kY) / 2;
    const uint16_t* u_plane = reinterpret_cast<const uint16_t*>(
        source_frame->visible_data(VideoFrame::Plane::kU) +
        (first_row / 2) * source_frame->stride(VideoFrame::Plane::kU));
    const size_t u_plane_stride =
        source_frame->stride(VideoFrame::Plane::kU) / 2;
    const uint16_t* v_plane = reinterpret_cast<const uint16_t*>(
        source_frame->visible_data(VideoFrame::Plane::kV) +
        (first_row / 2) * source_frame->stride(VideoFrame::Plane::kV));
    const size_t v_plane_stride =
        source_frame->stride(VideoFrame::Plane::kV) / 2;

    libyuv::I010ToNV12(y_plane, y_plane_stride, u_plane, u_plane_stride,
                       v_plane, v_plane_stride,
                       dest_y + first_row * dest_stride_y, dest_stride_y,
                       dest_uv + (first_row / 2) * dest_stride_uv,
                       dest_stride_uv, width, rows);
  }
}

void CopyRowsToRGB10Buffer(bool is_rgba,
                           int first_row,
                           int rows,
                           int width,
                           const VideoFrame* source_frame,
                           uint8_t* output,
                           int dest_stride) {
  TRACE_EVENT2("media", "CopyRowsToRGB10Buffer", "bytes_per_row", width * 2,
               "rows", rows);
  if (!output)
    return;

  DCHECK_NE(dest_stride, 0);
  DCHECK_LE(width, std::abs(dest_stride / 2));
  DCHECK_EQ(0, first_row % 2);
  DCHECK_EQ(source_frame->format(), PIXEL_FORMAT_YUV420P10);

  const auto* y_plane = reinterpret_cast<const uint16_t*>(
      source_frame->visible_data(VideoFrame::Plane::kY) +
      first_row * source_frame->stride(VideoFrame::Plane::kY));
  const auto* u_plane = reinterpret_cast<const uint16_t*>(
      source_frame->visible_data(VideoFrame::Plane::kU) +
      first_row / 2 * source_frame->stride(VideoFrame::Plane::kU));
  const auto* v_plane = reinterpret_cast<const uint16_t*>(
      source_frame->visible_data(VideoFrame::Plane::kV) +
      first_row / 2 * source_frame->stride(VideoFrame::Plane::kV));

  size_t y_plane_stride = source_frame->stride(VideoFrame::Plane::kY) / 2;
  size_t u_plane_stride = source_frame->stride(VideoFrame::Plane::kU) / 2;
  size_t v_plane_stride = source_frame->stride(VideoFrame::Plane::kV) / 2;

  uint8_t* dest_rgb10 = output + first_row * dest_stride;

  SkYUVColorSpace yuv_cs = kRec601_Limited_SkYUVColorSpace;
  source_frame->ColorSpace().ToSkYUVColorSpace(source_frame->BitDepth(),
                                               &yuv_cs);

  // libyuv uses little-endian for RGBx formats, whereas here we use big
  // endian.
  const bool is_libyuv_abgr = is_rgba;
  const auto* matrix = GetYuvContantsForColorSpace(
      yuv_cs, /*output_argb_matrix=*/!is_libyuv_abgr);
  if (is_libyuv_abgr) {
    std::swap(u_plane, v_plane);
    std::swap(u_plane_stride, v_plane_stride);
  }

  // Note: We always use I010ToAR30Matrix() here since `matrix` and
  // parameter order is changed based on whether we need to output ARGB or ABGR.
  libyuv::I010ToAR30Matrix(y_plane, y_plane_stride, u_plane, u_plane_stride,
                           v_plane, v_plane_stride, dest_rgb10, dest_stride,
                           matrix, width, rows);
}

gfx::Size CodedSize(const VideoFrame* video_frame,
                    GpuVideoAcceleratorFactories::OutputFormat output_format) {
  DCHECK(gfx::Rect(video_frame->coded_size())
             .Contains(video_frame->visible_rect()));

  size_t width = video_frame->visible_rect().width();
  size_t height = video_frame->visible_rect().height();
  gfx::Size output;
  switch (output_format) {
    case GpuVideoAcceleratorFactories::OutputFormat::YV12:
    case GpuVideoAcceleratorFactories::OutputFormat::P010:
    case GpuVideoAcceleratorFactories::OutputFormat::NV12:
      DCHECK_EQ(video_frame->visible_rect().x() % 2, 0);
      DCHECK_EQ(video_frame->visible_rect().y() % 2, 0);
      if (!gfx::IsOddWidthMultiPlanarBuffersAllowed())
        width = base::bits::AlignUp(width, size_t{2});
      if (!gfx::IsOddHeightMultiPlanarBuffersAllowed())
        height = base::bits::AlignUp(height, size_t{2});
      output = gfx::Size(width, height);
      break;
    case GpuVideoAcceleratorFactories::OutputFormat::XR30:
    case GpuVideoAcceleratorFactories::OutputFormat::XB30:
      output = gfx::Size(base::bits::AlignUp(width, size_t{2}), height);
      break;
    case GpuVideoAcceleratorFactories::OutputFormat::UNDEFINED:
      NOTREACHED_IN_MIGRATION();
  }
  DCHECK(gfx::Rect(video_frame->coded_size()).Contains(gfx::Rect(output)));
  return output;
}

void SetPrefersExternalSampler(viz::SharedImageFormat& format) {
  if (format.is_multi_plane()) {
    // Set prefers external sampler only for multiplanar formats on ozone based
    // platforms.
#if BUILDFLAG(IS_OZONE)
    format.SetPrefersExternalSampler();
#endif
  }
}

gfx::ColorSpace GetOutputColorSpace(
    const gfx::ColorSpace& source_cs,
    GpuVideoAcceleratorFactories::OutputFormat output_format) {
  switch (output_format) {
    case GpuVideoAcceleratorFactories::OutputFormat::YV12:
    case GpuVideoAcceleratorFactories::OutputFormat::P010:
    case GpuVideoAcceleratorFactories::OutputFormat::NV12:
      // YUV formats are just repackaged without any conversion, so the color
      // space remains the same.
      return source_cs;

    case GpuVideoAcceleratorFactories::OutputFormat::XR30:
    case GpuVideoAcceleratorFactories::OutputFormat::XB30:
      // We've converted the YUV data to RGB, fix the color space.
      return source_cs.GetAsFullRangeRGB();

    case GpuVideoAcceleratorFactories::OutputFormat::UNDEFINED:
      NOTREACHED();
  }
}

}  // unnamed namespace

// Creates a VideoFrame backed by native textures starting from a software
// VideoFrame.
// The data contained in |video_frame| is copied into the VideoFrame passed to
// |frame_ready_cb|.
// This has to be called on the thread where |media_task_runner_| is current.
void GpuMemoryBufferVideoFramePool::PoolImpl::CreateHardwareFrame(
    scoped_refptr<VideoFrame> video_frame,
    FrameReadyCB frame_ready_cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  // Lazily initialize |output_format_| since VideoFrameOutputFormat() has to be
  // called on the media_thread while this object might be instantiated on any.
  const VideoPixelFormat pixel_format = video_frame->format();
  if (output_format_ == GpuVideoAcceleratorFactories::OutputFormat::UNDEFINED)
    output_format_ = gpu_factories_->VideoFrameOutputFormat(pixel_format);
  // Bail if we have a change of GpuVideoAcceleratorFactories::OutputFormat;
  // such changes should not happen in general (see https://crbug.com/875158).
  if (output_format_ != gpu_factories_->VideoFrameOutputFormat(pixel_format)) {
    std::move(frame_ready_cb).Run(std::move(video_frame));
    return;
  }

  bool is_software_backed_video_frame = !video_frame->HasSharedImage();
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  is_software_backed_video_frame &= !video_frame->HasDmaBufs();
#endif

  bool passthrough = false;
#if BUILDFLAG(IS_MAC)
  if (!IOSurfaceCanSetColorSpace(video_frame->ColorSpace()))
    passthrough = true;
#endif

  if (!video_frame->IsMappable()) {
    // Already a hardware frame.
    passthrough = true;
  }

  if (output_format_ == GpuVideoAcceleratorFactories::OutputFormat::UNDEFINED)
    passthrough = true;

  switch (pixel_format) {
    // Supported cases.
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_YUV420P10:
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV12A:
      break;
    // Unsupported cases.
    case PIXEL_FORMAT_I420A:
      // We don't have YUVA overlays and RGBA conversion at this stage
      // compromises on color and breaks some WebGL cases.
      // See https://crbug.com/355923583 and https://crbug.com/367746309
    case PIXEL_FORMAT_I422:
    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_NV21:
    case PIXEL_FORMAT_UYVY:
    case PIXEL_FORMAT_YUY2:
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_BGRA:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_RGB24:
    case PIXEL_FORMAT_MJPEG:
    case PIXEL_FORMAT_YUV422P9:
    case PIXEL_FORMAT_YUV420P9:
    case PIXEL_FORMAT_YUV444P9:
    case PIXEL_FORMAT_YUV422P10:
    case PIXEL_FORMAT_YUV444P10:
    case PIXEL_FORMAT_YUV420P12:
    case PIXEL_FORMAT_YUV422P12:
    case PIXEL_FORMAT_YUV444P12:
    case PIXEL_FORMAT_Y16:
    case PIXEL_FORMAT_ABGR:
    case PIXEL_FORMAT_XBGR:
    case PIXEL_FORMAT_NV16:
    case PIXEL_FORMAT_NV24:
    case PIXEL_FORMAT_P010LE:
    case PIXEL_FORMAT_P210LE:
    case PIXEL_FORMAT_P410LE:
    case PIXEL_FORMAT_XR30:
    case PIXEL_FORMAT_XB30:
    case PIXEL_FORMAT_RGBAF16:
    case PIXEL_FORMAT_I422A:
    case PIXEL_FORMAT_I444A:
    case PIXEL_FORMAT_YUV420AP10:
    case PIXEL_FORMAT_YUV422AP10:
    case PIXEL_FORMAT_YUV444AP10:
    case PIXEL_FORMAT_UNKNOWN:
      if (is_software_backed_video_frame) {
        UMA_HISTOGRAM_ENUMERATION(
            "Media.GpuMemoryBufferVideoFramePool.UnsupportedFormat",
            pixel_format, PIXEL_FORMAT_MAX + 1);
      }
      passthrough = true;
  }

  // TODO(crbug.com/40481128): Handle odd positioned video frame input.
  if (video_frame->visible_rect().x() % 2 ||
      video_frame->visible_rect().y() % 2) {
    passthrough = true;
  }

  // TODO(https://crbug.com/webrtc/9033): Eliminate odd size video frame input
  // cases as they are not valid.
  if (video_frame->coded_size().width() % 2 &&
      !gfx::IsOddWidthMultiPlanarBuffersAllowed()) {
    passthrough = true;
  }
  if (video_frame->coded_size().height() % 2 &&
      !gfx::IsOddHeightMultiPlanarBuffersAllowed()) {
    passthrough = true;
  }

  frame_copy_requests_.emplace_back(std::move(video_frame),
                                    std::move(frame_ready_cb), passthrough);
  if (frame_copy_requests_.size() == 1u)
    StartCopy();
}

bool GpuMemoryBufferVideoFramePool::PoolImpl::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  const int kImportance = 2;
  for (const FrameResource* frame_resource : resources_pool_) {
    scoped_refptr<gpu::ClientSharedImage> shared_image =
        frame_resource->shared_image;
    if (shared_image) {
      std::string dump_name =
          base::StringPrintf("media/video_frame_memory_%d/buffer_%d", pool_id_,
                             frame_resource->buffer_id);
      base::trace_event::MemoryAllocatorDump* dump =
          pmd->CreateAllocatorDump(dump_name);

      auto size = frame_resource->size;
      size_t buffer_size_in_bytes =
          shared_image->format().EstimatedSizeInBytes(size);

      dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                      base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                      buffer_size_in_bytes);
      dump->AddScalar("free_size",
                      base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                      frame_resource->is_used() ? 0 : buffer_size_in_bytes);
      shared_image->OnMemoryDump(pmd, dump->guid(), kImportance);
    }
  }
  return true;
}

void GpuMemoryBufferVideoFramePool::PoolImpl::Abort() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  // Abort any pending copy requests. If one is already in flight, we can't do
  // anything about it.
  if (frame_copy_requests_.size() <= 1u)
    return;
  frame_copy_requests_.erase(frame_copy_requests_.begin() + 1,
                             frame_copy_requests_.end());
}

void GpuMemoryBufferVideoFramePool::PoolImpl::OnCopiesDone(
    bool copy_failed,
    scoped_refptr<VideoFrame> video_frame,
    FrameResource* frame_resource) {
  if (!copy_failed && frame_resource->scoped_mapping) {
    frame_resource->scoped_mapping.reset();
#if BUILDFLAG(IS_MAC)
      frame_resource->shared_image->SetColorSpaceOnNativeBuffer(
          video_frame->ColorSpace());
#endif
  }

  TRACE_EVENT_NESTABLE_ASYNC_END0(
      "media", "CopyVideoFrameToGpuMemoryBuffer",
      TRACE_ID_WITH_SCOPE("CopyVideoFrameToGpuMemoryBuffer",
                          video_frame->timestamp().InNanoseconds()));

  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PoolImpl::OnCopiesDoneOnMediaThread, this, copy_failed,
                     std::move(video_frame), frame_resource));
}

void GpuMemoryBufferVideoFramePool::PoolImpl::StartCopy() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!frame_copy_requests_.empty());

  while (!frame_copy_requests_.empty()) {
    VideoFrameCopyRequest& request = frame_copy_requests_.front();

    // Some formats require conversion which may change the color space.
    auto output_color_space =
        request.passthrough
            ? request.video_frame->ColorSpace()
            : GetOutputColorSpace(request.video_frame->ColorSpace(),
                                  output_format_);

    // Acquire resource. Incompatible one will be dropped from the pool.
    FrameResource* frame_resource =
        request.passthrough
            ? nullptr
            : GetOrCreateFrameResource(
                  CodedSize(request.video_frame.get(), output_format_),
                  gfx::BufferUsage::SCANOUT_CPU_READ_WRITE, output_color_space);
    if (!frame_resource) {
      std::move(request.frame_ready_cb).Run(std::move(request.video_frame));
      frame_copy_requests_.pop_front();
      continue;
    }

    worker_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&PoolImpl::CopyVideoFrameToGpuMemoryBuffer,
                                  this, request.video_frame, frame_resource));
    break;
  }
}

// Copies |video_frame| into |frame_resource| asynchronously, posting n tasks
// that will be synchronized by a barrier.
// After the barrier is passed OnCopiesDone will be called.
void GpuMemoryBufferVideoFramePool::PoolImpl::CopyVideoFrameToGpuMemoryBuffer(
    scoped_refptr<VideoFrame> video_frame,
    FrameResource* frame_resource) {
  bool mapping_succeeded = false;

  scoped_refptr<gpu::ClientSharedImage> shared_image =
      frame_resource->shared_image;
  mapping_succeeded = shared_image && (frame_resource->scoped_mapping =
                                           shared_image->Map()) != nullptr;
  if (!mapping_succeeded) {
    DLOG(ERROR) << "Could not get or map buffer.";
    OnCopiesDone(/*copy_failed=*/true, std::move(video_frame), frame_resource);
    return;
  }

  auto on_copies_done =
      base::BindOnce(&PoolImpl::OnCopiesDone, this, /*copy_failed=*/false,
                     video_frame, frame_resource);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      "media", "CopyVideoFrameToGpuMemoryBuffer",
      TRACE_ID_WITH_SCOPE("CopyVideoFrameToGpuMemoryBuffer",
                          video_frame->timestamp().InNanoseconds()));

  // Compute the number of tasks to post and create the barrier.
  const gfx::Size coded_size = CodedSize(video_frame.get(), output_format_);
  size_t copies = 0;
  const int rows = VideoFrame::Rows(/*plane=*/0, VideoFormat(output_format_),
                                    coded_size.height());
  const int rows_per_copy =
      RowsPerCopy(VideoFormat(output_format_), coded_size.width());
  copies += rows / rows_per_copy;
  if (rows % rows_per_copy) {
    ++copies;
  }
  // If the frame can be copied in one step, do it directly.
  if (copies == 1) {
    DCHECK_LE(rows, rows_per_copy);
    CopyRowsToBuffer(output_format_, /*row=*/0, rows, coded_size,
                     video_frame.get(), frame_resource,
                     std::move(on_copies_done));
    return;
  }

  // |barrier| keeps refptr of |video_frame| until all copy tasks are done.
  const base::RepeatingClosure barrier =
      base::BarrierClosure(copies, std::move(on_copies_done));
  // If it is more than one copy, post each copy async.
  for (int row = 0; row < rows; row += rows_per_copy) {
    const int rows_to_copy = std::min(rows_per_copy, rows - row);
    worker_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CopyRowsToBuffer, output_format_, row, rows_to_copy,
                       coded_size, base::Unretained(video_frame.get()),
                       frame_resource, barrier));
  }
}

// static
void GpuMemoryBufferVideoFramePool::PoolImpl::CopyRowsToBuffer(
    GpuVideoAcceleratorFactories::OutputFormat output_format,
    const size_t row,
    const size_t rows_to_copy,
    const gfx::Size coded_size,
    const VideoFrame* video_frame,
    FrameResource* frame_resource,
    base::OnceClosure done) {
  base::ScopedClosureRunner done_runner(std::move(done));
  auto* scoped_mapping = frame_resource->scoped_mapping.get();

  // To handle plane 0 of the underlying buffer.
  uint8_t* memory_ptr0 = static_cast<uint8_t*>(scoped_mapping->Memory(0));
  size_t stride0 = scoped_mapping->Stride(0);

  switch (output_format) {
    case GpuVideoAcceleratorFactories::OutputFormat::YV12: {
      DCHECK(video_frame->format() == PIXEL_FORMAT_I420 ||
             video_frame->format() == PIXEL_FORMAT_YUV420P10)
          << VideoPixelFormatToString(video_frame->format());

      VideoPixelFormat pixel_format = VideoFormat(output_format);
      for (int dst_plane = 0; dst_plane < 3; ++dst_plane) {
        static constexpr VideoFrame::Plane kSrcPlanes[3] = {
            VideoFrame::Plane::kY, VideoFrame::Plane::kV,
            VideoFrame::Plane::kU};
        VideoFrame::Plane src_plane = kSrcPlanes[dst_plane];

        const size_t plane_row_start =
            row / VideoFrame::SampleSize(pixel_format, src_plane).height();
        const size_t plane_rows_to_copy =
            VideoFrame::Rows(src_plane, pixel_format, rows_to_copy);
        const size_t plane_bytes_per_row =
            VideoFrame::RowBytes(src_plane, pixel_format, coded_size.width());

        CopyRowsToI420Buffer(
            plane_row_start, plane_rows_to_copy, plane_bytes_per_row,
            video_frame->BitDepth(), video_frame->visible_data(src_plane),
            video_frame->stride(src_plane),
            static_cast<uint8_t*>(scoped_mapping->Memory(dst_plane)),
            scoped_mapping->Stride(dst_plane));
      }
      break;
    }

    case GpuVideoAcceleratorFactories::OutputFormat::P010:
      CopyRowsToP010Buffer(row, rows_to_copy, coded_size.width(), video_frame,
                           memory_ptr0, stride0,
                           static_cast<uint8_t*>(scoped_mapping->Memory(1)),
                           scoped_mapping->Stride(1));
      break;

    case GpuVideoAcceleratorFactories::OutputFormat::NV12:
      CopyRowsToNV12Buffer(row, rows_to_copy, coded_size.width(),
                           video_frame->BitDepth(), video_frame, memory_ptr0,
                           stride0,
                           static_cast<uint8_t*>(scoped_mapping->Memory(1)),
                           scoped_mapping->Stride(1));
      break;
    case GpuVideoAcceleratorFactories::OutputFormat::XB30:
    case GpuVideoAcceleratorFactories::OutputFormat::XR30: {
      const bool is_rgba =
          output_format == GpuVideoAcceleratorFactories::OutputFormat::XB30;
      CopyRowsToRGB10Buffer(is_rgba, row, rows_to_copy, coded_size.width(),
                            video_frame, memory_ptr0, stride0);
      break;
    }

    case GpuVideoAcceleratorFactories::OutputFormat::UNDEFINED:
      NOTREACHED_IN_MIGRATION();
  }
}

void GpuMemoryBufferVideoFramePool::PoolImpl::OnCopiesDoneOnMediaThread(
    bool copy_failed,
    scoped_refptr<VideoFrame> video_frame,
    FrameResource* frame_resource) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  if (copy_failed) {
    // Drop the resource if there was an error with it. If we're not in
    // shutdown we also need to remove the pool entry for the resource.
    if (!in_shutdown_) {
      auto it = base::ranges::find(resources_pool_, frame_resource);
      CHECK(it != resources_pool_.end(), base::NotFatalUntil::M130);
      resources_pool_.erase(it);
    }

    DeleteFrameResource(gpu_factories_, frame_resource);
    delete frame_resource;

    CompleteCopyRequestAndMaybeStartNextCopy(std::move(video_frame));
    return;
  }

  scoped_refptr<VideoFrame> frame = BindAndCreateMailboxHardwareFrameResource(
      frame_resource, CodedSize(video_frame.get(), output_format_),
      gfx::Rect(video_frame->visible_rect().size()),
      video_frame->natural_size(), video_frame->ColorSpace(),
      video_frame->timestamp(), video_frame->metadata().allow_overlay,
      video_frame->ycbcr_info());
  if (!frame) {
    CompleteCopyRequestAndMaybeStartNextCopy(std::move(video_frame));
    return;
  }

  bool new_allow_overlay = frame->metadata().allow_overlay;
  bool new_read_lock_fences_enabled =
      frame->metadata().read_lock_fences_enabled;
  frame->set_hdr_metadata(video_frame->hdr_metadata());
  frame->metadata().MergeMetadataFrom(video_frame->metadata());
  frame->metadata().allow_overlay = new_allow_overlay;
  frame->metadata().read_lock_fences_enabled = new_read_lock_fences_enabled;
  CompleteCopyRequestAndMaybeStartNextCopy(std::move(frame));
}

scoped_refptr<VideoFrame> GpuMemoryBufferVideoFramePool::PoolImpl::
    BindAndCreateMailboxHardwareFrameResource(
        FrameResource* frame_resource,
        const gfx::Size& coded_size,
        const gfx::Rect& visible_rect,
        const gfx::Size& natural_size,
        const gfx::ColorSpace& color_space,
        base::TimeDelta timestamp,
        bool video_frame_allow_overlay,
        const std::optional<gpu::VulkanYCbCrInfo>& ycbcr_info) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  gpu::SharedImageInterface* sii = gpu_factories_->SharedImageInterface();
  if (!sii) {
    frame_resource->MarkUnused(tick_clock_->NowTicks());
    return nullptr;
  }

  bool is_webgpu_compatible = false;

  // This method is only expected to be called when there is a
  // MappableSI and copy to it after mapping didn't fail.
  CHECK(frame_resource->shared_image);

  auto handle = frame_resource->shared_image->CloneGpuMemoryBufferHandle();

  // Log software/hardware backed MappableSI's
  // `output_format_` used to create the shared image.
  auto name = (handle.type == gfx::GpuMemoryBufferType::SHARED_MEMORY_BUFFER)
                  ? std::string("Media.GPU.OutputFormatSoftwareGmb")
                  : std::string("Media.GPU.OutputFormatHardwareGmb");
  base::UmaHistogramEnumeration(name, output_format_);

#if BUILDFLAG(IS_MAC)
  // Shared image uses iosurface as native resource which is compatible to
  // WebGPU always.
  is_webgpu_compatible =
      media::IOSurfaceIsWebGPUCompatible(handle.io_surface.get());
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  is_webgpu_compatible =
      handle.native_pixmap_handle.supports_zero_copy_webgpu_import;
#endif

  // Bind the texture and create or rebind the image. This image may be read
  // via the raster interface for import into canvas and/or 2-copy import into
  // WebGL as well as potentially being read via the GLES interface for 1-copy
  // import into WebGL.
  sii->UpdateSharedImage(frame_resource->sync_token,
                         frame_resource->shared_image->mailbox());

  // Insert a sync_token, this is needed to make sure that the textures the
  // mailboxes refer to will be used only after all the previous commands posted
  // in the SharedImageInterface have been processed.
  gpu::SyncToken sync_token = sii->GenUnverifiedSyncToken();

  VideoPixelFormat frame_format = VideoFormat(output_format_);

  // Create the VideoFrame backed by native textures.
  scoped_refptr<VideoFrame> frame = VideoFrame::WrapSharedImage(
      frame_format, frame_resource->shared_image, sync_token,
      VideoFrame::ReleaseMailboxCB(), coded_size, visible_rect, natural_size,
      timestamp);

  if (!frame) {
    frame_resource->MarkUnused(tick_clock_->NowTicks());
    MailboxHolderReleased(frame_resource, sync_token);
    return nullptr;
  }
  frame->SetReleaseMailboxCB(
      base::BindOnce(&PoolImpl::MailboxHolderReleased, this, frame_resource));

  frame->set_color_space(frame_resource->shared_image->color_space());

  if (ycbcr_info) {
    frame->set_ycbcr_info(ycbcr_info);
  }

  frame->set_shared_image_format_type(
      SharedImageFormatType::kSharedImageFormat);
  if (frame_resource->shared_image->format().PrefersExternalSampler()) {
    frame->set_shared_image_format_type(
        SharedImageFormatType::kSharedImageFormatExternalSampler);
  }

  bool allow_overlay = false;
  if (frame_resource->shared_image->usage().Has(
          gpu::SHARED_IMAGE_USAGE_SCANOUT)) {
#if BUILDFLAG(IS_WIN)
    // Windows direct composition path only supports NV12 video overlays.
    allow_overlay =
        output_format_ == GpuVideoAcceleratorFactories::OutputFormat::NV12;
#else
    switch (output_format_) {
      case GpuVideoAcceleratorFactories::OutputFormat::YV12:
        allow_overlay = video_frame_allow_overlay;
        break;
      case GpuVideoAcceleratorFactories::OutputFormat::P010:
      case GpuVideoAcceleratorFactories::OutputFormat::NV12:
        allow_overlay = true;
        break;
      case GpuVideoAcceleratorFactories::OutputFormat::XR30:
      case GpuVideoAcceleratorFactories::OutputFormat::XB30:
#if BUILDFLAG(IS_MAC)
        allow_overlay = IOSurfaceCanSetColorSpace(color_space);
#else
        // TODO(crbug.com/41350508): Enable this for ChromeOS.
        allow_overlay = false;
#endif
        break;
      case GpuVideoAcceleratorFactories::OutputFormat::UNDEFINED:
        break;
    }
#endif  // BUILDFLAG(IS_WIN)
  }
  frame->metadata().allow_overlay = allow_overlay;
  frame->metadata().read_lock_fences_enabled = true;
  frame->metadata().is_webgpu_compatible = is_webgpu_compatible;
  return frame;
}

// Destroy all the resources posting one task per FrameResource
// to the |media_task_runner_|.
GpuMemoryBufferVideoFramePool::PoolImpl::~PoolImpl() {
  DCHECK(in_shutdown_);
}

void GpuMemoryBufferVideoFramePool::PoolImpl::Shutdown() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  // Clients don't care about copies once shutdown has started, so abort them.
  Abort();

  // Delete all the resources on the media thread.
  in_shutdown_ = true;
  for (FrameResource* frame_resource : resources_pool_) {
    // Will be deleted later upon return to pool.
    if (frame_resource->is_used()) {
      continue;
    }

    media_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&PoolImpl::DeleteFrameResource,
                                  gpu_factories_, base::Owned(frame_resource)));
  }
  resources_pool_.clear();
}

void GpuMemoryBufferVideoFramePool::PoolImpl::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

// Tries to find the resource in the pool or creates it.
// Incompatible resource will be dropped.
GpuMemoryBufferVideoFramePool::PoolImpl::FrameResource*
GpuMemoryBufferVideoFramePool::PoolImpl::GetOrCreateFrameResource(
    const gfx::Size& size,
    gfx::BufferUsage usage,
    const gfx::ColorSpace& color_space) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  auto it = resources_pool_.begin();
  while (it != resources_pool_.end()) {
    FrameResource* frame_resource = *it;
    if (!frame_resource->is_used()) {
      if (IsFrameResourceCompatible(frame_resource, size, usage)) {
        frame_resource->MarkUsed();
        return frame_resource;
      } else {
        resources_pool_.erase(it++);
        DeleteFrameResource(gpu_factories_, frame_resource);
        delete frame_resource;
      }
    } else {
      it++;
    }
  }

  // Create the resource.
  FrameResource* frame_resource = new FrameResource(size, usage);
  resources_pool_.push_back(frame_resource);
  // Update the |buffer_id| to be used by memory dumps.
  frame_resource->buffer_id = ++buffer_id_;

  if (auto* sii = gpu_factories_->SharedImageInterface()) {
    viz::SharedImageFormat si_format =
        OutputFormatToSharedImageFormat(output_format_);

    // This needs to be called before creating the MappableSI
    // here. |si_format| could be modified internally later based on the
    // type of buffer (shared memory or native gpu buffer) backing the
    // shared image. https://issues.chromium.org/339546249.
    SetPrefersExternalSampler(si_format);

    gpu::SharedImageUsageSet si_usage = gpu::SHARED_IMAGE_USAGE_GLES2_READ |
                                        gpu::SHARED_IMAGE_USAGE_RASTER_READ |
                                        gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;

    bool add_scanout_usage = true;

    // SCANOUT usage was historically added unconditionally. However, it
    // actually should be added only if scanout of SharedImages for this use
    // case is supported.
    // TODO(crbug.com/330865436): Remove killswitch post-safe rollout.
    if (base::FeatureList::IsEnabled(
            features::
                kSWVideoFrameAddScanoutUsageOnlyIfSupportedBySharedImage)) {
      auto si_caps = sii->GetCapabilities();

#if BUILDFLAG(IS_WIN)
      // On Windows, overlays are in general not supported. However, in some
      // cases they are supported for the software video frame use case in
      // particular. This cap details whether that support is present.
      add_scanout_usage =
          si_caps.supports_scanout_shared_images_for_software_video_frames;
#else
      // On all other platforms, whether scanout for SharedImages is supported
      // for this particular use case is no different than the general case.
      add_scanout_usage = si_caps.supports_scanout_shared_images;
#endif
    }

    if (add_scanout_usage) {
      si_usage |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
    }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
    // TODO(crbug.com/40194712): Always add the flag once the
    // OzoneImageBacking is by default turned on.
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableUnsafeWebGPU)) {
      // This SharedImage may be used for zero-copy import into WebGPU.
      si_usage |= gpu::SHARED_IMAGE_USAGE_WEBGPU_READ;
    }
#endif
    // Create a Mappable shared image.
    frame_resource->shared_image =
        sii->CreateSharedImage({si_format, size, color_space, si_usage,
                                "MediaGmbVideoFramePoolMappableSI"},
                               gpu::kNullSurfaceHandle, usage);
    return frame_resource;
  }
  return nullptr;
}

void GpuMemoryBufferVideoFramePool::PoolImpl::
    CompleteCopyRequestAndMaybeStartNextCopy(
        scoped_refptr<VideoFrame> video_frame) {
  DCHECK(!frame_copy_requests_.empty());

  std::move(frame_copy_requests_.front().frame_ready_cb)
      .Run(std::move(video_frame));
  frame_copy_requests_.pop_front();
  if (!frame_copy_requests_.empty())
    StartCopy();
}

// static
void GpuMemoryBufferVideoFramePool::PoolImpl::DeleteFrameResource(
    GpuVideoAcceleratorFactories* const gpu_factories,
    FrameResource* frame_resource) {
  // TODO(dcastagna): As soon as the context lost is dealt with in media,
  // make sure that we won't execute this callback (use a weak pointer to
  // the old context).
  if (!gpu_factories->SharedImageInterface()) {
    return;
  }

  if (frame_resource->shared_image) {
    frame_resource->shared_image->UpdateDestructionSyncToken(
        frame_resource->sync_token);
  }
}

// Called when a VideoFrame is no longer referenced. Put back the resource in
// the pool.
void GpuMemoryBufferVideoFramePool::PoolImpl::MailboxHolderReleased(
    FrameResource* frame_resource,
    const gpu::SyncToken& release_sync_token) {
  if (!media_task_runner_->RunsTasksInCurrentSequence()) {
    media_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&PoolImpl::MailboxHolderReleased, this,
                                  frame_resource, release_sync_token));
    return;
  }
  frame_resource->sync_token = release_sync_token;

  if (in_shutdown_) {
    DeleteFrameResource(gpu_factories_, frame_resource);
    delete frame_resource;
    return;
  }

  const base::TimeTicks now = tick_clock_->NowTicks();
  frame_resource->MarkUnused(now);
  auto it = resources_pool_.begin();
  while (it != resources_pool_.end()) {
    FrameResource* resource = *it;

    constexpr base::TimeDelta kStaleFrameLimit = base::Seconds(10);
    if (!resource->is_used() &&
        now - resource->last_use_time() > kStaleFrameLimit) {
      resources_pool_.erase(it++);
      DeleteFrameResource(gpu_factories_, resource);
      delete resource;
    } else {
      it++;
    }
  }
}

GpuMemoryBufferVideoFramePool::GpuMemoryBufferVideoFramePool() = default;

GpuMemoryBufferVideoFramePool::GpuMemoryBufferVideoFramePool(
    const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    GpuVideoAcceleratorFactories* gpu_factories)
    : pool_impl_(
          new PoolImpl(media_task_runner, worker_task_runner, gpu_factories)) {
  base::trace_event::MemoryDumpManager::GetInstance()
      ->RegisterDumpProviderWithSequencedTaskRunner(
          pool_impl_.get(), "GpuMemoryBufferVideoFramePool", media_task_runner,
          base::trace_event::MemoryDumpProvider::Options());
}

GpuMemoryBufferVideoFramePool::~GpuMemoryBufferVideoFramePool() {
  // May be nullptr in tests.
  if (!pool_impl_)
    return;

  pool_impl_->Shutdown();
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      pool_impl_.get());
}

void GpuMemoryBufferVideoFramePool::MaybeCreateHardwareFrame(
    scoped_refptr<VideoFrame> video_frame,
    FrameReadyCB frame_ready_cb) {
  DCHECK(video_frame);
  pool_impl_->CreateHardwareFrame(std::move(video_frame),
                                  std::move(frame_ready_cb));
}

void GpuMemoryBufferVideoFramePool::Abort() {
  pool_impl_->Abort();
}

void GpuMemoryBufferVideoFramePool::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  pool_impl_->SetTickClockForTesting(tick_clock);
}

}  // namespace media
