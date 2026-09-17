// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/raster_interface.h"

#include <utility>

#include "base/check.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace gpu::raster {

RasterInterface::CopySharedImageResult RasterInterface::CopySharedImage(
    const scoped_refptr<ClientSharedImage>& source,
    const SyncToken& source_sync_token,
    const scoped_refptr<ClientSharedImage>& dest,
    const SyncToken& dest_sync_token,
    const gfx::Rect& source_rect,
    const gfx::Point& dest_offset) {
  CHECK(source);
  CHECK(dest);
  auto dst_access =
      dest->BeginRasterAccess(this, dest_sync_token, /*readonly=*/false);
  auto src_access =
      source->BeginRasterAccess(this, source_sync_token, /*readonly=*/true);
  CopySharedImage(source->mailbox(), dest->mailbox(), dest_offset.x(),
                  dest_offset.y(), source_rect.x(), source_rect.y(),
                  source_rect.width(), source_rect.height());
  auto src_completion_token =
      RasterScopedAccess::EndAccess(std::move(src_access));
  source->UpdateDestructionSyncToken(src_completion_token);
  auto dst_completion_token =
      RasterScopedAccess::EndAccess(std::move(dst_access));
  dest->UpdateDestructionSyncToken(dst_completion_token);
  return {src_completion_token, dst_completion_token};
}

SyncToken RasterInterface::WritePixels(
    const scoped_refptr<ClientSharedImage>& dest,
    const SyncToken& sync_token,
    int dst_x_offset,
    int dst_y_offset,
    const SkPixmap& src_sk_pixmap) {
  CHECK(dest);
  auto access = dest->BeginRasterAccess(this, sync_token, /*readonly=*/false);
  WritePixels(dest->mailbox(), dst_x_offset, dst_y_offset,
              dest->GetTextureTarget(), src_sk_pixmap);
  auto new_token = RasterScopedAccess::EndAccess(std::move(access));
  dest->UpdateDestructionSyncToken(new_token);
  return new_token;
}

}  // namespace gpu::raster
