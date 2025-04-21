// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/canvas_interventions/canvas_interventions_helper.h"

#include <cstdint>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/fingerprinting_protection/canvas_noise_token.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"
#include "third_party/blink/renderer/core/canvas_interventions/noise_hash.h"
#include "third_party/blink/renderer/core/canvas_interventions/noise_helper.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_host.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/mojo/mojo_binding_context.h"
#include "third_party/blink/renderer/platform/runtime_feature_state/runtime_feature_state_override_context.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/skia_span_util.h"

namespace blink {

namespace {

// Returns true when all criteria to apply noising are met. Currently this
// entails that
//   1) an operation was made on the canvas that triggers an
//   2) the render context is 2d
//   3) the raster mode is GPU unless an exception is made
//   4) the CanvasInterventions RuntimeEnabledFeature is force enabled for
//      testing.
bool ShouldApplyNoise(CanvasRenderingContext* rendering_context,
                      RasterMode raster_mode,
                      ExecutionContext* execution_context) {
  CanvasNoiseReason noise_reason = CanvasNoiseReason::kAllConditionsMet;
  if (!rendering_context) {
    noise_reason |= CanvasNoiseReason::kNoRenderContext;
  }
  if (rendering_context && !rendering_context->ShouldTriggerIntervention()) {
    noise_reason |= CanvasNoiseReason::kNoTrigger;
  }
  if (rendering_context && !rendering_context->IsRenderingContext2D()) {
    noise_reason |= CanvasNoiseReason::kNo2d;
  }
  if (!(raster_mode == RasterMode::kGPU ||
        RuntimeEnabledFeatures::CanvasInterventionsOnCpuForTestingEnabled())) {
    noise_reason |= CanvasNoiseReason::kNoGpu;
  }
  if (!execution_context) {
    noise_reason |= CanvasNoiseReason::kNoExecutionContext;
  }
  if (execution_context &&
      !execution_context->GetRuntimeFeatureStateOverrideContext()
           ->IsCanvasInterventionsForceEnabled()) {
    noise_reason |= CanvasNoiseReason::kNotEnabledInMode;
  }

  // When all conditions are met, none of the other reasons are possible.
  static constexpr int exclusive_max =
      static_cast<int>(CanvasNoiseReason::kMaxValue) << 1;

  UMA_HISTOGRAM_EXACT_LINEAR(
      "FingerprintingProtection.CanvasNoise.InterventionReason",
      static_cast<int>(noise_reason), exclusive_max);

  return noise_reason == CanvasNoiseReason::kAllConditionsMet;
}

}  // namespace

// static
const char CanvasInterventionsHelper::kSupplementName[] =
    "CanvasInterventionsHelper";

// static
bool CanvasInterventionsHelper::MaybeNoiseSnapshot(
    CanvasRenderingContext* rendering_context,
    ExecutionContext* execution_context,
    scoped_refptr<StaticBitmapImage>& snapshot,
    RasterMode raster_mode) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  CHECK(snapshot);

  if (!ShouldApplyNoise(rendering_context, raster_mode, execution_context)) {
    return false;
  }

  // Use kUnpremul_SkAlphaType as alpha type as we are changing the pixel values
  // of all channels, including the alpha channel.
  auto info = SkImageInfo::Make(
      snapshot->GetSize().width(), snapshot->GetSize().height(),
      snapshot->GetSkColorType(), kUnpremul_SkAlphaType,
      snapshot->GetSkColorSpace());
  SkBitmap bm;
  if (!bm.tryAllocPixels(info)) {
    return false;
  }

  // Copy the original pixels from snapshot to the modifiable SkPixmap. SkBitmap
  // should already allocate the correct amount of pixels, so this shouldn't
  // fail because of memory allocation.
  auto pixmap_to_noise = bm.pixmap();
  PaintImage paint_image = snapshot->PaintImageForCurrentFrame();
  if (!paint_image.readPixels(bm.info(), pixmap_to_noise.writable_addr(),
                              bm.rowBytes(), 0, 0)) {
    return false;
  }

  base::span<uint8_t> modify_pixels =
      gfx::SkPixmapToWritableSpan(pixmap_to_noise);

  auto token_hash = NoiseHash(CanvasNoiseToken::Get(),
                              execution_context->GetSecurityOrigin()
                                  ->GetOriginOrPrecursorOriginIfOpaque()
                                  ->RegistrableDomain()
                                  .Utf8());
  NoisePixels(token_hash, modify_pixels, pixmap_to_noise.width(),
              pixmap_to_noise.height());

  auto noised_image = bm.asImage();
  snapshot = blink::UnacceleratedStaticBitmapImage::Create(
      std::move(noised_image), snapshot->CurrentFrameOrientation());

  execution_context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kIntervention,
      mojom::blink::ConsoleMessageLevel::kInfo,
      "Noise was added to a canvas readback. If this has caused breakage, "
      "please file a bug at https://issues.chromium.org/issues/"
      "new?component=1456351&title=Canvas%20noise%20breakage. This "
      "feature can be disabled through chrome://flags/#enable-canvas-noise"));

  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;

  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "FingerprintingProtection.CanvasNoise.NoiseDuration", elapsed_time,
      base::Microseconds(50), base::Milliseconds(10), 50);
  UMA_HISTOGRAM_COUNTS_1M(
      "FingerprintingProtection.CanvasNoise.NoisedCanvasSize",
      pixmap_to_noise.width() * pixmap_to_noise.height());
  auto* helper = CanvasInterventionsHelper::From(execution_context);
  helper->IncrementNoisedCanvasReadbacks();

  return true;
}

// static
CanvasInterventionsHelper* CanvasInterventionsHelper::From(
    ExecutionContext* context) {
  CanvasInterventionsHelper* helper =
      Supplement<ExecutionContext>::From<CanvasInterventionsHelper>(context);
  if (!helper) {
    helper = MakeGarbageCollected<CanvasInterventionsHelper>(*context);
    Supplement<ExecutionContext>::ProvideTo(*context, helper);
  }
  return helper;
}

CanvasInterventionsHelper::CanvasInterventionsHelper(
    ExecutionContext& execution_context)
    : Supplement<ExecutionContext>(execution_context),
      ExecutionContextLifecycleObserver(&execution_context) {}

void CanvasInterventionsHelper::ContextDestroyed() {
  CHECK_GT(num_noised_canvas_readbacks_, 0);
  UMA_HISTOGRAM_COUNTS_100(
      "FingerprintingProtection.CanvasNoise.NoisedReadbacksPerContext",
      num_noised_canvas_readbacks_);
}

}  // namespace blink
