// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_TEST_SOCKET_BROKER_IMPL_H_
#define SERVICES_NETWORK_TEST_TEST_SOCKET_BROKER_IMPL_H_

#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/base/address_family.h"
#include "services/network/public/mojom/socket_broker.mojom.h"

namespace network {

// Implementation of mojom::SocketBroker for use in tests within
// //services/network.
class TestSocketBrokerImpl : public network::mojom::SocketBroker {
 public:
  explicit TestSocketBrokerImpl();
  ~TestSocketBrokerImpl() override;

  TestSocketBrokerImpl(const TestSocketBrokerImpl&) = delete;
  TestSocketBrokerImpl& operator=(const TestSocketBrokerImpl&) = delete;

  // mojom::SocketBroker implementation.
  void CreateTcpSocket(net::AddressFamily address_family,
                       CreateTcpSocketCallback callback) override;

  void CreateUdpSocket(net::AddressFamily address_family,
                       CreateUdpSocketCallback callback) override;

  // Used to set whether a test connection should fail.
  void SetConnectionFailure(bool connection_failure) {
    connection_failure_ = connection_failure;
  }

 private:
  mojo::ReceiverSet<network::mojom::SocketBroker> receivers_;

  // When true, CreateTcpSocket returns ERR_CONNECTION_FAILED to test a failed
  // connection.
  bool connection_failure_ = false;
};

}  // namespace network
#endif  // SERVICES_NETWORK_TEST_TEST_SOCKET_BROKER_IMPL_H_
