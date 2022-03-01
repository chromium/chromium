// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PPAPI_MIGRATION_BITMAP_H_
#define PDF_PPAPI_MIGRATION_BITMAP_H_

#include "third_party/skia/include/core/SkSize.h"

class SkBitmap;

namespace chrome_pdf {

// Creates an SkBitmap of a given `size`.
SkBitmap CreateN32PremulSkBitmap(const SkISize& size);

}  // namespace chrome_pdf

#endif  // PDF_PPAPI_MIGRATION_BITMAP_H_
