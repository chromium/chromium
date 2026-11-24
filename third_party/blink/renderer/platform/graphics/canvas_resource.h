// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_H_

#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_pool.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

// Resource type for SharedImage
class PLATFORM_EXPORT CanvasResource final : public gpu::ClientImage {
 public:
  explicit CanvasResource(scoped_refptr<gpu::ClientSharedImage> shared_image);

  // Save (and wait on) this sync token on the context used by this resource for
  // rendering.
  // TODO(crbug.com/40286368): completely defer the waiting to the
  // zero-parameter variant of WaitSyncToken().
  void WaitSyncToken(const gpu::SyncToken&);

  const gpu::SyncToken& acquire_sync_token() const {
    return acquire_sync_token_;
  }

  // Called when the resource is marked lost. Losing a resource does not mean
  // that the backing memory has been destroyed, since the resource itself keeps
  // a ref on that memory.
  // It means that the consumer (commonly the compositor) can not provide a sync
  // token for the resource to be safely recycled and its the GL state may be
  // inconsistent with when the resource was given to the compositor. So it
  // should not be recycled for writing again but can be safely read from.
  void NotifyResourceLost();

  bool IsLost() const { return resource_is_lost_; }

  ~CanvasResource() override;

 private:
  // These members are either only accessed on the owning thread, or are only
  // updated on the owning thread and then are read on a different thread.
  // We ensure to correctly update their state in Transfer, which is called
  // before a resource is used on a different thread.
  bool resource_is_lost_ = false;
  gpu::SyncToken acquire_sync_token_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_H_
