/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/graphics/canvas_2d_layer_bridge.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/location.h"
#include "base/rand_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/process_memory_dump.h"
#include "cc/layers/texture_layer.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/config/gpu_finch_features.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_context_rate_limiter.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/encode/SkPngEncoder.h"

namespace blink {

// static
bool Canvas2DLayerBridge::IsHibernationEnabled() {
  return base::FeatureList::IsEnabled(features::kCanvas2DHibernation);
}

HibernationHandler::~HibernationHandler() {
  DCheckInvariant();
  if (IsHibernating()) {
    HibernatedCanvasMemoryDumpProvider::GetInstance().Unregister(this);
  }
}

void HibernationHandler::TakeHibernationImage(sk_sp<SkImage>&& image) {
  DCheckInvariant();
  epoch_++;
  image_ = image;

  width_ = image_->width();
  height_ = image_->height();
  bytes_per_pixel_ = image_->imageInfo().bytesPerPixel();

  // If we had an encoded version, discard it.
  encoded_.reset();

  HibernatedCanvasMemoryDumpProvider::GetInstance().Register(this);

  // Don't bother compressing very small canvases.
  if (ImageMemorySize(*image) < 16 * 1024 ||
      !base::FeatureList::IsEnabled(features::kCanvasCompressHibernatedImage)) {
    return;
  }

  // Don't post the compression task to the thread pool with a delay right away.
  // The task increases the reference count on the SkImage. In the case of rapid
  // foreground / background transitions, each transition allocates a new
  // SkImage. If we post a compression task right away with a sk_sp<SkImage> as
  // a parameter, this takes a reference on the underlying SkImage, keeping it
  // alive until the task runs. This means that posting the compression task
  // right away would increase memory usage by a lot in these cases.
  //
  // Rather, post a main thread task later that will check whether we are still
  // in hibernation mode, and in the same hibernation "epoch" as last time. If
  // this is the case, then compress.
  //
  // This simplifies tracking of background / foreground cycles, at the cost of
  // running one extra trivial task for each cycle.
  //
  // Note: not using a delayed idle tasks, because idle tasks do not run when
  // the renderer is idle. In other words, a delayed idle task would not execute
  // as long as the renderer is in background, which completely defeats the
  // purpose.
  GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&HibernationHandler::OnAfterHibernation,
                     weak_ptr_factory_.GetWeakPtr(), epoch_),
      kBeforeCompressionDelay);
}

void HibernationHandler::OnAfterHibernation(uint64_t epoch) {
  DCheckInvariant();
  DCHECK(
      base::FeatureList::IsEnabled(features::kCanvasCompressHibernatedImage));
  // Either we no longer have the image (because we are not hibernating), or we
  // went through another visible / not visible cycle (in which case it is too
  // early to compress).
  if (epoch_ != epoch || !image_) {
    return;
  }
  auto task_runner = GetMainThreadTaskRunner();
  auto params = std::make_unique<BackgroundTaskParams>(
      image_, epoch_, weak_ptr_factory_.GetWeakPtr(), task_runner);

  if (background_thread_task_runner_for_testing_) {
    background_thread_task_runner_for_testing_->PostTask(
        FROM_HERE,
        base::BindOnce(&HibernationHandler::Encode, std::move(params)));
  } else {
    worker_pool::PostTask(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT},
        CrossThreadBindOnce(&HibernationHandler::Encode, std::move(params)));
  }
}

void HibernationHandler::OnEncoded(
    std::unique_ptr<HibernationHandler::BackgroundTaskParams> params,
    sk_sp<SkData> encoded) {
  DCheckInvariant();
  DCHECK(
      base::FeatureList::IsEnabled(features::kCanvasCompressHibernatedImage));
  // Discard the compressed image, it is no longer current.
  if (params->epoch != epoch_ || !IsHibernating()) {
    return;
  }

  DCHECK_EQ(image_.get(), params->image.get());
  encoded_ = encoded;
  image_ = nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
HibernationHandler::GetMainThreadTaskRunner() const {
  return main_thread_task_runner_for_testing_
             ? main_thread_task_runner_for_testing_
             : Thread::MainThread()->GetTaskRunner(
                   MainThreadTaskRunnerRestricted());
}

void HibernationHandler::Encode(
    std::unique_ptr<HibernationHandler::BackgroundTaskParams> params) {
  TRACE_EVENT0("blink", __PRETTY_FUNCTION__);
  DCHECK(
      base::FeatureList::IsEnabled(features::kCanvasCompressHibernatedImage));
  sk_sp<SkData> encoded =
      SkPngEncoder::Encode(nullptr, params->image.get(), {});

  size_t original_memory_size = ImageMemorySize(*params->image);
  int compression_ratio_percentage = static_cast<int>(
      (static_cast<size_t>(100) * encoded->size()) / original_memory_size);
  UMA_HISTOGRAM_PERCENTAGE("Blink.Canvas.2DLayerBridge.Compression.Ratio",
                           compression_ratio_percentage);
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Blink.Canvas.2DLayerBridge.Compression.SnapshotSizeKb",
      static_cast<int>(original_memory_size / 1024), 10, 500000, 50);

  auto* reply_task_runner = params->reply_task_runner.get();
  reply_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&HibernationHandler::OnEncoded, params->weak_instance,
                     std::move(params), encoded));
}

sk_sp<SkImage> HibernationHandler::GetImage() {
  TRACE_EVENT0("blink", __PRETTY_FUNCTION__);
  DCheckInvariant();
  if (image_) {
    return image_;
  }

  DCHECK(encoded_);
  DCHECK(
      base::FeatureList::IsEnabled(features::kCanvasCompressHibernatedImage));

  base::TimeTicks before = base::TimeTicks::Now();
  // Note: not discarding the encoded image.
  auto image = SkImages::DeferredFromEncodedData(encoded_)->makeRasterImage();
  base::TimeTicks after = base::TimeTicks::Now();
  UMA_HISTOGRAM_TIMES(
      "Blink.Canvas.2DLayerBridge.Compression.DecompressionTime",
      after - before);
  return image;
  ;
}

void HibernationHandler::Clear() {
  DCheckInvariant();
  HibernatedCanvasMemoryDumpProvider::GetInstance().Unregister(this);
  encoded_ = nullptr;
  image_ = nullptr;
}

size_t HibernationHandler::memory_size() const {
  DCheckInvariant();
  DCHECK(IsHibernating());
  if (is_encoded()) {
    return encoded_->size();
  } else {
    return original_memory_size();
  }
}

// static
size_t HibernationHandler::ImageMemorySize(const SkImage& image) {
  return static_cast<size_t>(image.height()) * image.width() *
         image.imageInfo().bytesPerPixel();
}

size_t HibernationHandler::original_memory_size() const {
  return static_cast<size_t>(width_) * height_ * bytes_per_pixel_;
}

// static
HibernatedCanvasMemoryDumpProvider&
HibernatedCanvasMemoryDumpProvider::GetInstance() {
  static base::NoDestructor<HibernatedCanvasMemoryDumpProvider> instance;
  return *instance.get();
}

void HibernatedCanvasMemoryDumpProvider::Register(HibernationHandler* handler) {
  DCHECK(IsMainThread());
  base::AutoLock locker(lock_);
  DCHECK(handler->IsHibernating());
  handlers_.insert(handler);
}

void HibernatedCanvasMemoryDumpProvider::Unregister(
    HibernationHandler* handler) {
  DCHECK(IsMainThread());
  base::AutoLock locker(lock_);
  DCHECK(handlers_.Contains(handler));
  handlers_.erase(handler);
}

bool HibernatedCanvasMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK(IsMainThread());

  size_t total_hibernated_size = 0;
  size_t total_original_size = 0;
  auto* dump = pmd->CreateAllocatorDump("canvas/hibernated");

  {
    base::AutoLock locker(lock_);
    int index = 0;
    for (HibernationHandler* handler : handlers_) {
      DCHECK(handler->IsHibernating());
      total_original_size += handler->original_memory_size();
      total_hibernated_size += handler->memory_size();

      if (args.level_of_detail ==
          base::trace_event::MemoryDumpLevelOfDetail::DETAILED) {
        auto* canvas_dump = pmd->CreateAllocatorDump(
            base::StringPrintf("canvas/hibernated/canvas_%d", index));
        canvas_dump->AddScalar("memory_size", "bytes", handler->memory_size());
        canvas_dump->AddScalar("is_encoded", "boolean", handler->is_encoded());
        canvas_dump->AddScalar("original_memory_size", "bytes",
                               handler->original_memory_size());
        canvas_dump->AddScalar("height", "pixels", handler->height());
        canvas_dump->AddScalar("width", "pixels", handler->width());
      }
      index++;
    }
  }

  dump->AddScalar("size", "bytes", total_hibernated_size);
  dump->AddScalar("original_size", "bytes", total_original_size);

  return true;
}

HibernatedCanvasMemoryDumpProvider::HibernatedCanvasMemoryDumpProvider() {
  DCHECK(IsMainThread());
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "hibernated_canvas",
      Thread::MainThread()->GetTaskRunner(MainThreadTaskRunnerRestricted()));
}

Canvas2DLayerBridge::Canvas2DLayerBridge(const gfx::Size& size,
                                         RasterMode raster_mode,
                                         OpacityMode opacity_mode)
    : logger_(std::make_unique<Logger>()),
      have_recorded_draw_commands_(false),
      is_hidden_(false),
      is_being_displayed_(false),
      raster_mode_(raster_mode),
      opacity_mode_(opacity_mode),
      size_(size),
      snapshot_state_(kInitialSnapshotState),
      resource_host_(nullptr) {
  // Used by browser tests to detect the use of a Canvas2DLayerBridge.
  TRACE_EVENT_INSTANT0("test_gpu", "Canvas2DLayerBridgeCreation",
                       TRACE_EVENT_SCOPE_GLOBAL);
}

Canvas2DLayerBridge::~Canvas2DLayerBridge() {
  ClearPendingRasterTimers();
  if (IsHibernating())
    logger_->ReportHibernationEvent(kHibernationEndedWithTeardown);
  ResetResourceProvider();

  if (!layer_)
    return;

  if (raster_mode_ == RasterMode::kGPU) {
    layer_->ClearTexture();
    // Orphaning the layer is required to trigger the recreation of a new layer
    // in the case where destruction is caused by a canvas resize. Test:
    // virtual/gpu/fast/canvas/canvas-resize-after-paint-without-layout.html
    layer_->RemoveFromParent();
  }
  layer_->ClearClient();
  layer_ = nullptr;
}

void Canvas2DLayerBridge::SetCanvasResourceHost(CanvasResourceHost* host) {
  resource_host_ = host;
}

void Canvas2DLayerBridge::ResetResourceProvider() {
  if (resource_host_)
    resource_host_->ReplaceResourceProvider(nullptr);
}

bool Canvas2DLayerBridge::ShouldAccelerate() const {
  bool use_gpu = raster_mode_ == RasterMode::kGPU;

  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper =
      SharedGpuContext::ContextProviderWrapper();
  if (use_gpu &&
      (!context_provider_wrapper ||
       context_provider_wrapper->ContextProvider()->IsContextLost())) {
    use_gpu = false;
  }
  return use_gpu;
}

bool Canvas2DLayerBridge::IsAccelerated() const {
  if (raster_mode_ == RasterMode::kCPU)
    return false;
  if (IsHibernating())
    return false;
  if (resource_host_ && resource_host_->ResourceProvider())
    return resource_host_->ResourceProvider()->IsAccelerated();

  // Whether or not to accelerate is not yet resolved, the canvas cannot be
  // accelerated if the gpu context is lost.
  return ShouldAccelerate();
}

bool Canvas2DLayerBridge::IsComposited() const {
  if (IsHibernating()) {
    return false;
  }

  if (UNLIKELY(!resource_host_)) {
    return false;
  }

  CanvasResourceProvider* resource_provider =
      resource_host_->ResourceProvider();
  if (UNLIKELY(!resource_provider)) {
    return false;
  }

  return resource_provider->SupportsDirectCompositing() &&
         !resource_host_->LowLatencyEnabled();
}

static void HibernateWrapper(base::WeakPtr<Canvas2DLayerBridge> bridge,
                             base::TimeTicks /*idleDeadline*/) {
  if (bridge) {
    bridge->Hibernate();
  } else {
    Canvas2DLayerBridge::Logger local_logger;
    local_logger.ReportHibernationEvent(
        Canvas2DLayerBridge::
            kHibernationAbortedDueToDestructionWhileHibernatePending);
  }
}

static void LoseContextInBackgroundWrapper(
    base::WeakPtr<Canvas2DLayerBridge> bridge,
    base::TimeTicks /*idleDeadline*/) {
  if (bridge) {
    bridge->LoseContext();
  }
}

void Canvas2DLayerBridge::Hibernate() {
  TRACE_EVENT0("blink", __PRETTY_FUNCTION__);
  DCHECK(!IsHibernating());
  DCHECK(hibernation_scheduled_);

  hibernation_scheduled_ = false;

  if (!resource_host_ || !resource_host_->ResourceProvider()) {
    logger_->ReportHibernationEvent(kHibernationAbortedBecauseNoSurface);
    return;
  }

  if (!IsHidden()) {
    logger_->ReportHibernationEvent(kHibernationAbortedDueToVisibilityChange);
    return;
  }

  if (!IsValid()) {
    logger_->ReportHibernationEvent(kHibernationAbortedDueGpuContextLoss);
    return;
  }

  if (!IsAccelerated()) {
    logger_->ReportHibernationEvent(
        kHibernationAbortedDueToSwitchToUnacceleratedRendering);
    return;
  }

  TRACE_EVENT0("blink", "Canvas2DLayerBridge::hibernate");
  // No HibernationEvent reported on success. This is on purppose to avoid
  // non-complementary stats. Each HibernationScheduled event is paired with
  // exactly one failure or exit event.
  FlushRecording(CanvasResourceProvider::FlushReason::kHibernating);
  // The following checks that the flush succeeded, which should always be the
  // case because flushRecording should only fail it it fails to allocate
  // a surface, and we have an early exit at the top of this function for when
  // 'this' does not already have a surface.
  DCHECK(!have_recorded_draw_commands_);
  SkPaint copy_paint;
  copy_paint.setBlendMode(SkBlendMode::kSrc);
  scoped_refptr<StaticBitmapImage> snapshot =
      resource_host_->ResourceProvider()->Snapshot(
          CanvasResourceProvider::FlushReason::kHibernating);
  if (!snapshot) {
    logger_->ReportHibernationEvent(kHibernationAbortedDueSnapshotFailure);
    return;
  }
  hibernation_handler_.TakeHibernationImage(
      snapshot->PaintImageForCurrentFrame().GetSwSkImage());

  ResetResourceProvider();
  if (layer_) {
    layer_->ClearTexture();
  }

  // shouldBeDirectComposited() may have changed.
  if (resource_host_) {
    resource_host_->SetNeedsCompositingUpdate();
  }
  logger_->DidStartHibernating();
}

void Canvas2DLayerBridge::LoseContext() {
  DCHECK(!lose_context_in_background_);
  DCHECK(lose_context_in_background_scheduled_);

  lose_context_in_background_scheduled_ = false;

  // If canvas becomes visible again or canvas already lost its resource,
  // return here.
  if (!resource_host_ || !resource_host_->ResourceProvider() || !IsHidden() ||
      !IsValid() || context_lost_)
    return;

  SkipQueuedDrawCommands();
  DCHECK(!have_recorded_draw_commands_);

  // Frees canvas resource.
  lose_context_in_background_ = true;
  ResetResourceProvider();

  if (layer_)
    layer_->ClearTexture();

  if (resource_host_)
    resource_host_->SetNeedsCompositingUpdate();
}

CanvasResourceProvider* Canvas2DLayerBridge::ResourceProvider() const {
  return resource_host_ ? resource_host_->ResourceProvider() : nullptr;
}

CanvasResourceProvider* Canvas2DLayerBridge::GetOrCreateResourceProvider() {
  DCHECK(resource_host_);
  CanvasResourceProvider* resource_provider = ResourceProvider();

  if (context_lost_) {
    DCHECK(!resource_provider);
    return nullptr;
  }

  if (resource_provider && resource_provider->IsValid()) {
#if DCHECK_IS_ON()
    // If resource provider is composited, a layer should already exist.
    // unless this is a canvas in low latency mode.
    // If this DCHECK fails, it probably means that
    // CanvasRenderingContextHost::GetOrCreateCanvasResourceProvider() was
    // called on a 2D context before this function.
    if (IsComposited()) {
      DCHECK(!!layer_);
    }
#endif
    return resource_provider;
  }

  // Restore() is tried at most four times in two seconds to recreate the
  // ResourceProvider before the final attempt, in which a new
  // Canvas2DLayerBridge is created along with its resource provider.

  bool want_acceleration = ShouldAccelerate();
  RasterModeHint adjusted_hint = want_acceleration ? RasterModeHint::kPreferGPU
                                                   : RasterModeHint::kPreferCPU;

  // Re-creation will happen through Restore().
  // If the Canvas2DLayerBridge has just been created, possibly due to failed
  // attempts of Restore(), the layer would not exist, therefore, it will not
  // fall through this clause to try Restore() again
  if (layer_ && adjusted_hint == RasterModeHint::kPreferGPU &&
      !lose_context_in_background_ && !IsHibernating()) {
    return nullptr;
  }

  // We call GetOrCreateCanvasResourceProviderImpl directly here to prevent a
  // circular callstack from HTMLCanvasElement.
  resource_provider =
      resource_host_->GetOrCreateCanvasResourceProviderImpl(adjusted_hint);
  if (!resource_provider || !resource_provider->IsValid())
    return nullptr;

  // Calling to DidDraw because GetOrCreateResourceProvider created a new
  // provider and cleared it
  // TODO crbug/1090081: Check possibility to move DidDraw inside Clear.
  DidDraw();

  if (IsComposited() && !layer_) {
    layer_ = cc::TextureLayer::CreateForMailbox(this);
    layer_->SetIsDrawable(true);
    layer_->SetHitTestable(true);
    layer_->SetContentsOpaque(opacity_mode_ == kOpaque);
    layer_->SetBlendBackgroundColor(opacity_mode_ != kOpaque);
    layer_->SetNearestNeighbor(resource_host_->FilterQuality() ==
                               cc::PaintFlags::FilterQuality::kNone);
    layer_->SetHDRConfiguration(resource_host_->GetHDRMode(),
                                resource_host_->GetHDRMetadata());
    layer_->SetFlipped(!resource_provider->IsOriginTopLeft());
  }
  // After the page becomes visible and successfully restored the canvas
  // resource provider, set |lose_context_in_background_| to false.
  if (lose_context_in_background_)
    lose_context_in_background_ = false;

  if (!IsHibernating())
    return resource_provider;

  if (resource_provider->IsAccelerated()) {
    logger_->ReportHibernationEvent(kHibernationEndedNormally);
  } else {
    if (IsHidden()) {
      logger_->ReportHibernationEvent(
          kHibernationEndedWithSwitchToBackgroundRendering);
    } else {
      logger_->ReportHibernationEvent(kHibernationEndedWithFallbackToSW);
    }
  }

  PaintImageBuilder builder = PaintImageBuilder::WithDefault();
  builder.set_image(hibernation_handler_.GetImage(),
                    PaintImage::GetNextContentId());
  builder.set_id(PaintImage::GetNextId());
  resource_provider->RestoreBackBuffer(builder.TakePaintImage());
  // The hibernation image is no longer valid, clear it.
  hibernation_handler_.Clear();
  DCHECK(!IsHibernating());

  if (resource_host_) {
    // shouldBeDirectComposited() may have changed.
    resource_host_->SetNeedsCompositingUpdate();
  }
  return resource_provider;
}

cc::PaintCanvas* Canvas2DLayerBridge::GetPaintCanvas() {
  DCHECK(resource_host_);
  // We avoid only using GetOrCreateResourceProvider() here to skip the
  // IsValid/ContextLost checks since this is in hot code paths. The context
  // does not need to be valid here since only the recording canvas is used.
  if (!ResourceProvider() && !GetOrCreateResourceProvider())
    return nullptr;
  return ResourceProvider()->Canvas();
}

void Canvas2DLayerBridge::SetFilterQuality(
    cc::PaintFlags::FilterQuality filter_quality) {
  if (CanvasResourceProvider* resource_provider = ResourceProvider())
    resource_provider->SetFilterQuality(filter_quality);
  if (layer_)
    layer_->SetNearestNeighbor(filter_quality ==
                               cc::PaintFlags::FilterQuality::kNone);
}

void Canvas2DLayerBridge::SetHDRConfiguration(
    gfx::HDRMode hdr_mode,
    absl::optional<gfx::HDRMetadata> hdr_metadata) {
  if (layer_)
    layer_->SetHDRConfiguration(hdr_mode, hdr_metadata);
}

void Canvas2DLayerBridge::SetIsInHiddenPage(bool hidden) {
  if (is_hidden_ == hidden)
    return;

  is_hidden_ = hidden;
  if (ResourceProvider())
    ResourceProvider()->SetResourceRecyclingEnabled(!IsHidden());

  // Conserve memory.
  if (base::FeatureList::IsEnabled(features::kCanvasFreeMemoryWhenHidden) &&
      IsAccelerated() && SharedGpuContext::ContextProviderWrapper() &&
      SharedGpuContext::ContextProviderWrapper()->ContextProvider()) {
    auto* context_support = SharedGpuContext::ContextProviderWrapper()
                                ->ContextProvider()
                                ->ContextSupport();
    if (context_support)
      context_support->SetAggressivelyFreeResources(hidden);
  }

  if (!lose_context_in_background_ && !lose_context_in_background_scheduled_ &&
      ResourceProvider() && !context_lost_ && IsHidden() &&
      base::FeatureList::IsEnabled(
          ::features::kCanvasContextLostInBackground)) {
    lose_context_in_background_scheduled_ = true;
    ThreadScheduler::Current()->PostIdleTask(
        FROM_HERE, WTF::BindOnce(&LoseContextInBackgroundWrapper,
                                 weak_ptr_factory_.GetWeakPtr()));
  } else if (IsHibernationEnabled() && ResourceProvider() && IsAccelerated() &&
             IsHidden() && !hibernation_scheduled_ &&
             !base::FeatureList::IsEnabled(
                 ::features::kCanvasContextLostInBackground)) {
    if (layer_)
      layer_->ClearTexture();
    logger_->ReportHibernationEvent(kHibernationScheduled);
    hibernation_scheduled_ = true;
    ThreadScheduler::Current()->PostIdleTask(
        FROM_HERE,
        WTF::BindOnce(&HibernateWrapper, weak_ptr_factory_.GetWeakPtr()));
  }
  if (!IsHidden() && (IsHibernating() || lose_context_in_background_))
    GetOrCreateResourceProvider();  // Rude awakening
}

void Canvas2DLayerBridge::SetIsBeingDisplayed(bool displayed) {
  is_being_displayed_ = displayed;
  // If the canvas is no longer being displayed, stop using the rate
  // limiter.
  if (!is_being_displayed_) {
    frames_since_last_commit_ = 0;
    if (rate_limiter_) {
      rate_limiter_->Reset();
      rate_limiter_.reset(nullptr);
    }
  }
}

void Canvas2DLayerBridge::DrawFullImage(const cc::PaintImage& image) {
  GetPaintCanvas()->drawImage(image, 0, 0);
}

bool Canvas2DLayerBridge::WritePixels(const SkImageInfo& orig_info,
                                      const void* pixels,
                                      size_t row_bytes,
                                      int x,
                                      int y) {
  if (!GetOrCreateResourceProvider())
    return false;

  if (x <= 0 && y <= 0 && x + orig_info.width() >= size_.width() &&
      y + orig_info.height() >= size_.height()) {
    SkipQueuedDrawCommands();
  } else {
    FlushRecording(CanvasResourceProvider::FlushReason::kWritePixels);
    if (!GetOrCreateResourceProvider())
      return false;
  }
  have_recorded_draw_commands_ = false;

  bool wrote_pixels =
      ResourceProvider()->WritePixels(orig_info, pixels, row_bytes, x, y);
  if (wrote_pixels)
    last_record_tainted_by_write_pixels_ = true;

  return wrote_pixels;
}

void Canvas2DLayerBridge::SkipQueuedDrawCommands() {
  ResourceProvider()->SkipQueuedDrawCommands();
  have_recorded_draw_commands_ = false;
}

void Canvas2DLayerBridge::ClearPendingRasterTimers() {
  gpu::raster::RasterInterface* raster_interface = nullptr;
  if (IsAccelerated() && SharedGpuContext::ContextProviderWrapper() &&
      SharedGpuContext::ContextProviderWrapper()->ContextProvider()) {
    raster_interface = SharedGpuContext::ContextProviderWrapper()
                           ->ContextProvider()
                           ->RasterInterface();
  }

  if (raster_interface) {
    while (!pending_raster_timers_.empty()) {
      RasterTimer rt = pending_raster_timers_.TakeFirst();
      raster_interface->DeleteQueriesEXT(1, &rt.gl_query_id);
    }
  } else {
    pending_raster_timers_.clear();
  }
}

void Canvas2DLayerBridge::FinishRasterTimers(
    gpu::raster::RasterInterface* raster_interface) {
  // If the context was lost, then the old queries are not valid anymore
  if (!CheckResourceProviderValid()) {
    ClearPendingRasterTimers();
    return;
  }

  // Finish up any pending queries that are complete
  while (!pending_raster_timers_.empty()) {
    auto it = pending_raster_timers_.begin();
    GLuint complete = 1;
    raster_interface->GetQueryObjectuivEXT(
        it->gl_query_id, GL_QUERY_RESULT_AVAILABLE_NO_FLUSH_CHROMIUM_EXT,
        &complete);
    if (!complete) {
      break;
    }

    GLuint raw_gpu_duration = 0u;
    raster_interface->GetQueryObjectuivEXT(it->gl_query_id, GL_QUERY_RESULT_EXT,
                                           &raw_gpu_duration);
    base::TimeDelta gpu_duration_microseconds =
        base::Microseconds(raw_gpu_duration);
    base::TimeDelta total_time =
        gpu_duration_microseconds + it->cpu_raster_duration;

    base::TimeDelta min = base::Microseconds(1);
    base::TimeDelta max = base::Milliseconds(100);
    int num_buckets = 100;
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Blink.Canvas.RasterDuration.Accelerated.GPU",
        gpu_duration_microseconds, min, max, num_buckets);
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Blink.Canvas.RasterDuration.Accelerated.CPU", it->cpu_raster_duration,
        min, max, num_buckets);
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Blink.Canvas.RasterDuration.Accelerated.Total", total_time, min, max,
        num_buckets);

    raster_interface->DeleteQueriesEXT(1, &it->gl_query_id);

    pending_raster_timers_.erase(it);
  }
}

void Canvas2DLayerBridge::FlushRecording(
    CanvasResourceProvider::FlushReason reason) {
  if (!have_recorded_draw_commands_ || !GetOrCreateResourceProvider())
    return;

  TRACE_EVENT0("cc", "Canvas2DLayerBridge::flushRecording");

  gpu::raster::RasterInterface* raster_interface = nullptr;
  if (IsAccelerated() && SharedGpuContext::ContextProviderWrapper() &&
      SharedGpuContext::ContextProviderWrapper()->ContextProvider()) {
    raster_interface = SharedGpuContext::ContextProviderWrapper()
                           ->ContextProvider()
                           ->RasterInterface();
    FinishRasterTimers(raster_interface);
  }

  // Sample one out of every kRasterMetricProbability frames to time
  // If the canvas is accelerated, we also need access to the raster_interface
  const bool will_measure =
      always_measure_for_testing_ ||
      metrics_subsampler_.ShouldSample(kRasterMetricProbability);
  const bool measure_raster_metric =
      (raster_interface || !IsAccelerated()) && will_measure;

  RasterTimer rasterTimer;
  absl::optional<base::ElapsedTimer> timer;
  // Start Recording the raster duration
  if (measure_raster_metric) {
    if (IsAccelerated()) {
      GLuint gl_id = 0u;
      raster_interface->GenQueriesEXT(1, &gl_id);
      raster_interface->BeginQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM, gl_id);
      rasterTimer.gl_query_id = gl_id;
    }
    timer.emplace();
  }

  last_recording_ = ResourceProvider()->FlushCanvas(reason);

  last_record_tainted_by_write_pixels_ = false;

  // Finish up the timing operation
  if (measure_raster_metric) {
    if (IsAccelerated()) {
      rasterTimer.cpu_raster_duration = timer->Elapsed();
      raster_interface->EndQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM);
      pending_raster_timers_.push_back(rasterTimer);
    } else {
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Blink.Canvas.RasterDuration.Unaccelerated", timer->Elapsed(),
          base::Microseconds(1), base::Milliseconds(100), 100);
    }
  }

  // Rastering the recording would have locked images, since we've flushed
  // all recorded ops, we should release all locked images as well.
  // A new null check on the resource provider is necessary just in case
  // the playback crashed the context.
  if (GetOrCreateResourceProvider())
    ResourceProvider()->ReleaseLockedImages();

  have_recorded_draw_commands_ = false;
}

bool Canvas2DLayerBridge::HasRateLimiterForTesting() {
  return !!rate_limiter_;
}

bool Canvas2DLayerBridge::IsValid() {
  return CheckResourceProviderValid();
}

bool Canvas2DLayerBridge::CheckResourceProviderValid() {
  if (IsHibernating()) {
    return true;
  }
  if (!layer_ || raster_mode_ == RasterMode::kCPU) {
    return true;
  }
  if (context_lost_) {
    return false;
  }
  if (ResourceProvider() && IsAccelerated() &&
      ResourceProvider()->IsGpuContextLost()) {
    context_lost_ = true;
    ClearPendingRasterTimers();
    ResetResourceProvider();
    if (resource_host_) {
      resource_host_->NotifyGpuContextLost();
    }
    return false;
  }
  return !!GetOrCreateResourceProvider();
}

bool Canvas2DLayerBridge::Restore() {
  DCHECK(context_lost_);
  if (!IsAccelerated())
    return false;
  DCHECK(!ResourceProvider());

  if (layer_)
    layer_->ClearTexture();

  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper =
      SharedGpuContext::ContextProviderWrapper();

  if (!context_provider_wrapper->ContextProvider()->IsContextLost()) {
    CanvasResourceProvider* resource_provider =
        resource_host_->GetOrCreateCanvasResourceProviderImpl(
            RasterModeHint::kPreferGPU);

    // The current paradigm does not support switching from accelerated to
    // non-accelerated, which would be tricky due to changes to the layer tree,
    // which can only happen at specific times during the document lifecycle.
    // Therefore, we can only accept the restored surface if it is accelerated.
    if (resource_provider && !IsAccelerated()) {
      resource_host_->ReplaceResourceProvider(nullptr);
      // FIXME: draw sad canvas picture into new buffer crbug.com/243842
    } else {
      context_lost_ = false;
    }
  }

  if (resource_host_)
    resource_host_->UpdateMemoryUsage();

  return ResourceProvider();
}

namespace {

// Adapter for wrapping a CanvasResourceReleaseCallback into a
// viz::ReleaseCallback
void ReleaseCanvasResource(CanvasResource::ReleaseCallback callback,
                           scoped_refptr<CanvasResource> canvas_resource,
                           const gpu::SyncToken& sync_token,
                           bool is_lost) {
  std::move(callback).Run(std::move(canvas_resource), sync_token, is_lost);
}

}  // unnamed namespace

bool Canvas2DLayerBridge::PrepareTransferableResource(
    cc::SharedBitmapIdRegistrar* bitmap_registrar,
    viz::TransferableResource* out_resource,
    viz::ReleaseCallback* out_release_callback) {
  DCHECK(layer_);  // This explodes if FinalizeFrame() was not called.

  frames_since_last_commit_ = 0;
  if (rate_limiter_)
    rate_limiter_->Reset();

  // If hibernating but not hidden, we want to wake up from hibernation.
  if (IsHibernating() && IsHidden())
    return false;

  if (!IsValid())
    return false;

  // The beforeprint event listener is sometimes scheduled in the same task
  // as BeginFrame, which means that this code may sometimes be called between
  // the event listener and its associated FinalizeFrame call. So in order to
  // preserve the display list for printing, FlushRecording needs to know
  // whether any printing occurred in the current task.
  CanvasResourceProvider::FlushReason reason =
      CanvasResourceProvider::FlushReason::kCanvasPushFrame;
  if (resource_host_->PrintedInCurrentTask() || resource_host_->IsPrinting()) {
    reason = CanvasResourceProvider::FlushReason::kCanvasPushFrameWhilePrinting;
  }
  FlushRecording(reason);

  // If the context is lost, we don't know if we should be producing GPU or
  // software frames, until we get a new context, since the compositor will
  // be trying to get a new context and may change modes.
  if (!GetOrCreateResourceProvider())
    return false;

  scoped_refptr<CanvasResource> frame =
      ResourceProvider()->ProduceCanvasResource(reason);
  if (!frame || !frame->IsValid())
    return false;

  CanvasResource::ReleaseCallback release_callback;
  if (!frame->PrepareTransferableResource(out_resource, &release_callback,
                                          kUnverifiedSyncToken) ||
      *out_resource == layer_->current_transferable_resource()) {
    // If the resource did not change, the release will be handled correctly
    // when the callback from the previous frame is dispatched. But run the
    // |release_callback| to release the ref acquired above.
    std::move(release_callback)
        .Run(std::move(frame), gpu::SyncToken(), false /* is_lost */);
    return false;
  }
  // Note: frame is kept alive via a reference kept in out_release_callback.
  *out_release_callback = base::BindOnce(
      ReleaseCanvasResource, std::move(release_callback), std::move(frame));

  return true;
}

cc::Layer* Canvas2DLayerBridge::Layer() {
  // Trigger lazy layer creation
  GetOrCreateResourceProvider();
  return layer_.get();
}

void Canvas2DLayerBridge::DidDraw() {
  have_recorded_draw_commands_ = true;
}

void Canvas2DLayerBridge::FinalizeFrame(
    CanvasResourceProvider::FlushReason reason) {
  TRACE_EVENT0("blink", "Canvas2DLayerBridge::FinalizeFrame");

  // Make sure surface is ready for painting: fix the rendering mode now
  // because it will be too late during the paint invalidation phase.
  if (!GetOrCreateResourceProvider())
    return;

  FlushRecording(reason);
  if (reason == CanvasResourceProvider::FlushReason::kCanvasPushFrame) {
    if (is_being_displayed_) {
      ++frames_since_last_commit_;
      // Make sure the GPU is never more than two animation frames behind.
      constexpr unsigned kMaxCanvasAnimationBacklog = 2;
      if (frames_since_last_commit_ >=
          static_cast<int>(kMaxCanvasAnimationBacklog)) {
        if (IsComposited() && !rate_limiter_) {
          rate_limiter_ = std::make_unique<SharedContextRateLimiter>(
              kMaxCanvasAnimationBacklog);
        }
      }
    }

    if (rate_limiter_) {
      rate_limiter_->Tick();
    }
  }
}

void Canvas2DLayerBridge::DoPaintInvalidation(const gfx::Rect& dirty_rect) {
  if (layer_ && IsComposited()) {
    layer_->SetNeedsDisplayRect(dirty_rect);
  }
}

scoped_refptr<StaticBitmapImage> Canvas2DLayerBridge::NewImageSnapshot(
    CanvasResourceProvider::FlushReason reason) {
  if (snapshot_state_ == kInitialSnapshotState)
    snapshot_state_ = kDidAcquireSnapshot;
  if (IsHibernating()) {
    return UnacceleratedStaticBitmapImage::Create(
        hibernation_handler_.GetImage());
  }
  if (!IsValid())
    return nullptr;
  // GetOrCreateResourceProvider needs to be called before FlushRecording, to
  // make sure "hint" is properly taken into account, as well as after
  // FlushRecording, in case the playback crashed the GPU context.
  if (!GetOrCreateResourceProvider())
    return nullptr;
  FlushRecording(reason);
  if (!GetOrCreateResourceProvider())
    return nullptr;
  return ResourceProvider()->Snapshot(reason);
}

void Canvas2DLayerBridge::WillOverwriteCanvas() {
  SkipQueuedDrawCommands();
}

void Canvas2DLayerBridge::Logger::ReportHibernationEvent(
    HibernationEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Blink.Canvas.HibernationEvents", event);
}

}  // namespace blink
