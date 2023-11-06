// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_proxy_client_socket.h"

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include <memory>
#include <string>

#include "base/check_op.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/address_list.h"
#include "net/base/auth.h"
#include "net/base/host_port_pair.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_auth_handler_basic.h"
#include "net/http/http_auth_handler_digest.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_auth_preferences.h"
#include "net/http/http_auth_scheme.h"
#include "net/log/net_log.h"
#include "net/log/test_net_log.h"
#include "net/socket/fuzzed_socket.h"
#include "net/socket/next_proto.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

// Fuzzer for HttpProxyClientSocket only tests establishing a connection when
// using the proxy as a tunnel.
//
// |data| is used to create a FuzzedSocket to fuzz reads and writes, see that
// class for details.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider data_provider(data, size);
  // Including an observer; even though the recorded results aren't currently
  // used, it'll ensure the netlogging code is fuzzed as well.
  net::RecordingNetLogObserver net_log_observer;

  net::TestCompletionCallback callback;
  auto fuzzed_socket =
      std::make_unique<net::FuzzedSocket>(&data_provider, net::NetLog::Get());
  CHECK_EQ(net::OK, fuzzed_socket->Connect(callback.callback()));

  // Create auth handler supporting basic and digest schemes.  Other schemes can
  // make system calls, which doesn't seem like a great idea.
  net::HttpAuthCache auth_cache(
      false /* key_server_entries_by_network_anonymization_key */);
  net::HttpAuthPreferences http_auth_preferences;
  http_auth_preferences.set_allowed_schemes(
      std::set<std::string>{net::kBasicAuthScheme, net::kDigestAuthScheme});
  net::HttpAuthHandlerRegistryFactory auth_handler_factory(
      &http_auth_preferences);

  scoped_refptr<net::HttpAuthController> auth_controller(
      base::MakeRefCounted<net::HttpAuthController>(
          net::HttpAuth::AUTH_PROXY, GURL("http://proxy:42/"),
          net::NetworkAnonymizationKey(), &auth_cache, &auth_handler_factory,
          nullptr));
  // Determine if the HttpProxyClientSocket should be told the underlying socket
  // is HTTPS.
  net::HttpProxyClientSocket socket(
      std::move(fuzzed_socket), "Bond/007", net::HostPortPair("foo", 80),
      net::ProxyChain(net::ProxyServer::SCHEME_HTTP,
                      net::HostPortPair("proxy", 42)),
      /*proxy_chain_index=*/0, auth_controller.get(),
      /*proxy_delegate=*/nullptr, TRAFFIC_ANNOTATION_FOR_TESTS);
  int result = socket.Connect(callback.callback());
  result = callback.GetResult(result);

  // Repeatedly try to log in with the same credentials.
  while (result == net::ERR_PROXY_AUTH_REQUESTED) {
    if (!auth_controller->HaveAuth()) {
      auth_controller->ResetAuth(net::AuthCredentials(u"user", u"pass"));
    }
    result = socket.RestartWithAuth(callback.callback());
    result = callback.GetResult(result);
  }

  return 0;
}
