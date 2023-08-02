// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/credential_provider_extension/password_spec_fetcher.h"

#import "base/base64.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/test_timeouts.h"
#import "components/autofill/core/browser/proto/password_requirements.pb.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using autofill::PasswordRequirementsSpec_CharacterClass;
using autofill::PasswordRequirementsSpec;
using autofill::DomainSuggestions;

class PasswordSpecFetcherTest : public PlatformTest {};

// Tests a dummy proto can be parsed.
TEST_F(PasswordSpecFetcherTest, DomainSuggestionProtoIsParsed) {
  // "CgYyBBAAGAA=" is the base64 representation of a proto that looks like:
  //  {
  //    "passwordRequirements": {
  //      "lowerCase": {
  //        "min": 0,
  //        "max": 0
  //      }
  //    }
  //  }
  const char* string = "CgYyBBAAGAA=";
  std::string decoded;
  EXPECT_TRUE(base::Base64Decode(string, &decoded));
  DomainSuggestions suggestions;
  EXPECT_TRUE(suggestions.ParseFromString(decoded));

  EXPECT_TRUE(suggestions.has_password_requirements());
  EXPECT_TRUE(suggestions.password_requirements().has_lower_case());
  EXPECT_EQ(suggestions.password_requirements().lower_case().min(), 0u);
  EXPECT_EQ(suggestions.password_requirements().lower_case().max(), 0u);
  EXPECT_FALSE(suggestions.password_requirements().has_upper_case());
}

// Tests spec is a default one when fetching hasn't been done.
TEST_F(PasswordSpecFetcherTest, DefaultSpecNoFetch) {
  PasswordSpecFetcher* fetcher = [[PasswordSpecFetcher alloc] initWithHost:@""
                                                                    APIKey:@""];
  PasswordRequirementsSpec spec;
  EXPECT_EQ(fetcher.spec.SerializeAsString(), spec.SerializeAsString());
}

// Tests spec is a default one when fetching returns an invalid response.
TEST_F(PasswordSpecFetcherTest, DefaultSpecInvalidFetch) {
  // The missing host will have and invalid response.
  PasswordSpecFetcher* fetcher = [[PasswordSpecFetcher alloc] initWithHost:@""
                                                                    APIKey:@""];
  __block bool block_ran = false;
  [fetcher fetchSpecWithCompletion:^(autofill::PasswordRequirementsSpec spec) {
    EXPECT_EQ(fetcher.spec.SerializeAsString(), spec.SerializeAsString());
    block_ran = true;
  }];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), ^{
        return block_ran;
      }));
}
