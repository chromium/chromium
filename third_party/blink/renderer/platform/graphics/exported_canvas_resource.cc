// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/exported_canvas_resource.h"

#include "base/functional/callback_helpers.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"

namespace blink {

ExportedCanvasResource::ExportedCanvasResource(
    scoped_refptr<CanvasResource> resource)
    : resource_(std::move(resource)) {
  CHECK(resource_);
  CHECK(!resource_->is_cross_thread());
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
void ExportedCanvasResource::OnPlaceholderReleasedResource(
    scoped_refptr<ExportedCanvasResource>&& exported_resource) {
  if (!exported_resource) {
    return;
  }

  if (exported_resource->resource_->is_cross_thread()) {
    auto& owning_thread_task_runner =
        exported_resource->resource_->owning_thread_task_runner_;
    owning_thread_task_runner->PostTask(
        FROM_HERE, base::DoNothingWithBoundArgs(std::move(exported_resource)));
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

void ExportedCanvasResource::Transfer() {
  resource_->Transfer();
}

void ExportedCanvasResource::EndDisplayCompositorAccess(
    gpu::SharedImageExportResult export_result,
    bool is_lost) {
  auto sync_token =
      resource_->GetSharedImage()->EndExport(std::move(export_result));

  resource_->WaitSyncToken(sync_token);
  if (is_lost) {
    resource_->NotifyResourceLost();
  }
}

}  // namespace blink
