// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_RESOURCE_SYNC_TOKEN_CLIENT_H_
#define MEDIA_RENDERERS_RESOURCE_SYNC_TOKEN_CLIENT_H_

#include "gpu/command_buffer/common/sync_token.h"
#include "media/base/media_export.h"
#include "media/base/video_frame.h"

namespace gpu {
class InterfaceBase;
}

namespace media {

// A SyncTokenClient specialized in handling SyncTokens provided for resource
// return during a viz::ReleaseCallback. It attempts to minimize the number of
// waits and new sync tokens generated.
class MEDIA_EXPORT ResourceSyncTokenClient
    : public VideoFrame::SyncTokenClient {
 public:
  // `old_frame_release_token` is the SyncToken that the underlying VideoFrame
  // had when the resource was imported and the ReleaseCallback bound.
  // `new_plane_release_token` is the SyncToken the plane ReleaseCallback was
  // called with. It must not be null.
  ResourceSyncTokenClient(gpu::InterfaceBase* gl,
                          gpu::SyncToken old_frame_release_token,
                          gpu::SyncToken new_plane_release_token);
  ~ResourceSyncTokenClient() override;

  ResourceSyncTokenClient(const ResourceSyncTokenClient&) = delete;
  ResourceSyncTokenClient& operator=(const ResourceSyncTokenClient&) = delete;

  // VideoFrame::SyncTokenClient implementation.
  void GenerateSyncToken(gpu::SyncToken* sync_token) final;
  void WaitSyncToken(const gpu::SyncToken& sync_token) final;

 private:
  raw_ptr<gpu::InterfaceBase> const ib_;
  gpu::SyncToken old_frame_release_token_;
  gpu::SyncToken new_plane_release_token_;
};

}  // namespace media

#endif  // MEDIA_RENDERERS_RESOURCE_SYNC_TOKEN_CLIENT_H_
