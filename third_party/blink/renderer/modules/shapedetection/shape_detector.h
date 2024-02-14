// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SHAPEDETECTION_SHAPE_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SHAPEDETECTION_SHAPE_DETECTOR_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace blink {

class MODULES_EXPORT ShapeDetector : public ScriptWrappable {
 public:
  ~ShapeDetector() override = default;

 protected:
  std::optional<SkBitmap> GetBitmapFromSource(
      ScriptState* script_state,
      const V8ImageBitmapSource* image_source,
      ExceptionState&);

 private:
  std::optional<SkBitmap> GetBitmapFromImageData(ScriptState*,
                                                 ImageData*,
                                                 ExceptionState&);
  std::optional<SkBitmap> GetBitmapFromImageElement(ScriptState*,
                                                    const HTMLImageElement*,
                                                    ExceptionState&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SHAPEDETECTION_SHAPE_DETECTOR_H_
