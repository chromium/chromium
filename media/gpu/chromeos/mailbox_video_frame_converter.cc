// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/mailbox_video_frame_converter.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_shared_image_interface.h"
#include "media/base/format_utils.h"
#include "media/base/video_util.h"
#include "media/gpu/macros.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

namespace media {
namespace {
// The SharedImage size ultimately must correspond to the size used to import
// the decoded frame into a graphics API (e.g., the EGL image size when using
// OpenGL). For most videos, this is simply frame->visible_rect().size().
// However, some H.264 videos specify a visible rectangle that doesn't start
// at (0, 0). Since clients are expected to calculate UV coordinates to handle
// these exotic visible rectangles, we must include the area on the left and
// on the top of the frames when computing the SharedImage size.
inline gfx::Size to_shared_image_size(FrameResource* origin_frame,
                                      scoped_refptr<FrameResource> frame) {
  return origin_frame->metadata().needs_detiling
             ? origin_frame->coded_size()
             : GetRectSizeFromOrigin(frame->visible_rect());
}

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
inline gfx::Size to_coded_size(scoped_refptr<FrameResource> frame) {
  return frame->metadata().needs_detiling
             ? frame->coded_size()
             : GetRectSizeFromOrigin(frame->visible_rect());
}
}  // namespace

// static
std::unique_ptr<FrameResourceConverter> MailboxVideoFrameConverter::Create(
    scoped_refptr<gpu::SharedImageInterface> sii) {
  return base::WrapUnique<FrameResourceConverter>(
      new MailboxVideoFrameConverter(std::move(sii)));
}

// static
std::unique_ptr<FrameResourceConverter> MailboxVideoFrameConverter::Create(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    GetCommandBufferStubCB get_stub_cb) {
  DCHECK(gpu_task_runner);
  DCHECK(get_stub_cb);

  scoped_refptr<gpu::SharedImageInterface> sii;

  base::WaitableEvent wait;
  bool success = gpu_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](GetCommandBufferStubCB get_stub_cb,
             scoped_refptr<gpu::SharedImageInterface>* sii,
             base::WaitableEvent* wait) {
            auto* cb_stub = get_stub_cb.Run();
            if (cb_stub) {
              DCHECK(cb_stub->channel());
              *sii = cb_stub->channel()
                         ->shared_image_stub()
                         ->shared_image_interface();
            }
            wait->Signal();
          },
          get_stub_cb, &sii, &wait));
  if (success) {
    // Sync wait for retrieval of `sii`.
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    wait.Wait();
  }
  return Create(std::move(sii));
}

MailboxVideoFrameConverter::MailboxVideoFrameConverter(
    scoped_refptr<gpu::SharedImageInterface> sii)
    : shared_image_interface_(std::move(sii)) {
  DVLOGF(2);
}

void MailboxVideoFrameConverter::Destroy() {
  DCHECK(!parent_task_runner() ||
         parent_task_runner()->RunsTasksInCurrentSequence());
  DVLOGF(2);

  weak_this_factory_.InvalidateWeakPtrs();
  delete this;
}

MailboxVideoFrameConverter::~MailboxVideoFrameConverter() {
  DVLOGF(2);
}

void MailboxVideoFrameConverter::ConvertFrameImpl(
    scoped_refptr<FrameResource> frame) {
  DVLOGF(4);

  if (!shared_image_interface_) {
    return OnError(FROM_HERE, "Initialized without SharedImageInterface");
  }

  if (!frame ||
      (frame->storage_type() != VideoFrame::STORAGE_DMABUFS &&
       frame->storage_type() != VideoFrame::STORAGE_GPU_MEMORY_BUFFER)) {
    return OnError(FROM_HERE, "Invalid frame.");
  }

  FrameResource* origin_frame = GetOriginalFrame(*frame);
  if (!origin_frame) {
    return OnError(FROM_HERE, "Failed to get origin frame.");
  }

  TRACE_EVENT1("media,gpu", "ConvertFrameImpl", "FrameResource id",
               origin_frame->unique_id());

  auto shared_image_it = shared_images_.find(origin_frame->unique_id());
  // If there's a |stored_shared_image| associated with |origin_frame|, update
  // it and call the continuation callback, otherwise create a SharedImage and
  // register it.
  if (shared_image_it != shared_images_.end()) {
    auto stored_shared_image = shared_image_it->second;
    // Check if the existing shared image is reusable.
    if (stored_shared_image &&
        stored_shared_image->size() ==
            to_shared_image_size(origin_frame, frame) &&
        stored_shared_image->color_space() == frame->ColorSpace()) {
      shared_image_interface_->UpdateSharedImage(
          gpu::SyncToken(), stored_shared_image->mailbox());
      WrapSharedImageAndVideoFrameAndOutput(
          origin_frame, std::move(frame), std::move(stored_shared_image),
          shared_image_interface_->GenVerifiedSyncToken());
      return;
    }
  }

  // Create a new shared_image.
  scoped_refptr<gpu::ClientSharedImage> new_client_shared_image =
      GenerateSharedImage(origin_frame, frame);
  if (!new_client_shared_image) {
    return;
  }

  auto sync_token = new_client_shared_image->creation_sync_token();
  shared_image_interface_->VerifySyncToken(sync_token);
  WrapSharedImageAndVideoFrameAndOutput(origin_frame, std::move(frame),
                                        std::move(new_client_shared_image),
                                        sync_token);
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

  // GenerateSharedImage() should have checked the |origin_frame|'s format
  // (which should be the same as the |frame|'s format).
  CHECK_EQ(frame->format(), origin_frame->format());

  const gfx::Size coded_size = to_coded_size(frame);
  scoped_refptr<VideoFrame> mailbox_frame = VideoFrame::WrapSharedImage(
      frame->format(), shared_image, shared_image_sync_token,
      /*mailbox_holder_release_cb=*/{}, coded_size, frame->visible_rect(),
      frame->natural_size(), frame->timestamp());
  mailbox_frame->set_color_space(shared_image->color_space());
  mailbox_frame->set_hdr_metadata(frame->hdr_metadata());
  mailbox_frame->set_metadata(frame->metadata());
  mailbox_frame->metadata().read_lock_fences_enabled = true;
  mailbox_frame->metadata().is_webgpu_compatible =
      frame->metadata().is_webgpu_compatible;

  mailbox_frame->AddDestructionObserver(
      base::DoNothingWithBoundArgs(std::move(frame)));

  Output(std::move(mailbox_frame));
}

scoped_refptr<gpu::ClientSharedImage>
MailboxVideoFrameConverter::GenerateSharedImage(
    FrameResource* origin_frame,
    scoped_refptr<FrameResource> frame) {
  auto si_format = VideoPixelFormatToSharedImageFormat(origin_frame->format());
  if (!si_format) {
    OnError(FROM_HERE, "Unsupported format: " +
                           VideoPixelFormatToString(origin_frame->format()));
    return nullptr;
  }
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  // If format is true multiplanar format, we prefer external sampler on
  // ChromeOS and Linux.
  if (si_format->is_multi_plane()) {
    si_format->SetPrefersExternalSampler();
  }
#endif

  auto gpu_memory_buffer_handle = origin_frame->CreateGpuMemoryBufferHandle();
  DCHECK(!gpu_memory_buffer_handle.is_null());
  DCHECK_EQ(gpu_memory_buffer_handle.type, gfx::NATIVE_PIXMAP);

  const gfx::Size shared_image_size = to_shared_image_size(origin_frame, frame);

  // The allocated SharedImages should be usable for the (Display) compositor
  // and, potentially, for overlays (Scanout). The shared image can be copied to
  // GL texture over WebGL either directly or over raster interface.
  gpu::SharedImageUsageSet shared_image_usage =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT |
      gpu::SHARED_IMAGE_USAGE_GLES2_READ | gpu::SHARED_IMAGE_USAGE_RASTER_READ;

  // These SharedImages might also be used for zero-copy import into WebGPU to
  // serve as the sources of WebGPU reads (e.g., for video effects processing).
  if (origin_frame->metadata().is_webgpu_compatible &&
      !shared_image_interface_->GetCapabilities()
           .disable_webgpu_shared_images) {
    shared_image_usage |= gpu::SHARED_IMAGE_USAGE_WEBGPU_READ;
  }

  scoped_refptr<gpu::ClientSharedImage> client_shared_image =
      shared_image_interface_->CreateSharedImage(
          {*si_format, shared_image_size, frame->ColorSpace(),
           shared_image_usage, "MailboxVideoFrameConverter"},
          std::move(gpu_memory_buffer_handle));
  if (!client_shared_image) {
    OnError(FROM_HERE, "Failed to create shared image.");
    return nullptr;
  }
  RegisterSharedImage(origin_frame, client_shared_image);

  // There's no need to UpdateSharedImage() after CreateSharedImage().
  return client_shared_image;
}

void MailboxVideoFrameConverter::RegisterSharedImage(
    FrameResource* origin_frame,
    scoped_refptr<gpu::ClientSharedImage> client_shared_image) {
  DVLOGF(4) << "frame: " << origin_frame->unique_id();
  DCHECK(parent_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(client_shared_image);
  DCHECK(!base::Contains(shared_images_, origin_frame->unique_id()) ||
         shared_images_.find(origin_frame->unique_id())->second !=
             client_shared_image);

  shared_images_[origin_frame->unique_id()] = client_shared_image;
  origin_frame->AddDestructionObserver(base::BindOnce(
      [](scoped_refptr<gpu::ClientSharedImage> shared_image,
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
      std::move(client_shared_image), parent_task_runner(),
      weak_this_factory_.GetWeakPtr(), origin_frame->unique_id()));
}

void MailboxVideoFrameConverter::UnregisterSharedImage(
    UniqueID origin_frame_id,
    scoped_refptr<gpu::ClientSharedImage> client_shared_image) {
  DCHECK(parent_task_runner()->RunsTasksInCurrentSequence());
  DVLOGF(4);

  auto it = shared_images_.find(origin_frame_id);
  CHECK(it != shared_images_.end());
  if (it->second == client_shared_image.get()) {
    shared_images_.erase(it);
  }
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
