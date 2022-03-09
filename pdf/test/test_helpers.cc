// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/test/test_helpers.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "pdf/ppapi_migration/bitmap.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace chrome_pdf {

base::FilePath GetTestDataFilePath(const base::FilePath& path) {
  base::FilePath source_root;
  if (!base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root))
    return {};

  return source_root.Append(FILE_PATH_LITERAL("pdf"))
      .Append(FILE_PATH_LITERAL("test"))
      .Append(FILE_PATH_LITERAL("data"))
      .Append(path);
}

SkBitmap CreateSkiaImageForTesting(const gfx::Size& size, SkColor color) {
  SkBitmap bitmap = CreateN32PremulSkBitmap(gfx::SizeToSkISize(size));
  bitmap.eraseColor(color);
  return bitmap;
}

}  // namespace chrome_pdf
