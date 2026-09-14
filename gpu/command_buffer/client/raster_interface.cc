// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/raster_interface.h"

#include <utility>

#include "base/check.h"
#include "gpu/command_buffer/client/client_shared_image.h"

namespace gpu::raster {

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
