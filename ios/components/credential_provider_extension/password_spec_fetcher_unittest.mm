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

using autofill::DomainSuggestions;
using autofill::PasswordRequirementsSpec;
using autofill::PasswordRequirementsSpec_CharacterClass;

@interface PasswordSpecFetcher (Testing)
- (void)onReceivedData:(NSData*)data
              response:(NSURLResponse*)response
                 error:(NSError*)error;
@end

namespace {

// Returns the base64-encoded body that the fetcher expects, wrapping `spec` in
// a `DomainSuggestions` message.
NSData* EncodedResponseBodyForSpec(const PasswordRequirementsSpec& spec) {
  DomainSuggestions suggestions;
  *suggestions.mutable_password_requirements() = spec;
  std::string encoded = base::Base64Encode(suggestions.SerializeAsString());
  return [NSData dataWithBytes:encoded.data() length:encoded.size()];
}

}  // namespace

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

// Tests that a valid response body is stored as the spec.
TEST_F(PasswordSpecFetcherTest, ValidResponseStoredAsSpec) {
  PasswordSpecFetcher* fetcher = [[PasswordSpecFetcher alloc] initWithHost:@""
                                                                    APIKey:@""];
  PasswordRequirementsSpec spec;
  spec.set_max_length(12u);
  spec.mutable_lower_case()->set_min(2u);
  [fetcher onReceivedData:EncodedResponseBodyForSpec(spec)
                 response:nil
                    error:nil];
  EXPECT_EQ(12u, fetcher.spec.max_length());
  EXPECT_EQ(2u, fetcher.spec.lower_case().min());
}

// Tests that a response body that overrides the lower case character set is
// replaced with an empty spec.
TEST_F(PasswordSpecFetcherTest, ResponseWithCharacterSetOverrideRejected) {
  PasswordSpecFetcher* fetcher = [[PasswordSpecFetcher alloc] initWithHost:@""
                                                                    APIKey:@""];
  PasswordRequirementsSpec spec;
  spec.set_max_length(12u);
  spec.mutable_lower_case()->set_character_set("a");
  [fetcher onReceivedData:EncodedResponseBodyForSpec(spec)
                 response:nil
                    error:nil];
  EXPECT_FALSE(fetcher.spec.has_max_length());
  EXPECT_FALSE(fetcher.spec.has_lower_case());
}

// Tests that a response body with a max length below the minimum that the
// generator should produce is replaced with an empty spec.
TEST_F(PasswordSpecFetcherTest, ResponseWithShortMaxLengthRejected) {
  PasswordSpecFetcher* fetcher = [[PasswordSpecFetcher alloc] initWithHost:@""
                                                                    APIKey:@""];
  PasswordRequirementsSpec spec;
  spec.set_max_length(4u);
  [fetcher onReceivedData:EncodedResponseBodyForSpec(spec)
                 response:nil
                    error:nil];
  EXPECT_FALSE(fetcher.spec.has_max_length());
}
