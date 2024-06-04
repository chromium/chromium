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

#include <utility>

#include "base/feature_list.h"
#include "cc/base/features.h"
#include "cc/layers/texture_layer.h"
#include "cc/layers/texture_layer_impl.h"
#include "components/viz/common/features.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/config/gpu_finch_features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_context_rate_limiter.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

gpu::ContextSupport* GetContextSupport() {
  if (!SharedGpuContext::ContextProviderWrapper() ||
      !SharedGpuContext::ContextProviderWrapper()->ContextProvider()) {
    return nullptr;
  }
  return SharedGpuContext::ContextProviderWrapper()
      ->ContextProvider()
      ->ContextSupport();
}

}  // namespace

// static
bool Canvas2DLayerBridge::IsHibernationEnabled() {
  return base::FeatureList::IsEnabled(features::kCanvas2DHibernation);
}

Canvas2DLayerBridge::Canvas2DLayerBridge()
    : logger_(std::make_unique<Logger>()),
      snapshot_state_(kInitialSnapshotState),
      resource_host_(nullptr) {
  // Used by browser tests to detect the use of a Canvas2DLayerBridge.
  TRACE_EVENT_INSTANT0("test_gpu", "Canvas2DLayerBridgeCreation",
                       TRACE_EVENT_SCOPE_GLOBAL);
}

Canvas2DLayerBridge::~Canvas2DLayerBridge() {
  if (IsHibernating())
    logger_->ReportHibernationEvent(kHibernationEndedWithTeardown);
}

void Canvas2DLayerBridge::SetCanvasResourceHost(CanvasResourceHost* host) {
  resource_host_ = host;
}

void Canvas2DLayerBridge::ResetResourceProvider() {
  if (resource_host_)
    resource_host_->ReplaceResourceProvider(nullptr);
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

void Canvas2DLayerBridge::Hibernate() {
  TRACE_EVENT0("blink", __PRETTY_FUNCTION__);
  CHECK(resource_host_);
  DCHECK(!IsHibernating());
  DCHECK(hibernation_scheduled_);
  CHECK(resource_host_);

  hibernation_scheduled_ = false;

  if (!resource_host_->ResourceProvider()) {
    logger_->ReportHibernationEvent(kHibernationAbortedBecauseNoSurface);
    return;
  }

  if (resource_host_->IsPageVisible()) {
    logger_->ReportHibernationEvent(kHibernationAbortedDueToVisibilityChange);
    return;
  }

  if (!resource_host_->IsResourceValid()) {
    logger_->ReportHibernationEvent(kHibernationAbortedDueGpuContextLoss);
    return;
  }

  if (resource_host_->GetRasterMode() == RasterMode::kCPU) {
    logger_->ReportHibernationEvent(
        kHibernationAbortedDueToSwitchToUnacceleratedRendering);
    return;
  }

  TRACE_EVENT0("blink", "Canvas2DLayerBridge::hibernate");
  // No HibernationEvent reported on success. This is on purppose to avoid
  // non-complementary stats. Each HibernationScheduled event is paired with
  // exactly one failure or exit event.
  FlushRecording(FlushReason::kHibernating);
  // The following checks that the flush succeeded, which should always be the
  // case because flushRecording should only fail it it fails to allocate
  // a surface, and we have an early exit at the top of this function for when
  // 'this' does not already have a surface.
  DCHECK(
      !resource_host_->ResourceProvider()->Recorder().HasReleasableDrawOps());
  SkPaint copy_paint;
  copy_paint.setBlendMode(SkBlendMode::kSrc);
  scoped_refptr<StaticBitmapImage> snapshot =
      resource_host_->ResourceProvider()->Snapshot(FlushReason::kHibernating);
  if (!snapshot) {
    logger_->ReportHibernationEvent(kHibernationAbortedDueSnapshotFailure);
    return;
  }
  sk_sp<SkImage> sw_image =
      snapshot->PaintImageForCurrentFrame().GetSwSkImage();
  if (!sw_image) {
    logger_->ReportHibernationEvent(kHibernationAbortedDueSnapshotFailure);
    return;
  }
  hibernation_handler_.SaveForHibernation(
      std::move(sw_image),
      resource_host_->ResourceProvider()->ReleaseRecorder());

  ResetResourceProvider();
  resource_host_->ClearLayerTexture();

  // shouldBeDirectComposited() may have changed.
  resource_host_->SetNeedsCompositingUpdate();
  logger_->DidStartHibernating();

  // We've just used a large transfer cache buffer to get the snapshot, make
  // sure that it's collected. Calling `SetAggressivelyFreeResources()` also
  // frees things immediately, so use that, since deferring cleanup until the
  // next flush is not a viable option (since we are not visible, when
  // will a flush come?).
  if (base::FeatureList::IsEnabled(
          features::kCanvas2DHibernationReleaseTransferMemory)) {
    if (auto* context_support = GetContextSupport()) {
      // Unnecessary since there would be an early return above otherwise, but
      // let's document that.
      DCHECK(!resource_host_->IsPageVisible());
      context_support->SetAggressivelyFreeResources(true);
    }
  }
}

CanvasResourceProvider* Canvas2DLayerBridge::ResourceProvider() const {
  return resource_host_ ? resource_host_->ResourceProvider() : nullptr;
}

CanvasResourceProvider* Canvas2DLayerBridge::GetOrCreateResourceProvider() {
  CHECK(resource_host_);
  CanvasResourceProvider* resource_provider = ResourceProvider();

  if (resource_host_->context_lost()) {
    DCHECK(!resource_provider);
    return nullptr;
  }

  if (resource_provider && resource_provider->IsValid()) {
    return resource_provider;
  }

  // Restore() is tried at most four times in two seconds to recreate the
  // ResourceProvider before the final attempt, in which a new
  // Canvas2DLayerBridge is created along with its resource provider.

  bool want_acceleration = resource_host_->ShouldTryToUseGpuRaster();
  RasterModeHint adjusted_hint = want_acceleration ? RasterModeHint::kPreferGPU
                                                   : RasterModeHint::kPreferCPU;

  // Re-creation will happen through Restore().
  // If the Canvas2DLayerBridge has just been created, possibly due to failed
  // attempts of Restore(), the layer would not exist, therefore, it will not
  // fall through this clause to try Restore() again
  if (resource_host_->CcLayer() &&
      adjusted_hint == RasterModeHint::kPreferGPU && !IsHibernating()) {
    return nullptr;
  }

  // We call GetOrCreateCanvasResourceProviderImpl directly here to prevent a
  // circular callstack from HTMLCanvasElement.
  resource_provider =
      resource_host_->GetOrCreateCanvasResourceProviderImpl(adjusted_hint);
  if (!resource_provider || !resource_provider->IsValid())
    return nullptr;

  if (!IsHibernating())
    return resource_provider;

  if (resource_provider->IsAccelerated()) {
    logger_->ReportHibernationEvent(kHibernationEndedNormally);
  } else {
    if (!resource_host_->IsPageVisible()) {
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
  resource_provider->SetRecorder(hibernation_handler_.ReleaseRecorder());
  // The hibernation image is no longer valid, clear it.
  hibernation_handler_.Clear();
  DCHECK(!IsHibernating());

  if (resource_host_) {
    // shouldBeDirectComposited() may have changed.
    resource_host_->SetNeedsCompositingUpdate();
  }
  return resource_provider;
}

void Canvas2DLayerBridge::PageVisibilityChanged() {
  bool page_is_visible = resource_host_->IsPageVisible();
  if (ResourceProvider())
    ResourceProvider()->SetResourceRecyclingEnabled(page_is_visible);

  // Conserve memory.
  if (resource_host_->GetRasterMode() == RasterMode::kGPU) {
    if (auto* context_support = GetContextSupport()) {
      context_support->SetAggressivelyFreeResources(!page_is_visible);
    }
  }

  if (IsHibernationEnabled() && ResourceProvider() &&
      resource_host_->GetRasterMode() == RasterMode::kGPU && !page_is_visible &&
      !hibernation_scheduled_) {
    resource_host_->ClearLayerTexture();
    logger_->ReportHibernationEvent(kHibernationScheduled);
    hibernation_scheduled_ = true;
    ThreadScheduler::Current()->PostIdleTask(
        FROM_HERE,
        WTF::BindOnce(&HibernateWrapper, weak_ptr_factory_.GetWeakPtr()));
  }

  // The impl tree may have dropped the transferable resource for this canvas
  // while it wasn't visible. Make sure that it gets pushed there again, now
  // that we've visible.
  //
  // This is done all the time, but it is especially important when canvas
  // hibernation is disabled. In this case, when the impl-side active tree
  // releases the TextureLayer's transferable resource, it will not be freed
  // since the texture has not been cleared above (there is a remaining
  // reference held from the TextureLayer). Then the next time the page becomes
  // visible, the TextureLayer will note the resource hasn't changed (in
  // Update()), and will not add the layer to the list of those that need to
  // push properties. But since the impl-side tree no longer holds the resource,
  // we need TreeSynchronizer to always consider this layer.
  //
  // This makes sure that we do push properties. It is a not needed when canvas
  // hibernation is enabled (since the resource will have changed, it will be
  // pushed), but we do it anyway, since these interactions are subtle.
  bool resource_may_have_been_dropped =
      cc::TextureLayerImpl::MayEvictResourceInBackground(
          viz::TransferableResource::ResourceSource::kCanvas);
  if (page_is_visible && resource_may_have_been_dropped) {
    resource_host_->SetNeedsPushProperties();
  }

  if (page_is_visible && IsHibernating()) {
    GetOrCreateResourceProvider();  // Rude awakening
  }
}

bool Canvas2DLayerBridge::WritePixels(const SkImageInfo& orig_info,
                                      const void* pixels,
                                      size_t row_bytes,
                                      int x,
                                      int y) {
  CHECK(resource_host_);
  CanvasResourceProvider* provider = GetOrCreateResourceProvider();
  if (provider == nullptr) {
    return false;
  }

  if (x <= 0 && y <= 0 &&
      x + orig_info.width() >= resource_host_->Size().width() &&
      y + orig_info.height() >= resource_host_->Size().height()) {
    MemoryManagedPaintRecorder& recorder = provider->Recorder();
    if (recorder.HasSideRecording()) {
      // Even with opened layers, WritePixels would write to the main canvas
      // surface under the layers. We can therefore clear the paint ops recorded
      // before the first `beginLayer`, but the layers themselves must be kept
      // untouched. Note that this operation makes little sense and is actually
      // disabled in `putImageData` by raising an exception if layers are
      // opened. Still, it's preferable to handle this scenario here because the
      // alternative would be to crash or leave the canvas in an invalid state.
      recorder.ReleaseMainRecording();
    } else {
      recorder.RestartRecording();
    }
  } else {
    FlushRecording(FlushReason::kWritePixels);
    if (!GetOrCreateResourceProvider())
      return false;
  }

  return ResourceProvider()->WritePixels(orig_info, pixels, row_bytes, x, y);
}

void Canvas2DLayerBridge::FlushRecording(FlushReason reason) {
  CHECK(resource_host_);
  CanvasResourceProvider* provider = GetOrCreateResourceProvider();
  if (!provider || !provider->Recorder().HasReleasableDrawOps()) {
    return;
  }

  TRACE_EVENT0("cc", "Canvas2DLayerBridge::flushRecording");

  ResourceProvider()->FlushCanvas(reason);

  // Rastering the recording would have locked images, since we've flushed
  // all recorded ops, we should release all locked images as well.
  // A new null check on the resource provider is necessary just in case
  // the playback crashed the context.
  if (GetOrCreateResourceProvider())
    ResourceProvider()->ReleaseLockedImages();
}

bool Canvas2DLayerBridge::Restore() {
  CHECK(resource_host_);
  CHECK(resource_host_->context_lost());
  if (resource_host_ && resource_host_->GetRasterMode() == RasterMode::kCPU) {
    return false;
  }
  DCHECK(!ResourceProvider());

  resource_host_->ClearLayerTexture();

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
    if (resource_provider &&
        resource_host_->GetRasterMode() == RasterMode::kCPU) {
      resource_host_->ReplaceResourceProvider(nullptr);
      // FIXME: draw sad canvas picture into new buffer crbug.com/243842
    } else {
      resource_host_->set_context_lost(false);
    }
  }

  if (resource_host_)
    resource_host_->UpdateMemoryUsage();

  return ResourceProvider();
}

void Canvas2DLayerBridge::FinalizeFrame(FlushReason reason) {
  TRACE_EVENT0("blink", "Canvas2DLayerBridge::FinalizeFrame");
  CHECK(resource_host_);

  // Make sure surface is ready for painting: fix the rendering mode now
  // because it will be too late during the paint invalidation phase.
  if (!GetOrCreateResourceProvider())
    return;

  FlushRecording(reason);
  if (reason == FlushReason::kCanvasPushFrame) {
    if (resource_host_->IsDisplayed()) {
      // Make sure the GPU is never more than two animation frames behind.
      constexpr unsigned kMaxCanvasAnimationBacklog = 2;
      if (resource_host_->IncrementFramesSinceLastCommit() >=
          static_cast<int>(kMaxCanvasAnimationBacklog)) {
        if (resource_host_->IsComposited() && !resource_host_->RateLimiter()) {
          resource_host_->CreateRateLimiter();
        }
      }
    }

    if (resource_host_->RateLimiter()) {
      resource_host_->RateLimiter()->Tick();
    }
  }
}

scoped_refptr<StaticBitmapImage> Canvas2DLayerBridge::NewImageSnapshot(
    FlushReason reason) {
  CHECK(resource_host_);
  if (snapshot_state_ == kInitialSnapshotState)
    snapshot_state_ = kDidAcquireSnapshot;
  if (IsHibernating()) {
    return UnacceleratedStaticBitmapImage::Create(
        hibernation_handler_.GetImage());
  }
  if (!resource_host_->IsResourceValid()) {
    return nullptr;
  }
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

void Canvas2DLayerBridge::Logger::ReportHibernationEvent(
    HibernationEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Blink.Canvas.HibernationEvents", event);
}

}  // namespace blink
