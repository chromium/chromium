// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/third_party_auth_config.h"

#include <sstream>

#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace key = ::policy::key;

TEST(ThirdPartyAuthConfig, ParseEmpty) {
  ThirdPartyAuthConfig third_party_auth_config;

  EXPECT_TRUE(
      ThirdPartyAuthConfig::ParseStrings("", "", "", &third_party_auth_config));
  EXPECT_TRUE(third_party_auth_config.is_null());
}

TEST(ThirdPartyAuthConfig, ParseValidAll) {
  ThirdPartyAuthConfig third_party_auth_config;

  EXPECT_TRUE(ThirdPartyAuthConfig::ParseStrings(
      "https://token.com/", "https://validation.com/", "certificate subject",
      &third_party_auth_config));
  EXPECT_FALSE(third_party_auth_config.is_null());
  EXPECT_EQ("https://token.com/", third_party_auth_config.token_url.spec());
  EXPECT_EQ("https://validation.com/",
            third_party_auth_config.token_validation_url.spec());
  EXPECT_EQ("certificate subject",
            third_party_auth_config.token_validation_cert_issuer);
}

TEST(ThirdPartyAuthConfig, ParseValidNoCert) {
  ThirdPartyAuthConfig third_party_auth_config;

  EXPECT_TRUE(ThirdPartyAuthConfig::ParseStrings("https://token.com/",
                                                 "https://validation.com/", "",
                                                 &third_party_auth_config));
  EXPECT_FALSE(third_party_auth_config.is_null());
  EXPECT_EQ("https://token.com/", third_party_auth_config.token_url.spec());
  EXPECT_EQ("https://validation.com/",
            third_party_auth_config.token_validation_url.spec());
  EXPECT_EQ("", third_party_auth_config.token_validation_cert_issuer);
}

// We validate https-vs-http only on Release builds to help with manual testing.
#if !defined(NDEBUG)
TEST(ThirdPartyAuthConfig, ParseHttp) {
  ThirdPartyAuthConfig third_party_auth_config;

  EXPECT_TRUE(ThirdPartyAuthConfig::ParseStrings("http://token.com/",
                                                 "http://validation.com/", "",
                                                 &third_party_auth_config));
  EXPECT_FALSE(third_party_auth_config.is_null());
  EXPECT_EQ("http://token.com/", third_party_auth_config.token_url.spec());
  EXPECT_EQ("http://validation.com/",
            third_party_auth_config.token_validation_url.spec());
}
#endif

namespace {

const std::string valid_url("https://valid.com");
const std::string valid_cert("certificate subject");

}  // namespace

class InvalidUrlTest : public ::testing::TestWithParam<const char*> {};

TEST_P(InvalidUrlTest, ParseInvalidUrl) {
  const std::string& invalid_url(GetParam());

  // Populate |config| with some known data - we will use this for validating
  // that |config| doesn't get modified by ParseStrings during subsequent
  // parsing
  // failure tests.
  ThirdPartyAuthConfig config;
  EXPECT_TRUE(ThirdPartyAuthConfig::ParseStrings(
      "https://token.com/", "https://validation.com/", "certificate subject",
      &config));

  // Test for parsing failure.
  EXPECT_FALSE(ThirdPartyAuthConfig::ParseStrings(invalid_url, valid_url,
                                                  valid_cert, &config));
  EXPECT_FALSE(ThirdPartyAuthConfig::ParseStrings(valid_url, invalid_url,
                                                  valid_cert, &config));
  EXPECT_FALSE(
      ThirdPartyAuthConfig::ParseStrings(invalid_url, "", "", &config));
  EXPECT_FALSE(
      ThirdPartyAuthConfig::ParseStrings("", invalid_url, "", &config));

  // Validate that ParseStrings doesn't modify its output upon parsing failure.
  EXPECT_EQ("https://token.com/", config.token_url.spec());
  EXPECT_EQ("https://validation.com/", config.token_validation_url.spec());
  EXPECT_EQ("certificate subject", config.token_validation_cert_issuer);
}

// We validate https-vs-http only on Release builds to help with manual testing.
#if defined(NDEBUG)
INSTANTIATE_TEST_SUITE_P(ThirdPartyAuthConfig,
                         InvalidUrlTest,
                         ::testing::Values("http://insecure.com",
                                           "I am not a URL"));
#else
INSTANTIATE_TEST_SUITE_P(ThirdPartyAuthConfig,
                         InvalidUrlTest,
                         ::testing::Values("I am not a URL"));
#endif

TEST(ThirdPartyAuthConfig, ParseInvalidCombination) {
  // Populate |config| with some known data - we will use this for validating
  // that |config| doesn't get modified by ParseStrings during subsequent
  // parsing
  // failure tests.
  ThirdPartyAuthConfig config;
  EXPECT_TRUE(ThirdPartyAuthConfig::ParseStrings(
      "https://token.com/", "https://validation.com/", "certificate subject",
      &config));

  // Only one of TokenUrl and TokenValidationUrl
  EXPECT_FALSE(
      ThirdPartyAuthConfig::ParseStrings("", valid_url, valid_cert, &config));
  EXPECT_FALSE(
      ThirdPartyAuthConfig::ParseStrings(valid_url, "", valid_cert, &config));

  // CertSubject when no TokenUrl and TokenValidationUrl
  EXPECT_FALSE(ThirdPartyAuthConfig::ParseStrings("", "", valid_cert, &config));

  // Validate that ParseStrings doesn't modify its output upon parsing failure.
  EXPECT_EQ("https://token.com/", config.token_url.spec());
  EXPECT_EQ("https://validation.com/", config.token_validation_url.spec());
  EXPECT_EQ("certificate subject", config.token_validation_cert_issuer);
}

TEST(ThirdPartyAuthConfig, ExtractEmpty) {
  base::Value::Dict dict;
  std::string url1, url2, cert;
  EXPECT_FALSE(ThirdPartyAuthConfig::ExtractStrings(dict, &url1, &url2, &cert));
}

TEST(ThirdPartyAuthConfig, ExtractUnknown) {
  base::Value::Dict dict;
  dict.Set("unknownName", "someValue");

  std::string url1, url2, cert;
  EXPECT_FALSE(ThirdPartyAuthConfig::ExtractStrings(dict, &url1, &url2, &cert));
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_IOS)
TEST(ThirdPartyAuthConfig, ExtractAll) {
  base::Value::Dict dict;
  dict.Set(key::kRemoteAccessHostTokenUrl, "test1");
  dict.Set(key::kRemoteAccessHostTokenValidationUrl, "test2");
  dict.Set(key::kRemoteAccessHostTokenValidationCertificateIssuer, "t3");

  std::string url1, url2, cert;
  EXPECT_TRUE(ThirdPartyAuthConfig::ExtractStrings(dict, &url1, &url2, &cert));
  EXPECT_EQ("test1", url1);
  EXPECT_EQ("test2", url2);
  EXPECT_EQ("t3", cert);
}

TEST(ThirdPartyAuthConfig, ExtractPartial) {
  base::Value::Dict dict;
  dict.Set(key::kRemoteAccessHostTokenValidationUrl, "test2");

  std::string url1, url2, cert;
  EXPECT_TRUE(ThirdPartyAuthConfig::ExtractStrings(dict, &url1, &url2, &cert));
  EXPECT_EQ("", url1);
  EXPECT_EQ("test2", url2);
  EXPECT_EQ("", cert);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS) &&
        // !BUILDFLAG(IS_IOS)

TEST(ThirdPartyAuthConfig, Output) {
  ThirdPartyAuthConfig third_party_auth_config;
  third_party_auth_config.token_url = GURL("https://token.com");
  third_party_auth_config.token_validation_url = GURL("https://validation.com");
  third_party_auth_config.token_validation_cert_issuer = "certificate subject";

  std::ostringstream str;
  str << third_party_auth_config;

  EXPECT_THAT(str.str(), testing::HasSubstr("token"));
  EXPECT_THAT(str.str(), testing::HasSubstr("validation"));
  EXPECT_THAT(str.str(), testing::HasSubstr("certificate subject"));
}

}  // namespace remoting
