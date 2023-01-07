// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/simple_sync_token_client.h"

namespace media {

SimpleSyncTokenClient::SimpleSyncTokenClient(const gpu::SyncToken& sync_token)
    : sync_token_(sync_token) {}

void SimpleSyncTokenClient::GenerateSyncToken(gpu::SyncToken* sync_token) {
  *sync_token = sync_token_;
}

void SimpleSyncTokenClient::WaitSyncToken(const gpu::SyncToken& sync_token) {}

}  // namespace media
