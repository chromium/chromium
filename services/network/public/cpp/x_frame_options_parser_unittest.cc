// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/x_frame_options_parser.h"

#include <string>
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/x_frame_options.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

net::HttpResponseHeaders* ConstructHeader(const char* value) {
  std::string header_string("HTTP/1.1 200 OK");
  if (value) {
    header_string += "\nX-Frame-Options: ";
    header_string += value;
  }
  header_string += "\n\n";

  std::replace(header_string.begin(), header_string.end(), '\n', '\0');
  net::HttpResponseHeaders* headers =
      new net::HttpResponseHeaders(header_string);

  return headers;
}

}  // namespace

namespace network {

TEST(XFrameOptionsTest, Parse) {
  struct TestCase {
    const char* header;
    mojom::XFrameOptionsValue expected;
  } cases[] = {
      // Single values:
      {nullptr, mojom::XFrameOptionsValue::kNone},
      {"DENY", mojom::XFrameOptionsValue::kDeny},
      {"SAMEORIGIN", mojom::XFrameOptionsValue::kSameOrigin},
      {"ALLOWALL", mojom::XFrameOptionsValue::kAllowAll},
      {"NOT-A-VALUE", mojom::XFrameOptionsValue::kInvalid},
      {"DeNy", mojom::XFrameOptionsValue::kDeny},
      {"SaMeOrIgIn", mojom::XFrameOptionsValue::kSameOrigin},
      {"AllOWaLL", mojom::XFrameOptionsValue::kAllowAll},

      // Repeated values:
      {"DENY,DENY", mojom::XFrameOptionsValue::kDeny},
      {"SAMEORIGIN,SAMEORIGIN", mojom::XFrameOptionsValue::kSameOrigin},
      {"ALLOWALL,ALLOWALL", mojom::XFrameOptionsValue::kAllowAll},
      {"DENY,DeNy", mojom::XFrameOptionsValue::kDeny},
      {"SAMEORIGIN,SaMeOrIgIn", mojom::XFrameOptionsValue::kSameOrigin},
      {"ALLOWALL,AllOWaLL", mojom::XFrameOptionsValue::kAllowAll},
      {"INVALID,INVALID", mojom::XFrameOptionsValue::kInvalid},
      {"INVALID,DIFFERENTLY-INVALID", mojom::XFrameOptionsValue::kInvalid},

      // Conflicting values:
      {"ALLOWALL,DENY", mojom::XFrameOptionsValue::kConflict},
      {"ALLOWALL,SAMEORIGIN", mojom::XFrameOptionsValue::kConflict},
      {"ALLOWALL,INVALID", mojom::XFrameOptionsValue::kConflict},
      {"DENY,ALLOWALL", mojom::XFrameOptionsValue::kConflict},
      {"DENY,SAMEORIGIN", mojom::XFrameOptionsValue::kConflict},
      {"DENY,INVALID", mojom::XFrameOptionsValue::kConflict},
      {"SAMEORIGIN,ALLOWALL", mojom::XFrameOptionsValue::kConflict},
      {"SAMEORIGIN,DENY", mojom::XFrameOptionsValue::kConflict},
      {"SAMEORIGIN,INVALID", mojom::XFrameOptionsValue::kConflict},
      {"INVALID,DENY", mojom::XFrameOptionsValue::kConflict},
      {"INVALID,SAMEORIGIN", mojom::XFrameOptionsValue::kConflict},
      {"INVALID,ALLOWALL", mojom::XFrameOptionsValue::kConflict},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(test.header);
    scoped_refptr<net::HttpResponseHeaders> headers =
        ConstructHeader(test.header);
    EXPECT_EQ(test.expected, ParseXFrameOptions(*headers));
  }
}

}  // namespace network
