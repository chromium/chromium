// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/webauthn_security_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace webauthn {

namespace {

TEST(WebAuthnSecurityUtilsTest, OriginAllowedToMakeWebAuthnRequests) {
  struct TestCase {
    const char* origin;
    ValidationStatus expected_status;
  } kTestCases[] = {
      {"https://example.com", ValidationStatus::kSuccess},
      {"https://foo.bar.net", ValidationStatus::kSuccess},
      {"http://localhost", ValidationStatus::kSuccess},
      {"isolated-app://"
       "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic",
       ValidationStatus::kSuccess},
      // Non-trustworthy HTTP
      {"http://example.com", ValidationStatus::kInvalidDomain},
      // IP addresses are disallowed.
      {"http://127.0.0.1", ValidationStatus::kInvalidDomain},
      {"https://127.0.0.1", ValidationStatus::kInvalidDomain},
      {"https://192.168.0.1", ValidationStatus::kInvalidDomain},
      // Invalid protocols
      {"file:///etc/passwd", ValidationStatus::kInvalidProtocol},
      {"chrome://settings", ValidationStatus::kInvalidProtocol},
      {"about:blank", ValidationStatus::kOpaqueDomain},
      // While Chrome does allow requests from Chrome extensions, this method
      // doesn't.
      {"chrome-extension://example", ValidationStatus::kInvalidProtocol},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.origin);
    EXPECT_EQ(OriginAllowedToMakeWebAuthnRequests(
                  url::Origin::Create(GURL(test_case.origin))),
              test_case.expected_status);
  }

  // Opaque origin
  EXPECT_EQ(OriginAllowedToMakeWebAuthnRequests(url::Origin()),
            ValidationStatus::kOpaqueDomain);
}

TEST(WebAuthnSecurityUtilsTest, OriginIsAllowedToClaimRelyingPartyId) {
  struct TestCase {
    const char* origin;
    const char* rp_id;
    bool expected_allowed;
  } kTestCases[] = {
      // Exact match
      {"https://example.com", "example.com", true},
      // Registrable suffix
      {"https://foo.example.com", "example.com", true},
      {"https://bar.foo.example.com", "example.com", true},
      // Not a suffix
      {"https://example.com", "google.com", false},
      {"https://notexample.com", "example.com", false},
      // Empty RP ID
      {"https://example.com", "", false},
      // Localhost
      {"http://localhost", "localhost", true},
      {"https://localhost", "localhost", true},
      {"https://localhost", "example.com", false},
      {"http://foo.localhost", "localhost", false},
      // IP addresses (not allowed to claim any RP ID)
      {"https://127.0.0.1", "127.0.0.1", false},
      // Registry controlled domains
      {"https://example.com", "com", false},
      // Disallowed origins
      {"http://example.com", "example.com", false},
      // Internal labels (disallowed by Chromium)
      {"https://login.awesomecompany", "awesomecompany", false},
      // IWAs can not claim any RP ID except via remoteDesktopClientOverride.
      {"isolated-app://"
       "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic",
       "awesomecompany", false},
      {"isolated-app://"
       "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic",
       "example.com", false},
      {"isolated-app://"
       "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic",
       "localhost", false},
      {"isolated-app://"
       "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic",
       "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic", false},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(std::string(test_case.origin) + " claiming " +
                 test_case.rp_id);
    EXPECT_EQ(OriginIsAllowedToClaimRelyingPartyId(
                  test_case.rp_id, url::Origin::Create(GURL(test_case.origin))),
              test_case.expected_allowed);
  }
}

}  // namespace

}  // namespace webauthn
