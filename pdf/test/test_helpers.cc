// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/test/test_helpers.h"

#include "pdf/ppapi_migration/bitmap.h"
#include "pdf/ppapi_migration/image.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/skia_util.h"

namespace chrome_pdf {

Image CreateSkiaImageForTesting(const gfx::Size& size, SkColor color) {
  SkBitmap bitmap = CreateN32PremulSkBitmap(gfx::SizeToSkISize(size));
  bitmap.eraseColor(color);
  return Image(bitmap);
}

}  // namespace chrome_pdf
