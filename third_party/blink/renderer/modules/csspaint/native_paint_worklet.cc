// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/native_paint_worklet.h"

#include "third_party/blink/renderer/modules/csspaint/paint_rendering_context_2d.h"
#include "third_party/blink/renderer/platform/graphics/paint_generated_image.h"

namespace blink {

NativePaintWorklet::NativePaintWorklet() = default;

NativePaintWorklet::~NativePaintWorklet() = default;

scoped_refptr<Image> NativePaintWorklet::Paint(const FloatSize& container_size,
                                               SkColor color) {
  PaintRenderingContext2DSettings* context_settings =
      PaintRenderingContext2DSettings::Create();
  auto* rendering_context = MakeGarbageCollected<PaintRenderingContext2D>(
      RoundedIntSize(container_size), context_settings, 1, 1);
  rendering_context->GetPaintCanvas()->drawColor(color);
  sk_sp<PaintRecord> paint_record = rendering_context->GetRecord();
  if (!paint_record)
    return nullptr;
  return PaintGeneratedImage::Create(paint_record, container_size);
}

}  // namespace blink
