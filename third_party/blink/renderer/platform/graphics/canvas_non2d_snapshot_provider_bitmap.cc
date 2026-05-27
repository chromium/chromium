// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_non2d_snapshot_provider_bitmap.h"

namespace blink {

std::unique_ptr<CanvasNon2DSnapshotProviderBitmap>
CanvasNon2DSnapshotProviderBitmap::Create(
    const CanvasSnapshotProvider::Info& info) {
  return base::WrapUnique<CanvasNon2DSnapshotProviderBitmap>(
      new CanvasNon2DSnapshotProviderBitmap(info));
}

CanvasNon2DSnapshotProviderBitmap::CanvasNon2DSnapshotProviderBitmap(
    const CanvasSnapshotProvider::Info& info)
    : info_(info) {}

CanvasNon2DSnapshotProviderBitmap::~CanvasNon2DSnapshotProviderBitmap() =
    default;

bool CanvasNon2DSnapshotProviderBitmap::IsGpuContextLost() const {
  return true;
}

bool CanvasNon2DSnapshotProviderBitmap::IsValid() const {
  // This class doesn't attempt to create an SkSurface until
  // DoExternalDrawAndSnapshot() is invoked; it will detect failure to create
  // the surface at that point and return nullptr.
  return true;
}

}  // namespace blink
