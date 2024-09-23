// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/imagebitmap/image_bitmap_source.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_bitmap_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

constexpr const char* kImageBitmapOptionNone = "none";

ScriptPromise<ImageBitmap> ImageBitmapSource::FulfillImageBitmap(
    ScriptState* script_state,
    ImageBitmap* image_bitmap,
    const ImageBitmapOptions* options,
    ExceptionState& exception_state) {
  if (!image_bitmap || !image_bitmap->BitmapImage()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The ImageBitmap could not be allocated.");
    return EmptyPromise();
  }

  // imageOrientation: 'from-image' will be used to replace imageOrientation:
  // 'none'. Adding a deprecation warning when 'none' is called in
  // createImageBitmap.
  if (options->imageOrientation() == kImageBitmapOptionNone) {
    auto* execution_context =
        ExecutionContext::From(script_state->GetContext());
    Deprecation::CountDeprecation(
        execution_context,
        WebFeature::kObsoleteCreateImageBitmapImageOrientationNone);
  }

  return ToResolvedPromise<ImageBitmap>(script_state, image_bitmap);
}

ScriptPromise<ImageBitmap> ImageBitmapSource::CreateImageBitmap(
    ScriptState* script_state,
    std::optional<gfx::Rect> crop_rect,
    const ImageBitmapOptions* options,
    ExceptionState& exception_state) {
  return EmptyPromise();
}

}  // namespace blink
