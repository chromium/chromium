// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/canvas_interventions/canvas_interventions_helper.h"

#include <cstdint>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
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
  if (!rendering_context) {
    return false;
  }
  if (!rendering_context->ShouldTriggerIntervention()) {
    return false;
  }
  if (!rendering_context->IsRenderingContext2D()) {
    return false;
  }
  if (!(raster_mode == RasterMode::kGPU ||
        RuntimeEnabledFeatures::CanvasInterventionsOnCpuForTestingEnabled())) {
    return false;
  }
  return execution_context &&
         execution_context->GetRuntimeFeatureStateOverrideContext()
             ->IsCanvasInterventionsForceEnabled();
}

}  // namespace

// static
bool CanvasInterventionsHelper::MaybeNoiseSnapshot(
    CanvasRenderingContext* rendering_context,
    ExecutionContext* execution_context,
    scoped_refptr<StaticBitmapImage>& snapshot,
    RasterMode raster_mode) {
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

  return true;
}
}  // namespace blink
