// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_shared_image_wrapper_cache.h"

#include "base/containers/adapters.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "cc/paint/display_item_list.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "third_party/blink/renderer/platform/graphics/canvas_image_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_shared_image_wrapper.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

WebGpuSharedImageWrapperLease::WebGpuSharedImageWrapperLease(
    std::unique_ptr<WebGpuSharedImageWrapper> shared_image_wrapper,
    base::WeakPtr<WebGpuSharedImageWrapperCache> cache)
    : shared_image_wrapper_(std::move(shared_image_wrapper)), cache_(cache) {
  CanvasMemoryDumpProvider::Instance()->RegisterClient(this);
}

WebGpuSharedImageWrapperLease::~WebGpuSharedImageWrapperLease() {
  CanvasMemoryDumpProvider::Instance()->UnregisterClient(this);
  if (cache_ && shared_image_wrapper_) {
    cache_->ReturnWebGpuSharedImageWrapper(std::move(shared_image_wrapper_),
                                           completion_sync_token_);
  }
}

scoped_refptr<gpu::ClientSharedImage>
WebGpuSharedImageWrapperLease::GetSharedImage() const {
  if (IsGpuContextLost()) {
    return nullptr;
  }
  return shared_image_wrapper_->shared_image_;
}

gpu::SyncToken WebGpuSharedImageWrapperLease::GetSyncToken() const {
  if (IsGpuContextLost()) {
    return gpu::SyncToken();
  }
  return shared_image_wrapper_->release_sync_token_;
}

gpu::raster::RasterInterface* WebGpuSharedImageWrapperLease::RasterInterface()
    const {
  if (!shared_image_wrapper_->context_provider_wrapper_) {
    return nullptr;
  }
  return shared_image_wrapper_->context_provider_wrapper_->ContextProvider()
      .RasterInterface();
}

bool WebGpuSharedImageWrapperLease::IsGpuContextLost() const {
  auto* raster_interface = RasterInterface();
  return !raster_interface ||
         raster_interface->GetGraphicsResetStatusKHR() != GL_NO_ERROR;
}

bool WebGpuSharedImageWrapperLease::UploadToBackingSharedImage(
    const SkPixmap& pixmap,
    uint32_t src_x,
    uint32_t src_y) {
  const int dest_width = shared_image_wrapper_->Size().width();
  const int dest_height = shared_image_wrapper_->Size().height();

  SkPixmap subset;
  if (!pixmap.extractSubset(
          &subset,
          SkIRect::MakeXYWH(static_cast<int>(src_x), static_cast<int>(src_y),
                            dest_width, dest_height))) {
    return false;
  }

  TRACE_EVENT0("blink",
               "WebGpuSharedImageWrapperLease::"
               "UploadToBackingSharedImage");
  if (IsGpuContextLost()) {
    return false;
  }

  auto access = shared_image_wrapper_->shared_image_->BeginRasterAccess(
      RasterInterface(), shared_image_wrapper_->acquire_sync_token_,
      /*readonly=*/false);

  RasterInterface()->WritePixels(
      shared_image_wrapper_->shared_image_->mailbox(), /*dst_x_offset=*/0,
      /*dst_y_offset=*/0,
      shared_image_wrapper_->shared_image_->GetTextureTarget(), subset);
  auto sync_token = gpu::RasterScopedAccess::EndAccess(std::move(access));
  shared_image_wrapper_->release_sync_token_ = sync_token;
  shared_image_wrapper_->shared_image_->UpdateDestructionSyncToken(sync_token);

  shared_image_wrapper_->is_cleared_ = true;

  return true;
}

void WebGpuSharedImageWrapperLease::DrawToBackingSharedImage(
    base::FunctionRef<void(cc::PaintCanvas&)> draw_callback) {
  if (IsGpuContextLost()) {
    return;
  }

  draw_callback(shared_image_wrapper_->recorder_for_external_draws_
                    ->getRecordingCanvas());
  if (shared_image_wrapper_->recorder_for_external_draws_
          ->HasReleasableDrawOps()) {
    cc::PaintRecord last_recording =
        shared_image_wrapper_->recorder_for_external_draws_
            ->ReleaseMainRecording();

    auto access = shared_image_wrapper_->shared_image_->BeginRasterAccess(
        RasterInterface(), shared_image_wrapper_->acquire_sync_token_,
        /*readonly=*/false);

    const bool needs_clear = !shared_image_wrapper_->is_cleared_;
    shared_image_wrapper_->is_cleared_ = true;

    gpu::raster::RasterInterface* ri = RasterInterface();
    SkColor4f background_color =
        shared_image_wrapper_->GetAlphaType() == kOpaque_SkAlphaType
            ? SkColors::kBlack
            : SkColors::kTransparent;

    auto list = base::MakeRefCounted<cc::DisplayItemList>();
    list->StartPaint();
    list->push<cc::DrawRecordOp>(std::move(last_recording));
    list->EndPaintOfUnpaired(gfx::Rect(shared_image_wrapper_->Size().width(),
                                       shared_image_wrapper_->Size().height()));
    list->Finalize();

    gfx::Size size(shared_image_wrapper_->Size().width(),
                   shared_image_wrapper_->Size().height());
    size_t max_op_size_hint =
        gpu::raster::RasterInterface::kDefaultMaxOpSizeHint;
    gfx::Rect full_raster_rect(shared_image_wrapper_->Size().width(),
                               shared_image_wrapper_->Size().height());
    gfx::Rect playback_rect(shared_image_wrapper_->Size().width(),
                            shared_image_wrapper_->Size().height());
    gfx::Vector2dF post_translate(0.f, 0.f);
    gfx::Vector2dF post_scale(1.f, 1.f);

    const bool can_use_lcd_text =
        shared_image_wrapper_->GetAlphaType() == kOpaque_SkAlphaType;
    const auto& caps =
        shared_image_wrapper_->context_provider_wrapper_->ContextProvider()
            .GetCapabilities();
    bool use_msaa = !caps.msaa_is_slow && !caps.avoid_stencil_buffers;
    ri->BeginRasterCHROMIUM(
        background_color, needs_clear,
        /*msaa_sample_count=*/use_msaa ? 1 : 0,
        use_msaa ? gpu::raster::MsaaMode::kDMSAA
                 : gpu::raster::MsaaMode::kNoMSAA,
        can_use_lcd_text, /*visible=*/true,
        shared_image_wrapper_->GetColorSpace(),
        /*hdr_headroom=*/0.f,
        shared_image_wrapper_->shared_image_->mailbox().name);

    auto& context_provider =
        shared_image_wrapper_->context_provider_wrapper_->ContextProvider();
    CanvasImageProvider image_provider(
        context_provider.ImageDecodeCache(kN32_SkColorType),
        shared_image_wrapper_->GetSharedImageFormat() ==
                viz::SinglePlaneFormat::kRGBA_F16
            ? context_provider.ImageDecodeCache(kRGBA_F16_SkColorType)
            : nullptr,
        shared_image_wrapper_->GetColorSpace(),
        shared_image_wrapper_->GetSharedImageFormat(),
        cc::PlaybackImageProvider::RasterMode::kGpu,
        shared_image_wrapper_->context_provider_wrapper_);

    ri->RasterCHROMIUM(
        list.get(), &image_provider, size, full_raster_rect, playback_rect,
        post_translate, post_scale, /*requires_clear=*/false,
        /*raster_inducing_scroll_offsets=*/nullptr, &max_op_size_hint,
        base::RepeatingCallback<void(SkCanvas*, uint32_t)>());

    ri->EndRasterCHROMIUM();
    auto sync_token = gpu::RasterScopedAccess::EndAccess(std::move(access));
    shared_image_wrapper_->release_sync_token_ = sync_token;
    shared_image_wrapper_->shared_image_->UpdateDestructionSyncToken(
        sync_token);

    image_provider.ReleaseLockedImages();
    image_provider.UnbindTextureBackedImages();
  }
}

const gpu::SyncToken& WebGpuSharedImageWrapperLease::acquire_sync_token()
    const {
  return shared_image_wrapper_->acquire_sync_token_;
}

void WebGpuSharedImageWrapperLease::set_release_sync_token(
    const gpu::SyncToken& token) {
  shared_image_wrapper_->release_sync_token_ = token;
}

void WebGpuSharedImageWrapperLease::WriteToBackingSharedImage(
    base::FunctionRef<
        gpu::SyncToken(const scoped_refptr<gpu::ClientSharedImage>&,
                       const gpu::SyncToken&)> overwrite_callback) {
  if (IsGpuContextLost()) {
    return;
  }

  // NOTE: Invoking BeginRasterAccess() ensures that this invocation of
  // EndAccess() will generate a new sync token.
  auto access = shared_image_wrapper_->shared_image_->BeginRasterAccess(
      RasterInterface(), shared_image_wrapper_->acquire_sync_token_,
      /*readonly=*/false);
  auto sync_token = gpu::RasterScopedAccess::EndAccess(std::move(access));
  shared_image_wrapper_->release_sync_token_ = sync_token;
  shared_image_wrapper_->shared_image_->UpdateDestructionSyncToken(sync_token);

  gpu::SyncToken external_write_sync_token =
      overwrite_callback(shared_image_wrapper_->shared_image_,
                         shared_image_wrapper_->release_sync_token_);

  if (IsGpuContextLost()) {
    return;
  }

  // Ensure that any subsequent internal accesses wait for the external write to
  // complete.
  shared_image_wrapper_->WaitSyncToken(external_write_sync_token);

  // Additionally ensure that the next external read waits for the external
  // write to complete by ensuring that a new sync token is generated on the
  // internal interface. This new sync token will be chained after
  // `external_write_sync_token` thanks to the wait above.
  access = shared_image_wrapper_->shared_image_->BeginRasterAccess(
      RasterInterface(), shared_image_wrapper_->acquire_sync_token_,
      /*readonly=*/true);
  sync_token = gpu::RasterScopedAccess::EndAccess(std::move(access));
  shared_image_wrapper_->release_sync_token_ = sync_token;
  shared_image_wrapper_->shared_image_->UpdateDestructionSyncToken(sync_token);
}

bool WebGpuSharedImageWrapperLease::CopyToBackingSharedImage(
    const scoped_refptr<gpu::ClientSharedImage>& shared_image,
    uint32_t src_x,
    uint32_t src_y,
    const gpu::SyncToken& ready_sync_token,
    gpu::SyncToken& completion_sync_token) {
  gpu::raster::RasterInterface* raster = RasterInterface();
  if (!raster) {
    return false;
  }

  if (IsGpuContextLost()) {
    return false;
  }

  gfx::Rect copy_rect(src_x, src_y, shared_image_wrapper_->Size().width(),
                      shared_image_wrapper_->Size().height());

  auto dst_access = shared_image_wrapper_->shared_image_->BeginRasterAccess(
      raster, shared_image_wrapper_->acquire_sync_token_,
      /*readonly=*/false);

  std::unique_ptr<gpu::RasterScopedAccess> src_access =
      shared_image->BeginRasterAccess(raster, ready_sync_token,
                                      /*readonly=*/true);
  raster->CopySharedImage(shared_image->mailbox(),
                          shared_image_wrapper_->shared_image_->mailbox(),
                          /*xoffset=*/0,
                          /*yoffset=*/0, copy_rect.x(), copy_rect.y(),
                          copy_rect.width(), copy_rect.height());
  completion_sync_token =
      gpu::RasterScopedAccess::EndAccess(std::move(src_access));
  auto sync_token = gpu::RasterScopedAccess::EndAccess(std::move(dst_access));
  shared_image_wrapper_->release_sync_token_ = sync_token;
  shared_image_wrapper_->shared_image_->UpdateDestructionSyncToken(sync_token);
  shared_image_wrapper_->is_cleared_ = true;
  return true;
}

void WebGpuSharedImageWrapperLease::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd) {
  std::string path = base::StringPrintf("canvas/ResourceProvider_0x%" PRIXPTR,
                                        reinterpret_cast<uintptr_t>(this));

  std::string dump_name =
      base::StringPrintf("%s/CanvasResource_0x%" PRIXPTR, path.c_str(),
                         reinterpret_cast<uintptr_t>(this));
  auto* dump = pmd->CreateAllocatorDump(dump_name);
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  GetSize());

  shared_image_wrapper_->shared_image_->OnMemoryDump(
      pmd, dump->guid(),
      static_cast<int>(gpu::TracingImportance::kClientOwner));
}

size_t WebGpuSharedImageWrapperLease::GetSize() const {
  return base::checked_cast<size_t>(
      shared_image_wrapper_->shared_image_->EstimatedSizeInBytes().InBytes());
}

WebGpuSharedImageWrapperCache::WebGpuSharedImageWrapperCache(
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : context_provider_(std::move(context_provider)),
      task_runner_(std::move(task_runner)) {
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
  timer_func_ = blink::BindRepeating(
      &WebGpuSharedImageWrapperCache::ReleaseStaleResources, weak_ptr_);

  DCHECK_LE(kTimerDurationInSeconds, kCleanUpDelayInSeconds);
  CanvasMemoryDumpProvider::Instance()->RegisterClient(this);
}

WebGpuSharedImageWrapperCache::~WebGpuSharedImageWrapperCache() {
  CanvasMemoryDumpProvider::Instance()->UnregisterClient(this);
}

void WebGpuSharedImageWrapperCache::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd) {
  for (const auto& unused_resource : unused_wrappers_) {
    std::string path =
        base::StringPrintf("canvas/ResourceProvider_0x%" PRIXPTR,
                           reinterpret_cast<uintptr_t>(
                               unused_resource.shared_image_wrapper_.get()));

    std::string dump_name =
        base::StringPrintf("%s/CanvasResource_0x%" PRIXPTR, path.c_str(),
                           reinterpret_cast<uintptr_t>(
                               unused_resource.shared_image_wrapper_.get()));
    auto* dump = pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    unused_resource.resource_size_);

    unused_resource.shared_image_wrapper_->shared_image_->OnMemoryDump(
        pmd, dump->guid(),
        static_cast<int>(gpu::TracingImportance::kClientOwner));
  }
}

size_t WebGpuSharedImageWrapperCache::GetSize() const {
  return base::checked_cast<size_t>(total_unused_resources_in_bytes_);
}

std::unique_ptr<WebGpuSharedImageWrapperLease>
WebGpuSharedImageWrapperCache::LeaseWebGpuSharedImageWrapper(
    viz::SharedImageFormat format,
    gfx::Size size,
    const gfx::ColorSpace& color_space,
    SkAlphaType alpha_type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::unique_ptr<WebGpuSharedImageWrapper> wrapper =
      AcquireCachedWrapper(size, format, alpha_type, color_space);
  if (!wrapper) {
    wrapper = WebGpuSharedImageWrapper::Create(size, format, alpha_type,
                                               color_space);
    if (!wrapper) {
      return nullptr;
    }
  }

  return std::make_unique<WebGpuSharedImageWrapperLease>(std::move(wrapper),
                                                         weak_ptr_);
}

void WebGpuSharedImageWrapperCache::ReturnWebGpuSharedImageWrapper(
    std::unique_ptr<WebGpuSharedImageWrapper> shared_image_wrapper,
    const gpu::SyncToken& completion_sync_token) {
  size_t resource_size =
      shared_image_wrapper->GetSharedImageFormat().EstimatedSizeInBytes(
          shared_image_wrapper->Size());

  if (context_provider_) {
    total_unused_resources_in_bytes_ += resource_size;

    shared_image_wrapper->WaitSyncToken(completion_sync_token);

    unused_wrappers_.push_front(Resource(std::move(shared_image_wrapper),
                                         current_timer_id_, resource_size));
  }

  // If the cache is full, release LRU from the back.
  while (total_unused_resources_in_bytes_ >
         kMaxSharedImageWrapperCachesInBytes) {
    total_unused_resources_in_bytes_ -= unused_wrappers_.back().resource_size_;
    unused_wrappers_.pop_back();
  }

  StartResourceCleanUpTimer();
}

WebGpuSharedImageWrapperCache::Resource::Resource(
    std::unique_ptr<WebGpuSharedImageWrapper> shared_image_wrapper,
    unsigned int timer_id,
    size_t resource_size)
    : shared_image_wrapper_(std::move(shared_image_wrapper)),
      timer_id_(timer_id),
      resource_size_(resource_size) {}

WebGpuSharedImageWrapperCache::Resource::Resource(Resource&& that) noexcept =
    default;

WebGpuSharedImageWrapperCache::Resource::~Resource() = default;

std::unique_ptr<WebGpuSharedImageWrapper>
WebGpuSharedImageWrapperCache::AcquireCachedWrapper(
    const gfx::Size& size,
    const viz::SharedImageFormat& format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space) {
  // Loop from MRU to LRU
  DequeSharedImageWrapper::iterator it;
  for (it = unused_wrappers_.begin(); it != unused_wrappers_.end(); ++it) {
    WebGpuSharedImageWrapper* wrapper = it->shared_image_wrapper_.get();
    if (wrapper->Size() == size && wrapper->GetSharedImageFormat() == format &&
        wrapper->GetAlphaType() == alpha_type &&
        wrapper->GetColorSpace() == color_space) {
      break;
    }
  }

  // Found one.
  if (it != unused_wrappers_.end()) {
    std::unique_ptr<WebGpuSharedImageWrapper> wrapper =
        (std::move(it->shared_image_wrapper_));
    total_unused_resources_in_bytes_ -= it->resource_size_;
    // TODO(magchen@): If the cache capacity increases a lot, will erase(it)
    // becomes inefficient?
    // Remove the wrapper from the |unused_wrappers_|.
    unused_wrappers_.erase(it);

    return wrapper;
  }
  return nullptr;
}

void WebGpuSharedImageWrapperCache::ReleaseStaleResources() {
  timer_is_running_ = false;

  // Loop from LRU to MRU
  int stale_resource_count = 0;
  for (const auto& unused_wrapper : base::Reversed(unused_wrappers_)) {
    if ((current_timer_id_ - unused_wrapper.timer_id_) <
        kTimerIdDeltaForDeletion) {
      // These are the resources which are recycled and stay in the cache for
      // less than kCleanUpDelayInSeconds. They are not to be deleted this time.
      break;
    }
    stale_resource_count++;
  }

  // Delete all stale resources.
  for (int i = 0; i < stale_resource_count; ++i) {
    total_unused_resources_in_bytes_ -= unused_wrappers_.back().resource_size_;
    unused_wrappers_.pop_back();
  }

  current_timer_id_++;
  StartResourceCleanUpTimer();
}
void WebGpuSharedImageWrapperCache::StartResourceCleanUpTimer() {
  if (unused_wrappers_.size() > 0 && !timer_is_running_) {
    task_runner_->PostDelayedTask(FROM_HERE, timer_func_,
                                  base::Seconds(kTimerDurationInSeconds));
    timer_is_running_ = true;
  }
}

wtf_size_t
WebGpuSharedImageWrapperCache::CleanUpResourcesAndReturnSizeForTesting() {
  ReleaseStaleResources();
  return unused_wrappers_.size();
}

}  // namespace blink
