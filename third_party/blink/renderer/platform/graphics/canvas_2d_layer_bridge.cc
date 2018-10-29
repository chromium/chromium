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

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "cc/layers/texture_layer.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/canvas_heuristic_parameters.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_context_rate_limiter.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace {
enum {
  InvalidMailboxIndex = -1,
  MaxCanvasAnimationBacklog = 2,  // Make sure the the GPU is never more than
                                  // two animation frames behind.
};
}  // namespace

namespace blink {

Canvas2DLayerBridge::Canvas2DLayerBridge(const IntSize& size,
                                         AccelerationMode acceleration_mode,
                                         const CanvasColorParams& color_params)
    : logger_(std::make_unique<Logger>()),
      bytes_allocated_(0),
      have_recorded_draw_commands_(false),
      is_hidden_(false),
      is_deferral_enabled_(true),
      software_rendering_while_hidden_(false),
      acceleration_mode_(acceleration_mode),
      color_params_(color_params),
      size_(size),
      snapshot_state_(kInitialSnapshotState),
      resource_host_(nullptr),
      weak_ptr_factory_(this) {
  // Used by browser tests to detect the use of a Canvas2DLayerBridge.
  TRACE_EVENT_INSTANT0("test_gpu", "Canvas2DLayerBridgeCreation",
                       TRACE_EVENT_SCOPE_GLOBAL);
  StartRecording();
  // Clear the background transparent or opaque. Similar code at
  // CanvasResourceProvider::Clear().
  if (IsValid()) {
    DCHECK(recorder_);
    recorder_->getRecordingCanvas()->clear(
        color_params_.GetOpacityMode() == kOpaque ? SK_ColorBLACK
                                                  : SK_ColorTRANSPARENT);
    DidDraw(FloatRect(FloatPoint(0, 0), FloatSize(size_)));
  }
}

Canvas2DLayerBridge::~Canvas2DLayerBridge() {
  UMA_HISTOGRAM_BOOLEAN("Blink.Canvas.2DLayerBridgeIsDeferred",
                        is_deferral_enabled_);

  if (IsHibernating())
    logger_->ReportHibernationEvent(kHibernationEndedWithTeardown);
  ResetResourceProvider();

  if (layer_ && acceleration_mode_ != kDisableAcceleration) {
    GraphicsLayer::UnregisterContentsLayer(layer_.get());
    layer_->ClearTexture();
    // Orphaning the layer is required to trigger the recration of a new layer
    // in the case where destruction is caused by a canvas resize. Test:
    // virtual/gpu/fast/canvas/canvas-resize-after-paint-without-layout.html
    layer_->RemoveFromParent();
  }

  DCHECK(!bytes_allocated_);

  if (layer_) {
    layer_->ClearClient();
    layer_ = nullptr;
  }
}

void Canvas2DLayerBridge::StartRecording() {
  DCHECK(is_deferral_enabled_);
  recorder_ = std::make_unique<PaintRecorder>();
  cc::PaintCanvas* canvas =
      recorder_->beginRecording(size_.Width(), size_.Height());
  // Always save an initial frame, to support resetting the top level matrix
  // and clip.
  canvas->save();

  if (resource_host_) {
    resource_host_->RestoreCanvasMatrixClipStack(canvas);
  }

  recording_pixel_count_ = 0;
}

void Canvas2DLayerBridge::SetLoggerForTesting(std::unique_ptr<Logger> logger) {
  logger_ = std::move(logger);
}

void Canvas2DLayerBridge::ResetResourceProvider() {
  if (resource_host_) {
    resource_host_->ReplaceResourceProvider(nullptr);
  }
}

bool Canvas2DLayerBridge::ShouldAccelerate(AccelerationHint hint) const {
  bool accelerate;
  if (software_rendering_while_hidden_) {
    accelerate = false;
  } else if (acceleration_mode_ == kForceAccelerationForTesting) {
    accelerate = true;
  } else if (acceleration_mode_ == kDisableAcceleration) {
    accelerate = false;
  } else {
    accelerate = hint == kPreferAcceleration ||
                 hint == kPreferAccelerationAfterVisibilityChange;
  }

  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper =
      SharedGpuContext::ContextProviderWrapper();
  if (accelerate && (!context_provider_wrapper ||
                     context_provider_wrapper->ContextProvider()
                             ->ContextGL()
                             ->GetGraphicsResetStatusKHR() != GL_NO_ERROR))
    accelerate = false;
  return accelerate;
}

bool Canvas2DLayerBridge::IsAccelerated() const {
  if (acceleration_mode_ == kDisableAcceleration)
    return false;
  if (IsHibernating())
    return false;
  if (software_rendering_while_hidden_)
    return false;
  if (resource_host_ && resource_host_->ResourceProvider())
    return resource_host_->ResourceProvider()->IsAccelerated();

  // Whether or not to accelerate is not yet resolved. Determine whether
  // immediate presentation of the canvas would result in the canvas being
  // accelerated. Presentation is assumed to be a 'PreferAcceleration'
  // operation.
  return ShouldAccelerate(kPreferAcceleration);
}

static void HibernateWrapper(base::WeakPtr<Canvas2DLayerBridge> bridge,
                             TimeTicks /*idleDeadline*/) {
  if (bridge) {
    bridge->Hibernate();
  } else {
    Canvas2DLayerBridge::Logger local_logger;
    local_logger.ReportHibernationEvent(
        Canvas2DLayerBridge::
            kHibernationAbortedDueToDestructionWhileHibernatePending);
  }
}

static void HibernateWrapperForTesting(
    base::WeakPtr<Canvas2DLayerBridge> bridge) {
  HibernateWrapper(std::move(bridge), TimeTicks());
}

void Canvas2DLayerBridge::Hibernate() {
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
  sk_sp<SkSurface> temp_hibernation_surface =
      SkSurface::MakeRasterN32Premul(size_.Width(), size_.Height());
  if (!temp_hibernation_surface) {
    logger_->ReportHibernationEvent(kHibernationAbortedDueToAllocationFailure);
    return;
  }
  // No HibernationEvent reported on success. This is on purppose to avoid
  // non-complementary stats. Each HibernationScheduled event is paired with
  // exactly one failure or exit event.
  FlushRecording();
  // The following checks that the flush succeeded, which should always be the
  // case because flushRecording should only fail it it fails to allocate
  // a surface, and we have an early exit at the top of this function for when
  // 'this' does not already have a surface.
  DCHECK(!have_recorded_draw_commands_);
  SkPaint copy_paint;
  copy_paint.setBlendMode(SkBlendMode::kSrc);
  scoped_refptr<StaticBitmapImage> snapshot =
      resource_host_->ResourceProvider()->Snapshot();
  temp_hibernation_surface->getCanvas()->drawImage(
      snapshot->PaintImageForCurrentFrame().GetSkImage(), 0, 0, &copy_paint);
  hibernation_image_ = temp_hibernation_surface->makeImageSnapshot();
  ResetResourceProvider();
  if (layer_)
    layer_->ClearTexture();

  // shouldBeDirectComposited() may have changed.
  if (resource_host_)
    resource_host_->SetNeedsCompositingUpdate();
  logger_->DidStartHibernating();
}

CanvasResourceProvider* Canvas2DLayerBridge::ResourceProvider() const {
  return resource_host_ ? resource_host_->ResourceProvider() : nullptr;
}

CanvasResourceProvider* Canvas2DLayerBridge::GetOrCreateResourceProvider(
    AccelerationHint hint) {
  DCHECK(resource_host_);
  CanvasResourceProvider* resource_provider = ResourceProvider();

  if (context_lost_) {
    DCHECK(!resource_provider);
    return nullptr;
  }

  if (resource_provider && resource_provider->IsValid()) {
#if DCHECK_IS_ON()
    // If resource provider is accelerated, a layer should already exist.
    // unless this is a canvas in low latency mode.
    // If this DCHECK fails, it probably means that
    // CanvasRenderingContextHost::GetOrCreateCanvasResourceProvider() was
    // called on a 2D context before this function.
    if (IsAccelerated()) {
      DCHECK(!!layer_ ||
             (resource_host_ && resource_host_->LowLatencyEnabled()));
    }
#endif
    return resource_provider;
  }

  if (layer_ && !IsHibernating() && hint == kPreferAcceleration &&
      acceleration_mode_ != kDisableAcceleration) {
    return nullptr;  // re-creation will happen through restore()
  }

  bool want_acceleration = ShouldAccelerate(hint);
  if (CANVAS2D_BACKGROUND_RENDER_SWITCH_TO_CPU && IsHidden() &&
      want_acceleration) {
    want_acceleration = false;
    software_rendering_while_hidden_ = true;
  }
  AccelerationHint adjusted_hint =
      want_acceleration ? kPreferAcceleration : kPreferNoAcceleration;

  // We call Impl directly here, to allow HTMLCanvasElement to call us
  // in GetOrCreateCanvasResourceProvider.
  resource_provider =
      resource_host_->GetOrCreateCanvasResourceProviderImpl(adjusted_hint);

  if (resource_provider && IsAccelerated() && !layer_) {
    layer_ = cc::TextureLayer::CreateForMailbox(this);
    layer_->SetIsDrawable(true);
    layer_->SetContentsOpaque(ColorParams().GetOpacityMode() == kOpaque);
    layer_->SetBlendBackgroundColor(ColorParams().GetOpacityMode() != kOpaque);
    layer_->SetNearestNeighbor(resource_host_->FilterQuality() ==
                               kNone_SkFilterQuality);
    GraphicsLayer::RegisterContentsLayer(layer_.get());
  }

  if (resource_provider && IsHibernating()) {
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

    PaintFlags copy_paint;
    copy_paint.setBlendMode(SkBlendMode::kSrc);
    PaintImageBuilder builder = PaintImageBuilder::WithDefault();
    builder.set_image(hibernation_image_, PaintImage::GetNextContentId());
    builder.set_id(PaintImage::GetNextId());
    resource_provider->Canvas()->drawImage(builder.TakePaintImage(), 0, 0,
                                           &copy_paint);
    hibernation_image_.reset();

    if (resource_host_) {
      if (!is_deferral_enabled_) {
        resource_host_->RestoreCanvasMatrixClipStack(
            resource_provider->Canvas());
      }

      // shouldBeDirectComposited() may have changed.
      resource_host_->SetNeedsCompositingUpdate();
    }
  }

  return resource_provider;
}

cc::PaintCanvas* Canvas2DLayerBridge::Canvas() {
  DCHECK(resource_host_);
  if (!is_deferral_enabled_) {
    if (GetOrCreateResourceProvider())
      return ResourceProvider()->Canvas();
    return nullptr;
  }
  return recorder_->getRecordingCanvas();
}

void Canvas2DLayerBridge::DisableDeferral(DisableDeferralReason reason) {
  // Disabling deferral is permanent: once triggered by disableDeferral()
  // we stay in immediate mode indefinitely. This is a performance heuristic
  // that significantly helps a number of use cases. The rationale is that if
  // immediate rendering was needed once, it is likely to be needed at least
  // once per frame, which eliminates the possibility for inter-frame
  // overdraw optimization. Furthermore, in cases where immediate mode is
  // required multiple times per frame, the repeated flushing of deferred
  // commands would cause significant overhead, so it is better to just stop
  // trying to defer altogether.
  if (!is_deferral_enabled_ || !resource_host_)
    return;

  DEFINE_STATIC_LOCAL(EnumerationHistogram, gpu_disabled_histogram,
                      ("Canvas.GPUAccelerated2DCanvasDisableDeferralReason",
                       kDisableDeferralReasonCount));
  gpu_disabled_histogram.Count(reason);
  FlushRecording();
  // Because we will be discarding the recorder, if the flush failed
  // content will be lost -> force m_haveRecordedDrawCommands to false
  have_recorded_draw_commands_ = false;

  is_deferral_enabled_ = false;
  recorder_.reset();
  // install the current matrix/clip stack onto the immediate canvas
  if (GetOrCreateResourceProvider())
    resource_host_->RestoreCanvasMatrixClipStack(ResourceProvider()->Canvas());
}

void Canvas2DLayerBridge::UpdateFilterQuality() {
  SkFilterQuality filter_quality = resource_host_->FilterQuality();
  if (GetOrCreateResourceProvider())
    ResourceProvider()->SetFilterQuality(filter_quality);
  if (layer_)
    layer_->SetNearestNeighbor(filter_quality == kNone_SkFilterQuality);
}

void Canvas2DLayerBridge::SetIsHidden(bool hidden) {
  if (is_hidden_ == hidden)
    return;

  is_hidden_ = hidden;
  if (ResourceProvider())
    ResourceProvider()->SetResourceRecyclingEnabled(!IsHidden());

  if (CANVAS2D_HIBERNATION_ENABLED && ResourceProvider() && IsAccelerated() &&
      IsHidden() && !hibernation_scheduled_) {
    if (layer_)
      layer_->ClearTexture();
    logger_->ReportHibernationEvent(kHibernationScheduled);
    hibernation_scheduled_ = true;
    if (dont_use_idle_scheduling_for_testing_) {
      Platform::Current()->CurrentThread()->GetTaskRunner()->PostTask(
          FROM_HERE, WTF::Bind(&HibernateWrapperForTesting,
                               weak_ptr_factory_.GetWeakPtr()));
    } else {
      Platform::Current()->CurrentThread()->Scheduler()->PostIdleTask(
          FROM_HERE,
          WTF::Bind(&HibernateWrapper, weak_ptr_factory_.GetWeakPtr()));
    }
  }
  if (!IsHidden() && software_rendering_while_hidden_) {
    FlushRecording();
    PaintFlags copy_paint;
    copy_paint.setBlendMode(SkBlendMode::kSrc);

    std::unique_ptr<CanvasResourceProvider> old_resource_provider =
        resource_host_->ReplaceResourceProvider(nullptr);

    software_rendering_while_hidden_ = false;
    GetOrCreateResourceProvider(kPreferAccelerationAfterVisibilityChange);

    if (ResourceProvider()) {
      if (old_resource_provider) {
        cc::PaintImage snapshot =
            old_resource_provider->Snapshot()->PaintImageForCurrentFrame();
        ResourceProvider()->Canvas()->drawImage(snapshot, 0, 0, &copy_paint);
      }
      if (resource_host_ && !is_deferral_enabled_) {
        resource_host_->RestoreCanvasMatrixClipStack(
            ResourceProvider()->Canvas());
      }
    } else {
      // New resource provider could not be created. Stay with old one.
      resource_host_->ReplaceResourceProvider(std::move(old_resource_provider));
    }
  }
  if (!IsHidden() && IsHibernating()) {
    GetOrCreateResourceProvider();  // Rude awakening
  }
}

void Canvas2DLayerBridge::DrawFullImage(const cc::PaintImage& image) {
  Canvas()->drawImage(image, 0, 0);
}

bool Canvas2DLayerBridge::WritePixels(const SkImageInfo& orig_info,
                                      const void* pixels,
                                      size_t row_bytes,
                                      int x,
                                      int y) {
  if (!GetOrCreateResourceProvider())
    return false;
  if (x <= 0 && y <= 0 && x + orig_info.width() >= size_.Width() &&
      y + orig_info.height() >= size_.Height()) {
    SkipQueuedDrawCommands();
  } else {
    FlushRecording();
    if (!GetOrCreateResourceProvider())
      return false;
  }

  ResourceProvider()->WritePixels(orig_info, pixels, row_bytes, x, y);
  DidDraw(FloatRect(x, y, orig_info.width(), orig_info.height()));

  return true;
}

void Canvas2DLayerBridge::SkipQueuedDrawCommands() {
  if (have_recorded_draw_commands_) {
    recorder_->finishRecordingAsPicture();
    StartRecording();
    have_recorded_draw_commands_ = false;
  }

  if (is_deferral_enabled_) {
    if (rate_limiter_)
      rate_limiter_->Reset();
  }
}

void Canvas2DLayerBridge::FlushRecording() {

  if (have_recorded_draw_commands_ && GetOrCreateResourceProvider()) {
    TRACE_EVENT0("cc", "Canvas2DLayerBridge::flushRecording");

    cc::PaintCanvas* canvas = ResourceProvider()->Canvas();
    {
      sk_sp<PaintRecord> recording = recorder_->finishRecordingAsPicture();
      canvas->drawPicture(recording);
    }

    // Rastering the recording would have locked images, since we've flushed
    // all recorded ops, we should relase all locked images as well.
    // A new null check on the resource provider is necessary just in case
    // the playback crashed the context.
    if (GetOrCreateResourceProvider())
      ResourceProvider()->ReleaseLockedImages();

    if (is_deferral_enabled_)
      StartRecording();
    have_recorded_draw_commands_ = false;
  }
}

bool Canvas2DLayerBridge::IsValid() const {
  return const_cast<Canvas2DLayerBridge*>(this)->CheckResourceProviderValid();
}

bool Canvas2DLayerBridge::CheckResourceProviderValid() {
  if (IsHibernating())
    return true;
  if (!layer_ || acceleration_mode_ == kDisableAcceleration)
    return true;
  if (context_lost_)
    return false;
  if (ResourceProvider() && IsAccelerated() &&
      ResourceProvider()->IsGpuContextLost()) {
    context_lost_ = true;
    ResetResourceProvider();
    if (resource_host_)
      resource_host_->NotifyGpuContextLost();
    return false;
  }
  return !!GetOrCreateResourceProvider();
}

bool Canvas2DLayerBridge::Restore() {
  DCHECK(context_lost_);
  if (!IsAccelerated())
    return false;
  DCHECK(!ResourceProvider());

  gpu::gles2::GLES2Interface* shared_gl = nullptr;
  layer_->ClearTexture();
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper =
      SharedGpuContext::ContextProviderWrapper();
  if (context_provider_wrapper)
    shared_gl = context_provider_wrapper->ContextProvider()->ContextGL();

  if (shared_gl && shared_gl->GetGraphicsResetStatusKHR() == GL_NO_ERROR) {
    CanvasResourceProvider* resource_provider =
        resource_host_->GetOrCreateCanvasResourceProviderImpl(
            kPreferAcceleration);

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

bool Canvas2DLayerBridge::PrepareTransferableResource(
    cc::SharedBitmapIdRegistrar* bitmap_registrar,
    viz::TransferableResource* out_resource,
    std::unique_ptr<viz::SingleReleaseCallback>* out_release_callback) {

  DCHECK(layer_);  // This explodes if FinalizeFrame() was not called.

  frames_since_last_commit_ = 0;
  if (rate_limiter_) {
    rate_limiter_->Reset();
  }

  // if hibernating but not hidden, we want to wake up from
  // hibernation
  if ((IsHibernating() || software_rendering_while_hidden_) && IsHidden())
    return false;

  if (!IsValid())
    return false;

  FlushRecording();

  // If the context is lost, we don't know if we should be producing GPU or
  // software frames, until we get a new context, since the compositor will
  // be trying to get a new context and may change modes.
  if (!GetOrCreateResourceProvider())
    return false;

  scoped_refptr<CanvasResource> frame = ResourceProvider()->ProduceFrame();
  if (frame && frame->IsValid()) {
    // Note frame is kept alive via a reference kept in out_release_callback.
    bool success = frame->PrepareTransferableResource(
        out_resource, out_release_callback, kUnverifiedSyncToken);
    return success;
  }
  return false;
}

cc::Layer* Canvas2DLayerBridge::Layer() {
  // Trigger lazy layer creation
  GetOrCreateResourceProvider(kPreferAcceleration);
  return layer_.get();
}

void Canvas2DLayerBridge::DidDraw(const FloatRect& rect) {
  if (snapshot_state_ == kDidAcquireSnapshot)
    snapshot_state_ = kDrawnToAfterSnapshot;
  if (is_deferral_enabled_) {
    have_recorded_draw_commands_ = true;
    IntRect pixel_bounds = EnclosingIntRect(rect);
    base::CheckedNumeric<int> pixel_bounds_size = pixel_bounds.Width();
    pixel_bounds_size *= pixel_bounds.Height();
    recording_pixel_count_ += pixel_bounds_size;
    if (!recording_pixel_count_.IsValid()) {
      DisableDeferral(kDisableDeferralReasonExpensiveOverdrawHeuristic);
      return;
    }
    base::CheckedNumeric<int> threshold_size = size_.Width();
    threshold_size *= size_.Height();
    threshold_size *= canvas_heuristic_parameters::kExpensiveOverdrawThreshold;
    if (!threshold_size.IsValid()) {
      DisableDeferral(kDisableDeferralReasonExpensiveOverdrawHeuristic);
      return;
    }
    if (recording_pixel_count_.ValueOrDie() >= threshold_size.ValueOrDie()) {
      DisableDeferral(kDisableDeferralReasonExpensiveOverdrawHeuristic);
    }
  }
}

void Canvas2DLayerBridge::FinalizeFrame() {
  TRACE_EVENT0("blink", "Canvas2DLayerBridge::FinalizeFrame");

  // Make sure surface is ready for painting: fix the rendering mode now
  // because it will be too late during the paint invalidation phase.
  if (!GetOrCreateResourceProvider(kPreferAcceleration))
    return;

  ++frames_since_last_commit_;

  if (frames_since_last_commit_ >= 2) {
    ResourceProvider()->FlushSkia();
    if (IsAccelerated()) {
      if (!rate_limiter_) {
        rate_limiter_ =
            SharedContextRateLimiter::Create(MaxCanvasAnimationBacklog);
      }
    }
  }

  if (rate_limiter_) {
    rate_limiter_->Tick();
  }
}

void Canvas2DLayerBridge::DoPaintInvalidation(const FloatRect& dirty_rect) {
  if (layer_ && acceleration_mode_ != kDisableAcceleration)
    layer_->SetNeedsDisplayRect(EnclosingIntRect(dirty_rect));
}

scoped_refptr<StaticBitmapImage> Canvas2DLayerBridge::NewImageSnapshot(
    AccelerationHint hint) {
  if (snapshot_state_ == kInitialSnapshotState)
    snapshot_state_ = kDidAcquireSnapshot;
  if (IsHibernating())
    return StaticBitmapImage::Create(hibernation_image_);
  if (!IsValid())
    return nullptr;
  // GetOrCreateResourceProvider needs to be called before FlushRecording, to
  // make sure "hint" is properly taken into account, as well as after
  // FlushRecording, in case the playback crashed the GPU context.
  if (!GetOrCreateResourceProvider(hint))
    return nullptr;
  FlushRecording();
  if (!GetOrCreateResourceProvider(hint))
    return nullptr;
  return ResourceProvider()->Snapshot();
}

void Canvas2DLayerBridge::WillOverwriteCanvas() {
  SkipQueuedDrawCommands();
}

void Canvas2DLayerBridge::Logger::ReportHibernationEvent(
    HibernationEvent event) {
  DEFINE_STATIC_LOCAL(EnumerationHistogram, hibernation_histogram,
                      ("Canvas.HibernationEvents", kHibernationEventCount));
  hibernation_histogram.Count(event);
}

}  // namespace blink
