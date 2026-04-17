// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/exported_canvas_resource.h"

#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"

namespace blink {

ExportedCanvasResource::ExportedCanvasResource(
    scoped_refptr<CanvasResource> resource)
    : resource_(std::move(resource)) {}

ExportedCanvasResource::~ExportedCanvasResource() = default;

// static
void ExportedCanvasResource::OnPlaceholderReleasedResource(
    scoped_refptr<ExportedCanvasResource>&& resource) {
  if (!resource) {
    return;
  }

  CHECK(resource->HasOneRef());
  CanvasResource::OnPlaceholderReleasedResource(std::move(resource->resource_));
}

gfx::Size ExportedCanvasResource::Size() const {
  return resource_->Size();
}

bool ExportedCanvasResource::OriginClean() const {
  return resource_->OriginClean();
}

scoped_refptr<StaticBitmapImage> ExportedCanvasResource::Bitmap() {
  return resource_->Bitmap();
}

}  // namespace blink
