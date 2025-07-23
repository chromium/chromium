// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/register_bound_session_payload.h"

#include "base/test/gmock_expected_support.h"
#include "base/test/values_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::base::test::ErrorIs;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::UnorderedElementsAre;

using Credential = ::RegisterBoundSessionPayload::Credential;
using ParserError = ::RegisterBoundSessionPayload::ParserError;
using Scope = ::RegisterBoundSessionPayload::Credential::Scope;

TEST(RegisterBoundSessionPayloadTest, ParseFromJsonSuccess) {
  const base::Value::Dict dict = base::test::ParseJsonDict(R"(
    {
      "session_identifier": "id",
      "credentials": [
        {
          "type": "cookie",
          "name": "__Secure-1PSIDTS",
          "scope": {
            "domain": ".youtube.com",
            "path": "/"
          }
        },
        {
          "type": "cookie",
          "name": "__Secure-3PSIDTS",
          "scope": {
            "domain": ".youtube.com",
            "path": "/"
          }
        }
      ],
      "refresh_url": "/RotateBoundCookies"
    }
  )");
  const base::expected<RegisterBoundSessionPayload, ParserError> payload =
      RegisterBoundSessionPayload::ParseFromJson(dict);
  ASSERT_TRUE(payload.has_value());
  EXPECT_THAT(
      *payload,
      AllOf(Field(&RegisterBoundSessionPayload::session_id, "id"),
            Field(&RegisterBoundSessionPayload::refresh_url,
                  "/RotateBoundCookies"),
            Field(&RegisterBoundSessionPayload::credentials,
                  UnorderedElementsAre(
                      AllOf(Field(&Credential::name, "__Secure-1PSIDTS"),
                            Field(&Credential::type, "cookie"),
                            Field(&Credential::scope,
                                  AllOf(Field(&Scope::domain, ".youtube.com"),
                                        Field(&Scope::path, "/")))),
                      AllOf(Field(&Credential::name, "__Secure-3PSIDTS"),
                            Field(&Credential::type, "cookie"),
                            Field(&Credential::scope,
                                  AllOf(Field(&Scope::domain, ".youtube.com"),
                                        Field(&Scope::path, "/"))))))));
}

TEST(RegisterBoundSessionPayloadTest, ParseFromJsonMissingSessionId) {
  const base::Value::Dict dict = base::test::ParseJsonDict(R"(
    {
      "credentials": [
        {
          "type": "cookie",
          "name": "__Secure-1PSIDTS",
          "scope": {
            "domain": ".youtube.com",
            "path": "/"
          }
        }
      ],
      "refresh_url": "/RotateBoundCookies"
    }
  )");
  const base::expected<RegisterBoundSessionPayload, ParserError> payload =
      RegisterBoundSessionPayload::ParseFromJson(dict);
  EXPECT_THAT(payload, ErrorIs(ParserError::kRequiredFieldMissing));
}

TEST(RegisterBoundSessionPayloadTest, ParseFromJsonMissingRefreshUrl) {
  const base::Value::Dict dict = base::test::ParseJsonDict(R"(
    {
      "session_identifier": "id",
      "credentials": [
        {
          "type": "cookie",
          "name": "__Secure-1PSIDTS",
          "scope": {
            "domain": ".youtube.com",
            "path": "/"
          }
        }
      ]
    }
  )");
  const base::expected<RegisterBoundSessionPayload, ParserError> payload =
      RegisterBoundSessionPayload::ParseFromJson(dict);
  EXPECT_THAT(payload, ErrorIs(ParserError::kRequiredFieldMissing));
}

TEST(RegisterBoundSessionPayloadTest, ParseFromJsonMissingCredentials) {
  const base::Value::Dict dict = base::test::ParseJsonDict(R"(
    {
      "session_identifier": "id",
      "refresh_url": "/RotateBoundCookies"
    }
  )");
  const base::expected<RegisterBoundSessionPayload, ParserError> payload =
      RegisterBoundSessionPayload::ParseFromJson(dict);
  EXPECT_THAT(payload, ErrorIs(ParserError::kRequiredFieldMissing));
}

TEST(RegisterBoundSessionPayloadTest, ParseFromJsonMissingCredentialName) {
  const base::Value::Dict dict = base::test::ParseJsonDict(R"(
    {
      "session_identifier": "id",
      "credentials": [
        {
          "type": "cookie",
          "scope": {
            "domain": ".youtube.com",
            "path": "/"
          }
        }
      ],
      "refresh_url": "/RotateBoundCookies"
    }
  )");
  const base::expected<RegisterBoundSessionPayload, ParserError> payload =
      RegisterBoundSessionPayload::ParseFromJson(dict);
  ASSERT_FALSE(payload.has_value());
  EXPECT_EQ(payload.error(), ParserError::kRequiredCredentialFieldMissing);
}

TEST(RegisterBoundSessionPayloadTest, ParseFromJsonMissingCredentialType) {
  const base::Value::Dict dict = base::test::ParseJsonDict(R"(
    {
      "session_identifier": "id",
      "credentials": [
        {
          "name": "__Secure-1PSIDTS",
          "scope": {
            "domain": ".youtube.com",
            "path": "/"
          }
        }
      ],
      "refresh_url": "/RotateBoundCookies"
    }
  )");
  const base::expected<RegisterBoundSessionPayload, ParserError> payload =
      RegisterBoundSessionPayload::ParseFromJson(dict);
  ASSERT_TRUE(payload.has_value());
  EXPECT_THAT(*payload,
              AllOf(Field(&RegisterBoundSessionPayload::session_id, "id"),
                    Field(&RegisterBoundSessionPayload::refresh_url,
                          "/RotateBoundCookies"),
                    Field(&RegisterBoundSessionPayload::credentials,
                          UnorderedElementsAre(AllOf(
                              Field(&Credential::name, "__Secure-1PSIDTS"),
                              Field(&Credential::scope,
                                    AllOf(Field(&Scope::domain, ".youtube.com"),
                                          Field(&Scope::path, "/"))))))));
}

TEST(RegisterBoundSessionPayloadTest, ParseFromJsonMissingCredentialScope) {
  const base::Value::Dict dict = base::test::ParseJsonDict(R"(
    {
      "session_identifier": "id",
      "credentials": [
        {
          "type": "cookie",
          "name": "__Secure-1PSIDTS"
        }
      ],
      "refresh_url": "/RotateBoundCookies"
    }
  )");
  const base::expected<RegisterBoundSessionPayload, ParserError> payload =
      RegisterBoundSessionPayload::ParseFromJson(dict);
  ASSERT_FALSE(payload.has_value());
  EXPECT_EQ(payload.error(), ParserError::kRequiredCredentialFieldMissing);
}

TEST(RegisterBoundSessionPayloadTest,
     ParseFromJsonMissingCredentialScopeDomain) {
  const base::Value::Dict dict = base::test::ParseJsonDict(R"(
    {
      "session_identifier": "id",
      "credentials": [
        {
          "type": "cookie",
          "name": "__Secure-1PSIDTS",
          "scope": {
            "path": "/"
          }
        }
      ],
      "refresh_url": "/RotateBoundCookies"
    }
  )");
  const base::expected<RegisterBoundSessionPayload, ParserError> payload =
      RegisterBoundSessionPayload::ParseFromJson(dict);
  ASSERT_FALSE(payload.has_value());
  EXPECT_EQ(payload.error(), ParserError::kRequiredCredentialFieldMissing);
}

TEST(RegisterBoundSessionPayloadTest, ParseFromJsonMissingCredentialScopePath) {
  const base::Value::Dict dict = base::test::ParseJsonDict(R"(
    {
      "session_identifier": "id",
      "credentials": [
        {
          "type": "cookie",
          "name": "__Secure-1PSIDTS",
          "scope": {
            "domain": ".youtube.com",
          }
        }
      ],
      "refresh_url": "/RotateBoundCookies"
    }
  )");
  const base::expected<RegisterBoundSessionPayload, ParserError> payload =
      RegisterBoundSessionPayload::ParseFromJson(dict);
  ASSERT_FALSE(payload.has_value());
  EXPECT_EQ(payload.error(), ParserError::kRequiredCredentialFieldMissing);
}

}  // namespace
