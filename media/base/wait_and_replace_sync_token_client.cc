// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/wait_and_replace_sync_token_client.h"
#include "gpu/command_buffer/client/interface_base.h"

namespace media {

WaitAndReplaceSyncTokenClient::WaitAndReplaceSyncTokenClient(
    gpu::InterfaceBase* ib)
    : ib_(ib) {}
WaitAndReplaceSyncTokenClient::WaitAndReplaceSyncTokenClient(
    gpu::InterfaceBase* ib,
    std::unique_ptr<gpu::RasterScopedAccess> ri_access)
    : ib_(ib), ri_access_(std::move(ri_access)) {}

WaitAndReplaceSyncTokenClient::~WaitAndReplaceSyncTokenClient() = default;

void WaitAndReplaceSyncTokenClient::GenerateSyncToken(
    gpu::SyncToken* sync_token) {
  if (ri_access_) {
    *sync_token = gpu::RasterScopedAccess::EndAccess(std::move(ri_access_));
    int8_t* sync_token_data = sync_token->GetData();
    ib_->VerifySyncTokensCHROMIUM(&sync_token_data, 1);
  } else {
    ib_->GenSyncTokenCHROMIUM(sync_token->GetData());
  }
}

void WaitAndReplaceSyncTokenClient::WaitSyncToken(
    const gpu::SyncToken& sync_token) {
  ib_->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
}

}  // namespace media
