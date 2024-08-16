// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/modules/canvas/canvas_test_utils.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_cssimagevalue_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_offscreencanvas_svgimageelement_videoframe.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_image_source.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_image_source_util.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

// Blink public API entry points used for canvas testing.

bool IsAcceleratedCanvasImageSource(v8::Isolate* isolate,
                                    v8::Local<v8::Value> value) {
  ExceptionState exception_state(isolate, v8::ExceptionContext::kUnknown,
                                 "IsAcceleratedCanvasImageSource");
  auto* v8_image_source =
      V8CanvasImageSource::Create(isolate, value, exception_state);
  if (exception_state.HadException()) {
    return false;
  }
  auto* image_source = ToCanvasImageSource(v8_image_source, exception_state);
  if (exception_state.HadException()) {
    return false;
  }

  return image_source->IsAccelerated();
}

}  // namespace blink
