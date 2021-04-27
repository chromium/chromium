// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include <memory>

#include "base/check_op.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/log/test_net_log.h"
#include "net/socket/fuzzed_socket.h"
#include "net/socket/socks_client_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

// Fuzzer for SocksClientSocket.  Only covers the SOCKS4 handshake.
//
// |data| is used to create a FuzzedSocket to fuzz reads and writes, see that
// class for details.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Use a test NetLog, to exercise logging code.
  net::RecordingTestNetLog test_net_log;

  FuzzedDataProvider data_provider(data, size);

  // Determine if the DNS lookup returns synchronously or asynchronously,
  // succeeds or fails, and returns an IPv4 or IPv6 address.
  net::MockHostResolver mock_host_resolver;
  scoped_refptr<net::RuleBasedHostResolverProc> rules(
      new net::RuleBasedHostResolverProc(nullptr));
  mock_host_resolver.set_synchronous_mode(data_provider.ConsumeBool());
  switch (data_provider.ConsumeIntegralInRange(0, 2)) {
    case 0:
      rules->AddRule("*", "127.0.0.1");
      break;
    case 1:
      rules->AddRule("*", "::1");
      break;
    case 2:
      rules->AddSimulatedFailure("*");
      break;
  }
  mock_host_resolver.set_rules(rules.get());

  net::TestCompletionCallback callback;
  std::unique_ptr<net::FuzzedSocket> fuzzed_socket(
      new net::FuzzedSocket(&data_provider, &test_net_log));
  CHECK_EQ(net::OK, fuzzed_socket->Connect(callback.callback()));

  net::SOCKSClientSocket socket(
      std::move(fuzzed_socket), net::HostPortPair("foo", 80),
      net::NetworkIsolationKey(), net::DEFAULT_PRIORITY, &mock_host_resolver,
      net::SecureDnsPolicy::kAllow, TRAFFIC_ANNOTATION_FOR_TESTS);
  int result = socket.Connect(callback.callback());
  callback.GetResult(result);
  return 0;
}
