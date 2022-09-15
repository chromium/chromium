// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/x_frame_options_parser.h"

#include <string>
#include "net/http/http_response_headers.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const std::string test_data(reinterpret_cast<const char*>(data), size);
  std::string header_string("HTTP/1.1 200 OK\nX-Frame-Options: ");
  header_string += test_data;
  header_string += "\n\n";

  std::replace(header_string.begin(), header_string.end(), '\n', '\0');
  scoped_refptr<net::HttpResponseHeaders> headers =
      new net::HttpResponseHeaders(header_string);

  network::ParseXFrameOptions(*headers);
  return 0;
}
