// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/wait_and_replace_sync_token_client.h"
#include "gpu/command_buffer/client/interface_base.h"

namespace media {

WaitAndReplaceSyncTokenClient::WaitAndReplaceSyncTokenClient(
    gpu::InterfaceBase* ib)
    : ib_(ib) {}

void WaitAndReplaceSyncTokenClient::GenerateSyncToken(
    gpu::SyncToken* sync_token) {
  ib_->GenSyncTokenCHROMIUM(sync_token->GetData());
}

void WaitAndReplaceSyncTokenClient::WaitSyncToken(
    const gpu::SyncToken& sync_token) {
  ib_->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
}

}  // namespace media
