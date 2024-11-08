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

// TODO(crbug.com/40280152): Remove this method when no longer used.
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
    ReportHibernationEvent(CanvasHibernationHandler::HibernationEvent::
                               kHibernationEndedWithTeardown);
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
        CanvasHibernationHandler::HibernationEvent::
            kHibernationAbortedDueToDestructionWhileHibernatePending);
  }
}

void Canvas2DLayerBridge::Hibernate() {
  TRACE_EVENT0("blink", __PRETTY_FUNCTION__);
  DCHECK(!hibernation_handler_.IsHibernating());
  DCHECK(hibernation_scheduled_);

  hibernation_scheduled_ = false;

  if (!resource_host_->ResourceProvider()) {
    ReportHibernationEvent(CanvasHibernationHandler::HibernationEvent::
                               kHibernationAbortedBecauseNoSurface);
    return;
  }

  if (resource_host_->IsPageVisible()) {
    ReportHibernationEvent(CanvasHibernationHandler::HibernationEvent::
                               kHibernationAbortedDueToVisibilityChange);
    return;
  }

  if (!resource_host_->IsResourceValid()) {
    ReportHibernationEvent(CanvasHibernationHandler::HibernationEvent::
                               kHibernationAbortedDueGpuContextLoss);
    return;
  }

  if (resource_host_->GetRasterMode() == RasterMode::kCPU) {
    ReportHibernationEvent(
        CanvasHibernationHandler::HibernationEvent::
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
    ReportHibernationEvent(CanvasHibernationHandler::HibernationEvent::
                               kHibernationAbortedDueSnapshotFailure);
    return;
  }
  sk_sp<SkImage> sw_image =
      snapshot->PaintImageForCurrentFrame().GetSwSkImage();
  if (!sw_image) {
    ReportHibernationEvent(CanvasHibernationHandler::HibernationEvent::
                               kHibernationAbortedDueSnapshotFailure);
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

void Canvas2DLayerBridge::InitiateHibernationIfNecessary() {
  if (hibernation_scheduled_) {
    return;
  }

  resource_host_->ClearLayerTexture();
  ReportHibernationEvent(
      CanvasHibernationHandler::HibernationEvent::kHibernationScheduled);
  hibernation_scheduled_ = true;
  ThreadScheduler::Current()->PostIdleTask(
      FROM_HERE, WTF::BindOnce(&Canvas2DLayerBridge::HibernateOrLogFailure,
                               weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace blink
