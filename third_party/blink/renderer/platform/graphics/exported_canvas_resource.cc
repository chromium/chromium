// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/exported_canvas_resource.h"

#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"

namespace blink {

ExportedCanvasResource::ExportedCanvasResource(
    scoped_refptr<CanvasResource> resource)
    : resource_(std::move(resource)) {
  CHECK(resource_);
}

ExportedCanvasResource::~ExportedCanvasResource() {
  // We PostTask ExportedCanvasResource to the CanvasResource owning thread, but
  // if the thread gets destroyed before, destructor will still run, but we
  // can't tell CanvasResource to recycle.
  if (!resource_->is_cross_thread()) {
    CanvasResource::DropRefOnOwningThread(std::move(resource_));
  }
}

// static
void ExportedCanvasResource::DropRefOnOwningThread(
    scoped_refptr<ExportedCanvasResource>&& exported_resource) {
  CHECK(exported_resource);
  CHECK(exported_resource->HasOneRef());
}

// static
void ExportedCanvasResource::OnPlaceholderReleasedResource(
    scoped_refptr<ExportedCanvasResource>&& exported_resource) {
  if (!exported_resource) {
    return;
  }

  CHECK(exported_resource->HasOneRef());

  if (exported_resource->resource_->is_cross_thread()) {
    auto& owning_thread_task_runner =
        exported_resource->resource_->owning_thread_task_runner_;
    owning_thread_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&DropRefOnOwningThread, std::move(exported_resource)));
  } else {
    DropRefOnOwningThread(std::move(exported_resource));
  }
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
