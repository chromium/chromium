// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SHAPEDETECTION_SHAPE_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SHAPEDETECTION_SHAPE_DETECTOR_H_

#include "skia/public/mojom/bitmap.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/canvas/imagebitmap/image_bitmap_source_union.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace blink {

class MODULES_EXPORT ShapeDetector : public ScriptWrappable {
 public:
  ~ShapeDetector() override = default;

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  ScriptPromise detect(ScriptState* script_state,
                       const V8ImageBitmapSource* image_source);
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  ScriptPromise detect(ScriptState*, const ImageBitmapSourceUnion&);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

 private:
  ScriptPromise DetectShapesOnImageData(ScriptPromiseResolver*, ImageData*);
  ScriptPromise DetectShapesOnImageElement(ScriptPromiseResolver*,
                                           const HTMLImageElement*);

  virtual ScriptPromise DoDetect(ScriptPromiseResolver*, SkBitmap) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SHAPEDETECTION_SHAPE_DETECTOR_H_
