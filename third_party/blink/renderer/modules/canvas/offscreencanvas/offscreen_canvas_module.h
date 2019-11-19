// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_OFFSCREENCANVAS_OFFSCREEN_CANVAS_MODULE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_OFFSCREENCANVAS_OFFSCREEN_CANVAS_MODULE_H_

#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CanvasContextCreationAttributesModule;
class OffscreenCanvas;

class MODULES_EXPORT OffscreenCanvasModule {
  STATIC_ONLY(OffscreenCanvasModule);

 public:
  static void getContext(ExecutionContext*,
                         OffscreenCanvas&,
                         const String&,
                         const CanvasContextCreationAttributesModule*,
                         ExceptionState&,
                         OffscreenRenderingContext&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_OFFSCREENCANVAS_OFFSCREEN_CANVAS_MODULE_H_
