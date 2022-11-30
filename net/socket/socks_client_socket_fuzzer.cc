// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include <memory>

#include "base/check_op.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/log/net_log.h"
#include "net/log/test_net_log.h"
#include "net/socket/fuzzed_socket.h"
#include "net/socket/socks_client_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

// Fuzzer for SocksClientSocket.  Only covers the SOCKS4 handshake.
//
// |data| is used to create a FuzzedSocket to fuzz reads and writes, see that
// class for details.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Including an observer; even though the recorded results aren't currently
  // used, it'll ensure the netlogging code is fuzzed as well.
  net::RecordingNetLogObserver net_log_observer;

  FuzzedDataProvider data_provider(data, size);

  // Determine if the DNS lookup returns synchronously or asynchronously,
  // succeeds or fails. Only returning an IPv4 address is fine, as SOCKS only
  // issues IPv4 requests.
  net::MockHostResolver mock_host_resolver;
  mock_host_resolver.set_synchronous_mode(data_provider.ConsumeBool());
  if (data_provider.ConsumeBool()) {
    mock_host_resolver.rules()->AddRule("*", "127.0.0.1");
  } else {
    mock_host_resolver.rules()->AddRule("*", net::ERR_NAME_NOT_RESOLVED);
  }

  net::TestCompletionCallback callback;
  auto fuzzed_socket =
      std::make_unique<net::FuzzedSocket>(&data_provider, net::NetLog::Get());
  CHECK_EQ(net::OK, fuzzed_socket->Connect(callback.callback()));

  net::SOCKSClientSocket socket(
      std::move(fuzzed_socket), net::HostPortPair("foo", 80),
      net::NetworkAnonymizationKey(), net::DEFAULT_PRIORITY,
      &mock_host_resolver, net::SecureDnsPolicy::kAllow,
      TRAFFIC_ANNOTATION_FOR_TESTS);
  int result = socket.Connect(callback.callback());
  callback.GetResult(result);
  return 0;
}
