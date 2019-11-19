// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/ancestor_throttle.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "net/http/http_response_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using HeaderDisposition = AncestorThrottle::HeaderDisposition;

net::HttpResponseHeaders* GetAncestorHeaders(const char* xfo, const char* csp) {
  std::string header_string("HTTP/1.1 200 OK\nX-Frame-Options: ");
  header_string += xfo;
  if (csp != nullptr) {
    header_string += "\nContent-Security-Policy: ";
    header_string += csp;
  }
  header_string += "\n\n";
  std::replace(header_string.begin(), header_string.end(), '\n', '\0');
  net::HttpResponseHeaders* headers =
      new net::HttpResponseHeaders(header_string);
  EXPECT_TRUE(headers->HasHeader("X-Frame-Options"));
  if (csp != nullptr)
    EXPECT_TRUE(headers->HasHeader("Content-Security-Policy"));
  return headers;
}

}  // namespace

// AncestorThrottleTest
// -------------------------------------------------------------

class AncestorThrottleTest : public testing::Test {};

TEST_F(AncestorThrottleTest, ParsingXFrameOptions) {
  struct TestCase {
    const char* header;
    AncestorThrottle::HeaderDisposition expected;
    const char* value;
  } cases[] = {
      // Basic keywords
      {"DENY", HeaderDisposition::DENY, "DENY"},
      {"SAMEORIGIN", HeaderDisposition::SAMEORIGIN, "SAMEORIGIN"},
      {"ALLOWALL", HeaderDisposition::ALLOWALL, "ALLOWALL"},

      // Repeated keywords
      {"DENY,DENY", HeaderDisposition::DENY, "DENY, DENY"},
      {"SAMEORIGIN,SAMEORIGIN", HeaderDisposition::SAMEORIGIN,
       "SAMEORIGIN, SAMEORIGIN"},
      {"ALLOWALL,ALLOWALL", HeaderDisposition::ALLOWALL, "ALLOWALL, ALLOWALL"},

      // Case-insensitive
      {"deNy", HeaderDisposition::DENY, "deNy"},
      {"sAmEorIgIn", HeaderDisposition::SAMEORIGIN, "sAmEorIgIn"},
      {"AlLOWaLL", HeaderDisposition::ALLOWALL, "AlLOWaLL"},

      // Trim whitespace
      {" DENY", HeaderDisposition::DENY, "DENY"},
      {"SAMEORIGIN ", HeaderDisposition::SAMEORIGIN, "SAMEORIGIN"},
      {" ALLOWALL ", HeaderDisposition::ALLOWALL, "ALLOWALL"},
      {"   DENY", HeaderDisposition::DENY, "DENY"},
      {"SAMEORIGIN   ", HeaderDisposition::SAMEORIGIN, "SAMEORIGIN"},
      {"   ALLOWALL   ", HeaderDisposition::ALLOWALL, "ALLOWALL"},
      {" DENY , DENY ", HeaderDisposition::DENY, "DENY, DENY"},
      {"SAMEORIGIN,  SAMEORIGIN", HeaderDisposition::SAMEORIGIN,
       "SAMEORIGIN, SAMEORIGIN"},
      {"ALLOWALL  ,ALLOWALL", HeaderDisposition::ALLOWALL,
       "ALLOWALL, ALLOWALL"},
  };

  AncestorThrottle throttle(nullptr);
  for (const auto& test : cases) {
    SCOPED_TRACE(test.header);
    scoped_refptr<net::HttpResponseHeaders> headers =
        GetAncestorHeaders(test.header, nullptr);
    std::string header_value;
    EXPECT_EQ(test.expected,
              throttle.ParseHeader(headers.get(), &header_value));
    EXPECT_EQ(test.value, header_value);
  }
}

TEST_F(AncestorThrottleTest, ErrorsParsingXFrameOptions) {
  struct TestCase {
    const char* header;
    AncestorThrottle::HeaderDisposition expected;
    const char* failure;
  } cases[] = {
      // Empty == Invalid.
      {"", HeaderDisposition::INVALID, ""},

      // Invalid
      {"INVALID", HeaderDisposition::INVALID, "INVALID"},
      {"INVALID DENY", HeaderDisposition::INVALID, "INVALID DENY"},
      {"DENY DENY", HeaderDisposition::INVALID, "DENY DENY"},
      {"DE NY", HeaderDisposition::INVALID, "DE NY"},

      // Conflicts
      {"INVALID,DENY", HeaderDisposition::CONFLICT, "INVALID, DENY"},
      {"DENY,ALLOWALL", HeaderDisposition::CONFLICT, "DENY, ALLOWALL"},
      {"SAMEORIGIN,DENY", HeaderDisposition::CONFLICT, "SAMEORIGIN, DENY"},
      {"ALLOWALL,SAMEORIGIN", HeaderDisposition::CONFLICT,
       "ALLOWALL, SAMEORIGIN"},
      {"DENY,  SAMEORIGIN", HeaderDisposition::CONFLICT, "DENY, SAMEORIGIN"}};

  AncestorThrottle throttle(nullptr);
  for (const auto& test : cases) {
    SCOPED_TRACE(test.header);
    scoped_refptr<net::HttpResponseHeaders> headers =
        GetAncestorHeaders(test.header, nullptr);
    std::string header_value;
    EXPECT_EQ(test.expected,
              throttle.ParseHeader(headers.get(), &header_value));
    EXPECT_EQ(test.failure, header_value);
  }
}

TEST_F(AncestorThrottleTest, IgnoreWhenFrameAncestorsPresent) {
  struct TestCase {
    const char* csp;
    AncestorThrottle::HeaderDisposition expected;
  } cases[] = {
      {"", HeaderDisposition::DENY},
      {"frame-ancestors 'none'", HeaderDisposition::BYPASS},
      {"frame-ancestors *", HeaderDisposition::BYPASS},
      {"frame-ancestors 'self'", HeaderDisposition::BYPASS},
      {"frame-ancestors https://example.com", HeaderDisposition::BYPASS},
      {"fRaMe-AnCeStOrS *", HeaderDisposition::BYPASS},
      {"directive1; frame-ancestors 'none'", HeaderDisposition::BYPASS},
      {"directive1; frame-ancestors *", HeaderDisposition::BYPASS},
      {"directive1; frame-ancestors 'self'", HeaderDisposition::BYPASS},
      {"directive1; frame-ancestors https://example.com",
       HeaderDisposition::BYPASS},
      {"directive1; fRaMe-AnCeStOrS *", HeaderDisposition::BYPASS},
      {"policy, frame-ancestors 'none'", HeaderDisposition::BYPASS},
      {"policy, frame-ancestors *", HeaderDisposition::BYPASS},
      {"policy, frame-ancestors 'self'", HeaderDisposition::BYPASS},
      {"policy, frame-ancestors https://example.com",
       HeaderDisposition::BYPASS},
      {"policy, frame-ancestors 'none'", HeaderDisposition::BYPASS},
      {"policy, directive1; frame-ancestors *", HeaderDisposition::BYPASS},
      {"policy, directive1; frame-ancestors 'self'", HeaderDisposition::BYPASS},
      {"policy, directive1; frame-ancestors https://example.com",
       HeaderDisposition::BYPASS},
      {"policy, directive1; fRaMe-AnCeStOrS *", HeaderDisposition::BYPASS},
      {"policy, directive1; fRaMe-AnCeStOrS *", HeaderDisposition::BYPASS},

      {"not-frame-ancestors *", HeaderDisposition::DENY},
      {"frame-ancestors-are-lovely", HeaderDisposition::DENY},
      {"directive1; not-frame-ancestors *", HeaderDisposition::DENY},
      {"directive1; frame-ancestors-are-lovely", HeaderDisposition::DENY},
      {"policy, not-frame-ancestors *", HeaderDisposition::DENY},
      {"policy, frame-ancestors-are-lovely", HeaderDisposition::DENY},
      {"policy, directive1; not-frame-ancestors *", HeaderDisposition::DENY},
      {"policy, directive1; frame-ancestors-are-lovely",
       HeaderDisposition::DENY},
  };

  AncestorThrottle throttle(nullptr);
  for (const auto& test : cases) {
    SCOPED_TRACE(test.csp);
    scoped_refptr<net::HttpResponseHeaders> headers =
        GetAncestorHeaders("DENY", test.csp);
    std::string header_value;
    EXPECT_EQ(test.expected,
              throttle.ParseHeader(headers.get(), &header_value));
    EXPECT_EQ("DENY", header_value);
  }
}

}  // namespace content
