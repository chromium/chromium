// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request.h"

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include <memory>

#include "base/run_loop.h"
#include "net/base/request_priority.h"
#include "net/socket/fuzzed_socket_factory.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "url/gurl.h"


// Restrict max input length to reject too long inputs that can be too slow to
// process and may lead to an unbounded corpus growth.
const size_t kMaxInputSize = 65536 + 257;

// Integration fuzzer for URLRequest's handling of HTTP requests. Can follow
// redirects, both on the same server (using a new socket or the old one) and
// across servers.
// TODO(mmenke): Add support for testing HTTPS, auth, proxies, uploading,
// cancelation, deferring reads / redirects, using preconnected sockets, SPDY,
// QUIC, DNS failures (they all currently resolve to localhost), IPv6 DNS
// results, URLs with IPs instead of hostnames (v4 and v6), etc.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size > kMaxInputSize)
    return 0;

  FuzzedDataProvider data_provider(data, size);
  net::TestURLRequestContext url_request_context(true);
  net::FuzzedSocketFactory fuzzed_socket_factory(&data_provider);
  url_request_context.set_client_socket_factory(&fuzzed_socket_factory);
  url_request_context.Init();

  net::TestDelegate delegate;

  std::unique_ptr<net::URLRequest> url_request(
      url_request_context.CreateRequest(GURL("http://foo/"),
                                        net::DEFAULT_PRIORITY, &delegate,
                                        TRAFFIC_ANNOTATION_FOR_TESTS));
  url_request->Start();
  // TestDelegate quits the message loop on completion.
  base::RunLoop().Run();
  return 0;
}
