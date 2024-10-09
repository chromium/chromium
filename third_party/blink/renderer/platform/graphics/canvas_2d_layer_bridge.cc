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
#include "cc/layers/texture_layer.h"
#include "cc/layers/texture_layer_impl.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/command_buffer/client/context_support.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_host.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

void ReportHibernationEvent(Canvas2DLayerBridge::HibernationEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Blink.Canvas.HibernationEvents", event);
}

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

Canvas2DLayerBridge::Canvas2DLayerBridge(CanvasResourceHost* resource_host)
    : resource_host_(resource_host) {
  CHECK(resource_host_);

  // Used by browser tests to detect the use of a Canvas2DLayerBridge.
  TRACE_EVENT_INSTANT0("test_gpu", "Canvas2DLayerBridgeCreation",
                       TRACE_EVENT_SCOPE_GLOBAL);
}

Canvas2DLayerBridge::~Canvas2DLayerBridge() {
  if (hibernation_handler_.IsHibernating()) {
    ReportHibernationEvent(kHibernationEndedWithTeardown);
  }
}

// static
void Canvas2DLayerBridge::HibernateOrLogFailure(
    base::WeakPtr<Canvas2DLayerBridge> bridge,
    base::TimeTicks /*idleDeadline*/) {
  if (bridge) {
    bridge->Hibernate();
  } else {
    ReportHibernationEvent(
        Canvas2DLayerBridge::
            kHibernationAbortedDueToDestructionWhileHibernatePending);
  }
}

void Canvas2DLayerBridge::Hibernate() {
  TRACE_EVENT0("blink", __PRETTY_FUNCTION__);
  DCHECK(!hibernation_handler_.IsHibernating());
  DCHECK(hibernation_scheduled_);

  hibernation_scheduled_ = false;

  if (!resource_host_->ResourceProvider()) {
    ReportHibernationEvent(kHibernationAbortedBecauseNoSurface);
    return;
  }

  if (resource_host_->IsPageVisible()) {
    ReportHibernationEvent(kHibernationAbortedDueToVisibilityChange);
    return;
  }

  if (!resource_host_->IsResourceValid()) {
    ReportHibernationEvent(kHibernationAbortedDueGpuContextLoss);
    return;
  }

  if (resource_host_->GetRasterMode() == RasterMode::kCPU) {
    ReportHibernationEvent(
        kHibernationAbortedDueToSwitchToUnacceleratedRendering);
    return;
  }

  TRACE_EVENT0("blink", "Canvas2DLayerBridge::hibernate");
  // No HibernationEvent reported on success. This is on purppose to avoid
  // non-complementary stats. Each HibernationScheduled event is paired with
  // exactly one failure or exit event.
  resource_host_->FlushRecording(FlushReason::kHibernating);
  scoped_refptr<StaticBitmapImage> snapshot =
      resource_host_->ResourceProvider()->Snapshot(FlushReason::kHibernating);
  if (!snapshot) {
    ReportHibernationEvent(kHibernationAbortedDueSnapshotFailure);
    return;
  }
  sk_sp<SkImage> sw_image =
      snapshot->PaintImageForCurrentFrame().GetSwSkImage();
  if (!sw_image) {
    ReportHibernationEvent(kHibernationAbortedDueSnapshotFailure);
    return;
  }
  hibernation_handler_.SaveForHibernation(
      std::move(sw_image),
      resource_host_->ResourceProvider()->ReleaseRecorder());

  resource_host_->ReplaceResourceProvider(nullptr);
  resource_host_->ClearLayerTexture();

  // shouldBeDirectComposited() may have changed.
  resource_host_->SetNeedsCompositingUpdate();

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

CanvasResourceProvider* Canvas2DLayerBridge::GetOrCreateResourceProvider() {
  CanvasResourceProvider* resource_provider =
      resource_host_->ResourceProvider();

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
      adjusted_hint == RasterModeHint::kPreferGPU &&
      !hibernation_handler_.IsHibernating()) {
    return nullptr;
  }

  // We call GetOrCreateCanvasResourceProviderImpl directly here to prevent a
  // circular callstack from HTMLCanvasElement.
  resource_provider =
      resource_host_->GetOrCreateCanvasResourceProviderImpl(adjusted_hint);
  if (!resource_provider || !resource_provider->IsValid())
    return nullptr;

  if (!hibernation_handler_.IsHibernating()) {
    return resource_provider;
  }

  if (resource_provider->IsAccelerated()) {
    ReportHibernationEvent(kHibernationEndedNormally);
  } else {
    if (!resource_host_->IsPageVisible()) {
      ReportHibernationEvent(kHibernationEndedWithSwitchToBackgroundRendering);
    } else {
      ReportHibernationEvent(kHibernationEndedWithFallbackToSW);
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
  DCHECK(!hibernation_handler_.IsHibernating());

  // shouldBeDirectComposited() may have changed.
  resource_host_->SetNeedsCompositingUpdate();

  return resource_provider;
}

void Canvas2DLayerBridge::PageVisibilityChanged() {
  bool page_is_visible = resource_host_->IsPageVisible();
  if (resource_host_->ResourceProvider()) {
    resource_host_->ResourceProvider()->SetResourceRecyclingEnabled(
        page_is_visible);
  }

  // Conserve memory.
  if (resource_host_->GetRasterMode() == RasterMode::kGPU) {
    if (auto* context_support = GetContextSupport()) {
      context_support->SetAggressivelyFreeResources(!page_is_visible);
    }
  }

  if (features::IsCanvas2DHibernationEnabled() &&
      resource_host_->ResourceProvider() &&
      resource_host_->GetRasterMode() == RasterMode::kGPU && !page_is_visible &&
      !hibernation_scheduled_) {
    resource_host_->ClearLayerTexture();
    ReportHibernationEvent(kHibernationScheduled);
    hibernation_scheduled_ = true;
    ThreadScheduler::Current()->PostIdleTask(
        FROM_HERE, WTF::BindOnce(&Canvas2DLayerBridge::HibernateOrLogFailure,
                                 weak_ptr_factory_.GetWeakPtr()));
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

  if (page_is_visible && hibernation_handler_.IsHibernating()) {
    GetOrCreateResourceProvider();  // Rude awakening
  }
}

}  // namespace blink
