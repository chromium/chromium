// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_decode_accelerator_stub.h"

#include <stddef.h>

#include <algorithm>
#include <new>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/common/discardable_handle.h"
#include "gpu/command_buffer/common/scheduling_priority.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "gpu/command_buffer/service/gr_shader_cache.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/service_transfer_cache.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/shared_image_stub.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/gpu_memory_buffer.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#endif

namespace gpu {
class Buffer;

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace {

struct CleanUpContext {
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  raw_ptr<SharedContextState> shared_context_state_ = nullptr;
  std::unique_ptr<SkiaImageRepresentation> skia_representation_;
  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess>
      skia_scoped_access_;
  size_t num_callbacks_pending_;
  CleanUpContext(scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
                 raw_ptr<SharedContextState> shared_context_state,
                 std::unique_ptr<SkiaImageRepresentation> skia_representation,
                 std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess>
                     skia_scoped_access)
      : main_task_runner_(main_task_runner),
        shared_context_state_(shared_context_state),
        skia_representation_(std::move(skia_representation)),
        skia_scoped_access_(std::move(skia_scoped_access)),
        num_callbacks_pending_(skia_representation_->NumPlanesExpected()) {}
};

void CleanUpResource(SkImages::ReleaseContext context) {
  auto* clean_up_context = static_cast<CleanUpContext*>(context);
  DCHECK(clean_up_context->main_task_runner_->BelongsToCurrentThread());

  // The context should be current as we set it to be current earlier, and this
  // call is coming from Skia itself.
  DCHECK(
      clean_up_context->shared_context_state_->IsCurrent(/*surface=*/nullptr));
  clean_up_context->skia_scoped_access_->ApplyBackendSurfaceEndState();

  CHECK_GT(clean_up_context->num_callbacks_pending_, 0u);
  clean_up_context->num_callbacks_pending_--;

  if (clean_up_context->num_callbacks_pending_ == 0u) {
    delete clean_up_context;
  }
}

}  // namespace
#endif

ImageDecodeAcceleratorStub::ImageDecodeAcceleratorStub(
    ImageDecodeAcceleratorWorker* worker,
    GpuChannel* channel,
    int32_t route_id)
    : worker_(worker),
      channel_(channel),
      sequence_(channel->scheduler()->CreateSequence(SchedulingPriority::kLow,
                                                     channel->task_runner())),
      sync_point_client_state_(
          channel->sync_point_manager()->CreateSyncPointClientState(
              CommandBufferNamespace::GPU_IO,
              CommandBufferIdFromChannelAndRoute(channel->client_id(),
                                                 route_id),
              sequence_)),
      main_task_runner_(channel->task_runner()),
      io_task_runner_(channel->io_task_runner()) {
  // We need the sequence to be initially disabled so that when we schedule a
  // task to release the decode sync token, it doesn't run immediately (we want
  // it to run when the decode is done).
  channel_->scheduler()->DisableSequence(sequence_);
}

void ImageDecodeAcceleratorStub::Shutdown() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(lock_);
  sync_point_client_state_->Destroy();
  channel_->scheduler()->DestroySequence(sequence_);
  channel_ = nullptr;
}

ImageDecodeAcceleratorStub::~ImageDecodeAcceleratorStub() {
  DCHECK(!channel_);
}

void ImageDecodeAcceleratorStub::ScheduleImageDecode(
    mojom::ScheduleImageDecodeParamsPtr params,
    uint64_t release_count) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (!base::FeatureList::IsEnabled(
          features::kVaapiJpegImageDecodeAcceleration) &&
      !base::FeatureList::IsEnabled(
          features::kVaapiWebPImageDecodeAcceleration)) {
    return;
  }

  DCHECK(io_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(lock_);
  if (!channel_) {
    // The channel is no longer available, so don't do anything.
    return;
  }

  mojom::ScheduleImageDecodeParams& decode_params = *params;

  // Start the actual decode.
  worker_->Decode(
      std::move(decode_params.encoded_data), decode_params.output_size,
      base::BindOnce(&ImageDecodeAcceleratorStub::OnDecodeCompleted,
                     base::WrapRefCounted(this), decode_params.output_size));

  // Schedule a task to eventually release the decode sync token. Note that this
  // task won't run until the sequence is re-enabled when a decode completes.
  const SyncToken discardable_handle_sync_token = SyncToken(
      CommandBufferNamespace::GPU_IO,
      CommandBufferIdFromChannelAndRoute(channel_->client_id(),
                                         decode_params.raster_decoder_route_id),
      decode_params.discardable_handle_release_count);
  channel_->scheduler()->ScheduleTask(Scheduler::Task(
      sequence_,
      base::BindOnce(&ImageDecodeAcceleratorStub::ProcessCompletedDecode,
                     base::WrapRefCounted(this), std::move(params),
                     release_count),
      {discardable_handle_sync_token} /* sync_token_fences */));
}

void ImageDecodeAcceleratorStub::ProcessCompletedDecode(
    mojom::ScheduleImageDecodeParamsPtr params_ptr,
    uint64_t decode_release_count) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(lock_);
  if (!channel_) {
    // The channel is no longer available, so don't do anything.
    return;
  }

  mojom::ScheduleImageDecodeParams& params = *params_ptr;

  DCHECK(!pending_completed_decodes_.empty());
  std::unique_ptr<ImageDecodeAcceleratorWorker::DecodeResult> completed_decode =
      std::move(pending_completed_decodes_.front());
  pending_completed_decodes_.pop();

  // Regardless of what happens next, make sure the sync token gets released and
  // the sequence gets disabled if there are no more completed decodes after
  // this. base::Unretained(this) is safe because *this outlives the
  // ScopedClosureRunner.
  absl::Cleanup finalizer = [this, decode_release_count] {
    lock_.AssertAcquired();
    FinishCompletedDecode(decode_release_count);
  };

  if (!completed_decode) {
    DLOG(ERROR) << "The image could not be decoded";
    return;
  }

  // TODO(crbug.com/40641220): the output_size parameter is going away, so this
  // validation is not needed. Checking if the size is too small should happen
  // at the level of the decoder (since that's the component that's aware of its
  // own capabilities).
  if (params.output_size.IsEmpty()) {
    DLOG(ERROR) << "Output dimensions are too small";
    return;
  }

  // Gain access to the transfer cache through the GpuChannelManager's
  // SharedContextState. We will also use that to get a GrContext that will be
  // used for Skia operations.
  ContextResult context_result;
  scoped_refptr<SharedContextState> shared_context_state =
      channel_->gpu_channel_manager()->GetSharedContextState(&context_result);
  if (context_result != ContextResult::kSuccess) {
    DLOG(ERROR) << "Unable to obtain the SharedContextState";
    return;
  }
  DCHECK(shared_context_state);

  if (!shared_context_state->gr_context()) {
    DLOG(ERROR) << "Could not get the GrContext";
    return;
  }
  if (!shared_context_state->MakeCurrent(nullptr /* surface */)) {
    DLOG(ERROR) << "Could not MakeCurrent the shared context";
    return;
  }

  std::vector<sk_sp<SkImage>> plane_sk_images;
  std::optional<base::ScopedClosureRunner> notify_gl_state_changed;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK_EQ(
      gfx::NumberOfPlanesForLinearBufferFormat(completed_decode->buffer_format),
      completed_decode->handle.native_pixmap_handle.planes.size());
  // We should notify the SharedContextState that we or Skia may have modified
  // the driver's GL state. We put this in a ScopedClosureRunner so that if we
  // return early, the SharedContextState ends up in a consistent state.
  // TODO(blundell): Determine whether this is still necessary after the
  // transition to SharedImage.
  notify_gl_state_changed.emplace(base::BindOnce(
      [](scoped_refptr<SharedContextState> scs) {
        scs->set_need_context_state_reset(true);
      },
      shared_context_state));

  const size_t num_planes =
      completed_decode->handle.native_pixmap_handle.planes.size();
  plane_sk_images.resize(num_planes);

  // Right now, we only support YUV 4:2:0 for the output of the decoder (either
  // as YV12 or NV12).
  CHECK(completed_decode->buffer_format == gfx::BufferFormat::YVU_420 ||
        completed_decode->buffer_format == gfx::BufferFormat::YUV_420_BIPLANAR);
  const auto format =
      viz::GetSharedImageFormat(completed_decode->buffer_format);
  const gfx::Size shared_image_size = completed_decode->visible_size;
  const gpu::Mailbox mailbox = gpu::Mailbox::Generate();
  if (!channel_->shared_image_stub()->CreateSharedImage(
          mailbox, std::move(completed_decode->handle), format,
          shared_image_size, gfx::ColorSpace(), kTopLeft_GrSurfaceOrigin,
          kOpaque_SkAlphaType,
          SHARED_IMAGE_USAGE_RASTER_READ | SHARED_IMAGE_USAGE_OOP_RASTERIZATION,
          "ImageDecodeAccelerator")) {
    DLOG(ERROR) << "Could not create SharedImage";
    return;
  }

  // Create the SkiaRepresentation::ScopedReadAccess from the SharedImage.
  // There is a need to be careful here as the SkiaRepresentation can outlive
  // the channel: the representation is effectively owned by the transfer
  // cache, which is owned by SharedContextState, which is destroyed by
  // GpuChannelManager *after* GpuChannelManager destroys the channels. Hence,
  // we cannot supply the channel's SharedImageStub as a MemoryTracker to
  // create a SharedImageRepresentationFactory here (the factory creates a
  // MemoryTypeTracker instance backed by that MemoryTracker that needs to
  // outlive the representation). Instead, we create the Skia representation
  // directly using the SharedContextState's MemoryTypeTracker instance.
  std::unique_ptr<SkiaImageRepresentation> skia_representation =
      channel_->gpu_channel_manager()->shared_image_manager()->ProduceSkia(
          mailbox, shared_context_state->memory_type_tracker(),
          shared_context_state);
  // Note that per the above reasoning, we have to make sure that the factory
  // representation doesn't outlive the channel (since it *was* created via
  // the channel). We can destroy it now that the Skia representation has been
  // created (or if creation failed, we'll early out shortly, but we still need
  // to destroy the SharedImage to avoid leaks).
  channel_->shared_image_stub()->factory()->DestroySharedImage(mailbox);
  if (!skia_representation) {
    DLOG(ERROR) << "Could not create a SkiaImageRepresentation";
    return;
  }
  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  auto skia_scoped_access = skia_representation->BeginScopedReadAccess(
      &begin_semaphores, &end_semaphores);

  if (!skia_scoped_access) {
    DLOG(ERROR) << "Could not get scoped access to SkiaImageRepresentation";
    return;
  }

  // As this SharedImage has just been created, there should not be any
  // semaphores.
  DCHECK(begin_semaphores.empty());
  DCHECK(end_semaphores.empty());

  // Create the SkImage for each plane, handing over lifetime management of the
  // skia image representation and scoped access.
  CleanUpContext* resource = new CleanUpContext(
      channel_->task_runner(), shared_context_state.get(),
      std::move(skia_representation), std::move(skia_scoped_access));
  const size_t num_planes_expected =
      resource->skia_representation_->NumPlanesExpected();
  for (size_t plane = 0u; plane < num_planes_expected; plane++) {
    plane_sk_images[plane] =
        resource->skia_scoped_access_->CreateSkImageForPlane(
            base::checked_cast<int>(plane), shared_context_state.get(),
            CleanUpResource, resource);
    if (!plane_sk_images[plane]) {
      DLOG(ERROR) << "Could not create planar SkImage";
      return;
    }
  }

  // Insert the cache entry in the transfer cache. Note that this section
  // validates several of the IPC parameters: |params.raster_decoder_route_id|,
  // |params.transfer_cache_entry_id|, |params.discardable_handle_shm_id|, and
  // |params.discardable_handle_shm_offset|.
  CommandBufferStub* command_buffer =
      channel_->LookupCommandBuffer(params.raster_decoder_route_id);
  if (!command_buffer) {
    DLOG(ERROR) << "Could not find the command buffer";
    return;
  }
  scoped_refptr<Buffer> handle_buffer =
      command_buffer->GetTransferBuffer(params.discardable_handle_shm_id);
  if (!DiscardableHandleBase::ValidateParameters(
          handle_buffer.get(), params.discardable_handle_shm_offset)) {
    DLOG(ERROR) << "Could not validate the discardable handle parameters";
    return;
  }
  DCHECK(command_buffer->decoder_context());
  if (command_buffer->decoder_context()->GetRasterDecoderId() < 0) {
    DLOG(ERROR) << "Could not get the raster decoder ID";
    return;
  }

  {
    auto* gr_shader_cache = channel_->gpu_channel_manager()->gr_shader_cache();
    std::optional<raster::GrShaderCache::ScopedCacheUse> cache_use;
    if (gr_shader_cache)
      cache_use.emplace(gr_shader_cache,
                        base::strict_cast<int32_t>(channel_->client_id()));
    DCHECK(shared_context_state->transfer_cache());
    SkYUVAInfo::PlaneConfig plane_config =
        completed_decode->buffer_format == gfx::BufferFormat::YVU_420
            ? SkYUVAInfo::PlaneConfig::kY_V_U
            : SkYUVAInfo::PlaneConfig::kY_UV;
    // TODO(andrescj): |params.target_color_space| is not needed because Skia
    // knows where it's drawing, so it can handle color space conversion without
    // us having to specify the target color space. However, we are currently
    // assuming that the color space of the image is sRGB. This means we don't
    // support images with embedded color profiles. We could rename
    // |params.target_color_space| to |params.image_color_space| and we can send
    // the embedded color profile from the renderer using that field.
    if (!shared_context_state->transfer_cache()
             ->CreateLockedHardwareDecodedImageEntry(
                 command_buffer->decoder_context()->GetRasterDecoderId(),
                 params.transfer_cache_entry_id,
                 ServiceDiscardableHandle(std::move(handle_buffer),
                                          params.discardable_handle_shm_offset,
                                          params.discardable_handle_shm_id),
                 shared_context_state->gr_context(), std::move(plane_sk_images),
                 plane_config, SkYUVAInfo::Subsampling::k420,
                 completed_decode->yuv_color_space,
                 completed_decode->buffer_byte_size, params.needs_mips)) {
      DLOG(ERROR) << "Could not create and insert the transfer cache entry";
      return;
    }
  }
  DCHECK(notify_gl_state_changed);
  notify_gl_state_changed->RunAndReset();
#else
  // Right now, we only support Chrome OS because we need to use the
  // |native_pixmap_handle| member of a GpuMemoryBufferHandle.
  NOTIMPLEMENTED()
      << "Image decode acceleration is unsupported for this platform";
#endif
}

void ImageDecodeAcceleratorStub::FinishCompletedDecode(
    uint64_t decode_release_count) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  lock_.AssertAcquired();
  sync_point_client_state_->ReleaseFenceSync(decode_release_count);
  if (pending_completed_decodes_.empty())
    channel_->scheduler()->DisableSequence(sequence_);
}

void ImageDecodeAcceleratorStub::OnDecodeCompleted(
    gfx::Size expected_output_size,
    std::unique_ptr<ImageDecodeAcceleratorWorker::DecodeResult> result) {
  base::AutoLock lock(lock_);
  if (!channel_) {
    // The channel is no longer available, so don't do anything.
    return;
  }

  // A sanity check on the output of the decoder.
  DCHECK(!result || expected_output_size == result->visible_size);

  // The decode is ready to be processed: add it to |pending_completed_decodes_|
  // so that ProcessCompletedDecode() can pick it up.
  pending_completed_decodes_.push(std::move(result));

  // We only need to enable the sequence when the number of pending completed
  // decodes is 1. If there are more, the sequence should already be enabled.
  if (pending_completed_decodes_.size() == 1u)
    channel_->scheduler()->EnableSequence(sequence_);
}

}  // namespace gpu
