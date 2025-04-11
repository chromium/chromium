// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/canvas_interventions/canvas_interventions_helper.h"

#include <cstdint>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "third_party/blink/public/common/fingerprinting_protection/canvas_noise_token.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"
#include "third_party/blink/renderer/core/canvas_interventions/noise_hash.h"
#include "third_party/blink/renderer/core/canvas_interventions/noise_helper.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_host.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/runtime_feature_state/runtime_feature_state_override_context.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
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
                      scoped_refptr<StaticBitmapImage>& snapshot,
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
  if (!snapshot->IsTextureBacked()) {
    return false;
  }
  return execution_context &&
         execution_context->GetRuntimeFeatureStateOverrideContext()
             ->IsCanvasInterventionsForceEnabled();
}

String GetDomainFromSecurityOrigin(const SecurityOrigin* security_origin) {
  const SecurityOrigin* precursor_origin =
      security_origin->GetOriginOrPrecursorOriginIfOpaque();
  if (precursor_origin->IsOpaque()) {
    return String::Format(
        "opaque || %u",
        WTF::GetHash(scoped_refptr<const SecurityOrigin>(precursor_origin)));
  }
  // RegistrableDomain() returns null in a couple of cases, such as URLs with IP
  // addresses. In these cases we can safely return the host.
  String domain = precursor_origin->RegistrableDomain();
  if (!domain.IsNull()) {
    return domain;
  }
  return precursor_origin->Host();
}

}  // namespace

// static
bool CanvasInterventionsHelper::MaybeNoiseSnapshot(
    CanvasRenderingContext* rendering_context,
    ExecutionContext* execution_context,
    scoped_refptr<StaticBitmapImage>& snapshot) {
  CHECK(snapshot);

  if (!ShouldApplyNoise(rendering_context, snapshot, execution_context)) {
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

  // TODO(crbug.com/377325952): Extend domain part to follow the general
  // partitioning properties.
  String noise_domain;
  if (auto* window = DynamicTo<LocalDOMWindow>(execution_context)) {
    Frame& top_frame = window->GetFrame()->Tree().Top();
    noise_domain = GetDomainFromSecurityOrigin(
        top_frame.GetSecurityContext()->GetSecurityOrigin());
  } else if (auto* worker = DynamicTo<WorkerGlobalScope>(execution_context)) {
    noise_domain =
        GetDomainFromSecurityOrigin(worker->top_level_frame_security_origin());
  } else {
    NOTREACHED();
  }

  // TODO(crbug.com/392627601): Use the token that is piped down from the
  // browser.
  auto token_hash = NoiseHash(CanvasNoiseToken::Get(), noise_domain);
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
