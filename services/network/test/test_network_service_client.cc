// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_network_service_client.h"

#include <utility>

#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/unguessable_token.h"

namespace network {

TestNetworkServiceClient::TestNetworkServiceClient() : receiver_(nullptr) {}

TestNetworkServiceClient::TestNetworkServiceClient(
    mojo::PendingReceiver<mojom::NetworkServiceClient> receiver)
    : receiver_(this, std::move(receiver)) {}

TestNetworkServiceClient::~TestNetworkServiceClient() {}

void TestNetworkServiceClient::OnLoadingStateUpdate(
    std::vector<mojom::LoadInfoPtr> infos,
    OnLoadingStateUpdateCallback callback) {}

void TestNetworkServiceClient::OnDataUseUpdate(
    int32_t network_traffic_annotation_id_hash,
    int64_t recv_bytes,
    int64_t sent_bytes) {}

}  // namespace network
