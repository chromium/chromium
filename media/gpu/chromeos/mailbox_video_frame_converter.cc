// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/mailbox_video_frame_converter.h"

#include <optional>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/not_fatal_until.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_shared_image_interface.h"
#include "media/base/format_utils.h"
#include "media/base/media_switches.h"
#include "media/base/video_util.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/macros.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/gl_bindings.h"

namespace {

// Based on `buffer_format` support by VideoPixelFormatToGfxBufferFormat.
viz::SharedImageFormat GetSharedImageFormat(gfx::BufferFormat buffer_format) {
  viz::SharedImageFormat format;
  switch (buffer_format) {
    case gfx::BufferFormat::RGBA_8888:
      format = viz::SinglePlaneFormat::kRGBA_8888;
      break;
    case gfx::BufferFormat::RGBX_8888:
      format = viz::SinglePlaneFormat::kRGBX_8888;
      break;
    case gfx::BufferFormat::BGRA_8888:
      format = viz::SinglePlaneFormat::kBGRA_8888;
      break;
    case gfx::BufferFormat::BGRX_8888:
      format = viz::SinglePlaneFormat::kBGRX_8888;
      break;
    case gfx::BufferFormat::YVU_420:
      format = viz::MultiPlaneFormat::kYV12;
      break;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      format = viz::MultiPlaneFormat::kNV12;
      break;
    case gfx::BufferFormat::YUVA_420_TRIPLANAR:
      format = viz::MultiPlaneFormat::kNV12A;
      break;
    case gfx::BufferFormat::P010:
      format = viz::MultiPlaneFormat::kP010;
      break;
    default:
      DLOG(WARNING) << "Unsupported buffer_format: "
                    << static_cast<int>(buffer_format);
      NOTREACHED();
  }
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  // If format is true multiplanar format, we prefer external sampler on
  // ChromeOS and Linux.
  if (format.is_multi_plane()) {
    format.SetPrefersExternalSampler();
  }
#endif
  return format;
}

}  // namespace

namespace media {
class MailboxVideoFrameConverter::ScopedSharedImage {
 public:
  ScopedSharedImage() = default;

  ScopedSharedImage(const ScopedSharedImage&) = delete;
  ScopedSharedImage& operator=(const ScopedSharedImage&) = delete;

  ~ScopedSharedImage() = default;

  void Reset(scoped_refptr<gpu::ClientSharedImage> shared_image) {
    DCHECK(shared_image);
    shared_image_ = std::move(shared_image);
  }

  bool HasData() const { return shared_image_ != nullptr; }
  const scoped_refptr<gpu::ClientSharedImage>& shared_image() {
    return shared_image_;
  }

 private:
  scoped_refptr<gpu::ClientSharedImage> shared_image_;
};

// static
std::unique_ptr<FrameResourceConverter> MailboxVideoFrameConverter::Create(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    GetCommandBufferStubCB get_stub_cb) {
  DCHECK(gpu_task_runner);
  DCHECK(get_stub_cb);

  scoped_refptr<gpu::SharedImageInterface> sii;
  gpu::Scheduler* scheduler;

  base::WaitableEvent wait;
  bool success = gpu_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](GetCommandBufferStubCB get_stub_cb,
             scoped_refptr<gpu::SharedImageInterface>* sii,
             gpu::Scheduler** scheduler, base::WaitableEvent* wait) {
            auto* cb_stub = get_stub_cb.Run();
            if (cb_stub) {
              DCHECK(cb_stub->channel());
              *sii = cb_stub->channel()
                         ->shared_image_stub()
                         ->shared_image_interface();
              *scheduler = cb_stub->channel()->scheduler();
            }
            wait->Signal();
          },
          get_stub_cb, &sii, &scheduler, &wait));
  if (success) {
    // Sync wait for retrieval of `sii`, `scheduler`, and `sequence`.
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    wait.Wait();
  }
  return (sii && scheduler)
             ? base::WrapUnique<FrameResourceConverter>(
                   new MailboxVideoFrameConverter(sii, scheduler))
             : nullptr;
}

MailboxVideoFrameConverter::MailboxVideoFrameConverter(
    scoped_refptr<gpu::SharedImageInterface> sii,
    base::RepeatingCallback<bool(scoped_refptr<FrameResource> frame,
                                 const gpu::SyncToken& sync_token)> release_cb)
    : shared_image_interface_(sii), release_cb_(release_cb) {
  DVLOGF(2);
  weak_this_ = weak_this_factory_.GetWeakPtr();
}

MailboxVideoFrameConverter::MailboxVideoFrameConverter(
    scoped_refptr<gpu::SharedImageInterface> sii,
    gpu::Scheduler* scheduler)
    : shared_image_interface_(sii),
      scheduler_(scheduler),
      sequence_(scheduler->CreateSequence(
          gpu::SchedulingPriority::kNormal,
          base::SingleThreadTaskRunner::GetCurrentDefault())) {
  DVLOGF(2);
  release_cb_ =
      base::BindRepeating(
          [](gpu::Scheduler* scheduler, gpu::SequenceId sequence,
             scoped_refptr<FrameResource> frame,
             const gpu::SyncToken& sync_token) {
            auto keep_video_frame_alive =
                base::DoNothingWithBoundArgs(std::move(frame));
            scheduler->ScheduleTask(gpu::Scheduler::Task(
                sequence, std::move(keep_video_frame_alive),
                std::vector<gpu::SyncToken>({sync_token})));
            return true;
          },
          scheduler_, sequence_);
  weak_this_ = weak_this_factory_.GetWeakPtr();
}

void MailboxVideoFrameConverter::Destroy() {
  DCHECK(!parent_task_runner() ||
         parent_task_runner()->RunsTasksInCurrentSequence());
  DVLOGF(2);
  if (scheduler_) {
    scheduler_->DestroySequence(sequence_);
  }

  weak_this_factory_.InvalidateWeakPtrs();
  delete this;
}

MailboxVideoFrameConverter::~MailboxVideoFrameConverter() {
  DVLOGF(2);
}

void MailboxVideoFrameConverter::ConvertFrameImpl(
    scoped_refptr<FrameResource> frame) {
  DVLOGF(4);

  if (!frame ||
      (frame->storage_type() != VideoFrame::STORAGE_DMABUFS &&
       frame->storage_type() != VideoFrame::STORAGE_GPU_MEMORY_BUFFER)) {
    return OnError(FROM_HERE, "Invalid frame.");
  }

  FrameResource* origin_frame = GetOriginalFrame(*frame);

  if (!origin_frame)
    return OnError(FROM_HERE, "Failed to get origin frame.");

  ScopedSharedImage* shared_image = nullptr;
  const UniqueID origin_frame_id = origin_frame->unique_id();
  auto shared_image_it = shared_images_.find(origin_frame_id);
  if (shared_image_it != shared_images_.end())
    shared_image = shared_image_it->second;

  input_frame_queue_.emplace(frame, origin_frame_id);

  ConvertFrame(origin_frame, std::move(frame), shared_image);
}

void MailboxVideoFrameConverter::WrapSharedImageAndVideoFrameAndOutput(
    FrameResource* origin_frame,
    scoped_refptr<FrameResource> frame,
    scoped_refptr<gpu::ClientSharedImage> shared_image,
    const gpu::SyncToken& shared_image_sync_token) {
  DCHECK(parent_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(shared_image);

  const UniqueID origin_frame_id = origin_frame->unique_id();
  DCHECK(base::Contains(shared_images_, origin_frame_id));
  DCHECK(!input_frame_queue_.empty() &&
         input_frame_queue_.front().second == origin_frame_id);

  input_frame_queue_.pop();

  const auto buffer_format = VideoPixelFormatToGfxBufferFormat(frame->format());
  // GenerateSharedImage() should have checked the |origin_frame|'s format
  // (which should be the same as the |frame|'s format).
  CHECK_EQ(frame->format(), origin_frame->format());
  CHECK(buffer_format);

  VideoFrame::ReleaseMailboxCB release_mailbox_cb = base::BindOnce(
      [](scoped_refptr<base::SequencedTaskRunner> parent_task_runner,
         base::WeakPtr<MailboxVideoFrameConverter> parent_weak_ptr,
         scoped_refptr<FrameResource> frame, const gpu::SyncToken& sync_token) {
        if (!sync_token.HasData()) {
          return;
        }
        if (parent_task_runner->RunsTasksInCurrentSequence()) {
          if (parent_weak_ptr) {
            parent_weak_ptr->ReleaseFrame(std::move(frame), sync_token);
            return;
          }
        }
        parent_task_runner->PostTask(
            FROM_HERE,
            base::BindOnce(&MailboxVideoFrameConverter::ReleaseFrame,
                           parent_weak_ptr, std::move(frame), sync_token));
      },
      parent_task_runner(), weak_this_, frame);

  // Note the use of GetRectSizeFromOrigin() as the coded size. The reason is
  // that the coded_size() of the outgoing FrameResource tells the client what
  // the "usable area" of the frame's buffer is so that it issues rendering
  // commands correctly. For most videos, this usable area is simply
  // frame->visible_rect().size(). However, some H.264 videos define a visible
  // rectangle that doesn't start at (0, 0). For these frames, the usable area
  // includes the non-visible area on the left and on top of the visible area
  // (so that the client can calculate the UV coordinates correctly). Hence the
  // use of GetRectSizeFromOrigin().
  //
  // Most video frames should use visible size instead of coded size because
  // some videos use 0s to pad the frames to coded size, which will cause
  // artifacting along the edges of the image when we scale using bilinear
  // filtering. Tiled protected content is an exception though, because we have
  // a custom Vulkan shader pipeline for scanning out these buffers that needs
  // to know the underlying coded buffer size for detiling computations.
  //
  // The metadata field |needs_detiling| technically comes from an untrusted
  // source, but we don't believe this is a security risk since the worst case
  // scenario simply involves video corruption when the Vulkan detiler
  // misinterprets the frame.
  const gfx::Size coded_size =
      frame->metadata().needs_detiling
          ? frame->coded_size()
          : GetRectSizeFromOrigin(frame->visible_rect());
  scoped_refptr<VideoFrame> mailbox_frame = VideoFrame::WrapSharedImage(
      frame->format(), shared_image, shared_image_sync_token,
      std::move(release_mailbox_cb), coded_size, frame->visible_rect(),
      frame->natural_size(), frame->timestamp());
  mailbox_frame->set_color_space(frame->ColorSpace());
  mailbox_frame->set_hdr_metadata(frame->hdr_metadata());
  mailbox_frame->set_metadata(frame->metadata());
  mailbox_frame->metadata().read_lock_fences_enabled = true;
  mailbox_frame->metadata().is_webgpu_compatible =
      frame->metadata().is_webgpu_compatible;

  Output(std::move(mailbox_frame));
}

void MailboxVideoFrameConverter::ConvertFrame(
    FrameResource* origin_frame,
    scoped_refptr<FrameResource> frame,
    ScopedSharedImage* stored_shared_image) {
  TRACE_EVENT1("media,gpu", "ConvertFrame", "FrameResource id",
               origin_frame->unique_id());
  const gfx::ColorSpace src_color_space = frame->ColorSpace();
  const gfx::Rect visible_rect = frame->visible_rect();

  // If there's a |stored_shared_image| associated with |origin_frame|, update
  // it and call the continuation callback, otherwise create a SharedImage and
  // register it.
  if (stored_shared_image) {
    DCHECK(stored_shared_image->HasData());
    bool res;
    const auto& client_shared_image = stored_shared_image->shared_image();
    std::optional<gpu::SyncToken> sync_token;
    if (client_shared_image->size() == GetRectSizeFromOrigin(visible_rect) &&
        client_shared_image->color_space() == src_color_space) {
      sync_token = UpdateSharedImage(client_shared_image->mailbox());
      res = sync_token.has_value();
    } else {
      // Either the existing shared image's size is no longer good enough or the
      // color space has changed. Let's create a new shared image.
      res = GenerateSharedImage(origin_frame, src_color_space, visible_rect,
                                stored_shared_image);
      sync_token = client_shared_image->creation_sync_token();
    }
    if (res) {
      DCHECK(stored_shared_image->HasData());
      WrapSharedImageAndVideoFrameAndOutput(origin_frame, std::move(frame),
                                            std::move(client_shared_image),
                                            sync_token.value());
    }
    return;
  }

  // There was no existing SharedImage: create a new one.
  auto new_shared_image = std::make_unique<ScopedSharedImage>();
  if (!GenerateSharedImage(origin_frame, src_color_space, visible_rect,
                           new_shared_image.get())) {
    return;
  }
  DCHECK(new_shared_image->HasData());

  scoped_refptr<gpu::ClientSharedImage> new_client_shared_image =
      new_shared_image->shared_image();
  gpu::SyncToken sync_token = new_client_shared_image->creation_sync_token();
  RegisterSharedImage(origin_frame, std::move(new_shared_image));
  WrapSharedImageAndVideoFrameAndOutput(origin_frame, std::move(frame),
                                        std::move(new_client_shared_image),
                                        sync_token);
}

bool MailboxVideoFrameConverter::GenerateSharedImage(
    FrameResource* origin_frame,
    const gfx::ColorSpace& src_color_space,
    const gfx::Rect& destination_visible_rect,
    ScopedSharedImage* shared_image) {
  DCHECK(shared_image);
  if (!shared_image_interface_) {
    OnError(FROM_HERE, "Initialized without SharedImageInterface");
    return false;
  }

  const auto buffer_format =
      VideoPixelFormatToGfxBufferFormat(origin_frame->format());
  if (!buffer_format) {
    OnError(FROM_HERE, "Unsupported format: " +
                           VideoPixelFormatToString(origin_frame->format()));
    return false;
  }

  auto gpu_memory_buffer_handle = origin_frame->CreateGpuMemoryBufferHandle();
  DCHECK(!gpu_memory_buffer_handle.is_null());
  DCHECK_EQ(gpu_memory_buffer_handle.type, gfx::NATIVE_PIXMAP);

  // The SharedImage size ultimately must correspond to the size used to import
  // the decoded frame into a graphics API (e.g., the EGL image size when using
  // OpenGL). For most videos, this is simply |destination_visible_rect|.size().
  // However, some H.264 videos specify a visible rectangle that doesn't start
  // at (0, 0). Since clients are expected to calculate UV coordinates to handle
  // these exotic visible rectangles, we must include the area on the left and
  // on the top of the frames when computing the SharedImage size.
  const gfx::Size shared_image_size =
      origin_frame->metadata().needs_detiling
          ? origin_frame->coded_size()
          : GetRectSizeFromOrigin(destination_visible_rect);

  const std::optional<gpu::SharedImageCapabilities> shared_image_caps =
      shared_image_interface_->GetCapabilities();

  if (!shared_image_caps.has_value()) {
    OnError(FROM_HERE, "Can't get the SharedImageCapabilities");
    return false;
  }

  // The allocated SharedImages should be usable for the (Display) compositor
  // and, potentially, for overlays (Scanout).
  gpu::SharedImageUsageSet shared_image_usage =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;

  // These SharedImages might also be used for zero-copy import into WebGPU to
  // serve as the sources of WebGPU reads (e.g., for video effects processing).
  if (origin_frame->metadata().is_webgpu_compatible &&
      !shared_image_caps->disable_webgpu_shared_images) {
    shared_image_usage |= gpu::SHARED_IMAGE_USAGE_WEBGPU_READ;
  }

  scoped_refptr<gpu::ClientSharedImage> client_shared_image =
      shared_image_interface_->CreateSharedImage(
          {GetSharedImageFormat(*buffer_format), shared_image_size,
           src_color_space, shared_image_usage, "MailboxVideoFrameConverter"},
          std::move(gpu_memory_buffer_handle));
  if (!client_shared_image) {
    OnError(FROM_HERE, "Failed to create shared image.");
    return false;
  }
  // There's no need to UpdateSharedImage() after CreateSharedImage().

  shared_image->Reset(std::move(client_shared_image));
  return true;
}

void MailboxVideoFrameConverter::RegisterSharedImage(
    FrameResource* origin_frame,
    std::unique_ptr<ScopedSharedImage> scoped_shared_image) {
  DVLOGF(4) << "frame: " << origin_frame->unique_id();
  DCHECK(parent_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(scoped_shared_image);
  DCHECK(scoped_shared_image->HasData());
  DCHECK(!base::Contains(shared_images_, origin_frame->unique_id()));

  shared_images_[origin_frame->unique_id()] = scoped_shared_image.get();
  origin_frame->AddDestructionObserver(base::BindOnce(
      [](std::unique_ptr<ScopedSharedImage> shared_image,
         scoped_refptr<base::SequencedTaskRunner> parent_task_runner,
         base::WeakPtr<MailboxVideoFrameConverter> parent_weak_ptr,
         UniqueID origin_frame_id) {
        if (parent_task_runner->RunsTasksInCurrentSequence()) {
          if (parent_weak_ptr)
            parent_weak_ptr->UnregisterSharedImage(origin_frame_id,
                                                   std::move(shared_image));
          return;
        }
        parent_task_runner->PostTask(
            FROM_HERE,
            base::BindOnce(&MailboxVideoFrameConverter::UnregisterSharedImage,
                           parent_weak_ptr, origin_frame_id,
                           std::move(shared_image)));
      },
      std::move(scoped_shared_image), parent_task_runner(), weak_this_,
      origin_frame->unique_id()));
}

std::optional<gpu::SyncToken> MailboxVideoFrameConverter::UpdateSharedImage(
    const gpu::Mailbox& mailbox) {
  shared_image_interface_->UpdateSharedImage(gpu::SyncToken(), mailbox);
  std::optional<gpu::SyncToken> sync_token =
      shared_image_interface_->GenVerifiedSyncToken();
  if (!sync_token.has_value()) {
    OnError(FROM_HERE, "Could not update shared image");
  }
  return sync_token;
}

void MailboxVideoFrameConverter::ReleaseFrame(
    scoped_refptr<FrameResource> frame,
    const gpu::SyncToken& sync_token) {
  if (!release_cb_) {
    return OnError(FROM_HERE,
                   "Could not schedule a task to wait on SyncToken!");
  }

  release_cb_.Run(std::move(frame), sync_token);
}

void MailboxVideoFrameConverter::UnregisterSharedImage(
    UniqueID origin_frame_id,
    std::unique_ptr<ScopedSharedImage> scoped_shared_image) {
  DCHECK(parent_task_runner()->RunsTasksInCurrentSequence());
  DVLOGF(4);

  auto it = shared_images_.find(origin_frame_id);
  CHECK(it != shared_images_.end());
  DCHECK(it->second == scoped_shared_image.get());
  shared_images_.erase(it);
}

void MailboxVideoFrameConverter::AbortPendingFramesImpl() {
  DVLOGF(4) << "Number of pending frames: " << input_frame_queue_.size();

  input_frame_queue_ = {};
}

bool MailboxVideoFrameConverter::HasPendingFramesImpl() const {
  DVLOGF(4) << "Number of pending frames: " << input_frame_queue_.size();

  return !input_frame_queue_.empty();
}

bool MailboxVideoFrameConverter::UsesGetOriginalFrameCBImpl() const {
  return true;
}

void MailboxVideoFrameConverter::OnError(const base::Location& location,
                                         const std::string& msg) {
  DCHECK(parent_task_runner()->RunsTasksInCurrentSequence());
  FrameResourceConverter::AbortPendingFrames();
  FrameResourceConverter::OnError(location, msg);
}
}  // namespace media
