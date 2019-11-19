// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/gpu_memory_buffer_video_frame_pool.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <list>
#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/circular_deque.h"
#include "base/containers/stack_container.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "media/base/bind_to_current_loop.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/color_space.h"
#include "ui/gl/trace_util.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "ui/gfx/mac/io_surface.h"
#endif

namespace media {

// Implementation of a pool of GpuMemoryBuffers used to back VideoFrames.
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
  PoolImpl(const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
           const scoped_refptr<base::TaskRunner>& worker_task_runner,
           GpuVideoAcceleratorFactories* const gpu_factories)
      : media_task_runner_(media_task_runner),
        worker_task_runner_(worker_task_runner),
        gpu_factories_(gpu_factories),
        output_format_(GpuVideoAcceleratorFactories::OutputFormat::UNDEFINED),
        tick_clock_(base::DefaultTickClock::GetInstance()),
        in_shutdown_(false) {
    DCHECK(media_task_runner_);
    DCHECK(worker_task_runner_);
  }

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

  // Resource to represent a plane.
  struct PlaneResource {
    gfx::Size size;
    std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer;
    gpu::Mailbox mailbox;
  };

  // All the resources needed to compose a frame.
  // TODO(dalecurtis): The method of use marking used is very brittle
  // and prone to leakage. Switch this to pass around std::unique_ptr
  // such that callers own resources explicitly.
  struct FrameResources {
    explicit FrameResources(const gfx::Size& size) : size(size) {}
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
    PlaneResource plane_resources[VideoFrame::kMaxPlanes];
    // The sync token used to recycle or destroy the resources. It is set when
    // the resources are returned from the VideoFrame (via
    // MailboxHoldersReleased).
    gpu::SyncToken sync_token;

   private:
    bool is_used_ = true;
    base::TimeTicks last_use_time_;
  };

  // Struct to keep track of requested videoframe copies.
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

  // Copy |video_frame| data into |frame_resources| and calls |frame_ready_cb|
  // when done.
  void CopyVideoFrameToGpuMemoryBuffers(scoped_refptr<VideoFrame> video_frame,
                                        FrameResources* frame_resources);

  // Called when all the data has been copied.
  void OnCopiesDone(scoped_refptr<VideoFrame> video_frame,
                    FrameResources* frame_resources);

  // Prepares GL resources, mailboxes and calls |frame_ready_cb| with the new
  // VideoFrame. This has to be run on |media_task_runner_| where
  // |frame_ready_cb| associated with video_frame will also be run.
  void BindAndCreateMailboxesHardwareFrameResources(
      scoped_refptr<VideoFrame> video_frame,
      FrameResources* frame_resources);

  // Return true if |resources| can be used to represent a frame for
  // specific |format| and |size|.
  static bool AreFrameResourcesCompatible(const FrameResources* resources,
                                          const gfx::Size& size) {
    return size == resources->size;
  }

  // Get the resources needed for a frame out of the pool, or create them if
  // necessary.
  // This also drops the LRU resources that can't be reuse for this frame.
  FrameResources* GetOrCreateFrameResources(
      const gfx::Size& size,
      GpuVideoAcceleratorFactories::OutputFormat format);

  // Calls the FrameReadyCB of the first entry in |frame_copy_requests_|, with
  // the provided |video_frame|, then deletes the entry from
  // |frame_copy_requests_| and attempts to start another copy if there are
  // other |frame_copy_requests_| elements.
  void CompleteCopyRequestAndMaybeStartNextCopy(
      scoped_refptr<VideoFrame> video_frame);

  // Callback called when a VideoFrame generated with GetFrameResources is no
  // longer referenced.
  void MailboxHoldersReleased(FrameResources* frame_resources,
                              const gpu::SyncToken& sync_token);

  // Delete resources. This has to be called on the thread where |task_runner|
  // is current.
  static void DeleteFrameResources(
      GpuVideoAcceleratorFactories* const gpu_factories,
      FrameResources* frame_resources);

  // Task runner associated to the GL context provided by |gpu_factories_|.
  const scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  // Task runner used to asynchronously copy planes.
  const scoped_refptr<base::TaskRunner> worker_task_runner_;

  // Interface to GPU related operations.
  GpuVideoAcceleratorFactories* const gpu_factories_;

  // Pool of resources.
  std::list<FrameResources*> resources_pool_;

  GpuVideoAcceleratorFactories::OutputFormat output_format_;

  // |tick_clock_| is always a DefaultTickClock outside of testing.
  const base::TickClock* tick_clock_;

  // Queued up video frames for copies. The front is the currently
  // in-flight copy, new copies are added at the end.
  base::circular_deque<VideoFrameCopyRequest> frame_copy_requests_;
  bool in_shutdown_;

  DISALLOW_COPY_AND_ASSIGN(PoolImpl);
};

namespace {

// VideoFrame copies to GpuMemoryBuffers will be split in copies where the
// output size is |kBytesPerCopyTarget| bytes and run in parallel.
constexpr size_t kBytesPerCopyTarget = 1024 * 1024;  // 1MB

// Return the GpuMemoryBuffer format to use for a specific VideoPixelFormat
// and plane.
gfx::BufferFormat GpuMemoryBufferFormat(
    GpuVideoAcceleratorFactories::OutputFormat format,
    size_t plane) {
  switch (format) {
    case GpuVideoAcceleratorFactories::OutputFormat::I420:
      DCHECK_LE(plane, 2u);
      return gfx::BufferFormat::R_8;
    case GpuVideoAcceleratorFactories::OutputFormat::NV12_SINGLE_GMB:
      DCHECK_LE(plane, 1u);
      return gfx::BufferFormat::YUV_420_BIPLANAR;
    case GpuVideoAcceleratorFactories::OutputFormat::NV12_DUAL_GMB:
      DCHECK_LE(plane, 1u);
      return plane == 0 ? gfx::BufferFormat::R_8 : gfx::BufferFormat::RG_88;
    case GpuVideoAcceleratorFactories::OutputFormat::XR30:
      DCHECK_EQ(0u, plane);
      return gfx::BufferFormat::BGRX_1010102;
    case GpuVideoAcceleratorFactories::OutputFormat::XB30:
      DCHECK_EQ(0u, plane);
      return gfx::BufferFormat::RGBX_1010102;
    case GpuVideoAcceleratorFactories::OutputFormat::RGBA:
      DCHECK_EQ(0u, plane);
      return gfx::BufferFormat::RGBA_8888;
    case GpuVideoAcceleratorFactories::OutputFormat::BGRA:
      DCHECK_EQ(0u, plane);
      return gfx::BufferFormat::BGRA_8888;
    case GpuVideoAcceleratorFactories::OutputFormat::UNDEFINED:
      NOTREACHED();
      break;
  }
  return gfx::BufferFormat::BGRA_8888;
}

// The number of output planes to be copied in each iteration.
size_t PlanesPerCopy(GpuVideoAcceleratorFactories::OutputFormat format) {
  switch (format) {
    case GpuVideoAcceleratorFactories::OutputFormat::I420:
    case GpuVideoAcceleratorFactories::OutputFormat::RGBA:
    case GpuVideoAcceleratorFactories::OutputFormat::BGRA:
      return 1;
    case GpuVideoAcceleratorFactories::OutputFormat::NV12_DUAL_GMB:
    case GpuVideoAcceleratorFactories::OutputFormat::NV12_SINGLE_GMB:
      return 2;
    case GpuVideoAcceleratorFactories::OutputFormat::XR30:
    case GpuVideoAcceleratorFactories::OutputFormat::XB30:
      return 3;
    case GpuVideoAcceleratorFactories::OutputFormat::UNDEFINED:
      NOTREACHED();
      break;
  }
  return 0;
}

VideoPixelFormat VideoFormat(
    GpuVideoAcceleratorFactories::OutputFormat format) {
  switch (format) {
    case GpuVideoAcceleratorFactories::OutputFormat::I420:
      return PIXEL_FORMAT_I420;
    case GpuVideoAcceleratorFactories::OutputFormat::NV12_SINGLE_GMB:
    case GpuVideoAcceleratorFactories::OutputFormat::NV12_DUAL_GMB:
      return PIXEL_FORMAT_NV12;
    case GpuVideoAcceleratorFactories::OutputFormat::BGRA:
      return PIXEL_FORMAT_ARGB;
    case GpuVideoAcceleratorFactories::OutputFormat::RGBA:
      return PIXEL_FORMAT_ABGR;
    case GpuVideoAcceleratorFactories::OutputFormat::XR30:
      return PIXEL_FORMAT_XR30;
    case GpuVideoAcceleratorFactories::OutputFormat::XB30:
      return PIXEL_FORMAT_XB30;
    case GpuVideoAcceleratorFactories::OutputFormat::UNDEFINED:
      NOTREACHED();
      break;
  }
  return PIXEL_FORMAT_UNKNOWN;
}

// The number of output planes to be copied in each iteration.
size_t NumGpuMemoryBuffers(GpuVideoAcceleratorFactories::OutputFormat format) {
  switch (format) {
    case GpuVideoAcceleratorFactories::OutputFormat::I420:
      return 3;
    case GpuVideoAcceleratorFactories::OutputFormat::NV12_SINGLE_GMB:
      return 1;
    case GpuVideoAcceleratorFactories::OutputFormat::NV12_DUAL_GMB:
      return 2;
    case GpuVideoAcceleratorFactories::OutputFormat::XR30:
    case GpuVideoAcceleratorFactories::OutputFormat::XB30:
      return 1;
    case GpuVideoAcceleratorFactories::OutputFormat::RGBA:
    case GpuVideoAcceleratorFactories::OutputFormat::BGRA:
      return 1;
    case GpuVideoAcceleratorFactories::OutputFormat::UNDEFINED:
      NOTREACHED();
      break;
  }
  NOTREACHED();
  return 0;
}

// The number of output rows to be copied in each iteration.
int RowsPerCopy(size_t plane, VideoPixelFormat format, int width) {
  int bytes_per_row = VideoFrame::RowBytes(plane, format, width);
  if (format == PIXEL_FORMAT_NV12) {
    DCHECK_EQ(0u, plane);
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
                          int dest_stride,
                          base::OnceClosure done) {
  base::ScopedClosureRunner done_runner(std::move(done));
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

void CopyRowsToNV12Buffer(int first_row,
                          int rows,
                          int bytes_per_row,
                          const VideoFrame* source_frame,
                          uint8_t* dest_y,
                          int dest_stride_y,
                          uint8_t* dest_uv,
                          int dest_stride_uv,
                          base::OnceClosure done) {
  base::ScopedClosureRunner done_runner(std::move(done));
  TRACE_EVENT2("media", "CopyRowsToNV12Buffer", "bytes_per_row", bytes_per_row,
               "rows", rows);

  if (!dest_y || !dest_uv)
    return;

  DCHECK_NE(dest_stride_y, 0);
  DCHECK_NE(dest_stride_uv, 0);
  DCHECK_LE(bytes_per_row, std::abs(dest_stride_y));
  DCHECK_LE(bytes_per_row, std::abs(dest_stride_uv));
  DCHECK_EQ(0, first_row % 2);
  DCHECK(source_frame->format() == PIXEL_FORMAT_I420 ||
         source_frame->format() == PIXEL_FORMAT_YV12);
  libyuv::I420ToNV12(
      source_frame->visible_data(VideoFrame::kYPlane) +
          first_row * source_frame->stride(VideoFrame::kYPlane),
      source_frame->stride(VideoFrame::kYPlane),
      source_frame->visible_data(VideoFrame::kUPlane) +
          first_row / 2 * source_frame->stride(VideoFrame::kUPlane),
      source_frame->stride(VideoFrame::kUPlane),
      source_frame->visible_data(VideoFrame::kVPlane) +
          first_row / 2 * source_frame->stride(VideoFrame::kVPlane),
      source_frame->stride(VideoFrame::kVPlane),
      dest_y + first_row * dest_stride_y, dest_stride_y,
      dest_uv + first_row / 2 * dest_stride_uv, dest_stride_uv, bytes_per_row,
      rows);
}

void CopyRowsToRGB10Buffer(bool is_argb,
                           int first_row,
                           int rows,
                           int width,
                           const VideoFrame* source_frame,
                           uint8_t* output,
                           int dest_stride,
                           base::OnceClosure done) {
  base::ScopedClosureRunner done_runner(std::move(done));
  TRACE_EVENT2("media", "CopyRowsToXR30Buffer", "bytes_per_row", width * 2,
               "rows", rows);
  if (!output)
    return;

  DCHECK_NE(dest_stride, 0);
  DCHECK_LE(width, std::abs(dest_stride / 2));
  DCHECK_EQ(0, first_row % 2);
  DCHECK_EQ(source_frame->format(), PIXEL_FORMAT_YUV420P10);

  const uint16_t* y_plane = reinterpret_cast<const uint16_t*>(
      source_frame->visible_data(VideoFrame::kYPlane) +
      first_row * source_frame->stride(VideoFrame::kYPlane));
  const size_t y_plane_stride = source_frame->stride(VideoFrame::kYPlane) / 2;
  const uint16_t* v_plane = reinterpret_cast<const uint16_t*>(
      source_frame->visible_data(VideoFrame::kVPlane) +
      first_row / 2 * source_frame->stride(VideoFrame::kVPlane));
  const size_t v_plane_stride = source_frame->stride(VideoFrame::kVPlane) / 2;
  const uint16_t* u_plane = reinterpret_cast<const uint16_t*>(
      source_frame->visible_data(VideoFrame::kUPlane) +
      first_row / 2 * source_frame->stride(VideoFrame::kUPlane));
  const size_t u_plane_stride = source_frame->stride(VideoFrame::kUPlane) / 2;
  uint8_t* dest_rgb10 = output + first_row * dest_stride;

  SkYUVColorSpace skyuv = kRec709_SkYUVColorSpace;
  source_frame->ColorSpace().ToSkYUVColorSpace(&skyuv);

  if (skyuv == kRec601_SkYUVColorSpace) {
    if (is_argb) {
      libyuv::I010ToAR30(y_plane, y_plane_stride, u_plane, u_plane_stride,
                         v_plane, v_plane_stride, dest_rgb10, dest_stride,
                         width, rows);
    } else {
      libyuv::I010ToAB30(y_plane, y_plane_stride, u_plane, u_plane_stride,
                         v_plane, v_plane_stride, dest_rgb10, dest_stride,
                         width, rows);
    }
  } else if (skyuv == kBT2020_SkYUVColorSpace) {
    if (is_argb) {
      libyuv::U010ToAR30(y_plane, y_plane_stride, u_plane, u_plane_stride,
                         v_plane, v_plane_stride, dest_rgb10, dest_stride,
                         width, rows);
    } else {
      libyuv::U010ToAB30(y_plane, y_plane_stride, u_plane, u_plane_stride,
                         v_plane, v_plane_stride, dest_rgb10, dest_stride,
                         width, rows);
    }
  } else {  // BT.709
    if (is_argb) {
      libyuv::H010ToAR30(y_plane, y_plane_stride, u_plane, u_plane_stride,
                         v_plane, v_plane_stride, dest_rgb10, dest_stride,
                         width, rows);
    } else {
      libyuv::H010ToAB30(y_plane, y_plane_stride, u_plane, u_plane_stride,
                         v_plane, v_plane_stride, dest_rgb10, dest_stride,
                         width, rows);
    }
  }
}

void CopyRowsToRGBABuffer(bool is_rgba,
                          int first_row,
                          int rows,
                          int width,
                          const VideoFrame* source_frame,
                          uint8_t* output,
                          int dest_stride,
                          base::OnceClosure done) {
  base::ScopedClosureRunner done_runner(std::move(done));
  TRACE_EVENT2("media", "CopyRowsToRGBABuffer", "bytes_per_row", width * 2,
               "rows", rows);

  if (!output)
    return;

  DCHECK_NE(dest_stride, 0);
  DCHECK_LE(width, std::abs(dest_stride / 2));
  DCHECK_EQ(0, first_row % 2);
  DCHECK_EQ(source_frame->format(), PIXEL_FORMAT_I420A);

  // libyuv uses little-endian for RGBx formats, whereas here we use big endian.
  auto* func_ptr = is_rgba ? libyuv::I420AlphaToABGR : libyuv::I420AlphaToARGB;

  func_ptr(source_frame->visible_data(VideoFrame::kYPlane) +
               first_row * source_frame->stride(VideoFrame::kYPlane),
           source_frame->stride(VideoFrame::kYPlane),
           source_frame->visible_data(VideoFrame::kUPlane) +
               first_row / 2 * source_frame->stride(VideoFrame::kUPlane),
           source_frame->stride(VideoFrame::kUPlane),
           source_frame->visible_data(VideoFrame::kVPlane) +
               first_row / 2 * source_frame->stride(VideoFrame::kVPlane),
           source_frame->stride(VideoFrame::kVPlane),
           source_frame->visible_data(VideoFrame::kAPlane) +
               first_row * source_frame->stride(VideoFrame::kAPlane),
           source_frame->stride(VideoFrame::kAPlane),
           output + first_row * dest_stride, dest_stride, width, rows,
           // Textures are expected to be premultiplied by GL and compositors.
           1 /* attenuate, meaning premultiply */);
}

gfx::Size CodedSize(const VideoFrame* video_frame,
                    GpuVideoAcceleratorFactories::OutputFormat output_format) {
  DCHECK(gfx::Rect(video_frame->coded_size())
             .Contains(video_frame->visible_rect()));
  DCHECK((video_frame->visible_rect().x() & 1) == 0);
  gfx::Size output;
  switch (output_format) {
    case GpuVideoAcceleratorFactories::OutputFormat::I420:
    case GpuVideoAcceleratorFactories::OutputFormat::NV12_SINGLE_GMB:
    case GpuVideoAcceleratorFactories::OutputFormat::NV12_DUAL_GMB:
      DCHECK((video_frame->visible_rect().y() & 1) == 0);
      output = gfx::Size((video_frame->visible_rect().width() + 1) & ~1,
                         (video_frame->visible_rect().height() + 1) & ~1);
      break;
    case GpuVideoAcceleratorFactories::OutputFormat::XR30:
    case GpuVideoAcceleratorFactories::OutputFormat::XB30:
    case GpuVideoAcceleratorFactories::OutputFormat::RGBA:
    case GpuVideoAcceleratorFactories::OutputFormat::BGRA:
      output = gfx::Size((video_frame->visible_rect().width() + 1) & ~1,
                         video_frame->visible_rect().height());
      break;
    case GpuVideoAcceleratorFactories::OutputFormat::UNDEFINED:
      NOTREACHED();
  }
  DCHECK(gfx::Rect(video_frame->coded_size()).Contains(gfx::Rect(output)));
  return output;
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
  DCHECK(media_task_runner_->BelongsToCurrentThread());
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

  bool is_software_backed_video_frame = !video_frame->HasTextures();
#if defined(OS_LINUX)
  is_software_backed_video_frame &= !video_frame->HasDmaBufs();
#endif

  bool passthrough = false;
#if defined(OS_MACOSX)
  if (!IOSurfaceCanSetColorSpace(video_frame->ColorSpace()))
    passthrough = true;
#endif
  if (output_format_ == GpuVideoAcceleratorFactories::OutputFormat::UNDEFINED)
    passthrough = true;
  switch (pixel_format) {
    // Supported cases.
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_YUV420P9:
    case PIXEL_FORMAT_YUV420P10:
    case PIXEL_FORMAT_YUV420P12:
    case PIXEL_FORMAT_I420A:
      break;
    // Unsupported cases.
    case PIXEL_FORMAT_I422:
    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV21:
    case PIXEL_FORMAT_YUY2:
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_BGRA:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_RGB24:
    case PIXEL_FORMAT_MJPEG:
    case PIXEL_FORMAT_YUV422P9:
    case PIXEL_FORMAT_YUV444P9:
    case PIXEL_FORMAT_YUV422P10:
    case PIXEL_FORMAT_YUV444P10:
    case PIXEL_FORMAT_YUV422P12:
    case PIXEL_FORMAT_YUV444P12:
    case PIXEL_FORMAT_Y16:
    case PIXEL_FORMAT_ABGR:
    case PIXEL_FORMAT_XBGR:
    case PIXEL_FORMAT_P016LE:
    case PIXEL_FORMAT_XR30:
    case PIXEL_FORMAT_XB30:
    case PIXEL_FORMAT_UNKNOWN:
      if (is_software_backed_video_frame) {
        UMA_HISTOGRAM_ENUMERATION(
            "Media.GpuMemoryBufferVideoFramePool.UnsupportedFormat",
            pixel_format, PIXEL_FORMAT_MAX + 1);
      }
      passthrough = true;
  }
  // TODO(dcastagna): Handle odd positioned video frame input, see
  // https://crbug.com/638906.
  // TODO(emircan): Eliminate odd size video frame input cases as they are not
  // valid, see https://crbug.com/webrtc/9033.
  if ((video_frame->visible_rect().x() & 1) ||
      (video_frame->visible_rect().y() & 1) ||
      (video_frame->coded_size().width() & 1) ||
      (video_frame->coded_size().height() & 1)) {
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
  const uint64_t tracing_process_id =
      base::trace_event::MemoryDumpManager::GetInstance()
          ->GetTracingProcessId();
  const int kImportance = 2;
  for (const FrameResources* frame_resources : resources_pool_) {
    for (const PlaneResource& plane_resource :
         frame_resources->plane_resources) {
      if (plane_resource.gpu_memory_buffer) {
        gfx::GpuMemoryBufferId buffer_id =
            plane_resource.gpu_memory_buffer->GetId();
        std::string dump_name = base::StringPrintf(
            "media/video_frame_memory/buffer_%d", buffer_id.id);
        base::trace_event::MemoryAllocatorDump* dump =
            pmd->CreateAllocatorDump(dump_name);
        size_t buffer_size_in_bytes = gfx::BufferSizeForBufferFormat(
            plane_resource.size, plane_resource.gpu_memory_buffer->GetFormat());
        dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                        base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                        buffer_size_in_bytes);
        dump->AddScalar("free_size",
                        base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                        frame_resources->is_used() ? 0 : buffer_size_in_bytes);
        plane_resource.gpu_memory_buffer->OnMemoryDump(
            pmd, dump->guid(), tracing_process_id, kImportance);
      }
    }
  }
  return true;
}

void GpuMemoryBufferVideoFramePool::PoolImpl::Abort() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  // Abort any pending copy requests. If one is already in flight, we can't do
  // anything about it.
  if (frame_copy_requests_.size() <= 1u)
    return;
  frame_copy_requests_.erase(frame_copy_requests_.begin() + 1,
                             frame_copy_requests_.end());
}

void GpuMemoryBufferVideoFramePool::PoolImpl::OnCopiesDone(
    scoped_refptr<VideoFrame> video_frame,
    FrameResources* frame_resources) {
  for (const auto& plane_resource : frame_resources->plane_resources) {
    if (plane_resource.gpu_memory_buffer) {
      plane_resource.gpu_memory_buffer->Unmap();
      plane_resource.gpu_memory_buffer->SetColorSpace(
          video_frame->ColorSpace());
    }
  }

  TRACE_EVENT_ASYNC_END0("media", "CopyVideoFrameToGpuMemoryBuffers",
                         video_frame->timestamp().InNanoseconds() /* id */);

  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PoolImpl::BindAndCreateMailboxesHardwareFrameResources,
                     this, std::move(video_frame), frame_resources));
}

void GpuMemoryBufferVideoFramePool::PoolImpl::StartCopy() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(!frame_copy_requests_.empty());

  while (!frame_copy_requests_.empty()) {
    VideoFrameCopyRequest& request = frame_copy_requests_.front();
    // Acquire resources. Incompatible ones will be dropped from the pool.
    FrameResources* frame_resources =
        request.passthrough
            ? nullptr
            : GetOrCreateFrameResources(
                  CodedSize(request.video_frame.get(), output_format_),
                  output_format_);
    if (!frame_resources) {
      std::move(request.frame_ready_cb).Run(std::move(request.video_frame));
      frame_copy_requests_.pop_front();
      continue;
    }

    worker_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&PoolImpl::CopyVideoFrameToGpuMemoryBuffers,
                                  this, request.video_frame, frame_resources));
    break;
  }
}

// Copies |video_frame| into |frame_resources| asynchronously, posting n tasks
// that will be synchronized by a barrier.
// After the barrier is passed OnCopiesDone will be called.
void GpuMemoryBufferVideoFramePool::PoolImpl::CopyVideoFrameToGpuMemoryBuffers(
    scoped_refptr<VideoFrame> video_frame,
    FrameResources* frame_resources) {
  // Compute the number of tasks to post and create the barrier.
  const size_t num_planes = VideoFrame::NumPlanes(VideoFormat(output_format_));
  const size_t planes_per_copy = PlanesPerCopy(output_format_);
  const gfx::Size coded_size = CodedSize(video_frame.get(), output_format_);
  size_t copies = 0;
  for (size_t i = 0; i < num_planes; i += planes_per_copy) {
    const int rows =
        VideoFrame::Rows(i, VideoFormat(output_format_), coded_size.height());
    const int rows_per_copy =
        RowsPerCopy(i, VideoFormat(output_format_), coded_size.width());
    copies += rows / rows_per_copy;
    if (rows % rows_per_copy)
      ++copies;
  }

  // |barrier| keeps refptr of |video_frame| until all copy tasks are done.
  const base::RepeatingClosure barrier = base::BarrierClosure(
      copies, base::BindOnce(&PoolImpl::OnCopiesDone, this, video_frame,
                             frame_resources));

  // Map the buffers.
  for (size_t i = 0; i < NumGpuMemoryBuffers(output_format_); i++) {
    gfx::GpuMemoryBuffer* buffer =
        frame_resources->plane_resources[i].gpu_memory_buffer.get();

    if (!buffer || !buffer->Map()) {
      DLOG(ERROR) << "Could not get or Map() buffer";
      frame_resources->MarkUnused(tick_clock_->NowTicks());
      return;
    }
  }

  TRACE_EVENT_ASYNC_BEGIN0("media", "CopyVideoFrameToGpuMemoryBuffers",
                           video_frame->timestamp().InNanoseconds() /* id */);
  // Post all the async tasks.
  for (size_t i = 0; i < num_planes; i += planes_per_copy) {
    gfx::GpuMemoryBuffer* buffer =
        frame_resources->plane_resources[i].gpu_memory_buffer.get();
    const int rows =
        VideoFrame::Rows(i, VideoFormat(output_format_), coded_size.height());
    const int rows_per_copy =
        RowsPerCopy(i, VideoFormat(output_format_), coded_size.width());

    for (int row = 0; row < rows; row += rows_per_copy) {
      const int rows_to_copy = std::min(rows_per_copy, rows - row);
      switch (output_format_) {
        case GpuVideoAcceleratorFactories::OutputFormat::I420: {
          const int bytes_per_row = VideoFrame::RowBytes(
              i, VideoFormat(output_format_), coded_size.width());
          worker_task_runner_->PostTask(
              FROM_HERE,
              base::BindOnce(&CopyRowsToI420Buffer, row, rows_to_copy,
                             bytes_per_row, video_frame->BitDepth(),
                             video_frame->visible_data(i),
                             video_frame->stride(i),
                             static_cast<uint8_t*>(buffer->memory(0)),
                             buffer->stride(0), barrier));
          break;
        }
        case GpuVideoAcceleratorFactories::OutputFormat::NV12_SINGLE_GMB:
          // Using base::Unretained(video_frame) here is safe because |barrier|
          // keeps refptr of |video_frame| until all copy tasks are done.
          worker_task_runner_->PostTask(
              FROM_HERE,
              base::BindOnce(
                  &CopyRowsToNV12Buffer, row, rows_to_copy, coded_size.width(),
                  base::Unretained(video_frame.get()),
                  static_cast<uint8_t*>(buffer->memory(0)), buffer->stride(0),
                  static_cast<uint8_t*>(buffer->memory(1)), buffer->stride(1),
                  barrier));
          break;
        case GpuVideoAcceleratorFactories::OutputFormat::NV12_DUAL_GMB: {
          gfx::GpuMemoryBuffer* buffer2 =
              frame_resources->plane_resources[1].gpu_memory_buffer.get();
          // Using base::Unretained(video_frame) here is safe because |barrier|
          // keeps refptr of |video_frame| until all copy tasks are done.
          worker_task_runner_->PostTask(
              FROM_HERE,
              base::BindOnce(
                  &CopyRowsToNV12Buffer, row, rows_to_copy, coded_size.width(),
                  base::Unretained(video_frame.get()),
                  static_cast<uint8_t*>(buffer->memory(0)), buffer->stride(0),
                  static_cast<uint8_t*>(buffer2->memory(0)), buffer2->stride(0),
                  barrier));
          break;
        }

        case GpuVideoAcceleratorFactories::OutputFormat::XR30:
        case GpuVideoAcceleratorFactories::OutputFormat::XB30: {
          const bool is_argb = output_format_ ==
                               GpuVideoAcceleratorFactories::OutputFormat::XR30;
          // Using base::Unretained(video_frame) here is safe because |barrier|
          // keeps refptr of |video_frame| until all copy tasks are done.
          worker_task_runner_->PostTask(
              FROM_HERE,
              base::BindOnce(&CopyRowsToRGB10Buffer, is_argb, row, rows_to_copy,
                             coded_size.width(),
                             base::Unretained(video_frame.get()),
                             static_cast<uint8_t*>(buffer->memory(0)),
                             buffer->stride(0), barrier));
          break;
        }

        case GpuVideoAcceleratorFactories::OutputFormat::RGBA:
        case GpuVideoAcceleratorFactories::OutputFormat::BGRA: {
          const bool is_rgba = output_format_ ==
                               GpuVideoAcceleratorFactories::OutputFormat::RGBA;
          // Using base::Unretained(video_frame) here is safe because |barrier|
          // keeps refptr of |video_frame| until all copy tasks are done.
          worker_task_runner_->PostTask(
              FROM_HERE,
              base::BindOnce(&CopyRowsToRGBABuffer, is_rgba, row, rows_to_copy,
                             coded_size.width(),
                             base::Unretained(video_frame.get()),
                             static_cast<uint8_t*>(buffer->memory(0)),
                             buffer->stride(0), barrier));
          break;
        }

        case GpuVideoAcceleratorFactories::OutputFormat::UNDEFINED:
          NOTREACHED();
      }
    }
  }
}

void GpuMemoryBufferVideoFramePool::PoolImpl::
    BindAndCreateMailboxesHardwareFrameResources(
        scoped_refptr<VideoFrame> video_frame,
        FrameResources* frame_resources) {
  gpu::SharedImageInterface* sii = gpu_factories_->SharedImageInterface();
  if (!sii) {
    frame_resources->MarkUnused(tick_clock_->NowTicks());
    CompleteCopyRequestAndMaybeStartNextCopy(std::move(video_frame));
    return;
  }

  const gfx::Size coded_size = CodedSize(video_frame.get(), output_format_);
  gpu::MailboxHolder mailbox_holders[VideoFrame::kMaxPlanes];
  // Set up the planes creating the mailboxes needed to refer to the textures.
  for (size_t i = 0; i < NumGpuMemoryBuffers(output_format_); i++) {
    PlaneResource& plane_resource = frame_resources->plane_resources[i];
    const gfx::BufferFormat buffer_format =
        GpuMemoryBufferFormat(output_format_, i);
    unsigned texture_target = gpu_factories_->ImageTextureTarget(buffer_format);
    // Bind the texture and create or rebind the image.
    if (plane_resource.gpu_memory_buffer && plane_resource.mailbox.IsZero()) {
      uint32_t usage =
          gpu::SHARED_IMAGE_USAGE_GLES2 | gpu::SHARED_IMAGE_USAGE_RASTER |
          gpu::SHARED_IMAGE_USAGE_DISPLAY | gpu::SHARED_IMAGE_USAGE_SCANOUT;
      plane_resource.mailbox =
          sii->CreateSharedImage(plane_resource.gpu_memory_buffer.get(),
                                 gpu_factories_->GpuMemoryBufferManager(),
                                 video_frame->ColorSpace(), usage);
    } else if (!plane_resource.mailbox.IsZero()) {
      sii->UpdateSharedImage(frame_resources->sync_token,
                             plane_resource.mailbox);
    }
    mailbox_holders[i] = gpu::MailboxHolder(plane_resource.mailbox,
                                            gpu::SyncToken(), texture_target);
  }

  // Insert a sync_token, this is needed to make sure that the textures the
  // mailboxes refer to will be used only after all the previous commands posted
  // in the SharedImageInterface have been processed.
  gpu::SyncToken sync_token = sii->GenUnverifiedSyncToken();
  for (size_t i = 0; i < NumGpuMemoryBuffers(output_format_); i++)
    mailbox_holders[i].sync_token = sync_token;

  VideoPixelFormat frame_format = VideoFormat(output_format_);

  // Create the VideoFrame backed by native textures.
  gfx::Size visible_size = video_frame->visible_rect().size();
  scoped_refptr<VideoFrame> frame = VideoFrame::WrapNativeTextures(
      frame_format, mailbox_holders, VideoFrame::ReleaseMailboxCB(), coded_size,
      gfx::Rect(visible_size), video_frame->natural_size(),
      video_frame->timestamp());

  if (!frame) {
    frame_resources->MarkUnused(tick_clock_->NowTicks());
    MailboxHoldersReleased(frame_resources, sync_token);
    CompleteCopyRequestAndMaybeStartNextCopy(std::move(video_frame));
    return;
  }
  frame->SetReleaseMailboxCB(
      base::BindOnce(&PoolImpl::MailboxHoldersReleased, this, frame_resources));

  frame->set_color_space(video_frame->ColorSpace());

  bool allow_overlay = false;
#if defined(OS_WIN)
  // Windows direct composition path only supports dual GMB NV12 video overlays.
  allow_overlay = (output_format_ ==
                   GpuVideoAcceleratorFactories::OutputFormat::NV12_DUAL_GMB);
#else
  switch (output_format_) {
    case GpuVideoAcceleratorFactories::OutputFormat::I420:
      allow_overlay =
          video_frame->metadata()->IsTrue(VideoFrameMetadata::ALLOW_OVERLAY);
      break;
    case GpuVideoAcceleratorFactories::OutputFormat::NV12_SINGLE_GMB:
      allow_overlay = true;
      break;
    case GpuVideoAcceleratorFactories::OutputFormat::NV12_DUAL_GMB:
      // Only used on Windows where we can't use single NV12 textures.
      break;
    case GpuVideoAcceleratorFactories::OutputFormat::XR30:
    case GpuVideoAcceleratorFactories::OutputFormat::XB30:
      // TODO(mcasas): Enable this for ChromeOS https://crbug.com/776093.
      allow_overlay = false;
#if defined(OS_MACOSX)
      allow_overlay = IOSurfaceCanSetColorSpace(video_frame->ColorSpace());
#endif
      // We've converted the YUV to RGB, fix the color space.
      // TODO(hubbe): The libyuv YUV to RGB conversion may not have
      // honored the color space conversion 100%. We should either fix
      // libyuv or find a way for later passes to make up the difference.
      frame->set_color_space(video_frame->ColorSpace().GetAsRGB());
      break;
    case GpuVideoAcceleratorFactories::OutputFormat::RGBA:
    case GpuVideoAcceleratorFactories::OutputFormat::BGRA:
      allow_overlay = true;
      break;
    case GpuVideoAcceleratorFactories::OutputFormat::UNDEFINED:
      break;
  }
#endif  // OS_WIN
  frame->metadata()->MergeMetadataFrom(video_frame->metadata());
  frame->metadata()->SetBoolean(VideoFrameMetadata::ALLOW_OVERLAY,
                                allow_overlay);
  frame->metadata()->SetBoolean(VideoFrameMetadata::READ_LOCK_FENCES_ENABLED,
                                true);

  CompleteCopyRequestAndMaybeStartNextCopy(std::move(frame));
}

// Destroy all the resources posting one task per FrameResources
// to the |media_task_runner_|.
GpuMemoryBufferVideoFramePool::PoolImpl::~PoolImpl() {
  DCHECK(in_shutdown_);
}

void GpuMemoryBufferVideoFramePool::PoolImpl::Shutdown() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  // Clients don't care about copies once shutdown has started, so abort them.
  Abort();

  // Delete all the resources on the media thread.
  in_shutdown_ = true;
  for (auto* frame_resources : resources_pool_) {
    // Will be deleted later upon return to pool.
    if (frame_resources->is_used())
      continue;

    media_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PoolImpl::DeleteFrameResources, gpu_factories_,
                       base::Owned(frame_resources)));
  }
  resources_pool_.clear();
}

void GpuMemoryBufferVideoFramePool::PoolImpl::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

// Tries to find the resources in the pool or create them.
// Incompatible resources will be dropped.
GpuMemoryBufferVideoFramePool::PoolImpl::FrameResources*
GpuMemoryBufferVideoFramePool::PoolImpl::GetOrCreateFrameResources(
    const gfx::Size& size,
    GpuVideoAcceleratorFactories::OutputFormat format) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  auto it = resources_pool_.begin();
  while (it != resources_pool_.end()) {
    FrameResources* frame_resources = *it;
    if (!frame_resources->is_used()) {
      if (AreFrameResourcesCompatible(frame_resources, size)) {
        frame_resources->MarkUsed();
        return frame_resources;
      } else {
        resources_pool_.erase(it++);
        DeleteFrameResources(gpu_factories_, frame_resources);
        delete frame_resources;
      }
    } else {
      it++;
    }
  }

  // Create the resources.
  FrameResources* frame_resources = new FrameResources(size);
  resources_pool_.push_back(frame_resources);
  for (size_t i = 0; i < NumGpuMemoryBuffers(output_format_); i++) {
    PlaneResource& plane_resource = frame_resources->plane_resources[i];
    const size_t width =
        VideoFrame::Columns(i, VideoFormat(format), size.width());
    const size_t height =
        VideoFrame::Rows(i, VideoFormat(format), size.height());
    plane_resource.size = gfx::Size(width, height);

    const gfx::BufferFormat buffer_format = GpuMemoryBufferFormat(format, i);
    plane_resource.gpu_memory_buffer = gpu_factories_->CreateGpuMemoryBuffer(
        plane_resource.size, buffer_format,
        gfx::BufferUsage::SCANOUT_CPU_READ_WRITE);
  }
  return frame_resources;
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
void GpuMemoryBufferVideoFramePool::PoolImpl::DeleteFrameResources(
    GpuVideoAcceleratorFactories* const gpu_factories,
    FrameResources* frame_resources) {
  // TODO(dcastagna): As soon as the context lost is dealt with in media,
  // make sure that we won't execute this callback (use a weak pointer to
  // the old context).
  gpu::SharedImageInterface* sii = gpu_factories->SharedImageInterface();
  if (!sii)
    return;

  for (PlaneResource& plane_resource : frame_resources->plane_resources) {
    if (!plane_resource.mailbox.IsZero()) {
      sii->DestroySharedImage(frame_resources->sync_token,
                              plane_resource.mailbox);
    }
  }
}

// Called when a VideoFrame is no longer referenced.
// Put back the resources in the pool.
void GpuMemoryBufferVideoFramePool::PoolImpl::MailboxHoldersReleased(
    FrameResources* frame_resources,
    const gpu::SyncToken& release_sync_token) {
  if (!media_task_runner_->BelongsToCurrentThread()) {
    media_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&PoolImpl::MailboxHoldersReleased, this,
                                  frame_resources, release_sync_token));
    return;
  }
  frame_resources->sync_token = release_sync_token;

  if (in_shutdown_) {
    DeleteFrameResources(gpu_factories_, frame_resources);
    delete frame_resources;
    return;
  }

  const base::TimeTicks now = tick_clock_->NowTicks();
  frame_resources->MarkUnused(now);
  auto it = resources_pool_.begin();
  while (it != resources_pool_.end()) {
    FrameResources* frame_resources = *it;

    constexpr base::TimeDelta kStaleFrameLimit =
        base::TimeDelta::FromSeconds(10);
    if (!frame_resources->is_used() &&
        now - frame_resources->last_use_time() > kStaleFrameLimit) {
      resources_pool_.erase(it++);
      DeleteFrameResources(gpu_factories_, frame_resources);
      delete frame_resources;
    } else {
      it++;
    }
  }
}

GpuMemoryBufferVideoFramePool::GpuMemoryBufferVideoFramePool() = default;

GpuMemoryBufferVideoFramePool::GpuMemoryBufferVideoFramePool(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    GpuVideoAcceleratorFactories* gpu_factories)
    : pool_impl_(
          new PoolImpl(media_task_runner, worker_task_runner, gpu_factories)) {
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      pool_impl_.get(), "GpuMemoryBufferVideoFramePool", media_task_runner);
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
