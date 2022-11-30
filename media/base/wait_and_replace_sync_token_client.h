// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WAIT_AND_REPLACE_SYNC_TOKEN_CLIENT_H_
#define MEDIA_BASE_WAIT_AND_REPLACE_SYNC_TOKEN_CLIENT_H_

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "media/base/media_export.h"
#include "media/base/video_frame.h"

namespace gpu {
class InterfaceBase;
}

namespace media {

class MEDIA_EXPORT WaitAndReplaceSyncTokenClient
    : public VideoFrame::SyncTokenClient {
 public:
  explicit WaitAndReplaceSyncTokenClient(gpu::InterfaceBase* ib);

  WaitAndReplaceSyncTokenClient(const WaitAndReplaceSyncTokenClient&) = delete;
  WaitAndReplaceSyncTokenClient& operator=(
      const WaitAndReplaceSyncTokenClient&) = delete;

  void GenerateSyncToken(gpu::SyncToken* sync_token) final;
  void WaitSyncToken(const gpu::SyncToken& sync_token) final;

 private:
  raw_ptr<gpu::InterfaceBase> ib_;
};

}  // namespace media

#endif  // MEDIA_BASE_WAIT_AND_REPLACE_SYNC_TOKEN_CLIENT_H_
