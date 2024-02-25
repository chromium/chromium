// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cross_origin_opener_policy_parser.h"

#include <string>
#include <string_view>

#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/cross_origin_opener_policy.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string_view test_data(reinterpret_cast<const char*>(data), size);
  std::string header_string = base::StrCat(
      {"HTTP/1.1 200 OK\nCross-Origin-Opener-Policy: ", test_data, "\n\n"});

  std::replace(header_string.begin(), header_string.end(), '\n', '\0');
  scoped_refptr<net::HttpResponseHeaders> headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(header_string);

  network::ParseCrossOriginOpenerPolicy(*headers);
  return 0;
}
