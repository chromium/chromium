// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <memory>
#include <string>

#include "net/base/network_isolation_key.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_handler.h"
#include "net/http/http_auth_handler_basic.h"
#include "net/log/net_log_with_source.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string input(reinterpret_cast<const char*>(data), size);
  std::string challenge = "Basic " + input;

  // Dummies
  net::SSLInfo null_ssl_info;
  url::SchemeHostPort scheme_host_port(GURL("https://foo.test/"));
  auto host_resolver = std::make_unique<net::MockHostResolver>();
  std::unique_ptr<net::HttpAuthHandler> basic;

  net::HttpAuthHandlerBasic::Factory factory;
  factory.CreateAuthHandlerFromString(
      challenge, net::HttpAuth::AUTH_SERVER, null_ssl_info,
      net::NetworkAnonymizationKey(), scheme_host_port, net::NetLogWithSource(),
      host_resolver.get(), &basic);
  return 0;
}
