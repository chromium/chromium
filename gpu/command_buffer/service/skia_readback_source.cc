// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/skia_readback_source.h"

#include "base/check.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace gpu {

SkiaReadbackSource::SkiaReadbackSource(SkSurface* surface) : source_(surface) {
  CHECK(surface);
}

SkiaReadbackSource::SkiaReadbackSource(SkImage* image) : source_(image) {
  CHECK(image);
}

SkiaReadbackSource::SkiaReadbackSource(const SkiaReadbackSource& other) =
    default;
SkiaReadbackSource& SkiaReadbackSource::operator=(
    const SkiaReadbackSource& other) = default;

SkiaReadbackSource::~SkiaReadbackSource() = default;

SkAlphaType SkiaReadbackSource::GetAlphaType() const {
  return surface() ? surface()->imageInfo().alphaType() : image()->alphaType();
}

}  // namespace gpu
