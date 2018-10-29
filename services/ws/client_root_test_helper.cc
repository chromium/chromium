// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/client_root_test_helper.h"

#include "services/ws/client_root.h"

namespace ws {

ClientRootTestHelper::ClientRootTestHelper(ClientRoot* client_root)
    : client_root_(client_root) {}

ClientRootTestHelper::~ClientRootTestHelper() = default;

aura::ClientSurfaceEmbedder* ClientRootTestHelper::GetClientSurfaceEmbedder() {
  return client_root_->client_surface_embedder_.get();
}

}  // namespace ws
