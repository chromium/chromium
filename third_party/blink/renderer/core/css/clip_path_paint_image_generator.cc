// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/clip_path_paint_image_generator.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

namespace {

ClipPathPaintImageGenerator::ClipPathPaintImageGeneratorCreateFunction*
    g_create_function = nullptr;

}  // namespace

// static
void ClipPathPaintImageGenerator::Init(
    ClipPathPaintImageGeneratorCreateFunction* create_function) {
  DCHECK(!g_create_function);
  g_create_function = create_function;
}

// static
gfx::RectF ClipPathPaintImageGenerator::GetAnimationBoundingRect() {
  // Similar to InfiniteIntRect() but shifted by 4 bits to decrease floating
  // point precision errors. This rect size is still large enough to encompass
  // and reasonable paint area but not so large as to cause errors.
  constexpr int kInfiniteXY = LayoutUnit::Min().ToInt() / 64;
  constexpr int kInfiniteWH = LayoutUnit::Max().ToInt() / 32;
  return gfx::RectF(kInfiniteXY, kInfiniteXY, kInfiniteWH, kInfiniteWH);
}

ClipPathPaintImageGenerator* ClipPathPaintImageGenerator::Create(
    LocalFrame& local_root) {
  DCHECK(g_create_function);
  DCHECK(local_root.IsLocalRoot());
  return g_create_function(local_root);
}

}  // namespace blink
