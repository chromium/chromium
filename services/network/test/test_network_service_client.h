// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_TEST_NETWORK_SERVICE_CLIENT_H_
#define SERVICES_NETWORK_TEST_TEST_NETWORK_SERVICE_CLIENT_H_

#include <string>
#include <vector>

#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace network {

// A helper class with a basic NetworkServiceClient implementation for use in
// unittests, which may need an implementation (for things like file uploads),
// but don't have the real implementation available.
class TestNetworkServiceClient : public network::mojom::NetworkServiceClient {
 public:
  TestNetworkServiceClient();
  explicit TestNetworkServiceClient(
      mojo::PendingReceiver<mojom::NetworkServiceClient> receiver);
  ~TestNetworkServiceClient() override;

  // network::mojom::NetworkServiceClient implementation:
  void OnDataUseUpdate(int32_t network_traffic_annotation_id_hash,
                       int64_t recv_bytes,
                       int64_t sent_bytes) override;

 private:
  mojo::Receiver<mojom::NetworkServiceClient> receiver_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkServiceClient);
};

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_TEST_NETWORK_SERVICE_CLIENT_H_
