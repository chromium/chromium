// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_QUIC_SIMPLE_TEST_SERVER_H_
#define NET_TEST_QUIC_SIMPLE_TEST_SERVER_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "net/base/host_port_pair.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "url/gurl.h"

namespace net {

class QuicSimpleTestServer {
 public:
  static bool Start();
  static void Shutdown();

  // Shuts down the server dispatcher, which results in sending ConnectionClose
  // frames to all connected clients.
  static void ShutdownDispatcherForTesting();

  // Add a response to `path`.
  static void AddResponse(const std::string& path,
                          quiche::HttpHeaderBlock response_headers,
                          const std::string& response_body);

  // Add a response to `path` with Early Hints.
  static void AddResponseWithEarlyHints(
      const std::string& path,
      const quiche::HttpHeaderBlock& response_headers,
      const std::string& response_body,
      const std::vector<quiche::HttpHeaderBlock>& early_hints);

  // Set a delay to `path`.
  static void SetResponseDelay(const std::string& path, base::TimeDelta delay);

  // Returns example.com
  static std::string const GetDomain();
  // Returns test.example.com
  static std::string const GetHost();
  // Returns port number of the server.
  static int GetPort();
  // Returns test.example.com:port
  static HostPortPair const GetHostPort();

  // Returns URL with host, port and file path, for example
  // https://test.example.com:12345/{file_path}
  static GURL GetFileURL(const std::string& file_path);

  static std::string const GetStatusHeaderName();

  // Server returns response with HTTP/2 headers and trailers. Does not include
  // |port| as it is resolved differently: https://test.example.com/hello.txt
  static GURL GetHelloURL();
  static std::string const GetHelloPath();
  static std::string const GetHelloBodyValue();
  static std::string const GetHelloStatus();
  static std::string const GetHelloHeaderName();
  static std::string const GetHelloHeaderValue();
  static std::string const GetCombinedHeaderName();
  static std::string const GetHelloTrailerName();
  static std::string const GetHelloTrailerValue();

  // Server returns response without HTTP/2 trailers.
  // https://test.example.com/simple.txt
  static GURL GetSimpleURL();
  static std::string const GetSimpleBodyValue();
  static std::string const GetSimpleStatus();
  static std::string const GetSimpleHeaderName();
  static std::string const GetSimpleHeaderValue();
};

}  // namespace net

#endif  // NET_TEST_QUIC_SIMPLE_TEST_SERVER_H_
