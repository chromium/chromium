// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_IMAGEBITMAP_IMAGE_BITMAP_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_IMAGEBITMAP_IMAGE_BITMAP_SOURCE_H_

#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ImageBitmap;
class ImageBitmapOptions;

class CORE_EXPORT ImageBitmapSource {
  DISALLOW_NEW();

 public:
  virtual IntSize BitmapSourceSize() const { return IntSize(); }
  virtual ScriptPromise CreateImageBitmap(ScriptState*,
                                          EventTarget&,
                                          base::Optional<IntRect>,
                                          const ImageBitmapOptions*);

  virtual bool IsBlob() const { return false; }

  static ScriptPromise FulfillImageBitmap(ScriptState*, ImageBitmap*);

 protected:
  virtual ~ImageBitmapSource() = default;
};

}  // namespace blink

#endif
