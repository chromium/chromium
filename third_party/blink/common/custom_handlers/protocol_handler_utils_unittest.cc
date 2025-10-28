// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/custom_handlers/protocol_handler_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/security/protocol_handler_security_level.h"
#include "url/gurl.h"

namespace blink {

TEST(ProtocolHandlerUtilTest, ValidCustomHandlerSyntax) {
  struct {
    const char* title;
    std::string user_url;
  } test_cases[]{

      {
          "Basic case",
          "https://mydomain.com/path?=%s",
      },
      {
          "Unknown scheme",
          "scheme://mydomain.com/path?=%s",
      },
      {
          "Not Secure Context",
          "http://mydomain.com/path?=%s",
      },
      {
          "Opaque Origin",
          "data:/%s",
      }};

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.title);
    GURL full_url(test_case.user_url);
    URLSyntaxErrorCode code = IsValidCustomHandlerURLSyntax(
        full_url, test_case.user_url, ProtocolHandlerSecurityLevel::kStrict);
    EXPECT_EQ(code, URLSyntaxErrorCode::kNoError);
  };
}

TEST(ProtocolHandlerUtilTest, InvalidCustomHandlerSyntax) {
  struct {
    const char* title;
    std::string user_url;
    URLSyntaxErrorCode expected_error;
  } test_cases[]{
      // Invalid url
      {
          "Empty url",
          "",
          URLSyntaxErrorCode::kInvalidUrl,
      },
      {
          "http://%s.com",
          "http://%s.com",
          URLSyntaxErrorCode::kInvalidUrl,
      },
      {
          "http://%s.example.com",
          "http://%s.example.com",
          URLSyntaxErrorCode::kInvalidUrl,
      },
      {
          "http://[v8.:::]//url=%s",
          "http://[v8.:::]//url=%s",
          URLSyntaxErrorCode::kInvalidUrl,
      },
      {
          "https://test:test/",
          "https://test:test/",
          URLSyntaxErrorCode::kInvalidUrl,
      },
      {
          "file://%s",
          "file://%s",
          URLSyntaxErrorCode::kInvalidUrl,
      },
      // Missing token
      {
          "http://example.com/%",
          "http://example.com/%",
          URLSyntaxErrorCode::kMissingToken,
      },
      {
          "http://example.com/%a",
          "http://example.com/%a",
          URLSyntaxErrorCode::kMissingToken,
      },
      {
          "http://example.com",
          "http://example.com",
          URLSyntaxErrorCode::kMissingToken,
      },
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.title);
    GURL full_url(test_case.user_url);
    URLSyntaxErrorCode code = IsValidCustomHandlerURLSyntax(
        full_url, test_case.user_url, ProtocolHandlerSecurityLevel::kStrict);
    EXPECT_EQ(code, test_case.expected_error);
  };
}

#if (BUILDFLAG(IS_WIN) && !defined(NDEBUG))
// Flaky on Windows
#define MAYBE_InvalidURL DISABLED_InvalidURL
#else
#define MAYBE_InvalidURL InvalidURL
#endif
TEST(ProtocolHandlerUtilTest, MAYBE_InvalidURL) {
  struct {
    const char* title;
    std::string user_url;
    URLSyntaxErrorCode expected_error;
  } test_cases[]{
      {
          "Empty url",
          "",
          URLSyntaxErrorCode::kInvalidUrl,
      },
      {
          "http://%s.com",
          "http://%s.com",
          URLSyntaxErrorCode::kInvalidUrl,
      },
      {
          "http://%s.example.com",
          "http://%s.example.com",
          URLSyntaxErrorCode::kInvalidUrl,
      },
      {
          "http://[v8.:::]//url=%s",
          "http://[v8.:::]//url=%s",
          URLSyntaxErrorCode::kInvalidUrl,
      },
      {
          "https://test:test/",
          "https://test:test/",
          URLSyntaxErrorCode::kInvalidUrl,
      },
      {
          "file://%s",
          "file://%s",
          URLSyntaxErrorCode::kInvalidUrl,
      },
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.title);
    GURL full_url(test_case.user_url);
    EXPECT_FALSE(full_url.is_valid());
    URLSyntaxErrorCode code = IsValidCustomHandlerURLSyntax(
        full_url, ProtocolHandlerSecurityLevel::kStrict);
    EXPECT_EQ(code, test_case.expected_error);
  };
}

TEST(ProtocolHandlerUtilTest, ValidCustomHandlerSchemeSafeList) {
  static constexpr const char* const kProtocolSafelist[] = {
      "bitcoin", "cabal",  "dat",    "did",  "doi",  "dweb", "ethereum",
      "geo",     "hyper",  "im",     "ipfs", "ipns", "irc",  "ircs",
      "magnet",  "mailto", "matrix", "mms",  "news", "nntp", "openpgp4fpr",
      "sip",     "sms",    "smsto",  "ssb",  "ssh",  "tel",  "urn",
      "webcal",  "wtai",   "xmpp"};
  for (const auto* scheme : kProtocolSafelist) {
    SCOPED_TRACE(std::string("Testing safelist: ") + scheme);
    bool has_prefix = true;
    EXPECT_TRUE(IsValidCustomHandlerScheme(
        scheme, ProtocolHandlerSecurityLevel::kStrict, &has_prefix));
    EXPECT_FALSE(has_prefix);
  };
}

TEST(ProtocolHandlerUtilTest, CustomHandlerSchemePrefixed) {
  struct {
    const char* title;
    std::string scheme;
    ProtocolHandlerSecurityLevel security_level;
    bool is_valid;
    bool has_prefix;
  } test_cases[]{
      // Valid cases
      {
          "Basic 'web+' case",
          "web+scheme",
          ProtocolHandlerSecurityLevel::kStrict,
          true,
          true,
      },
      {
          "Basic 'ext+' case",
          "ext+scheme",
          ProtocolHandlerSecurityLevel::kExtensionFeatures,
          true,
          true,
      },
      // Prefix ext+ not allowed by the security level
      {
          "Prefixed scheme not allowed with kStrict",
          "ext+scheme",
          ProtocolHandlerSecurityLevel::kStrict,
          false,
          false,
      },
      {
          "Prefixed scheme not allowed with kSameOrigin",
          "ext+search",
          ProtocolHandlerSecurityLevel::kSameOrigin,
          false,
          false,
      },
      {
          "Prefixed scheme not allowed with kUntrustedOrigin",
          "ext+protocol",
          ProtocolHandlerSecurityLevel::kUntrustedOrigins,
          false,
      },
      {
          "Prefix ext+ must have a non-empty sufix",
          "ext+",
          ProtocolHandlerSecurityLevel::kExtensionFeatures,
          false,
          true,
      },
      {
          "Prefix web+ must have a non-empty sufix",
          "web+",
          ProtocolHandlerSecurityLevel::kExtensionFeatures,
          false,
          true,
      },
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.title);
    bool has_prefix = false;
    bool is_valid = IsValidCustomHandlerScheme(
        test_case.scheme, test_case.security_level, &has_prefix);
    EXPECT_EQ(is_valid, test_case.is_valid);
    EXPECT_EQ(has_prefix, test_case.has_prefix);
  };
}

TEST(ProtocolHandlerUtilTest, CustomHandlerFTPIncludedInSafelist) {
  static constexpr const char* const kProtocolSafelist[] = {"ftp", "ftps",
                                                            "sftp"};
  for (const auto* scheme : kProtocolSafelist) {
    SCOPED_TRACE(std::string("Testing FTP schemes are included in safelist: ") +
                 scheme);
    EXPECT_TRUE(IsValidCustomHandlerScheme(
        scheme, ProtocolHandlerSecurityLevel::kStrict));
  };
}

TEST(ProtocolHandlerUtilTest, CustomHandlerPaytoNotIncludedInSafelist) {
  // The kSafelistPaytoToRegisterProtocolHandler feature is disabled by default.
  SCOPED_TRACE("Testing 'payto' is not included in safelist.");
  EXPECT_FALSE(IsValidCustomHandlerScheme(
      "payto", ProtocolHandlerSecurityLevel::kStrict));
}

TEST(ProtocolHandlerUtilTest, CustomHandlerURLIsAllowed) {
  struct {
    const char* title;
    std::string url;
    ProtocolHandlerSecurityLevel security_level;
    bool is_allowed;
  } test_cases[]{
      // Allowed URLs
      {
          "HTTPS scheme and Trusworthy url",
          "https://example.com",
          ProtocolHandlerSecurityLevel::kStrict,
          true,
      },
      {
          "File scheme with SameOrigin security level",
          "file://path",
          ProtocolHandlerSecurityLevel::kSameOrigin,
          true,
      },
      {
          "Data scheme with SameOrigin security level",
          "data://path",
          ProtocolHandlerSecurityLevel::kSameOrigin,
          true,
      },
      // Not allowed URLs
      {
          "HTTP schemes doesn't provide a Trusworthy url",
          "http://example.com",
          ProtocolHandlerSecurityLevel::kStrict,
          false,
      },
      {
          "File scheme with Strict securoty level",
          "file://path",
          ProtocolHandlerSecurityLevel::kStrict,
          false,
      },
      {
          "Data scheme with Strict securoty level",
          "data://path",
          ProtocolHandlerSecurityLevel::kStrict,
          false,
      },
  };
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.title);
    GURL url(test_case.url);
    bool is_allowed = IsAllowedCustomHandlerURL(url, test_case.security_level);
    EXPECT_EQ(is_allowed, test_case.is_allowed);
  };
}

TEST(ProtocolHandlerUtilTest, IsolatedAppFeaturesValidCustomScheme) {
  struct {
    std::string_view title;
    std::string_view scheme;
    bool allowed;
  } test_cases[]{
      {
          "Valid lowercase ASCII alphas",
          "myproto",
          true,
      },
      {
          "Valid mixed-case ASCII alphas",
          "PrOtOcOl",
          true,
      },
      {
          "Valid ASCII alphas with web+ prefix",
          "web+meow",
          true,
      },
      {
          "Valid scheme: ASCII alphas + dashes",
          "iwa-scheme",
          true,
      },
      {
          "Invalid scheme: numbers",
          "myproto3",
          false,
      },
      {
          "Invalid scheme: dots/dashes",
          "mypro.to3-sth",
          false,
      },
      {
          "Invalid scheme: empty",
          "",
          false,
      },
      {
          "Invalid scheme: one char",
          "C",
          false,
      },
      {
          "Invalid scheme: empty after web+",
          "web+",
          false,
      },
      {
          "Invalid scheme: empty chunk after dash",
          "meow-",
          false,
      },
      {
          "Invalid scheme: empty chunk between dashes",
          "meow--meow",
          false,
      },
      {
          "Invalid scheme: empty chunk before dash",
          "-meow",
          false,
      },
      {
          "Invalid scheme: two dashes and no ASCII alphas",
          "--",
          false,
      },
  };
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.title);
    bool allowed = IsValidCustomHandlerScheme(
        test_case.scheme, ProtocolHandlerSecurityLevel::kIsolatedAppFeatures);
    EXPECT_EQ(allowed, test_case.allowed);
  };
}

TEST(ProtocolHandlerUtilTest, IsolatedAppFeaturesValidCustomURLSyntax) {
  struct {
    std::string_view title;
    std::string_view url;
    bool allowed;
  } test_cases[]{
      {
          "Valid IWA URL with placeholder in query",
          "isolated-app://"
          "amoiebz32b7o24tilu257xne2yf3nkblkploanxzm7ebeglseqpfeaacai?params=%"
          "s",
          true,
      },
      {
          "IWA URL without %s",
          "isolated-app://"
          "amoiebz32b7o24tilu257xne2yf3nkblkploanxzm7ebeglseqpfeaacai",
          false,
      },
      {
          "IWA URL with non-query placeholder",
          "isolated-app://%s",
          false,
      },
      {
          "IWA URL with multiple placeholders",
          "isolated-app://"
          "amoiebz32b7o24tilu257xne2yf3nkblkploanxzm7ebeglseqpfeaacai?params=%"
          "s&other_params=%s",
          false,
      },
  };
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.title);
    bool allowed = IsValidCustomHandlerURLSyntax(
                       GURL(test_case.url),
                       ProtocolHandlerSecurityLevel::kIsolatedAppFeatures) ==
                   URLSyntaxErrorCode::kNoError;
    EXPECT_EQ(allowed, test_case.allowed);
  };
}

}  // namespace blink
