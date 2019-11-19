// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/imagebitmap/image_bitmap_source.h"

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap_options.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

ScriptPromise ImageBitmapSource::FulfillImageBitmap(ScriptState* script_state,
                                                    ImageBitmap* image_bitmap) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  if (image_bitmap && image_bitmap->BitmapImage()) {
    resolver->Resolve(image_bitmap);
  } else {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "The ImageBitmap could not be allocated."));
  }
  return promise;
}

ScriptPromise ImageBitmapSource::CreateImageBitmap(
    ScriptState* script_state,
    EventTarget& event_target,
    base::Optional<IntRect> crop_rect,
    const ImageBitmapOptions* options) {
  return ScriptPromise();
}

}  // namespace blink
