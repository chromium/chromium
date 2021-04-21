// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_TESTING_INTERNALS_CANVAS_RENDERING_CONTEXT_2D_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_TESTING_INTERNALS_CANVAS_RENDERING_CONTEXT_2D_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class CanvasRenderingContext2D;
class Internals;

class InternalsCanvasRenderingContext2D {
  STATIC_ONLY(InternalsCanvasRenderingContext2D);

 public:
  static uint32_t countHitRegions(Internals&, CanvasRenderingContext2D*);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_TESTING_INTERNALS_CANVAS_RENDERING_CONTEXT_2D_H_
