// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Notes about usage of this object by VideoCaptureImplManager.
//
// VideoCaptureImplManager access this object by using a Unretained()
// binding and tasks on the IO thread. It is then important that
// VideoCaptureImpl never post task to itself. All operations must be
// synchronous.

#include "third_party/blink/renderer/platform/video_capture/video_capture_impl.h"

#include <stddef.h>
#include <algorithm>
#include <memory>
#include <utility>

#include <GLES2/gl2extchromium.h>
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/token.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "media/capture/mojom/video_capture_buffer.mojom-blink.h"
#include "media/capture/mojom/video_capture_types.mojom-blink.h"
#include "media/capture/video_capture_types.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

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
  gpu::Mailbox mailboxes[media::VideoFrame::kMaxPlanes];
  // The release sync token for |mailboxes|.
  gpu::SyncToken release_sync_token;
};

struct VideoCaptureImpl::BufferContext
    : public base::RefCountedThreadSafe<BufferContext> {
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
      case VideoFrameBufferHandleType::kMailboxHandles:
        InitializeFromMailbox(std::move(buffer_handle->get_mailbox_handles()));
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
  const Vector<gpu::MailboxHolder>& mailbox_holders() const {
    return mailbox_holders_;
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
      gpu::Mailbox mailbox,
      gpu::SyncToken release_sync_token) {
    if (!mailbox.IsZero()) {
      auto* sii = gpu_factories->SharedImageInterface();
      if (!sii)
        return;
      sii->DestroySharedImage(release_sync_token, mailbox);
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

  void InitializeFromMailbox(
      media::mojom::blink::MailboxBufferHandleSetPtr mailbox_handles) {
    DCHECK_EQ(media::VideoFrame::kMaxPlanes,
              mailbox_handles->mailbox_holder.size());
    mailbox_holders_ = std::move(mailbox_handles->mailbox_holder);
  }

  void InitializeFromGpuMemoryBufferHandle(
      gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle) {
    gmb_resources_ = std::make_unique<GpuMemoryBufferResources>(
        std::move(gpu_memory_buffer_handle));
  }

  friend class base::RefCountedThreadSafe<BufferContext>;
  virtual ~BufferContext() {
    if (!gmb_resources_)
      return;
    for (size_t plane = 0; plane < media::VideoFrame::kMaxPlanes; ++plane) {
      if (!gmb_resources_->mailboxes[plane].IsSharedImage())
        continue;
      media_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&BufferContext::DestroyTextureOnMediaThread,
                         gpu_factories_, gmb_resources_->mailboxes[plane],
                         gmb_resources_->release_sync_token));
    }
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
  const uint8_t* data_ = nullptr;
  size_t data_size_ = 0;

  // Only valid for |buffer_type_ == MAILBOX_HANDLES|.
  Vector<gpu::MailboxHolder> mailbox_holders_;

  // The following is for |buffer_type == GPU_MEMORY_BUFFER_HANDLE|.

  // Uses to create SharedImage from |gpu_memory_buffer_|.
  media::GpuVideoAcceleratorFactories* gpu_factories_ = nullptr;
  // The task runner that |gpu_factories_| runs on.
  const scoped_refptr<base::SequencedTaskRunner> media_task_runner_;

  std::unique_ptr<GpuMemoryBufferResources> gmb_resources_;
};

VideoCaptureImpl::VideoFrameBufferPreparer::VideoFrameBufferPreparer(
    VideoCaptureImpl& video_capture_impl,
    media::mojom::blink::ReadyBufferPtr ready_buffer)
    : video_capture_impl_(video_capture_impl),
      buffer_id_(ready_buffer->buffer_id),
      frame_info_(std::move(ready_buffer->info)) {}

int32_t VideoCaptureImpl::VideoFrameBufferPreparer::buffer_id() const {
  return buffer_id_;
}

const media::mojom::blink::VideoFrameInfoPtr&
VideoCaptureImpl::VideoFrameBufferPreparer::frame_info() const {
  return frame_info_;
}

scoped_refptr<media::VideoFrame>
VideoCaptureImpl::VideoFrameBufferPreparer::frame() const {
  return frame_;
}

scoped_refptr<VideoCaptureImpl::BufferContext>
VideoCaptureImpl::VideoFrameBufferPreparer::buffer_context() const {
  return buffer_context_;
}

bool VideoCaptureImpl::VideoFrameBufferPreparer::Initialize() {
  // Prior to initializing, |frame_| and |gpu_memory_buffer_| are null.
  DCHECK(!frame_ && !gpu_memory_buffer_);
  const auto& iter = video_capture_impl_.client_buffers_.find(buffer_id_);
  DCHECK(iter != video_capture_impl_.client_buffers_.end());
  buffer_context_ = iter->second;
  switch (buffer_context_->buffer_type()) {
    case VideoFrameBufferHandleType::kUnsafeShmemRegion:
      // The frame is backed by a writable (unsafe) shared memory handle, but as
      // it is not sent cross-process the region does not need to be attached to
      // the frame. See also the case for kReadOnlyShmemRegion.
      if (frame_info_->strides) {
        CHECK(IsYuvPlanar(frame_info_->pixel_format) &&
              (media::VideoFrame::NumPlanes(frame_info_->pixel_format) == 3))
            << "Currently, only YUV formats support custom strides.";
        uint8_t* y_data = const_cast<uint8_t*>(buffer_context_->data());
        uint8_t* u_data =
            y_data + (media::VideoFrame::Rows(
                          media::VideoFrame::kYPlane, frame_info_->pixel_format,
                          frame_info_->coded_size.height()) *
                      frame_info_->strides->stride_by_plane[0]);
        uint8_t* v_data =
            u_data + (media::VideoFrame::Rows(
                          media::VideoFrame::kUPlane, frame_info_->pixel_format,
                          frame_info_->coded_size.height()) *
                      frame_info_->strides->stride_by_plane[1]);
        frame_ = media::VideoFrame::WrapExternalYuvData(
            frame_info_->pixel_format, gfx::Size(frame_info_->coded_size),
            gfx::Rect(frame_info_->visible_rect),
            frame_info_->visible_rect.size(),
            frame_info_->strides->stride_by_plane[0],
            frame_info_->strides->stride_by_plane[1],
            frame_info_->strides->stride_by_plane[2], y_data, u_data, v_data,
            frame_info_->timestamp);
      } else {
        frame_ = media::VideoFrame::WrapExternalData(
            frame_info_->pixel_format, gfx::Size(frame_info_->coded_size),
            gfx::Rect(frame_info_->visible_rect),
            frame_info_->visible_rect.size(),
            const_cast<uint8_t*>(buffer_context_->data()),
            buffer_context_->data_size(), frame_info_->timestamp);
      }
      break;
    case VideoFrameBufferHandleType::kReadOnlyShmemRegion:
      // As with the kSharedBufferHandle type, it is sufficient to just wrap
      // the data without attaching the shared region to the frame.
      frame_ = media::VideoFrame::WrapExternalData(
          frame_info_->pixel_format, gfx::Size(frame_info_->coded_size),
          gfx::Rect(frame_info_->visible_rect),
          frame_info_->visible_rect.size(),
          const_cast<uint8_t*>(buffer_context_->data()),
          buffer_context_->data_size(), frame_info_->timestamp);
      frame_->BackWithSharedMemory(buffer_context_->read_only_shmem_region());
      break;
    case VideoFrameBufferHandleType::kMailboxHandles: {
      gpu::MailboxHolder mailbox_holder_array[media::VideoFrame::kMaxPlanes];
      CHECK_EQ(media::VideoFrame::kMaxPlanes,
               buffer_context_->mailbox_holders().size());
      for (int i = 0; i < media::VideoFrame::kMaxPlanes; i++) {
        mailbox_holder_array[i] = buffer_context_->mailbox_holders()[i];
      }
      frame_ = media::VideoFrame::WrapNativeTextures(
          frame_info_->pixel_format, mailbox_holder_array,
          media::VideoFrame::ReleaseMailboxCB(),
          gfx::Size(frame_info_->coded_size),
          gfx::Rect(frame_info_->visible_rect),
          frame_info_->visible_rect.size(), frame_info_->timestamp);
      break;
    }
    case VideoFrameBufferHandleType::kGpuMemoryBufferHandle: {
#if BUILDFLAG(IS_APPLE)
      // On macOS, an IOSurfaces passed as a GpuMemoryBufferHandle can be
      // used by both hardware and software paths.
      // https://crbug.com/1125879
      if (!video_capture_impl_.gpu_factories_ ||
          !video_capture_impl_.media_task_runner_) {
        frame_ = media::VideoFrame::WrapUnacceleratedIOSurface(
            buffer_context_->TakeGpuMemoryBufferHandle(),
            gfx::Rect(frame_info_->visible_rect), frame_info_->timestamp);
        break;
      }
#endif
#if BUILDFLAG(IS_WIN)
      // The associated shared memory region is mapped only once
      if (frame_info_->is_premapped && !buffer_context_->data()) {
        auto gmb_handle = buffer_context_->TakeGpuMemoryBufferHandle();
        buffer_context_->InitializeFromUnsafeShmemRegion(
            std::move(gmb_handle.region));
        DCHECK(buffer_context_->data());
      }
      // On Windows it might happen that the Renderer process loses GPU
      // connection, while the capturer process will continue to produce
      // GPU backed frames.
      if (!video_capture_impl_.gpu_factories_ ||
          !video_capture_impl_.media_task_runner_ ||
          video_capture_impl_.gmb_not_supported_) {
        video_capture_impl_.RequirePremappedFrames();
        if (!frame_info_->is_premapped || !buffer_context_->data()) {
          // If the frame isn't premapped, can't do anything here.
          return false;
        }

        frame_ = media::VideoFrame::WrapExternalData(
            frame_info_->pixel_format, gfx::Size(frame_info_->coded_size),
            gfx::Rect(frame_info_->visible_rect),
            frame_info_->visible_rect.size(),
            const_cast<uint8_t*>(buffer_context_->data()),
            buffer_context_->data_size(), frame_info_->timestamp);

        if (!frame_) {
          return false;
        }
        break;
      }
#endif
      CHECK(video_capture_impl_.gpu_factories_);
      CHECK(video_capture_impl_.media_task_runner_);
      // Create GpuMemoryBuffer from handle.
      if (!buffer_context_->GetGpuMemoryBuffer()) {
        gfx::BufferFormat gfx_format;
        switch (frame_info_->pixel_format) {
          case media::VideoPixelFormat::PIXEL_FORMAT_NV12:
            gfx_format = gfx::BufferFormat::YUV_420_BIPLANAR;
            break;
          default:
            LOG(FATAL) << "Unsupported pixel format";
            return false;
        }
        // The GpuMemoryBuffer is allocated and owned by the video capture
        // buffer pool from the video capture service process, so we don't need
        // to destroy the GpuMemoryBuffer here.
        auto gmb =
            video_capture_impl_.gpu_memory_buffer_support_
                ->CreateGpuMemoryBufferImplFromHandle(
                    buffer_context_->TakeGpuMemoryBufferHandle(),
                    gfx::Size(frame_info_->coded_size), gfx_format,
                    gfx::BufferUsage::SCANOUT_VEA_CPU_READ, base::DoNothing(),
                    video_capture_impl_.gpu_factories_
                        ->GpuMemoryBufferManager(),
                    video_capture_impl_.pool_);

        // Keep one GpuMemoryBuffer for current GpuMemoryHandle alive,
        // so that any associated structures are kept alive while this buffer id
        // is still used (e.g. DMA buf handles for linux/CrOS).
        buffer_context_->SetGpuMemoryBuffer(std::move(gmb));
      }
      CHECK(buffer_context_->GetGpuMemoryBuffer());

      auto buffer_handle = buffer_context_->GetGpuMemoryBuffer()->CloneHandle();
#if BUILDFLAG(IS_CHROMEOS)
      is_webgpu_compatible_ =
          buffer_handle.native_pixmap_handle.supports_zero_copy_webgpu_import;
#endif
      // No need to propagate shared memory region further as it's already
      // exposed by |buffer_context_->data()|.
      buffer_handle.region = base::UnsafeSharedMemoryRegion();
      // The buffer_context_ might still have a mapped shared memory region.
      // However, it contains valid data only if |is_premapped| is set.
      uint8_t* premapped_data =
          frame_info_->is_premapped
              ? const_cast<uint8_t*>(buffer_context_->data())
              : nullptr;

      // Clone the GpuMemoryBuffer and wrap it in a VideoFrame.
      gpu_memory_buffer_ =
          video_capture_impl_.gpu_memory_buffer_support_
              ->CreateGpuMemoryBufferImplFromHandle(
                  std::move(buffer_handle),
                  buffer_context_->GetGpuMemoryBuffer()->GetSize(),
                  buffer_context_->GetGpuMemoryBuffer()->GetFormat(),
                  gfx::BufferUsage::SCANOUT_VEA_CPU_READ, base::DoNothing(),
                  video_capture_impl_.gpu_factories_->GpuMemoryBufferManager(),
                  video_capture_impl_.pool_,
                  base::span<uint8_t>(premapped_data,
                                      buffer_context_->data_size()));
      if (!gpu_memory_buffer_) {
        LOG(ERROR) << "Failed to open GpuMemoryBuffer handle";
        return false;
      }
    }
  }
  // After initializing, either |frame_| or |gpu_memory_buffer_| has been set.
  DCHECK(frame_ || gpu_memory_buffer_);
  return true;
}

bool VideoCaptureImpl::VideoFrameBufferPreparer::IsVideoFrameBound() const {
  return frame_.get();
}

// Creates SharedImage mailboxes for |gpu_memory_buffer_handle_| and wraps the
// mailboxes with the buffer handles in a DMA-buf VideoFrame.  The consumer of
// the VideoFrame can access the data either through mailboxes (e.g. display)
// or through the DMA-buf FDs (e.g. video encoder).
bool VideoCaptureImpl::VideoFrameBufferPreparer::BindVideoFrameOnMediaThread(
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  DCHECK(gpu_factories);
  DCHECK(!IsVideoFrameBound());
  DCHECK_EQ(frame_info_->pixel_format, media::PIXEL_FORMAT_NV12);

  bool should_recreate_shared_image = false;
  if (gpu_factories != buffer_context_->gpu_factories()) {
    DVLOG(1) << "GPU context changed; re-creating SharedImage objects";
    buffer_context_->SetGpuFactories(gpu_factories);
    should_recreate_shared_image = true;
  }
#if BUILDFLAG(IS_WIN)
  // If the renderer is running in d3d9 mode due to e.g. driver bugs
  // workarounds, DXGI D3D11 textures won't be supported.
  // Can't check this from the ::Initialize() since media context provider can
  // be accessed only on the Media thread.
  const gpu::Capabilities* context_capabilites =
      gpu_factories->ContextCapabilities();
  if (!context_capabilites || !context_capabilites->shared_image_d3d) {
    video_capture_impl_.RequirePremappedFrames();
    video_capture_impl_.gmb_not_supported_ = true;
    return false;
  }
#endif

  // Create GPU texture and bind GpuMemoryBuffer to the texture.
  auto* sii = buffer_context_->gpu_factories()->SharedImageInterface();
  if (!sii) {
    DVLOG(1) << "GPU context lost";
    return false;
  }
  // Don't check VideoFrameOutputFormat until we ensure the context has not
  // been lost (if it is lost, then the format will be UNKNOWN).
  const auto output_format =
      buffer_context_->gpu_factories()->VideoFrameOutputFormat(
          frame_info_->pixel_format);
  DCHECK(
      output_format ==
          media::GpuVideoAcceleratorFactories::OutputFormat::NV12_SINGLE_GMB ||
      output_format ==
          media::GpuVideoAcceleratorFactories::OutputFormat::NV12_DUAL_GMB);

  std::vector<gfx::BufferPlane> planes;

  uint32_t usage =
      gpu::SHARED_IMAGE_USAGE_GLES2 | gpu::SHARED_IMAGE_USAGE_RASTER |
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;
#if BUILDFLAG(IS_APPLE)
  usage |= gpu::SHARED_IMAGE_USAGE_MACOS_VIDEO_TOOLBOX;
#endif
#if BUILDFLAG(IS_CHROMEOS)
  usage |= gpu::SHARED_IMAGE_USAGE_WEBGPU;
#endif

  // The feature flags here are a little subtle:
  // * IsMultiPlaneFormatForHardwareVideoEnabled() controls whether Multiplanar
  //   SI is used (i.e., whether a single SharedImage is created via passing a
  //   viz::MultiPlaneFormat rather than the legacy codepath of passing a
  //   GMB).
  // * kMultiPlaneVideoCaptureSharedImages controls whether planes are sampled
  //   individually rather than using external sampling.
  //
  // These two flags are orthogonal:
  // * If both flags are true, one SharedImage with format MultiPlaneFormat::
  //   kNV12 will be created.
  // * If using multiplane SI without per-plane sampling, one SharedImage with
  //   format MultiPlaneFormat::kNV12 configured to use external sampling
  //   will be created (this is supported only on Ozone-based platforms and
  //   not expected to be requested on other platforms).
  // * If using per-plane sampling without multiplane SI, one SharedImage will
  //   be created for each plane via the legacy "pass GMB" entrypoint.
  // * If both flags are false, one SharedImage will be created via the legacy
  //   "pass GMB" entrypoint (this uses external sampling on the other side
  //   based on the format of the GMB).
  bool create_multiplanar_image =
      media::IsMultiPlaneFormatForHardwareVideoEnabled();
  bool use_per_plane_sampling =
      base::FeatureList::IsEnabled(media::kMultiPlaneVideoCaptureSharedImages);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  // External sampling isn't supported on Windows/Mac with Multiplane SI (it's
  // not supported with legacy SI either for that matter, but we restricted
  // the CHECK here to Multiplane SI as in the case of legacy SI the flow is
  // more nebulous and we wanted to restrict any impact here to the Multiplane
  // SI flow).
  // NOTE: This CHECK would ideally be done if !BUILDFLAG(IS_OZONE), but this
  // codepath is entered in tests for Android, which does not have
  // kMultiPlaneVideoCaptureSharedImages set. This codepath is not entered in
  // production for Android (see
  // https://chromium-review.googlesource.com/c/chromium/src/+/4640009/comment/29c99ef9_587e49dc/
  // for a detailed discussion).
  CHECK(!create_multiplanar_image || use_per_plane_sampling);
#endif

  if (create_multiplanar_image || !use_per_plane_sampling) {
    planes.push_back(gfx::BufferPlane::DEFAULT);
  } else {
    // Using per-plane sampling without multiplane SI.
    planes.push_back(gfx::BufferPlane::Y);
    planes.push_back(gfx::BufferPlane::UV);
  }
  CHECK(planes.size() == 1 || !create_multiplanar_image);

  for (size_t plane = 0; plane < planes.size(); ++plane) {
    if (should_recreate_shared_image ||
        buffer_context_->gmb_resources()->mailboxes[plane].IsZero()) {
      auto multiplanar_si_format = viz::MultiPlaneFormat::kNV12;
#if BUILDFLAG(IS_OZONE)
      if (!use_per_plane_sampling) {
        multiplanar_si_format.SetPrefersExternalSampler();
      }
#endif
      CHECK_EQ(gpu_memory_buffer_->GetFormat(),
               gfx::BufferFormat::YUV_420_BIPLANAR);
      buffer_context_->gmb_resources()->mailboxes[plane] =
          create_multiplanar_image
              ? sii->CreateSharedImage(
                    multiplanar_si_format, gpu_memory_buffer_->GetSize(),
                    frame_info_->color_space, kTopLeft_GrSurfaceOrigin,
                    kPremul_SkAlphaType, usage, "VideoCaptureFrameBuffer",
                    gpu_memory_buffer_->CloneHandle())
              : sii->CreateSharedImage(
                    gpu_memory_buffer_.get(),
                    buffer_context_->gpu_factories()->GpuMemoryBufferManager(),
                    planes[plane], frame_info_->color_space,
                    kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage,
                    "VideoCaptureFrameBuffer");
    } else {
      sii->UpdateSharedImage(
          buffer_context_->gmb_resources()->release_sync_token,
          buffer_context_->gmb_resources()->mailboxes[plane]);
    }
  }

  const unsigned texture_target =
#if BUILDFLAG(IS_LINUX)
      // Explicitly set GL_TEXTURE_EXTERNAL_OES as the
      // `media::VideoFrame::RequiresExternalSampler()` requires it for NV12
      // format, while the `ImageTextureTarget()` will return GL_TEXTURE_2D.
      (frame_info_->pixel_format == media::PIXEL_FORMAT_NV12)
          ? GL_TEXTURE_EXTERNAL_OES
          :
#endif
          buffer_context_->gpu_factories()->ImageTextureTarget(
              gpu_memory_buffer_->GetFormat());

  const gpu::SyncToken sync_token = sii->GenVerifiedSyncToken();

  gpu::MailboxHolder mailbox_holder_array[media::VideoFrame::kMaxPlanes];
  for (size_t plane = 0; plane < planes.size(); ++plane) {
    DCHECK(!buffer_context_->gmb_resources()->mailboxes[plane].IsZero());
    DCHECK(buffer_context_->gmb_resources()->mailboxes[plane].IsSharedImage());
    mailbox_holder_array[plane] =
        gpu::MailboxHolder(buffer_context_->gmb_resources()->mailboxes[plane],
                           sync_token, texture_target);
  }

  const auto gmb_size = gpu_memory_buffer_->GetSize();
  frame_ = media::VideoFrame::WrapExternalGpuMemoryBuffer(
      gfx::Rect(frame_info_->visible_rect), gmb_size,
      std::move(gpu_memory_buffer_), mailbox_holder_array,
      base::BindOnce(&BufferContext::MailboxHolderReleased, buffer_context_),
      frame_info_->timestamp);
  if (!frame_) {
    LOG(ERROR) << "Can't wrap GpuMemoryBuffer as VideoFrame";
    return false;
  }

  // If we created a single multiplanar image, inform the VideoFrame that it
  // should go down the normal SharedImageFormat codepath rather than the
  // codepath used for legacy multiplanar formats.
  if (create_multiplanar_image) {
    frame_->set_shared_image_format_type(
        use_per_plane_sampling
            ? media::SharedImageFormatType::kSharedImageFormat
            : media::SharedImageFormatType::kSharedImageFormatExternalSampler);
  }

  frame_->metadata().allow_overlay = true;
  frame_->metadata().read_lock_fences_enabled = true;
#if BUILDFLAG(IS_CHROMEOS)
  frame_->metadata().is_webgpu_compatible = is_webgpu_compatible_;
#endif
  return true;
}

void VideoCaptureImpl::VideoFrameBufferPreparer::Finalize() {
  DCHECK(frame_);
  frame_->AddDestructionObserver(
      base::BindOnce(&VideoCaptureImpl::DidFinishConsumingFrame,
                     base::BindPostTaskToCurrentDefault(base::BindOnce(
                         &VideoCaptureImpl::OnAllClientsFinishedConsumingFrame,
                         video_capture_impl_.weak_factory_.GetWeakPtr(),
                         buffer_id_, buffer_context_))));
  if (frame_info_->color_space.IsValid()) {
    frame_->set_color_space(frame_info_->color_space);
  }
  frame_->metadata().MergeMetadataFrom(frame_info_->metadata);
}

// Information about a video capture client of ours.
struct VideoCaptureImpl::ClientInfo {
  ClientInfo() = default;

  ClientInfo(const ClientInfo& other) = default;

  ~ClientInfo() = default;

  media::VideoCaptureParams params;

  VideoCaptureStateUpdateCB state_update_cb;

  VideoCaptureDeliverFrameCB deliver_frame_cb;

  VideoCaptureCropVersionCB crop_version_cb;
};

VideoCaptureImpl::VideoCaptureImpl(
    media::VideoCaptureSessionId session_id,
    scoped_refptr<base::SequencedTaskRunner> main_task_runner,
    BrowserInterfaceBrokerProxy* browser_interface_broker)
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

  browser_interface_broker->GetInterface(
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
    const VideoCaptureCropVersionCB& crop_version_cb) {
  DVLOG(1) << __func__ << " |device_id_| = " << device_id_;
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  OnLog("VideoCaptureImpl got request to start capture.");

  ClientInfo client_info;
  client_info.params = params;
  client_info.state_update_cb = state_update_cb;
  client_info.deliver_frame_cb = deliver_frame_cb;
  client_info.crop_version_cb = crop_version_cb;

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
    case VIDEO_CAPTURE_STATE_PAUSED:
    case VIDEO_CAPTURE_STATE_RESUMED:
      // The internal |state_| is never set to PAUSED/RESUMED since
      // VideoCaptureImpl is not modified by those.
      NOTREACHED();
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

void VideoCaptureImpl::OnFrameDropped(
    media::VideoCaptureFrameDropReason reason) {
  GetVideoCaptureHost()->OnFrameDropped(device_id_, reason);
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
          .emplace(buffer_id, new BufferContext(std::move(buffer_handle),
                                                media_task_runner_))
          .second;
  DCHECK(inserted);
}

void VideoCaptureImpl::OnBufferReady(
    media::mojom::blink::ReadyBufferPtr buffer,
    Vector<media::mojom::blink::ReadyBufferPtr> scaled_buffers) {
  DVLOG(1) << __func__ << " buffer_id: " << buffer->buffer_id;
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);

  bool consume_buffer = state_ == VIDEO_CAPTURE_STATE_STARTED;
  if (!consume_buffer) {
    OnFrameDropped(
        media::VideoCaptureFrameDropReason::kVideoCaptureImplNotInStartedState);
    GetVideoCaptureHost()->ReleaseBuffer(device_id_, buffer->buffer_id,
                                         DefaultFeedback());
    for (auto& scaled_buffer : scaled_buffers) {
      GetVideoCaptureHost()->ReleaseBuffer(device_id_, scaled_buffer->buffer_id,
                                           DefaultFeedback());
    }
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

  // Create and initialize frame preparers for the non-scaled and the scaled
  // frames.
  auto frame_preparer =
      std::make_unique<VideoFrameBufferPreparer>(*this, std::move(buffer));
  bool init_successful = frame_preparer->Initialize();
  bool need_binding_on_media_thread = !frame_preparer->IsVideoFrameBound();
  std::vector<std::unique_ptr<VideoFrameBufferPreparer>> scaled_frame_preparers;
  scaled_frame_preparers.reserve(scaled_buffers.size());
  for (media::mojom::blink::ReadyBufferPtr& scaled_buffer : scaled_buffers) {
    auto scaled_frame_preparer = std::make_unique<VideoFrameBufferPreparer>(
        *this, std::move(scaled_buffer));
    init_successful &= scaled_frame_preparer->Initialize();
    need_binding_on_media_thread |= !scaled_frame_preparer->IsVideoFrameBound();
    scaled_frame_preparers.push_back(std::move(scaled_frame_preparer));
  }
  if (!init_successful) {
    OnFrameDropped(media::VideoCaptureFrameDropReason::
                       kVideoCaptureImplFailedToWrapDataAsMediaVideoFrame);
    GetVideoCaptureHost()->ReleaseBuffer(
        device_id_, frame_preparer->buffer_id(), DefaultFeedback());
    for (auto& scaled_frame_preparer : scaled_frame_preparers) {
      GetVideoCaptureHost()->ReleaseBuffer(
          device_id_, scaled_frame_preparer->buffer_id(), DefaultFeedback());
    }
    return;
  }

  // If any of the video frames needs to be bound we do a round-trip time to the
  // media thread, otherwise we'll go directly to OnVideoFrameReady().
  if (need_binding_on_media_thread) {
    media_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VideoCaptureImpl::BindVideoFramesOnMediaThread,
                       gpu_factories_, std::move(frame_preparer),
                       std::move(scaled_frame_preparers),
                       base::BindPostTaskToCurrentDefault(base::BindOnce(
                           &VideoCaptureImpl::OnVideoFrameReady,
                           weak_factory_.GetWeakPtr(), reference_time)),
                       base::BindPostTask(
                           main_task_runner_,
                           base::BindOnce(&VideoCaptureImpl::OnGpuContextLost,
                                          weak_factory_.GetWeakPtr()))));
    return;
  }
  OnVideoFrameReady(reference_time, std::move(frame_preparer),
                    std::move(scaled_frame_preparers));
}

// static
void VideoCaptureImpl::BindVideoFramesOnMediaThread(
    media::GpuVideoAcceleratorFactories* gpu_factories,
    std::unique_ptr<VideoFrameBufferPreparer> frame_preparer,
    std::vector<std::unique_ptr<VideoFrameBufferPreparer>>
        scaled_frame_preparers,
    base::OnceCallback<
        void(std::unique_ptr<VideoFrameBufferPreparer>,
             std::vector<std::unique_ptr<VideoFrameBufferPreparer>>)>
        on_frame_ready_callback,
    base::OnceCallback<void()> on_gpu_context_lost) {
  // Bind all frames that needs binding, i.e. the GPU frames. Software frames
  // already bound are not touched, allowing mixing software and hardware
  // frames.
  bool bind_failed = false;
  if (!frame_preparer->IsVideoFrameBound()) {
    bind_failed |= !frame_preparer->BindVideoFrameOnMediaThread(gpu_factories);
  }
  for (auto& scaled_frame_preparer : scaled_frame_preparers) {
    if (!scaled_frame_preparer->IsVideoFrameBound()) {
      bind_failed |=
          !scaled_frame_preparer->BindVideoFrameOnMediaThread(gpu_factories);
    }
  }
  if (bind_failed) {
    std::move(on_gpu_context_lost).Run();
    // Proceed to invoke |on_frame_ready_callback| even though we failed. It
    // takes care of dropping the frames.
  }
  std::move(on_frame_ready_callback)
      .Run(std::move(frame_preparer), std::move(scaled_frame_preparers));
}

void VideoCaptureImpl::OnVideoFrameReady(
    base::TimeTicks reference_time,
    std::unique_ptr<VideoFrameBufferPreparer> frame_preparer,
    std::vector<std::unique_ptr<VideoFrameBufferPreparer>>
        scaled_frame_preparers) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);

  // If any of the frames are not bound and ready we drop all of them.
  bool is_any_frame_not_bound = !frame_preparer->IsVideoFrameBound();
  for (const auto& scaled_frame_preparer : scaled_frame_preparers) {
    is_any_frame_not_bound |= !scaled_frame_preparer->IsVideoFrameBound();
  }
  if (is_any_frame_not_bound) {
    OnFrameDropped(media::VideoCaptureFrameDropReason::
                       kVideoCaptureImplFailedToWrapDataAsMediaVideoFrame);
    // Release all buffers.
    GetVideoCaptureHost()->ReleaseBuffer(
        device_id_, frame_preparer->buffer_id(), DefaultFeedback());
    for (const auto& scaled_frame_preparer : scaled_frame_preparers) {
      GetVideoCaptureHost()->ReleaseBuffer(
          device_id_, scaled_frame_preparer->buffer_id(), DefaultFeedback());
    }
    return;
  }

  // The buffers will be used. Finaize them.
  frame_preparer->Finalize();
  std::vector<scoped_refptr<media::VideoFrame>> scaled_video_frames;
  scaled_video_frames.reserve(scaled_frame_preparers.size());
  for (const auto& scaled_frame_preparer : scaled_frame_preparers) {
    scaled_frame_preparer->Finalize();
    scaled_video_frames.push_back(scaled_frame_preparer->frame());
  }

  // TODO(qiangchen): Dive into the full code path to let frame metadata hold
  // reference time rather than using an extra parameter.
  for (const auto& client : clients_) {
    client.second.deliver_frame_cb.Run(frame_preparer->frame(),
                                       scaled_video_frames, reference_time);
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

void VideoCaptureImpl::OnNewCropVersion(uint32_t crop_version) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);

  for (const auto& client : clients_) {
    client.second.crop_version_cb.Run(crop_version);
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

void VideoCaptureImpl::RequirePremappedFrames() {
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
