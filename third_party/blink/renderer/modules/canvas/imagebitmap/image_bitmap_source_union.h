// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_IMAGEBITMAP_IMAGE_BITMAP_SOURCE_UNION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_IMAGEBITMAP_IMAGE_BITMAP_SOURCE_UNION_H_

#include "third_party/blink/renderer/bindings/modules/v8/image_bitmap_source.h"

namespace blink {

using ImageBitmapSourceUnion =
    HTMLImageElementOrSVGImageElementOrHTMLVideoElementOrHTMLCanvasElementOrBlobOrImageDataOrImageBitmapOrOffscreenCanvas;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_IMAGEBITMAP_IMAGE_BITMAP_SOURCE_UNION_H_
