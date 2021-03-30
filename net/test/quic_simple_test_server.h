// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_QUIC_SIMPLE_TEST_SERVER_H_
#define NET_TEST_QUIC_SIMPLE_TEST_SERVER_H_

#include <string>
#include <vector>

#include "net/third_party/quiche/src/spdy/core/spdy_header_block.h"
#include "url/gurl.h"

namespace net {

class QuicSimpleTestServer {
 public:
  static bool Start();
  static void Shutdown();

  // Shuts down the server dispatcher, which results in sending ConnectionClose
  // frames to all connected clients.
  static void ShutdownDispatcherForTesting();

  // Add a response to `path` with Early Hints.
  static void AddResponseWithEarlyHints(
      const std::string& path,
      const spdy::Http2HeaderBlock& response_headers,
      const std::string& response_body,
      const std::vector<spdy::Http2HeaderBlock>& early_hints);

  // Returns example.com
  static const std::string GetDomain();
  // Returns test.example.com
  static const std::string GetHost();
  // Returns port number of the server.
  static int GetPort();
  // Returns test.example.com:port
  static const std::string GetHostPort();

  // Returns URL with host, port and file path, for example
  // https://test.example.com:12345/{file_path}
  static GURL GetFileURL(const std::string& file_path);

  static const std::string GetStatusHeaderName();

  // Server returns response with HTTP/2 headers and trailers. Does not include
  // |port| as it is resolved differently: https://test.example.com/hello.txt
  static GURL GetHelloURL();
  static const std::string GetHelloPath();
  static const std::string GetHelloBodyValue();
  static const std::string GetHelloStatus();
  static const std::string GetHelloHeaderName();
  static const std::string GetHelloHeaderValue();
  static const std::string GetCombinedHeaderName();
  static const std::string GetHelloTrailerName();
  static const std::string GetHelloTrailerValue();

  // Server returns response without HTTP/2 trailers.
  // https://test.example.com/simple.txt
  static GURL GetSimpleURL();
  static const std::string GetSimpleBodyValue();
  static const std::string GetSimpleStatus();
  static const std::string GetSimpleHeaderName();
  static const std::string GetSimpleHeaderValue();
};

}  // namespace net

#endif  // NET_TEST_QUIC_SIMPLE_TEST_SERVER_H_
