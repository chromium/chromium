// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_TEST_SOCKET_BROKER_IMPL_H_
#define SERVICES_NETWORK_TEST_TEST_SOCKET_BROKER_IMPL_H_

#include "services/network/public/cpp/socket_broker_impl.h"

namespace network {

// Implementation of mojom::SocketBroker for use in tests within
// //services/network.
class TestSocketBrokerImpl : public SocketBrokerImpl {
 public:
  // Used to set whether a test connection should fail.
  void SetConnectionFailure(bool connection_failure);
};

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_TEST_SOCKET_BROKER_IMPL_H_
