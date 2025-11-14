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
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

using Credential = ::RegisterBoundSessionPayload::Credential;
using ParserError = ::RegisterBoundSessionPayload::ParserError;
using Scope = ::RegisterBoundSessionPayload::Scope;

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
      RegisterBoundSessionPayload::ParseFromJson(
          dict, /*parse_for_dbsc_standard=*/false);
  ASSERT_OK(payload);
  EXPECT_THAT(
      *payload,
      AllOf(
          Field(&RegisterBoundSessionPayload::parsed_for_dbsc_standard, false),
          Field(&RegisterBoundSessionPayload::session_id, "id"),
          Field(&RegisterBoundSessionPayload::refresh_url,
                "/RotateBoundCookies"),
          Field(&RegisterBoundSessionPayload::credentials,
                UnorderedElementsAre(
                    AllOf(Field(&Credential::name, "__Secure-1PSIDTS"),
                          Field(&Credential::scope,
                                AllOf(Field(&Scope::domain, ".youtube.com"),
                                      Field(&Scope::path, "/")))),
                    AllOf(Field(&Credential::name, "__Secure-3PSIDTS"),
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
      RegisterBoundSessionPayload::ParseFromJson(
          dict, /*parse_for_dbsc_standard=*/false);
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
      RegisterBoundSessionPayload::ParseFromJson(
          dict, /*parse_for_dbsc_standard=*/false);
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
      RegisterBoundSessionPayload::ParseFromJson(
          dict, /*parse_for_dbsc_standard=*/false);
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
      RegisterBoundSessionPayload::ParseFromJson(
          dict, /*parse_for_dbsc_standard=*/false);
  EXPECT_THAT(payload, ErrorIs(ParserError::kRequiredCredentialFieldMissing));
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
      RegisterBoundSessionPayload::ParseFromJson(
          dict, /*parse_for_dbsc_standard=*/false);
  ASSERT_OK(payload);
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
      RegisterBoundSessionPayload::ParseFromJson(
          dict, /*parse_for_dbsc_standard=*/false);
  EXPECT_THAT(payload, ErrorIs(ParserError::kRequiredCredentialFieldMissing));
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
      RegisterBoundSessionPayload::ParseFromJson(
          dict, /*parse_for_dbsc_standard=*/false);
  EXPECT_THAT(payload, ErrorIs(ParserError::kRequiredScopeFieldMissing));
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
      RegisterBoundSessionPayload::ParseFromJson(
          dict, /*parse_for_dbsc_standard=*/false);
  EXPECT_THAT(payload, ErrorIs(ParserError::kRequiredScopeFieldMissing));
}

TEST(RegisterBoundSessionPayloadTest, ParseFromJsonStandardFormatSuccess) {
  const base::Value::Dict dict = base::test::ParseJsonDict(R"(
    {
      "session_identifier": "session_id",
      "refresh_url": "/refresh_url",
      "scope": {
        "origin": "https://a.test",
        "include_site": true,
        "scope_specification" : [
          {
            "type": "include",
            "domain": "trusted.a.test",
            "path": "/only_trusted_path"
          }
        ]
      },
      "credentials": [{
        "type": "cookie",
        "name": "auth_cookie",
        "attributes": "Domain=a.test; Path=/; Secure; SameSite=None"
      }],
      "allowed_refresh_initiators": ["https://a.test"]
    }
  )");
  const base::expected<RegisterBoundSessionPayload, ParserError> payload =
      RegisterBoundSessionPayload::ParseFromJson(
          dict, /*parse_for_dbsc_standard=*/true);
  ASSERT_OK(payload);
  EXPECT_THAT(
      *payload,
      AllOf(
          Field(&RegisterBoundSessionPayload::parsed_for_dbsc_standard, true),
          Field(&RegisterBoundSessionPayload::session_id, "session_id"),
          Field(&RegisterBoundSessionPayload::refresh_url, "/refresh_url"),
          Field(
              &RegisterBoundSessionPayload::scope,
              AllOf(
                  Field(&RegisterBoundSessionPayload::SessionScope::origin,
                        "https://a.test"),
                  Field(
                      &RegisterBoundSessionPayload::SessionScope::include_site,
                      true),
                  Field(&RegisterBoundSessionPayload::SessionScope::
                            specifications,
                        UnorderedElementsAre(AllOf(
                            Field(&Scope::type, Scope::Type::kInclude),
                            Field(&Scope::domain, "trusted.a.test"),
                            Field(&Scope::path, "/only_trusted_path")))))),
          Field(&RegisterBoundSessionPayload::credentials,
                UnorderedElementsAre(AllOf(
                    Field(&Credential::name, "auth_cookie"),
                    Field(&Credential::type, "cookie"),
                    Field(&Credential::attributes,
                          "Domain=a.test; Path=/; Secure; SameSite=None")))),
          Field(&RegisterBoundSessionPayload::allowed_refresh_initiators,
                UnorderedElementsAre("https://a.test"))));
}

TEST(RegisterBoundSessionPayloadTest,
     ParseFromJsonStandardFormatSessionScopeOriginMissing) {
  const base::Value::Dict dict = base::test::ParseJsonDict(R"(
    {
      "session_identifier": "session_id",
      "refresh_url": "/refresh_url",
      "scope": {
        "include_site": true,
        "scope_specification" : [
          {
            "type": "include",
            "domain": "trusted.a.test",
            "path": "/only_trusted_path"
          }
        ]
      },
      "credentials": [{
        "type": "cookie",
        "name": "auth_cookie",
        "attributes": "Domain=a.test; Path=/; Secure; SameSite=None"
      }]
    }
  )");
  const base::expected<RegisterBoundSessionPayload, ParserError> payload =
      RegisterBoundSessionPayload::ParseFromJson(
          dict, /*parse_for_dbsc_standard=*/true);
  ASSERT_OK(payload);
  EXPECT_THAT(
      *payload,
      Field(&RegisterBoundSessionPayload::scope,
            Field(&RegisterBoundSessionPayload::SessionScope::origin, "")));
}

TEST(RegisterBoundSessionPayloadTest,
     ParseFromJsonStandardFormatSessionScopeSpecificationMissing) {
  const base::Value::Dict dict = base::test::ParseJsonDict(R"(
    {
      "session_identifier": "session_id",
      "refresh_url": "/refresh_url",
      "scope": {
        "origin": "https://a.test",
        "include_site": true
      },
      "credentials": [{
        "type": "cookie",
        "name": "auth_cookie",
        "attributes": "Domain=a.test; Path=/; Secure; SameSite=None"
      }]
    }
  )");
  const base::expected<RegisterBoundSessionPayload, ParserError> payload =
      RegisterBoundSessionPayload::ParseFromJson(
          dict, /*parse_for_dbsc_standard=*/true);
  ASSERT_OK(payload);
  EXPECT_THAT(
      *payload,
      Field(&RegisterBoundSessionPayload::scope,
            Field(&RegisterBoundSessionPayload::SessionScope::specifications,
                  IsEmpty())));
}

TEST(RegisterBoundSessionPayloadTest,
     ParseFromJsonStandardFormatSessionScopeSpecificationDomainMissing) {
  const base::Value::Dict dict = base::test::ParseJsonDict(R"(
    {
      "session_identifier": "session_id",
      "refresh_url": "/refresh_url",
      "scope": {
        "origin": "https://a.test",
        "include_site": true,
        "scope_specification" : [
          {
            "type": "include",
            "path": "/only_trusted_path"
          }
        ]
      },
      "credentials": [{
        "type": "cookie",
        "name": "auth_cookie",
        "attributes": "Domain=a.test; Path=/; Secure; SameSite=None"
      }]
    }
  )");
  const base::expected<RegisterBoundSessionPayload, ParserError> payload =
      RegisterBoundSessionPayload::ParseFromJson(
          dict, /*parse_for_dbsc_standard=*/true);
  ASSERT_OK(payload);
  EXPECT_THAT(
      *payload,
      Field(&RegisterBoundSessionPayload::scope,
            Field(&RegisterBoundSessionPayload::SessionScope::specifications,
                  UnorderedElementsAre(Field(&Scope::domain, "*")))));
}

TEST(RegisterBoundSessionPayloadTest,
     ParseFromJsonStandardFormatSessionScopeSpecificationPathMissing) {
  const base::Value::Dict dict = base::test::ParseJsonDict(R"(
    {
      "session_identifier": "session_id",
      "refresh_url": "/refresh_url",
      "scope": {
        "origin": "https://a.test",
        "include_site": true,
        "scope_specification" : [
          {
            "type": "include",
            "domain": "trusted.a.test"
          }
        ]
      },
      "credentials": [{
        "type": "cookie",
        "name": "auth_cookie",
        "attributes": "Domain=a.test; Path=/; Secure; SameSite=None"
      }]
    }
  )");
  const base::expected<RegisterBoundSessionPayload, ParserError> payload =
      RegisterBoundSessionPayload::ParseFromJson(
          dict, /*parse_for_dbsc_standard=*/true);
  ASSERT_OK(payload);
  EXPECT_THAT(
      *payload,
      Field(&RegisterBoundSessionPayload::scope,
            Field(&RegisterBoundSessionPayload::SessionScope::specifications,
                  UnorderedElementsAre(Field(&Scope::path, "/")))));
}

TEST(RegisterBoundSessionPayloadTest,
     ParseFromJsonStandardFormatSuccessSessionScopeIncludeSiteMissing) {
  const base::Value::Dict dict = base::test::ParseJsonDict(R"(
    {
      "session_identifier": "session_id",
      "refresh_url": "/refresh_url",
      "scope": {
        "origin": "https://a.test",
        "scope_specification" : [
          {
            "type": "include",
            "domain": "trusted.a.test",
            "path": "/only_trusted_path"
          }
        ]
      },
      "credentials": [{
        "type": "cookie",
        "name": "auth_cookie",
        "attributes": "Domain=a.test; Path=/; Secure; SameSite=None"
      }]
    }
  )");
  const base::expected<RegisterBoundSessionPayload, ParserError> payload =
      RegisterBoundSessionPayload::ParseFromJson(
          dict, /*parse_for_dbsc_standard=*/true);
  ASSERT_OK(payload);
  EXPECT_THAT(
      *payload,
      Field(&RegisterBoundSessionPayload::scope,
            Field(&RegisterBoundSessionPayload::SessionScope::include_site,
                  false)));
}

TEST(RegisterBoundSessionPayloadTest,
     ParseFromJsonStandardFormatSuccessCredentialAttributesMissing) {
  const base::Value::Dict dict = base::test::ParseJsonDict(R"(
    {
      "session_identifier": "session_id",
      "refresh_url": "/refresh_url",
      "scope": {
        "origin": "https://a.test",
        "include_site": true,
        "scope_specification" : [
          {
            "type": "include",
            "domain": "trusted.a.test",
            "path": "/only_trusted_path"
          }
        ]
      },
      "credentials": [{
        "type": "cookie",
        "name": "auth_cookie"
      }]
    }
  )");
  const base::expected<RegisterBoundSessionPayload, ParserError> payload =
      RegisterBoundSessionPayload::ParseFromJson(
          dict, /*parse_for_dbsc_standard=*/true);
  ASSERT_OK(payload);
  EXPECT_THAT(*payload,
              Field(&RegisterBoundSessionPayload::credentials,
                    UnorderedElementsAre(Field(&Credential::attributes, ""))));
}

TEST(RegisterBoundSessionPayloadTest,
     ParseFromJsonStandardFormatSuccessRefreshInitiatorsMissing) {
  const base::Value::Dict dict = base::test::ParseJsonDict(R"(
    {
      "session_identifier": "session_id",
      "refresh_url": "/refresh_url",
      "scope": {
        "origin": "https://a.test",
        "include_site": true,
        "scope_specification" : [
          {
            "type": "include",
            "domain": "trusted.a.test",
            "path": "/only_trusted_path"
          }
        ]
      },
      "credentials": [{
        "type": "cookie",
        "name": "auth_cookie",
        "attributes": "Domain=a.test; Path=/; Secure; SameSite=None"
      }]
    }
  )");
  const base::expected<RegisterBoundSessionPayload, ParserError> payload =
      RegisterBoundSessionPayload::ParseFromJson(
          dict, /*parse_for_dbsc_standard=*/true);
  ASSERT_OK(payload);
  EXPECT_THAT(*payload,
              Field(&RegisterBoundSessionPayload::allowed_refresh_initiators,
                    IsEmpty()));
}

TEST(RegisterBoundSessionPayloadTest,
     ParseFromJsonStandardFormatSessionScopeMissing) {
  const base::Value::Dict dict = base::test::ParseJsonDict(R"(
    {
      "session_identifier": "session_id",
      "refresh_url": "/refresh_url",
      "credentials": [{
        "type": "cookie",
        "name": "auth_cookie",
        "attributes": "Domain=a.test; Path=/; Secure; SameSite=None"
      }]
    }
  )");
  const base::expected<RegisterBoundSessionPayload, ParserError> payload =
      RegisterBoundSessionPayload::ParseFromJson(
          dict, /*parse_for_dbsc_standard=*/true);
  EXPECT_THAT(payload, ErrorIs(ParserError::kRequiredFieldMissing));
}

TEST(RegisterBoundSessionPayloadTest,
     ParseFromJsonStandardFormatSessionScopeMalformed) {
  const base::Value::Dict dict = base::test::ParseJsonDict(R"(
    {
      "session_identifier": "session_id",
      "refresh_url": "/refresh_url",
      "scope": {
        "origin": "https://a.test",
        "include_site": true,
        "scope_specification" : ["malformed"]
      },
      "credentials": [{
        "type": "cookie",
        "name": "auth_cookie",
        "attributes": "Domain=a.test; Path=/; Secure; SameSite=None"
      }]
    }
  )");
  const base::expected<RegisterBoundSessionPayload, ParserError> payload =
      RegisterBoundSessionPayload::ParseFromJson(
          dict, /*parse_for_dbsc_standard=*/true);
  EXPECT_THAT(payload,
              ErrorIs(ParserError::kMalformedSessionScopeSpecification));
}

TEST(RegisterBoundSessionPayloadTest,
     ParseFromJsonStandardFormatSessionScopeSpecificationInvalidType) {
  const base::Value::Dict dict = base::test::ParseJsonDict(R"(
    {
      "session_identifier": "session_id",
      "refresh_url": "/refresh_url",
      "scope": {
        "origin": "https://a.test",
        "include_site": true,
        "scope_specification" : [
          {
            "type": "inv4lid",
            "domain": "trusted.a.test",
            "path": "/only_trusted_path"
          }
        ]
      },
      "credentials": [{
        "type": "cookie",
        "name": "auth_cookie",
        "attributes": "Domain=a.test; Path=/; Secure; SameSite=None"
      }]
    }
  )");
  const base::expected<RegisterBoundSessionPayload, ParserError> payload =
      RegisterBoundSessionPayload::ParseFromJson(
          dict, /*parse_for_dbsc_standard=*/true);
  EXPECT_THAT(payload, ErrorIs(ParserError::kInvalidScopeType));
}

TEST(RegisterBoundSessionPayloadTest,
     ParseFromJsonStandardFormatCredentialsMissing) {
  const base::Value::Dict dict = base::test::ParseJsonDict(R"(
    {
      "session_identifier": "session_id",
      "refresh_url": "/refresh_url",
      "scope": {
        "origin": "https://a.test",
        "include_site": true,
        "scope_specification" : [
          {
            "type": "include",
            "domain": "trusted.a.test",
            "path": "/only_trusted_path"
          }
        ]
      }
    }
  )");
  const base::expected<RegisterBoundSessionPayload, ParserError> payload =
      RegisterBoundSessionPayload::ParseFromJson(
          dict, /*parse_for_dbsc_standard=*/true);
  EXPECT_THAT(payload, ErrorIs(ParserError::kRequiredFieldMissing));
}

TEST(RegisterBoundSessionPayloadTest,
     ParseFromJsonStandardFormatCredentialTypeMissing) {
  const base::Value::Dict dict = base::test::ParseJsonDict(R"(
    {
      "session_identifier": "session_id",
      "refresh_url": "/refresh_url",
      "scope": {
        "origin": "https://a.test",
        "include_site": true,
        "scope_specification" : [
          {
            "type": "include",
            "domain": "trusted.a.test",
            "path": "/only_trusted_path"
          }
        ]
      },
      "credentials": [{
        "name": "auth_cookie",
        "attributes": "Domain=a.test; Path=/; Secure; SameSite=None"
      }]
    }
  )");
  const base::expected<RegisterBoundSessionPayload, ParserError> payload =
      RegisterBoundSessionPayload::ParseFromJson(
          dict, /*parse_for_dbsc_standard=*/true);
  EXPECT_THAT(payload, ErrorIs(ParserError::kRequiredCredentialFieldMissing));
}

TEST(RegisterBoundSessionPayloadTest,
     ParseFromJsonStandardFormatCredentialTypeInvalid) {
  const base::Value::Dict dict = base::test::ParseJsonDict(R"(
    {
      "session_identifier": "session_id",
      "refresh_url": "/refresh_url",
      "scope": {
        "origin": "https://a.test",
        "include_site": true,
        "scope_specification" : [
          {
            "type": "include",
            "domain": "trusted.a.test",
            "path": "/only_trusted_path"
          }
        ]
      },
      "credentials": [{
        "type": "invalid",
        "name": "auth_cookie",
        "attributes": "Domain=a.test; Path=/; Secure; SameSite=None"
      }]
    }
  )");
  const base::expected<RegisterBoundSessionPayload, ParserError> payload =
      RegisterBoundSessionPayload::ParseFromJson(
          dict, /*parse_for_dbsc_standard=*/true);
  EXPECT_THAT(payload, ErrorIs(ParserError::kInvalidCredentialType));
}

TEST(RegisterBoundSessionPayloadTest,
     ParseFromJsonStandardFormatCredentialNameMissing) {
  const base::Value::Dict dict = base::test::ParseJsonDict(R"(
    {
      "session_identifier": "session_id",
      "refresh_url": "/refresh_url",
      "scope": {
        "origin": "https://a.test",
        "include_site": true,
        "scope_specification" : [
          {
            "type": "include",
            "domain": "trusted.a.test",
            "path": "/only_trusted_path"
          }
        ]
      },
      "credentials": [{
        "type": "cookie",
        "attributes": "Domain=a.test; Path=/; Secure; SameSite=None"
      }]
    }
  )");
  const base::expected<RegisterBoundSessionPayload, ParserError> payload =
      RegisterBoundSessionPayload::ParseFromJson(
          dict, /*parse_for_dbsc_standard=*/true);
  EXPECT_THAT(payload, ErrorIs(ParserError::kRequiredCredentialFieldMissing));
}

TEST(RegisterBoundSessionPayloadTest,
     ParseFromJsonStandardFormatRefreshInitiatorsMalformed) {
  const base::Value::Dict dict = base::test::ParseJsonDict(R"(
    {
      "session_identifier": "session_id",
      "refresh_url": "/refresh_url",
      "scope": {
        "origin": "https://a.test",
        "include_site": true,
        "scope_specification" : [
          {
            "type": "include",
            "domain": "trusted.a.test",
            "path": "/only_trusted_path"
          }
        ]
      },
      "credentials": [{
        "type": "cookie",
        "name": "auth_cookie",
        "attributes": "Domain=a.test; Path=/; Secure; SameSite=None"
      }],
      "allowed_refresh_initiators": [1]
    }
  )");
  const base::expected<RegisterBoundSessionPayload, ParserError> payload =
      RegisterBoundSessionPayload::ParseFromJson(
          dict, /*parse_for_dbsc_standard=*/true);
  EXPECT_THAT(payload, ErrorIs(ParserError::kMalformedRefreshInitiator));
}

}  // namespace
