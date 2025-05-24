// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>

#include <memory>
#include <string>

#include "net/base/network_isolation_key.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_challenge_tokenizer.h"
#include "net/http/http_auth_handler.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_auth_scheme.h"
#include "net/log/net_log_with_source.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider data_provider{data, size};

  std::string scheme;
  if (data_provider.ConsumeBool()) {
    scheme = std::string(data_provider.PickValueInArray(
        {net::kBasicAuthScheme, net::kDigestAuthScheme, net::kNtlmAuthScheme,
         net::kNegotiateAuthScheme}));
  } else {
    scheme = data_provider.ConsumeRandomLengthString(10);
  }
  std::unique_ptr<net::HttpAuthHandlerRegistryFactory> factory =
      net::HttpAuthHandlerFactory::CreateDefault();

  if (!factory->IsSchemeAllowedForTesting(scheme))
    return 0;

  std::string challenge = data_provider.ConsumeRandomLengthString(500);

  // Dummies
  net::SSLInfo null_ssl_info;
  url::SchemeHostPort scheme_host_port(GURL("https://foo.test/"));
  auto host_resolver = std::make_unique<net::MockHostResolver>();
  std::unique_ptr<net::HttpAuthHandler> handler;

  factory->CreateAuthHandlerFromString(
      challenge, net::HttpAuth::AUTH_SERVER, null_ssl_info,
      net::NetworkAnonymizationKey(), scheme_host_port, net::NetLogWithSource(),
      host_resolver.get(), &handler);

  if (handler) {
    auto followup = data_provider.ConsumeRemainingBytesAsString();
    net::HttpAuthChallengeTokenizer tokenizer{followup};
    handler->HandleAnotherChallenge(&tokenizer);
  }
  return 0;
}
