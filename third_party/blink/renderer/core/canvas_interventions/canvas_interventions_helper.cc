// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/canvas_interventions/canvas_interventions_helper.h"

#include <cstdint>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/mojo_binding_context.h"
#include "third_party/blink/renderer/platform/runtime_feature_state/runtime_feature_state_override_context.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/skia_span_util.h"

namespace blink {

// static
const char CanvasInterventionsHelper::kSupplementName[] =
    "CanvasInterventionsHelper";

CanvasInterventionsHelper::CanvasInterventionsHelper(ExecutionContext& context)
    : Supplement<ExecutionContext>(context), execution_context_(context) {}

// static
// TODO(https://crbug.com/392627601): Pipe session seeds.
CanvasInterventionsHelper* CanvasInterventionsHelper::Create(
    ExecutionContext* context) {
  CanvasInterventionsHelper* helper =
      Supplement<ExecutionContext>::From<CanvasInterventionsHelper>(context);
  if (!helper) {
    helper = MakeGarbageCollected<CanvasInterventionsHelper>(*context);
    Supplement<ExecutionContext>::ProvideTo(*context, helper);
  }
  return helper;
}

scoped_refptr<StaticBitmapImage>
CanvasInterventionsHelper::MaybeGetNoisedSnapshot(
    scoped_refptr<StaticBitmapImage> input_snapshot) {
  CHECK(input_snapshot);

  auto original_info = SkImageInfo::Make(
      input_snapshot->GetSize().width(), input_snapshot->GetSize().height(),
      input_snapshot->GetSkColorType(), input_snapshot->GetAlphaType(),
      input_snapshot->GetSkColorSpace());
  SkBitmap bm;
  if (!bm.tryAllocPixels(original_info)) {
    return input_snapshot;
  }

  // Copy the original pixels from snapshot to the modifiable SkPixmap. SkBitmap
  // should already allocate the correct amount of pixels, so this shouldn't
  // fail because of memory allocation.
  auto pixmap_to_noise = bm.pixmap();
  PaintImage paint_image = input_snapshot->PaintImageForCurrentFrame();
  if (!paint_image.readPixels(original_info, pixmap_to_noise.writable_addr(),
                              original_info.minRowBytes(), 0, 0)) {
    return input_snapshot;
  }

  base::span<uint8_t> modify_pixels =
      gfx::SkPixmapToWritableSpan(pixmap_to_noise);

  if (MaybeNoisePixels(modify_pixels, pixmap_to_noise.width(),
                       pixmap_to_noise.height())) {
    auto noised_image = bm.asImage();
    scoped_refptr<blink::StaticBitmapImage> noised_snapshot =
        blink::UnacceleratedStaticBitmapImage::Create(
            std::move(noised_image), input_snapshot->CurrentFrameOrientation());
    return noised_snapshot;
  }

  return input_snapshot;
}

bool CanvasInterventionsHelper::MaybeNoisePixels(
    base::span<uint8_t> source_pixels,
    uint32_t sw,
    uint32_t sh) {
  // TODO(https://crbug.com/380463018): We are currently unconditionally
  // noising. Once signatures have been implemented, add conditional logic here.
  if (!ShouldApplyNoise()) {
    return false;
  }

  // TODO(https://crbug.com/385739564): Apply noising algorithm here.
  return true;
}

bool CanvasInterventionsHelper::ShouldApplyNoise() const {
  // TODO(https://crbug.com/392627601): Ensure session seed is initialized.
  return GetExecutionContext()
      ->GetRuntimeFeatureStateOverrideContext()
      ->IsCanvasInterventionsForceEnabled();
}
}  // namespace blink
