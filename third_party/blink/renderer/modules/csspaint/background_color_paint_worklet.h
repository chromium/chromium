// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_BACKGROUND_COLOR_PAINT_WORKLET_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_BACKGROUND_COLOR_PAINT_WORKLET_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/renderer/modules/csspaint/native_paint_worklet.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {

class Image;
class LocalFrame;

class MODULES_EXPORT BackgroundColorPaintWorklet : public NativePaintWorklet {
  DISALLOW_COPY_AND_ASSIGN(BackgroundColorPaintWorklet);

 public:
  static BackgroundColorPaintWorklet* Create(LocalFrame&);

  explicit BackgroundColorPaintWorklet(LocalFrame&);
  ~BackgroundColorPaintWorklet() final;

  // The |container_size| is without subpixel snapping.
  scoped_refptr<Image> Paint(const FloatSize& container_size, SkColor color);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_BACKGROUND_COLOR_PAINT_WORKLET_H_
