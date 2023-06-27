// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/cors/cors.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"

namespace blink {

namespace {

class CorsExposedHeadersTest : public testing::Test {
 public:
  using CredentialsMode = network::mojom::CredentialsMode;

  HTTPHeaderSet Parse(CredentialsMode credentials_mode,
                      const char* header) const {
    return Parse(credentials_mode, AtomicString(header));
  }

  HTTPHeaderSet Parse(CredentialsMode credentials_mode,
                      const AtomicString& header) const {
    ResourceResponse response;
    response.AddHttpHeaderField(http_names::kAccessControlExposeHeaders,
                                header);

    return cors::ExtractCorsExposedHeaderNamesList(credentials_mode, response);
  }
};

TEST_F(CorsExposedHeadersTest, ValidInput) {
  EXPECT_EQ(Parse(CredentialsMode::kOmit, "valid"), HTTPHeaderSet({"valid"}));

  EXPECT_EQ(Parse(CredentialsMode::kOmit, "a,b"), HTTPHeaderSet({"a", "b"}));

  EXPECT_EQ(Parse(CredentialsMode::kOmit, "   a ,  b "),
            HTTPHeaderSet({"a", "b"}));

  EXPECT_EQ(Parse(CredentialsMode::kOmit, " \t   \t\t a"),
            HTTPHeaderSet({"a"}));

  EXPECT_EQ(Parse(CredentialsMode::kOmit, "a , "), HTTPHeaderSet({"a"}));
  EXPECT_EQ(Parse(CredentialsMode::kOmit, " , a"), HTTPHeaderSet({"a"}));
}

TEST_F(CorsExposedHeadersTest, DuplicatedEntries) {
  EXPECT_EQ(Parse(CredentialsMode::kOmit, "a, a"), HTTPHeaderSet{"a"});

  EXPECT_EQ(Parse(CredentialsMode::kOmit, "a, a, b"),
            HTTPHeaderSet({"a", "b"}));
}

TEST_F(CorsExposedHeadersTest, InvalidInput) {
  EXPECT_TRUE(Parse(CredentialsMode::kOmit, "not valid").empty());

  EXPECT_TRUE(Parse(CredentialsMode::kOmit, "///").empty());

  EXPECT_TRUE(Parse(CredentialsMode::kOmit, "/a/").empty());

  EXPECT_TRUE(Parse(CredentialsMode::kOmit, ",").empty());

  EXPECT_TRUE(Parse(CredentialsMode::kOmit, " , ").empty());

  EXPECT_TRUE(Parse(CredentialsMode::kOmit, "").empty());

  EXPECT_TRUE(Parse(CredentialsMode::kOmit, " ").empty());

  // U+0141 which is 'A' (0x41) + 0x100.
  EXPECT_TRUE(
      Parse(CredentialsMode::kOmit, AtomicString(String::FromUTF8("\xC5\x81")))
          .empty());
}

TEST_F(CorsExposedHeadersTest, WithEmptyElements) {
  EXPECT_EQ(Parse(CredentialsMode::kOmit, ", bb-8"), HTTPHeaderSet({"bb-8"}));

  EXPECT_EQ(Parse(CredentialsMode::kOmit, ", , , bb-8"),
            HTTPHeaderSet({"bb-8"}));

  EXPECT_EQ(Parse(CredentialsMode::kOmit, ", , , bb-8,"),
            HTTPHeaderSet({"bb-8"}));
}

TEST_F(CorsExposedHeadersTest, Wildcard) {
  ResourceResponse response;
  response.AddHttpHeaderField(http_names::kAccessControlExposeHeaders,
                              AtomicString("a, b, *"));
  response.AddHttpHeaderField(AtomicString("b"), AtomicString("-"));
  response.AddHttpHeaderField(AtomicString("c"), AtomicString("-"));
  response.AddHttpHeaderField(AtomicString("d"), AtomicString("-"));
  response.AddHttpHeaderField(AtomicString("*"), AtomicString("-"));

  EXPECT_EQ(
      cors::ExtractCorsExposedHeaderNamesList(CredentialsMode::kOmit, response),
      HTTPHeaderSet({"Access-Control-Expose-Headers", "b", "c", "d", "*"}));

  EXPECT_EQ(
      cors::ExtractCorsExposedHeaderNamesList(CredentialsMode::kSameOrigin,
                                              response),
      HTTPHeaderSet({"Access-Control-Expose-Headers", "b", "c", "d", "*"}));
}

TEST_F(CorsExposedHeadersTest, Asterisk) {
  ResourceResponse response;
  response.AddHttpHeaderField(http_names::kAccessControlExposeHeaders,
                              AtomicString("a, b, *"));
  response.AddHttpHeaderField(AtomicString("b"), AtomicString("-"));
  response.AddHttpHeaderField(AtomicString("c"), AtomicString("-"));
  response.AddHttpHeaderField(AtomicString("d"), AtomicString("-"));
  response.AddHttpHeaderField(AtomicString("*"), AtomicString("-"));

  EXPECT_EQ(cors::ExtractCorsExposedHeaderNamesList(CredentialsMode::kInclude,
                                                    response),
            HTTPHeaderSet({"a", "b", "*"}));
}

}  // namespace

}  // namespace blink
