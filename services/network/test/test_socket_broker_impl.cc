// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_socket_broker_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "net/base/net_errors.h"

namespace network {

void TestSocketBrokerImpl::SetConnectionFailure(bool connection_failure) {
  if (connection_failure) {
    set_socket_creation_interceptor_for_testing(
        base::BindRepeating([] -> int { return net::ERR_CONNECTION_FAILED; }));
  } else {
    set_socket_creation_interceptor_for_testing(SocketCreationInterceptor());
  }
}

}  // namespace network
