// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// Notes about usage of this object by VideoCaptureImplManager.
//
// VideoCaptureImplManager access this object by using a Unretained()
// binding and tasks on the IO thread. It is then important that
// VideoCaptureImpl never post task to itself. All operations must be
// synchronous.

#include "third_party/blink/renderer/platform/video_capture/video_capture_impl.h"

#include <GLES2/gl2extchromium.h>
#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/token.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"
#include "media/capture/mojom/video_capture_buffer.mojom-blink.h"
#include "media/capture/mojom/video_capture_types.mojom-blink.h"
#include "media/capture/video_capture_types.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

#if BUILDFLAG(IS_MAC)
#include "media/base/mac/video_frame_mac.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#endif  // BUILDFLAG(IS_WIN)

namespace blink {

constexpr int kMaxFirstFrameLogs = 5;

BASE_FEATURE(kTimeoutHangingVideoCaptureStarts,
             "TimeoutHangingVideoCaptureStarts",
             base::FEATURE_ENABLED_BY_DEFAULT);

using VideoFrameBufferHandleType = media::mojom::blink::VideoBufferHandle::Tag;

// A collection of all types of handles that we use to reference a camera buffer
// backed with GpuMemoryBuffer.
struct GpuMemoryBufferResources {
  explicit GpuMemoryBufferResources(gfx::GpuMemoryBufferHandle handle)
      : gpu_memory_buffer_handle(std::move(handle)) {}
  // Stores the GpuMemoryBufferHandle when a new buffer is first registered.
  // |gpu_memory_buffer_handle| is converted to |gpu_memory_buffer| below when
  // the camera frame is ready for the first time.
  gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle;
  // The GpuMemoryBuffer backing the camera frame.
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer;
  // The SharedImage created from |gpu_memory_buffer|.
  scoped_refptr<gpu::ClientSharedImage> shared_image;
  // The release sync token for |shared_images|.
  gpu::SyncToken release_sync_token;
};

struct VideoCaptureImpl::BufferContext
    : public ThreadSafeRefCounted<BufferContext> {
 public:
  BufferContext(media::mojom::blink::VideoBufferHandlePtr buffer_handle,
                scoped_refptr<base::SequencedTaskRunner> media_task_runner)
      : buffer_type_(buffer_handle->which()),
        media_task_runner_(media_task_runner) {
    switch (buffer_type_) {
      case VideoFrameBufferHandleType::kUnsafeShmemRegion:
        InitializeFromUnsafeShmemRegion(
            std::move(buffer_handle->get_unsafe_shmem_region()));
        break;
      case VideoFrameBufferHandleType::kReadOnlyShmemRegion:
        InitializeFromReadOnlyShmemRegion(
            std::move(buffer_handle->get_read_only_shmem_region()));
        break;
      case VideoFrameBufferHandleType::kSharedImageHandle:
        InitializeFromSharedImage(
            std::move(buffer_handle->get_shared_image_handle()));
        break;
      case VideoFrameBufferHandleType::kGpuMemoryBufferHandle:
#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_WIN)
        // On macOS, an IOSurfaces passed as a GpuMemoryBufferHandle can be
        // used by both hardware and software paths.
        // https://crbug.com/1125879
        // On Windows, GMBs might be passed by the capture process even if
        // the acceleration disabled during the capture.
        CHECK(media_task_runner_);
#endif
        InitializeFromGpuMemoryBufferHandle(
            std::move(buffer_handle->get_gpu_memory_buffer_handle()));
        break;
    }
  }
  BufferContext(const BufferContext&) = delete;
  BufferContext& operator=(const BufferContext&) = delete;

  VideoFrameBufferHandleType buffer_type() const { return buffer_type_; }
  const uint8_t* data() const { return data_; }
  size_t data_size() const { return data_size_; }
  const base::ReadOnlySharedMemoryRegion* read_only_shmem_region() const {
    return &read_only_shmem_region_;
  }
  const scoped_refptr<gpu::ClientSharedImage>& shared_image() const {
    return shared_image_;
  }
  const gpu::SyncToken& shared_image_sync_token() const {
    return shared_image_sync_token_;
  }
  media::GpuVideoAcceleratorFactories* gpu_factories() const {
    return gpu_factories_;
  }
  void SetGpuFactories(media::GpuVideoAcceleratorFactories* gpu_factories) {
    gpu_factories_ = gpu_factories;
  }
  GpuMemoryBufferResources* gmb_resources() const {
    return gmb_resources_.get();
  }

  gfx::GpuMemoryBufferHandle TakeGpuMemoryBufferHandle() {
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)
    // The same GpuMemoryBuffersHandles will be reused repeatedly by the
    // unaccelerated macOS path. Each of these uses will call this function.
    // Ensure that this function doesn't invalidate the GpuMemoryBufferHandle
    // on macOS for this reason.
    // https://crbug.com/1159722
    // It will also be reused repeatedly if GPU process is unavailable in
    // Windows zero-copy path (e.g. due to repeated GPU process crashes).
    return gmb_resources_->gpu_memory_buffer_handle.Clone();
#else
    return std::move(gmb_resources_->gpu_memory_buffer_handle);
#endif
  }

  void SetGpuMemoryBuffer(
      std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer) {
    gmb_resources_->gpu_memory_buffer = std::move(gpu_memory_buffer);
  }
  gfx::GpuMemoryBuffer* GetGpuMemoryBuffer() {
    return gmb_resources_->gpu_memory_buffer.get();
  }

  static void MailboxHolderReleased(
      scoped_refptr<BufferContext> buffer_context,
      const gpu::SyncToken& release_sync_token,
      std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer) {
    if (!buffer_context->media_task_runner_->RunsTasksInCurrentSequence()) {
      buffer_context->media_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&BufferContext::MailboxHolderReleased, buffer_context,
                         release_sync_token, std::move(gpu_memory_buffer)));
      return;
    }
    buffer_context->gmb_resources_->release_sync_token = release_sync_token;
    // Free |gpu_memory_buffer|.
  }

  static void DestroyTextureOnMediaThread(
      media::GpuVideoAcceleratorFactories* gpu_factories,
      scoped_refptr<gpu::ClientSharedImage> shared_image,
      gpu::SyncToken release_sync_token) {
    if (shared_image) {
      auto* sii = gpu_factories->SharedImageInterface();
      if (!sii)
        return;
      sii->DestroySharedImage(release_sync_token, std::move(shared_image));
    }
  }

  // Public because it may be called after initialization when GPU process
  // dies on Windows to wrap premapped GMBs.
  void InitializeFromUnsafeShmemRegion(base::UnsafeSharedMemoryRegion region) {
    DCHECK(region.IsValid());
    backup_mapping_ = region.Map();
    DCHECK(backup_mapping_.IsValid());
    data_ = backup_mapping_.GetMemoryAsSpan<uint8_t>().data();
    data_size_ = backup_mapping_.size();
  }

 private:
  void InitializeFromReadOnlyShmemRegion(
      base::ReadOnlySharedMemoryRegion region) {
    DCHECK(region.IsValid());
    read_only_mapping_ = region.Map();
    DCHECK(read_only_mapping_.IsValid());
    data_ = read_only_mapping_.GetMemoryAsSpan<uint8_t>().data();
    data_size_ = read_only_mapping_.size();
    read_only_shmem_region_ = std::move(region);
  }

  void InitializeFromSharedImage(
      media::mojom::blink::SharedImageBufferHandleSetPtr shared_image_handle) {
    shared_image_ = gpu::ClientSharedImage::ImportUnowned(
        shared_image_handle->shared_image);
    shared_image_sync_token_ = shared_image_handle->sync_token;
  }

  void InitializeFromGpuMemoryBufferHandle(
      gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle) {
    gmb_resources_ = std::make_unique<GpuMemoryBufferResources>(
        std::move(gpu_memory_buffer_handle));
  }

  friend class ThreadSafeRefCounted<BufferContext>;
  virtual ~BufferContext() {
    if (!gmb_resources_)
      return;
    if (!gmb_resources_->shared_image) {
      return;
    }
    media_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&BufferContext::DestroyTextureOnMediaThread,
                       gpu_factories_, std::move(gmb_resources_->shared_image),
                       gmb_resources_->release_sync_token));
  }

  VideoFrameBufferHandleType buffer_type_;

  // Only valid for |buffer_type_ == SHARED_BUFFER_HANDLE|.
  base::WritableSharedMemoryMapping writable_mapping_;

  // Only valid for |buffer_type_ == READ_ONLY_SHMEM_REGION|.
  base::ReadOnlySharedMemoryRegion read_only_shmem_region_;
  base::ReadOnlySharedMemoryMapping read_only_mapping_;

  // Only valid for |buffer_type == GPU_MEMORY_BUFFER_HANDLE|
  // if on windows, gpu_factories are unavailable, and
  // GMB comes premapped from the capturer.
  base::WritableSharedMemoryMapping backup_mapping_;

  // These point into one of the above mappings, which hold the mapping open for
  // the lifetime of this object.
  raw_ptr<const uint8_t> data_ = nullptr;
  size_t data_size_ = 0;

  // Only valid for |buffer_type_ == SHARED_IMAGE_HANDLE|.
  scoped_refptr<gpu::ClientSharedImage> shared_image_;
  gpu::SyncToken shared_image_sync_token_;

  // The following is for |buffer_type == GPU_MEMORY_BUFFER_HANDLE|.

  // Uses to create SharedImage from |gpu_memory_buffer_|.
  raw_ptr<media::GpuVideoAcceleratorFactories> gpu_factories_ = nullptr;
  // The task runner that |gpu_factories_| runs on.
  const scoped_refptr<base::SequencedTaskRunner> media_task_runner_;

  std::unique_ptr<GpuMemoryBufferResources> gmb_resources_;
};

// Holds data needed to convert `ready_buffer` to a media::VideoFrame.
struct VideoCaptureImpl::VideoFrameInitData {
  media::mojom::blink::ReadyBufferPtr ready_buffer;
  scoped_refptr<BufferContext> buffer_context;
  bool is_webgpu_compatible = false;
  absl::variant<scoped_refptr<media::VideoFrame>,
                std::unique_ptr<gfx::GpuMemoryBuffer>>
      frame_or_buffer;
};

std::optional<VideoCaptureImpl::VideoFrameInitData>
VideoCaptureImpl::CreateVideoFrameInitData(
    media::mojom::blink::ReadyBufferPtr ready_buffer) {
  const auto iter = client_buffers_.find(ready_buffer->buffer_id);
  CHECK(iter != client_buffers_.end());
  scoped_refptr<BufferContext> buffer_context = iter->second;

  VideoFrameInitData video_frame_init_data = {
      .ready_buffer = std::move(ready_buffer),
      .buffer_context = buffer_context};

  switch (buffer_context->buffer_type()) {
    case VideoFrameBufferHandleType::kUnsafeShmemRegion:
      // The frame is backed by a writable (unsafe) shared memory handle, but as
      // it is not sent cross-process the region does not need to be attached to
      // the frame. See also the case for kReadOnlyShmemRegion.
      if (video_frame_init_data.ready_buffer->info->strides) {
        CHECK(
            IsYuvPlanar(
                video_frame_init_data.ready_buffer->info->pixel_format) &&
            (media::VideoFrame::NumPlanes(
                 video_frame_init_data.ready_buffer->info->pixel_format) == 3))
            << "Currently, only YUV formats support custom strides.";
        uint8_t* y_data = const_cast<uint8_t*>(buffer_context->data());
        uint8_t* u_data =
            y_data +
            (media::VideoFrame::Rows(
                 media::VideoFrame::Plane::kY,
                 video_frame_init_data.ready_buffer->info->pixel_format,
                 video_frame_init_data.ready_buffer->info->coded_size
                     .height()) *
             video_frame_init_data.ready_buffer->info->strides
                 ->stride_by_plane[0]);
        uint8_t* v_data =
            u_data +
            (media::VideoFrame::Rows(
                 media::VideoFrame::Plane::kU,
                 video_frame_init_data.ready_buffer->info->pixel_format,
                 video_frame_init_data.ready_buffer->info->coded_size
                     .height()) *
             video_frame_init_data.ready_buffer->info->strides
                 ->stride_by_plane[1]);
        video_frame_init_data.frame_or_buffer =
            media::VideoFrame::WrapExternalYuvData(
                video_frame_init_data.ready_buffer->info->pixel_format,
                gfx::Size(video_frame_init_data.ready_buffer->info->coded_size),
                gfx::Rect(
                    video_frame_init_data.ready_buffer->info->visible_rect),
                video_frame_init_data.ready_buffer->info->visible_rect.size(),
                video_frame_init_data.ready_buffer->info->strides
                    ->stride_by_plane[0],
                video_frame_init_data.ready_buffer->info->strides
                    ->stride_by_plane[1],
                video_frame_init_data.ready_buffer->info->strides
                    ->stride_by_plane[2],
                y_data, u_data, v_data,
                video_frame_init_data.ready_buffer->info->timestamp);
      } else {
        video_frame_init_data.frame_or_buffer =
            media::VideoFrame::WrapExternalData(
                video_frame_init_data.ready_buffer->info->pixel_format,
                gfx::Size(video_frame_init_data.ready_buffer->info->coded_size),
                gfx::Rect(
                    video_frame_init_data.ready_buffer->info->visible_rect),
                video_frame_init_data.ready_buffer->info->visible_rect.size(),
                const_cast<uint8_t*>(buffer_context->data()),
                buffer_context->data_size(),
                video_frame_init_data.ready_buffer->info->timestamp);
      }
      break;
    case VideoFrameBufferHandleType::kReadOnlyShmemRegion: {
      // As with the kSharedBufferHandle type, it is sufficient to just wrap
      // the data without attaching the shared region to the frame.
      scoped_refptr<media::VideoFrame> frame =
          media::VideoFrame::WrapExternalData(
              video_frame_init_data.ready_buffer->info->pixel_format,
              gfx::Size(video_frame_init_data.ready_buffer->info->coded_size),
              gfx::Rect(video_frame_init_data.ready_buffer->info->visible_rect),
              video_frame_init_data.ready_buffer->info->visible_rect.size(),
              const_cast<uint8_t*>(buffer_context->data()),
              buffer_context->data_size(),
              video_frame_init_data.ready_buffer->info->timestamp);
      frame->BackWithSharedMemory(buffer_context->read_only_shmem_region());
      video_frame_init_data.frame_or_buffer = frame;
      break;
    }
    case VideoFrameBufferHandleType::kSharedImageHandle: {
      CHECK(buffer_context->shared_image());
      video_frame_init_data.frame_or_buffer =
          media::VideoFrame::WrapSharedImage(
              video_frame_init_data.ready_buffer->info->pixel_format,
              buffer_context->shared_image(),
              buffer_context->shared_image_sync_token(),
              media::VideoFrame::ReleaseMailboxCB(),
              gfx::Size(video_frame_init_data.ready_buffer->info->coded_size),
              gfx::Rect(video_frame_init_data.ready_buffer->info->visible_rect),
              video_frame_init_data.ready_buffer->info->visible_rect.size(),
              video_frame_init_data.ready_buffer->info->timestamp);
      break;
    }
    case VideoFrameBufferHandleType::kGpuMemoryBufferHandle: {
#if BUILDFLAG(IS_APPLE)
      // On macOS, an IOSurfaces passed as a GpuMemoryBufferHandle can be
      // used by both hardware and software paths.
      // https://crbug.com/1125879
      if (!gpu_factories_ || !media_task_runner_) {
        video_frame_init_data.frame_or_buffer =
            media::VideoFrame::WrapUnacceleratedIOSurface(
                buffer_context->TakeGpuMemoryBufferHandle(),
                gfx::Rect(
                    video_frame_init_data.ready_buffer->info->visible_rect),
                video_frame_init_data.ready_buffer->info->timestamp);
        break;
      }
#endif
#if BUILDFLAG(IS_WIN)
      // The associated shared memory region is mapped only once
      if (video_frame_init_data.ready_buffer->info->is_premapped &&
          !buffer_context->data()) {
        auto gmb_handle = buffer_context->TakeGpuMemoryBufferHandle();
        buffer_context->InitializeFromUnsafeShmemRegion(
            std::move(gmb_handle.region));
        DCHECK(buffer_context->data());
      }
      // On Windows it might happen that the Renderer process loses GPU
      // connection, while the capturer process will continue to produce
      // GPU backed frames.
      if (!gpu_factories_ || !media_task_runner_ || gmb_not_supported_) {
        RequirePremappedFrames();
        if (!video_frame_init_data.ready_buffer->info->is_premapped ||
            !buffer_context->data()) {
          // If the frame isn't premapped, can't do anything here.
          return std::nullopt;
        }

        scoped_refptr<media::VideoFrame> frame =
            media::VideoFrame::WrapExternalData(
                video_frame_init_data.ready_buffer->info->pixel_format,
                gfx::Size(video_frame_init_data.ready_buffer->info->coded_size),
                gfx::Rect(
                    video_frame_init_data.ready_buffer->info->visible_rect),
                video_frame_init_data.ready_buffer->info->visible_rect.size(),
                const_cast<uint8_t*>(buffer_context->data()),
                buffer_context->data_size(),
                video_frame_init_data.ready_buffer->info->timestamp);
        if (!frame) {
          return std::nullopt;
        }
        video_frame_init_data.frame_or_buffer = frame;
        break;
      }
#endif
      CHECK(gpu_factories_);
      CHECK(media_task_runner_);
      // Create GpuMemoryBuffer from handle.
      if (!buffer_context->GetGpuMemoryBuffer()) {
        gfx::BufferFormat gfx_format;
        switch (video_frame_init_data.ready_buffer->info->pixel_format) {
          case media::VideoPixelFormat::PIXEL_FORMAT_NV12:
            gfx_format = gfx::BufferFormat::YUV_420_BIPLANAR;
            break;
          default:
            LOG(FATAL) << "Unsupported pixel format";
        }
        // The GpuMemoryBuffer is allocated and owned by the video capture
        // buffer pool from the video capture service process, so we don't need
        // to destroy the GpuMemoryBuffer here.
        auto gmb =
            gpu_memory_buffer_support_->CreateGpuMemoryBufferImplFromHandle(
                buffer_context->TakeGpuMemoryBufferHandle(),
                gfx::Size(video_frame_init_data.ready_buffer->info->coded_size),
                gfx_format, gfx::BufferUsage::SCANOUT_VEA_CPU_READ,
                base::DoNothing(), gpu_factories_->GpuMemoryBufferManager(),
                pool_);

        // Keep one GpuMemoryBuffer for current GpuMemoryHandle alive,
        // so that any associated structures are kept alive while this buffer id
        // is still used (e.g. DMA buf handles for linux/CrOS).
        buffer_context->SetGpuMemoryBuffer(std::move(gmb));
      }
      CHECK(buffer_context->GetGpuMemoryBuffer());

      auto buffer_handle = buffer_context->GetGpuMemoryBuffer()->CloneHandle();
#if BUILDFLAG(IS_CHROMEOS)
      video_frame_init_data.is_webgpu_compatible =
          buffer_handle.native_pixmap_handle.supports_zero_copy_webgpu_import;
#elif BUILDFLAG(IS_MAC)
      video_frame_init_data.is_webgpu_compatible =
          media::IOSurfaceIsWebGPUCompatible(buffer_handle.io_surface.get());
#elif BUILDFLAG(IS_WIN)
      video_frame_init_data.is_webgpu_compatible =
          buffer_handle.type == gfx::GpuMemoryBufferType::DXGI_SHARED_HANDLE;
#endif
      // No need to propagate shared memory region further as it's already
      // exposed by |buffer_context->data()|.
      buffer_handle.region = base::UnsafeSharedMemoryRegion();
      // The buffer_context might still have a mapped shared memory region.
      // However, it contains valid data only if |is_premapped| is set.
      uint8_t* premapped_data =
          video_frame_init_data.ready_buffer->info->is_premapped
              ? const_cast<uint8_t*>(buffer_context->data())
              : nullptr;
      size_t premapped_data_size =
          video_frame_init_data.ready_buffer->info->is_premapped
              ? buffer_context->data_size()
              : 0;

      // Clone the GpuMemoryBuffer and wrap it in a VideoFrame.
      std::unique_ptr<gfx::GpuMemoryBuffer> buffer =
          gpu_memory_buffer_support_->CreateGpuMemoryBufferImplFromHandle(
              std::move(buffer_handle),
              buffer_context->GetGpuMemoryBuffer()->GetSize(),
              buffer_context->GetGpuMemoryBuffer()->GetFormat(),
              gfx::BufferUsage::SCANOUT_VEA_CPU_READ, base::DoNothing(),
              gpu_factories_->GpuMemoryBufferManager(), pool_,
              base::span<uint8_t>(premapped_data, premapped_data_size));
      if (!buffer) {
        LOG(ERROR) << "Failed to open GpuMemoryBuffer handle";
        return std::nullopt;
      }
      video_frame_init_data.frame_or_buffer = std::move(buffer);
    }
  }
  CHECK(absl::holds_alternative<scoped_refptr<media::VideoFrame>>(
            video_frame_init_data.frame_or_buffer) ||
        absl::holds_alternative<std::unique_ptr<gfx::GpuMemoryBuffer>>(
            video_frame_init_data.frame_or_buffer));
  return video_frame_init_data;
}

// Creates SharedImage mailboxes for |gpu_memory_buffer_handle_| and wraps the
// mailboxes with the buffer handles in a DMA-buf VideoFrame.  The consumer of
// the VideoFrame can access the data either through mailboxes (e.g. display)
// or through the DMA-buf FDs (e.g. video encoder).
bool VideoCaptureImpl::BindVideoFrameOnMediaTaskRunner(
    media::GpuVideoAcceleratorFactories* gpu_factories,
    VideoFrameInitData& video_frame_init_data,
    base::OnceCallback<void()> on_gmb_not_supported) {
  DCHECK(gpu_factories);
  DCHECK_EQ(video_frame_init_data.ready_buffer->info->pixel_format,
            media::PIXEL_FORMAT_NV12);

  CHECK(absl::holds_alternative<std::unique_ptr<gfx::GpuMemoryBuffer>>(
      video_frame_init_data.frame_or_buffer));
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer(
      absl::get<std::unique_ptr<gfx::GpuMemoryBuffer>>(
          video_frame_init_data.frame_or_buffer)
          .release());
  CHECK(gpu_memory_buffer);

  bool should_recreate_shared_image = false;
  if (gpu_factories != video_frame_init_data.buffer_context->gpu_factories()) {
    DVLOG(1) << "GPU context changed; re-creating SharedImage objects";
    video_frame_init_data.buffer_context->SetGpuFactories(gpu_factories);
    should_recreate_shared_image = true;
  }
#if BUILDFLAG(IS_WIN)
  // If the renderer is running in d3d9 mode due to e.g. driver bugs
  // workarounds, DXGI D3D11 textures won't be supported.
  // Can't check this from the ::Initialize() since media context provider can
  // be accessed only on the Media thread.
  gpu::SharedImageInterface* shared_image_interface =
      gpu_factories->SharedImageInterface();
  if (!shared_image_interface ||
      !shared_image_interface->GetCapabilities().shared_image_d3d) {
    std::move(on_gmb_not_supported).Run();
    return false;
  }
#endif

  // Create GPU texture and bind GpuMemoryBuffer to the texture.
  auto* sii = video_frame_init_data.buffer_context->gpu_factories()
                  ->SharedImageInterface();
  if (!sii) {
    DVLOG(1) << "GPU context lost";
    return false;
  }
  // Don't check VideoFrameOutputFormat until we ensure the context has not
  // been lost (if it is lost, then the format will be UNKNOWN).
  const auto output_format =
      video_frame_init_data.buffer_context->gpu_factories()
          ->VideoFrameOutputFormat(
              video_frame_init_data.ready_buffer->info->pixel_format);
  DCHECK(output_format ==
         media::GpuVideoAcceleratorFactories::OutputFormat::NV12);

  // The SharedImages here are used to back VideoFrames. They may be read by the
  // raster interface for format conversion (e.g., for 2-copy import into WebGL)
  // as well as by the GLES2 interface for one-copy import into WebGL.
  gpu::SharedImageUsageSet usage =
      gpu::SHARED_IMAGE_USAGE_GLES2_READ | gpu::SHARED_IMAGE_USAGE_RASTER_READ |
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;
#if BUILDFLAG(IS_APPLE)
  usage |= gpu::SHARED_IMAGE_USAGE_MACOS_VIDEO_TOOLBOX;
#endif
#if BUILDFLAG(IS_CHROMEOS)
  // These SharedImages may be used for zero-copy of VideoFrames into WebGPU.
  usage |= gpu::SHARED_IMAGE_USAGE_WEBGPU_READ;
#endif

  if (should_recreate_shared_image ||
      !video_frame_init_data.buffer_context->gmb_resources()->shared_image) {
    auto multiplanar_si_format = viz::MultiPlaneFormat::kNV12;
#if BUILDFLAG(IS_OZONE)
    multiplanar_si_format.SetPrefersExternalSampler();
#endif
    CHECK_EQ(gpu_memory_buffer->GetFormat(),
             gfx::BufferFormat::YUV_420_BIPLANAR);
    scoped_refptr<gpu::ClientSharedImage> client_shared_image =
        sii->CreateSharedImage(
            {multiplanar_si_format, gpu_memory_buffer->GetSize(),
             video_frame_init_data.ready_buffer->info->color_space,
             gpu::SharedImageUsageSet(usage), "VideoCaptureFrameBuffer"},
            gpu_memory_buffer->CloneHandle());

    CHECK(client_shared_image);
    video_frame_init_data.buffer_context->gmb_resources()->shared_image =
        std::move(client_shared_image);
  } else {
    sii->UpdateSharedImage(video_frame_init_data.buffer_context->gmb_resources()
                               ->release_sync_token,
                           video_frame_init_data.buffer_context->gmb_resources()
                               ->shared_image->mailbox());
  }

  const gpu::SyncToken sync_token = sii->GenVerifiedSyncToken();

  auto& shared_image =
      video_frame_init_data.buffer_context->gmb_resources()->shared_image;
  DCHECK(shared_image);

  const auto gmb_size = gpu_memory_buffer->GetSize();
  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::WrapExternalGpuMemoryBuffer(
          gfx::Rect(video_frame_init_data.ready_buffer->info->visible_rect),
          gmb_size, std::move(gpu_memory_buffer), shared_image, sync_token,
          base::BindOnce(&BufferContext::MailboxHolderReleased,
                         video_frame_init_data.buffer_context),
          video_frame_init_data.ready_buffer->info->timestamp);
  if (!frame) {
    LOG(ERROR) << "Can't wrap GpuMemoryBuffer as VideoFrame";
    return false;
  }

  // For a single multiplanar image, inform the VideoFrame that it
  // should go down the normal SharedImageFormat codepath or the one with
  // ExternalSampler.
  frame->set_shared_image_format_type(
      shared_image->format().PrefersExternalSampler()
          ? media::SharedImageFormatType::kSharedImageFormatExternalSampler
          : media::SharedImageFormatType::kSharedImageFormat);

  frame->metadata().allow_overlay = true;
  frame->metadata().read_lock_fences_enabled = true;
  frame->metadata().is_webgpu_compatible =
      video_frame_init_data.is_webgpu_compatible;
  video_frame_init_data.frame_or_buffer = frame;
  return true;
}

// Information about a video capture client of ours.
struct VideoCaptureImpl::ClientInfo {
  ClientInfo() = default;
  ClientInfo(const ClientInfo& other) = default;
  ~ClientInfo() = default;

  media::VideoCaptureParams params;
  VideoCaptureStateUpdateCB state_update_cb;
  VideoCaptureDeliverFrameCB deliver_frame_cb;
  VideoCaptureSubCaptureTargetVersionCB sub_capture_target_version_cb;
  VideoCaptureNotifyFrameDroppedCB frame_dropped_cb;
};

VideoCaptureImpl::VideoCaptureImpl(
    media::VideoCaptureSessionId session_id,
    scoped_refptr<base::SequencedTaskRunner> main_task_runner,
    const BrowserInterfaceBrokerProxy& browser_interface_broker)
    : device_id_(session_id),
      session_id_(session_id),
      video_capture_host_for_testing_(nullptr),
      state_(blink::VIDEO_CAPTURE_STATE_STOPPED),
      main_task_runner_(std::move(main_task_runner)),
      gpu_memory_buffer_support_(new gpu::GpuMemoryBufferSupport()),
      pool_(base::MakeRefCounted<base::UnsafeSharedMemoryPool>()) {
  CHECK(!session_id.is_empty());
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  DETACH_FROM_THREAD(io_thread_checker_);

  browser_interface_broker.GetInterface(
      pending_video_capture_host_.InitWithNewPipeAndPassReceiver());

  gpu_factories_ = Platform::Current()->GetGpuFactories();
  if (gpu_factories_) {
    media_task_runner_ = gpu_factories_->GetTaskRunner();
  }
  weak_this_ = weak_factory_.GetWeakPtr();
}

void VideoCaptureImpl::OnGpuContextLost(
    base::WeakPtr<VideoCaptureImpl> video_capture_impl) {
  // Called on the main task runner.
  auto* gpu_factories = Platform::Current()->GetGpuFactories();
  Platform::Current()->GetIOTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoCaptureImpl::SetGpuFactoriesHandleOnIOTaskRunner,
                     video_capture_impl, gpu_factories));
}

void VideoCaptureImpl::SetGpuFactoriesHandleOnIOTaskRunner(
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  if (gpu_factories != gpu_factories_) {
    LOG(ERROR) << "GPU factories handle changed; assuming GPU context lost";
    gpu_factories_ = gpu_factories;
  }
}

VideoCaptureImpl::~VideoCaptureImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  if ((state_ == VIDEO_CAPTURE_STATE_STARTING ||
       state_ == VIDEO_CAPTURE_STATE_STARTED) &&
      GetVideoCaptureHost())
    GetVideoCaptureHost()->Stop(device_id_);
}

void VideoCaptureImpl::SuspendCapture(bool suspend) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  if (suspend)
    GetVideoCaptureHost()->Pause(device_id_);
  else
    GetVideoCaptureHost()->Resume(device_id_, session_id_, params_);
}

void VideoCaptureImpl::StartCapture(
    int client_id,
    const media::VideoCaptureParams& params,
    const VideoCaptureStateUpdateCB& state_update_cb,
    const VideoCaptureDeliverFrameCB& deliver_frame_cb,
    const VideoCaptureSubCaptureTargetVersionCB& sub_capture_target_version_cb,
    const VideoCaptureNotifyFrameDroppedCB& frame_dropped_cb) {
  DVLOG(1) << __func__ << " |device_id_| = " << device_id_;
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  OnLog("VideoCaptureImpl got request to start capture.");

  ClientInfo client_info;
  client_info.params = params;
  client_info.state_update_cb = state_update_cb;
  client_info.deliver_frame_cb = deliver_frame_cb;
  client_info.sub_capture_target_version_cb = sub_capture_target_version_cb;
  client_info.frame_dropped_cb = frame_dropped_cb;

  switch (state_) {
    case VIDEO_CAPTURE_STATE_STARTING:
    case VIDEO_CAPTURE_STATE_STARTED:
      clients_[client_id] = client_info;
      OnLog("VideoCaptureImpl capture is already started or starting.");
      // TODO(sheu): Allowing resolution change will require that all
      // outstanding clients of a capture session support resolution change.
      DCHECK_EQ(params_.resolution_change_policy,
                params.resolution_change_policy);
      return;
    case VIDEO_CAPTURE_STATE_STOPPING:
      clients_pending_on_restart_[client_id] = client_info;
      DVLOG(1) << __func__ << " Got new resolution while stopping: "
               << params.requested_format.frame_size.ToString();
      return;
    case VIDEO_CAPTURE_STATE_STOPPED:
    case VIDEO_CAPTURE_STATE_ENDED:
      clients_[client_id] = client_info;
      params_ = params;
      params_.requested_format.frame_rate =
          std::min(params_.requested_format.frame_rate,
                   static_cast<float>(media::limits::kMaxFramesPerSecond));

      DVLOG(1) << "StartCapture: starting with first resolution "
               << params_.requested_format.frame_size.ToString();
      OnLog("VideoCaptureImpl starting capture.");
      StartCaptureInternal();
      return;
    case VIDEO_CAPTURE_STATE_ERROR:
      OnLog("VideoCaptureImpl is in error state.");
      state_update_cb.Run(blink::VIDEO_CAPTURE_STATE_ERROR);
      return;
    case VIDEO_CAPTURE_STATE_ERROR_SYSTEM_PERMISSIONS_DENIED:
      OnLog("VideoCaptureImpl is in system permissions error state.");
      state_update_cb.Run(
          blink::VIDEO_CAPTURE_STATE_ERROR_SYSTEM_PERMISSIONS_DENIED);
      return;
    case VIDEO_CAPTURE_STATE_ERROR_CAMERA_BUSY:
      OnLog("VideoCaptureImpl is in camera busy error state.");
      state_update_cb.Run(blink::VIDEO_CAPTURE_STATE_ERROR_CAMERA_BUSY);
      return;
    case VIDEO_CAPTURE_STATE_PAUSED:
    case VIDEO_CAPTURE_STATE_RESUMED:
      // The internal |state_| is never set to PAUSED/RESUMED since
      // VideoCaptureImpl is not modified by those.
      NOTREACHED_IN_MIGRATION();
      return;
  }
}

void VideoCaptureImpl::StopCapture(int client_id) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  // A client ID can be in only one client list.
  // If this ID is in any client list, we can just remove it from
  // that client list and don't have to run the other following RemoveClient().
  if (!RemoveClient(client_id, &clients_pending_on_restart_)) {
    RemoveClient(client_id, &clients_);
  }

  if (!clients_.empty())
    return;
  DVLOG(1) << "StopCapture: No more client, stopping ...";
  StopDevice();
  client_buffers_.clear();
  weak_factory_.InvalidateWeakPtrs();
}

void VideoCaptureImpl::RequestRefreshFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  GetVideoCaptureHost()->RequestRefreshFrame(device_id_);
}

void VideoCaptureImpl::GetDeviceSupportedFormats(
    VideoCaptureDeviceFormatsCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  GetVideoCaptureHost()->GetDeviceSupportedFormats(
      device_id_, session_id_,
      base::BindOnce(&VideoCaptureImpl::OnDeviceSupportedFormats,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void VideoCaptureImpl::GetDeviceFormatsInUse(
    VideoCaptureDeviceFormatsCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  GetVideoCaptureHost()->GetDeviceFormatsInUse(
      device_id_, session_id_,
      base::BindOnce(&VideoCaptureImpl::OnDeviceFormatsInUse,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void VideoCaptureImpl::OnLog(const String& message) {
  GetVideoCaptureHost()->OnLog(device_id_, message);
}

void VideoCaptureImpl::SetGpuMemoryBufferSupportForTesting(
    std::unique_ptr<gpu::GpuMemoryBufferSupport> gpu_memory_buffer_support) {
  gpu_memory_buffer_support_ = std::move(gpu_memory_buffer_support);
}

void VideoCaptureImpl::OnStateChanged(
    media::mojom::blink::VideoCaptureResultPtr result) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);

  // Stop the startup deadline timer as something has happened.
  startup_timeout_.Stop();

  if (result->which() ==
      media::mojom::blink::VideoCaptureResult::Tag::kErrorCode) {
    DVLOG(1) << __func__ << " Failed with an error.";
    if (result->get_error_code() ==
        media::VideoCaptureError::kWinMediaFoundationSystemPermissionDenied) {
      state_ = VIDEO_CAPTURE_STATE_ERROR_SYSTEM_PERMISSIONS_DENIED;
      OnLog(
          "VideoCaptureImpl changing state to "
          "VIDEO_CAPTURE_STATE_ERROR_SYSTEM_PERMISSIONS_DENIED");
    } else if (result->get_error_code() ==
               media::VideoCaptureError::kWinMediaFoundationCameraBusy) {
      state_ = VIDEO_CAPTURE_STATE_ERROR_CAMERA_BUSY;
      OnLog(
          "VideoCaptureImpl changing state to "
          "VIDEO_CAPTURE_STATE_ERROR_CAMERA_BUSY");
    } else {
      state_ = VIDEO_CAPTURE_STATE_ERROR;
      OnLog("VideoCaptureImpl changing state to VIDEO_CAPTURE_STATE_ERROR");
    }
    for (const auto& client : clients_)
      client.second.state_update_cb.Run(state_);
    clients_.clear();
    RecordStartOutcomeUMA(result->get_error_code());
    return;
  }

  media::mojom::VideoCaptureState state = result->get_state();
  DVLOG(1) << __func__ << " state: " << state;
  switch (state) {
    case media::mojom::VideoCaptureState::STARTED:
      OnLog("VideoCaptureImpl changing state to VIDEO_CAPTURE_STATE_STARTED");
      state_ = VIDEO_CAPTURE_STATE_STARTED;
      for (const auto& client : clients_)
        client.second.state_update_cb.Run(blink::VIDEO_CAPTURE_STATE_STARTED);
      // In case there is any frame dropped before STARTED, always request for
      // a frame refresh to start the video call with.
      // Capture device will make a decision if it should refresh a frame.
      RequestRefreshFrame();
      RecordStartOutcomeUMA(media::VideoCaptureError::kNone);
      break;
    case media::mojom::VideoCaptureState::STOPPED:
      OnLog("VideoCaptureImpl changing state to VIDEO_CAPTURE_STATE_STOPPED");
      state_ = VIDEO_CAPTURE_STATE_STOPPED;
      client_buffers_.clear();
      weak_factory_.InvalidateWeakPtrs();
      if (!clients_.empty() || !clients_pending_on_restart_.empty()) {
        OnLog("VideoCaptureImpl restarting capture");
        RestartCapture();
      }
      break;
    case media::mojom::VideoCaptureState::PAUSED:
      for (const auto& client : clients_)
        client.second.state_update_cb.Run(blink::VIDEO_CAPTURE_STATE_PAUSED);
      break;
    case media::mojom::VideoCaptureState::RESUMED:
      for (const auto& client : clients_)
        client.second.state_update_cb.Run(blink::VIDEO_CAPTURE_STATE_RESUMED);
      break;
    case media::mojom::VideoCaptureState::ENDED:
      OnLog("VideoCaptureImpl changing state to VIDEO_CAPTURE_STATE_ENDED");
      // We'll only notify the client that the stream has stopped.
      for (const auto& client : clients_)
        client.second.state_update_cb.Run(blink::VIDEO_CAPTURE_STATE_STOPPED);
      clients_.clear();
      state_ = VIDEO_CAPTURE_STATE_ENDED;
      break;
  }
}

void VideoCaptureImpl::OnNewBuffer(
    int32_t buffer_id,
    media::mojom::blink::VideoBufferHandlePtr buffer_handle) {
  DVLOG(1) << __func__ << " buffer_id: " << buffer_id;
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);

  const bool inserted =
      client_buffers_
          .emplace(buffer_id, base::MakeRefCounted<BufferContext>(
                                  std::move(buffer_handle), media_task_runner_))
          .second;
  DCHECK(inserted);
}

void VideoCaptureImpl::OnBufferReady(
    media::mojom::blink::ReadyBufferPtr buffer) {
  DVLOG(1) << __func__ << " buffer_id: " << buffer->buffer_id;
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);

  if (state_ != VIDEO_CAPTURE_STATE_STARTED) {
    OnFrameDropped(
        media::VideoCaptureFrameDropReason::kVideoCaptureImplNotInStartedState);
    GetVideoCaptureHost()->ReleaseBuffer(device_id_, buffer->buffer_id,
                                         DefaultFeedback());
    return;
  }

  base::TimeTicks reference_time = *buffer->info->metadata.reference_time;

  if (first_frame_ref_time_.is_null()) {
    first_frame_ref_time_ = reference_time;
    if (num_first_frame_logs_ < kMaxFirstFrameLogs) {
      OnLog("First frame received for this VideoCaptureImpl instance");
      num_first_frame_logs_++;
    } else if (num_first_frame_logs_ == kMaxFirstFrameLogs) {
      OnLog(
          "First frame received for this VideoCaptureImpl instance. This will "
          "not be logged anymore for this VideoCaptureImpl instance.");
      num_first_frame_logs_++;
    }
  }

  // If the timestamp is not prepared, we use reference time to make a rough
  // estimate. e.g. ThreadSafeCaptureOracle::DidCaptureFrame().
  if (buffer->info->timestamp.is_zero())
    buffer->info->timestamp = reference_time - first_frame_ref_time_;

  // If the capture_begin_time was not set use the reference time. This ensures
  // there is a captureTime available for local sources for
  // requestVideoFrameCallback.
  if (!buffer->info->metadata.capture_begin_time)
    buffer->info->metadata.capture_begin_time = reference_time;

  // TODO(qiangchen): Change the metric name to "reference_time" and
  // "timestamp", so that we have consistent naming everywhere.
  // Used by chrome/browser/media/cast_mirroring_performance_browsertest.cc
  TRACE_EVENT_INSTANT2("cast_perf_test", "OnBufferReceived",
                       TRACE_EVENT_SCOPE_THREAD, "timestamp",
                       (reference_time - base::TimeTicks()).InMicroseconds(),
                       "time_delta", buffer->info->timestamp.InMicroseconds());

  const int buffer_id = buffer->buffer_id;
  // Convert `buffer` into a media::VideoFrame or a gfx::GpuMemoryBuffer.
  std::optional<VideoFrameInitData> video_frame_init_data =
      CreateVideoFrameInitData(std::move(buffer));
  if (!video_frame_init_data.has_value()) {
    // Error during initialization of the frame or buffer.
    OnFrameDropped(media::VideoCaptureFrameDropReason::
                       kVideoCaptureImplFailedToWrapDataAsMediaVideoFrame);
    GetVideoCaptureHost()->ReleaseBuffer(device_id_, buffer_id,
                                         DefaultFeedback());
    return;
  }

  if (absl::holds_alternative<std::unique_ptr<gfx::GpuMemoryBuffer>>(
          video_frame_init_data->frame_or_buffer)) {
    // To make the frame ready we must convert gfx::GpuMemoryBuffer to
    // media::VideoFrame on the media task runner.
    media_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(
                       [](media::GpuVideoAcceleratorFactories* gpu_factories,
                          VideoFrameInitData video_frame_init_data,
                          base::OnceCallback<void(VideoFrameInitData)>
                              on_frame_ready_callback,
                          base::OnceCallback<void()> on_gpu_context_lost,
                          base::OnceCallback<void()> on_gmb_not_supported) {
                         if (!VideoCaptureImpl::BindVideoFrameOnMediaTaskRunner(
                                 gpu_factories, video_frame_init_data,
                                 std::move(on_gmb_not_supported))) {
                           // Bind failed.
                           std::move(on_gpu_context_lost).Run();
                           // Proceed to invoke |on_frame_ready_callback| even
                           // though we failed - it takes care of reporting the
                           // frame as dropped when it is set to null.
                           video_frame_init_data.frame_or_buffer =
                               scoped_refptr<media::VideoFrame>(nullptr);
                         }
                         std::move(on_frame_ready_callback)
                             .Run(std::move(video_frame_init_data));
                       },
                       gpu_factories_, std::move(*video_frame_init_data),
                       base::BindPostTaskToCurrentDefault(base::BindOnce(
                           &VideoCaptureImpl::OnVideoFrameReady,
                           weak_factory_.GetWeakPtr(), reference_time)),
                       base::BindPostTask(
                           main_task_runner_,
                           base::BindOnce(&VideoCaptureImpl::OnGpuContextLost,
                                          weak_factory_.GetWeakPtr())),
                       base::BindPostTaskToCurrentDefault(
                           base::BindOnce(&VideoCaptureImpl::OnGmbNotSupported,
                                          weak_factory_.GetWeakPtr()))));
    return;
  }

  // No round-trip to media task runner needed.
  CHECK(absl::holds_alternative<scoped_refptr<media::VideoFrame>>(
      video_frame_init_data->frame_or_buffer));
  OnVideoFrameReady(reference_time, std::move(*video_frame_init_data));
}

void VideoCaptureImpl::OnVideoFrameReady(
    base::TimeTicks reference_time,
    VideoFrameInitData video_frame_init_data) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);

  CHECK(absl::holds_alternative<scoped_refptr<media::VideoFrame>>(
      video_frame_init_data.frame_or_buffer));
  scoped_refptr<media::VideoFrame> video_frame =
      absl::get<scoped_refptr<media::VideoFrame>>(
          video_frame_init_data.frame_or_buffer);

  // If we don't have a media::VideoFrame here then we've failed to convert the
  // gfx::GpuMemoryBuffer, dropping frame.
  if (!video_frame) {
    OnFrameDropped(media::VideoCaptureFrameDropReason::
                       kVideoCaptureImplFailedToWrapDataAsMediaVideoFrame);
    GetVideoCaptureHost()->ReleaseBuffer(
        device_id_, video_frame_init_data.ready_buffer->buffer_id,
        DefaultFeedback());
    return;
  }

  // Ensure the buffer is released when no longer needed by wiring up
  // DidFinishConsumingFrame() as a destruction observer.
  video_frame->AddDestructionObserver(
      base::BindOnce(&VideoCaptureImpl::DidFinishConsumingFrame,
                     base::BindPostTaskToCurrentDefault(base::BindOnce(
                         &VideoCaptureImpl::OnAllClientsFinishedConsumingFrame,
                         weak_factory_.GetWeakPtr(),
                         video_frame_init_data.ready_buffer->buffer_id,
                         std::move(video_frame_init_data.buffer_context)))));
  if (video_frame_init_data.ready_buffer->info->color_space.IsValid()) {
    video_frame->set_color_space(
        video_frame_init_data.ready_buffer->info->color_space);
  }
  video_frame->metadata().MergeMetadataFrom(
      video_frame_init_data.ready_buffer->info->metadata);

  // TODO(qiangchen): Dive into the full code path to let frame metadata hold
  // reference time rather than using an extra parameter.
  for (const auto& client : clients_) {
    client.second.deliver_frame_cb.Run(video_frame, reference_time);
  }
}

void VideoCaptureImpl::OnBufferDestroyed(int32_t buffer_id) {
  DVLOG(1) << __func__ << " buffer_id: " << buffer_id;
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);

  const auto& cb_iter = client_buffers_.find(buffer_id);
  if (cb_iter != client_buffers_.end()) {
    // If the BufferContext is non-null, the GpuMemoryBuffer-backed frames can
    // have more than one reference (held by MailboxHolderReleased). Otherwise,
    // only one reference should be held.
    DCHECK(!cb_iter->second.get() ||
           cb_iter->second->buffer_type() ==
               VideoFrameBufferHandleType::kGpuMemoryBufferHandle ||
           cb_iter->second->HasOneRef())
        << "Instructed to delete buffer we are still using.";
    client_buffers_.erase(cb_iter);
  }
}

void VideoCaptureImpl::OnFrameDropped(
    media::VideoCaptureFrameDropReason reason) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  for (const auto& client : clients_) {
    client.second.frame_dropped_cb.Run(reason);
  }
}

void VideoCaptureImpl::OnNewSubCaptureTargetVersion(
    uint32_t sub_capture_target_version) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);

  for (const auto& client : clients_) {
    client.second.sub_capture_target_version_cb.Run(sub_capture_target_version);
  }
}

constexpr base::TimeDelta VideoCaptureImpl::kCaptureStartTimeout;

void VideoCaptureImpl::OnAllClientsFinishedConsumingFrame(
    int buffer_id,
    scoped_refptr<BufferContext> buffer_context) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);

// Subtle race note: It's important that the |buffer_context| argument be
// std::move()'ed to this method and never copied. This is so that the caller,
// DidFinishConsumingFrame(), does not implicitly retain a reference while it
// is running the trampoline callback on another thread. This is necessary to
// ensure the reference count on the BufferContext will be correct at the time
// OnBufferDestroyed() is called. http://crbug.com/797851
#if DCHECK_IS_ON()
  // The BufferContext should have exactly two references to it at this point,
  // one is this method's second argument and the other is from
  // |client_buffers_|.
  DCHECK(!buffer_context->HasOneRef());
  BufferContext* const buffer_raw_ptr = buffer_context.get();
  buffer_context = nullptr;
  // For non-GMB case, there should be only one reference, from
  // |client_buffers_|. This DCHECK is invalid for GpuMemoryBuffer backed
  // frames, because MailboxHolderReleased may hold on to a reference to
  // |buffer_context|.
  if (buffer_raw_ptr->buffer_type() !=
      VideoFrameBufferHandleType::kGpuMemoryBufferHandle) {
    DCHECK(buffer_raw_ptr->HasOneRef());
  }
#else
  buffer_context = nullptr;
#endif

  if (require_premapped_frames_) {
    feedback_.require_mapped_frame = true;
  }
  GetVideoCaptureHost()->ReleaseBuffer(device_id_, buffer_id, feedback_);
  feedback_ = media::VideoCaptureFeedback();
}

void VideoCaptureImpl::StopDevice() {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  if (state_ != VIDEO_CAPTURE_STATE_STARTING &&
      state_ != VIDEO_CAPTURE_STATE_STARTED)
    return;
  state_ = VIDEO_CAPTURE_STATE_STOPPING;
  OnLog("VideoCaptureImpl changing state to VIDEO_CAPTURE_STATE_STOPPING");
  GetVideoCaptureHost()->Stop(device_id_);
  params_.requested_format.frame_size.SetSize(0, 0);
}

void VideoCaptureImpl::RestartCapture() {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  DCHECK_EQ(state_, VIDEO_CAPTURE_STATE_STOPPED);

  int width = 0;
  int height = 0;
  clients_.insert(clients_pending_on_restart_.begin(),
                  clients_pending_on_restart_.end());
  clients_pending_on_restart_.clear();
  for (const auto& client : clients_) {
    width = std::max(width,
                     client.second.params.requested_format.frame_size.width());
    height = std::max(
        height, client.second.params.requested_format.frame_size.height());
  }
  params_.requested_format.frame_size.SetSize(width, height);
  DVLOG(1) << __func__ << " " << params_.requested_format.frame_size.ToString();
  StartCaptureInternal();
}

void VideoCaptureImpl::StartCaptureInternal() {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  state_ = VIDEO_CAPTURE_STATE_STARTING;
  OnLog("VideoCaptureImpl changing state to VIDEO_CAPTURE_STATE_STARTING");

  if (base::FeatureList::IsEnabled(kTimeoutHangingVideoCaptureStarts)) {
    startup_timeout_.Start(FROM_HERE, kCaptureStartTimeout,
                           base::BindOnce(&VideoCaptureImpl::OnStartTimedout,
                                          base::Unretained(this)));
  }
  start_outcome_reported_ = false;
  base::UmaHistogramBoolean("Media.VideoCapture.Start", true);

  GetVideoCaptureHost()->Start(device_id_, session_id_, params_,
                               observer_receiver_.BindNewPipeAndPassRemote());
}

void VideoCaptureImpl::OnStartTimedout() {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  OnLog("VideoCaptureImpl timed out during starting");

  OnStateChanged(media::mojom::blink::VideoCaptureResult::NewErrorCode(
      media::VideoCaptureError::kVideoCaptureImplTimedOutOnStart));
}

void VideoCaptureImpl::OnDeviceSupportedFormats(
    VideoCaptureDeviceFormatsCallback callback,
    const Vector<media::VideoCaptureFormat>& supported_formats) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  std::move(callback).Run(supported_formats);
}

void VideoCaptureImpl::OnDeviceFormatsInUse(
    VideoCaptureDeviceFormatsCallback callback,
    const Vector<media::VideoCaptureFormat>& formats_in_use) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  std::move(callback).Run(formats_in_use);
}

bool VideoCaptureImpl::RemoveClient(int client_id, ClientInfoMap* clients) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);

  const ClientInfoMap::iterator it = clients->find(client_id);
  if (it == clients->end())
    return false;

  it->second.state_update_cb.Run(blink::VIDEO_CAPTURE_STATE_STOPPED);
  clients->erase(it);
  return true;
}

media::mojom::blink::VideoCaptureHost* VideoCaptureImpl::GetVideoCaptureHost() {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  if (video_capture_host_for_testing_)
    return video_capture_host_for_testing_;

  if (!video_capture_host_.is_bound())
    video_capture_host_.Bind(std::move(pending_video_capture_host_));
  return video_capture_host_.get();
}

void VideoCaptureImpl::RecordStartOutcomeUMA(
    media::VideoCaptureError error_code) {
  // Record the success or failure of starting only the first time we transition
  // into such a state, not eg when resuming after pausing.
  if (!start_outcome_reported_) {
    VideoCaptureStartOutcome outcome;
    switch (error_code) {
      case media::VideoCaptureError::kNone:
        outcome = VideoCaptureStartOutcome::kStarted;
        break;
      case media::VideoCaptureError::kVideoCaptureImplTimedOutOnStart:
        outcome = VideoCaptureStartOutcome::kTimedout;
        break;
      default:
        outcome = VideoCaptureStartOutcome::kFailed;
        break;
    }
    base::UmaHistogramEnumeration("Media.VideoCapture.StartOutcome", outcome);
    base::UmaHistogramEnumeration("Media.VideoCapture.StartErrorCode",
                                  error_code);
    start_outcome_reported_ = true;
  }
}

// static
void VideoCaptureImpl::DidFinishConsumingFrame(
    BufferFinishedCallback callback_to_io_thread) {
  // Note: This function may be called on any thread by the VideoFrame
  // destructor.  |metadata| is still valid for read-access at this point.
  std::move(callback_to_io_thread).Run();
}

void VideoCaptureImpl::ProcessFeedback(
    const media::VideoCaptureFeedback& feedback) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  feedback_ = feedback;
}

void VideoCaptureImpl::OnGmbNotSupported() {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  RequirePremappedFrames();
  gmb_not_supported_ = true;
}

void VideoCaptureImpl::RequirePremappedFrames() {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  require_premapped_frames_ = true;
}

media::VideoCaptureFeedback VideoCaptureImpl::DefaultFeedback() {
  media::VideoCaptureFeedback feedback;
  feedback.require_mapped_frame = require_premapped_frames_;
  return feedback;
}

base::WeakPtr<VideoCaptureImpl> VideoCaptureImpl::GetWeakPtr() {
  return weak_this_;
}

}  // namespace blink
