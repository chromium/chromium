// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"

#include <utility>

#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/sync_token.h"

namespace blink {

CanvasResource::CanvasResource(
    scoped_refptr<gpu::ClientSharedImage> shared_image)
    : gpu::ClientImage(std::move(shared_image)) {
  subclass_manages_destruction_sync_token_ = true;
  if (!GetSharedImage()->is_software()) {
    // Wait for the mailbox to be ready to be used.
    WaitSyncToken(GetSharedImage()->creation_sync_token());
  }
}

CanvasResource::~CanvasResource() = default;

void CanvasResource::WaitSyncToken(const gpu::SyncToken& sync_token) {
  if (sync_token.HasData()) {
    acquire_sync_token_ = sync_token;
    GetSharedImage()->UpdateDestructionSyncToken(acquire_sync_token_);
  }
}

void CanvasResource::NotifyResourceLost() {
  resource_is_lost_ = true;
}

}  // namespace blink
