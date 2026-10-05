// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_2D_BITMAP_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_2D_BITMAP_PROVIDER_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkSurface;

namespace blink {

// Renders canvas2D ops to a Skia RAM-backed bitmap. Mailboxing is not
// supported : cannot be directly composited. For usage by (Offscreen)Canvas2D
// as a last-case resort when it is not possible to create
// Canvas2DResourceProvider.
class PLATFORM_EXPORT Canvas2DBitmapProvider final {
 public:
  explicit Canvas2DBitmapProvider(sk_sp<SkSurface> surface);
  ~Canvas2DBitmapProvider();

  SkSurface* surface() const { return surface_.get(); }

 private:
  const sk_sp<SkSurface> surface_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_2D_BITMAP_PROVIDER_H_
