// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_pool_service.h"

namespace gpu {

SharedImagePoolService::SharedImagePoolService(
    SharedImagePoolId pool_id,
    mojo::PendingRemote<mojom::SharedImagePoolClientInterface> client_remote,
    SharedImageFactory* factory)
    : pool_id_(pool_id),
      client_remote_(std::move(client_remote)),
      factory_(std::move(factory)) {}

SharedImagePoolService::~SharedImagePoolService() = default;

void SharedImagePoolService::NotifyClearPool() {
  if (client_remote_.is_bound()) {
    client_remote_->OnClearPool();
  }
}

}  // namespace gpu
