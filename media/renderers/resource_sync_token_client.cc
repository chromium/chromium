// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/resource_sync_token_client.h"
#include "gpu/command_buffer/client/interface_base.h"

namespace media {

ResourceSyncTokenClient::ResourceSyncTokenClient(
    gpu::InterfaceBase* ib,
    gpu::SyncToken old_frame_release_token,
    gpu::SyncToken new_plane_release_token)
    : ib_(ib),
      old_frame_release_token_(std::move(old_frame_release_token)),
      new_plane_release_token_(std::move(new_plane_release_token)) {
  DCHECK(new_plane_release_token_.HasData());
  DCHECK(ib_);
}

ResourceSyncTokenClient::~ResourceSyncTokenClient() = default;

void ResourceSyncTokenClient::GenerateSyncToken(gpu::SyncToken* sync_token) {
  if (new_plane_release_token_.HasData()) {
    *sync_token = new_plane_release_token_;
    return;
  }

  ib_->GenSyncTokenCHROMIUM(sync_token->GetData());
}

void ResourceSyncTokenClient::WaitSyncToken(const gpu::SyncToken& sync_token) {
  // This function isn't called if `sync_token` is null.
  DCHECK(sync_token.HasData());

  // Do nothing if `sync_token` matches the one we're injecting. It can end up
  // matching since we can get the same SyncToken back for each plane in the
  // VideoFrame yet we only have a single per-frame release token.
  if (new_plane_release_token_ == sync_token) {
    return;
  }

  // If the frame's release token is the same as it was when the resource was
  // imported by viz, we can adopt 'new_plane_release_token_' without waiting
  // since it's guaranteed to be later in sequence.
  if (old_frame_release_token_ == sync_token) {
    return;
  }

  // Otherwise we must wait on the old one and the new one and generate an even
  // newer one during the GenerateSyncToken() call above.
  ib_->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  ib_->WaitSyncTokenCHROMIUM(new_plane_release_token_.GetConstData());
  new_plane_release_token_.Clear();
}

}  // namespace media
