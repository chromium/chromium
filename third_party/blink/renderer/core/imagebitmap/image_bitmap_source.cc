// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/imagebitmap/image_bitmap_source.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_bitmap_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

ScriptPromise ImageBitmapSource::FulfillImageBitmap(
    ScriptState* script_state,
    ImageBitmap* image_bitmap,
    ExceptionState& exception_state) {
  if (!image_bitmap || !image_bitmap->BitmapImage()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The ImageBitmap could not be allocated.");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  resolver->Resolve(image_bitmap);
  return promise;
}

ScriptPromise ImageBitmapSource::CreateImageBitmap(
    ScriptState* script_state,
    absl::optional<gfx::Rect> crop_rect,
    const ImageBitmapOptions* options,
    ExceptionState& exception_state) {
  return ScriptPromise();
}

}  // namespace blink
