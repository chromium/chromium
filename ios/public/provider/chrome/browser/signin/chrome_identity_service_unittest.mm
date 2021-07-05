// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_mock_clock_override.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {
namespace {

class TestChromeIdentityService : public ChromeIdentityService {
 public:
  struct FetchCapabilitiesRequest {
    NSArray* capabilities;
    ChromeIdentity* identity;
    ChromeIdentityCapabilitiesFetchCompletionBlock completion;
  };

  TestChromeIdentityService() = default;
  ~TestChromeIdentityService() override = default;

  const FetchCapabilitiesRequest& fetch_capabilities_request() const {
    return fetch_capabilities_request_.value();
  }

  void RunFinishCapabilitiesCompletion(NSDictionary* capabilities,
                                       NSError* error) {
    EXPECT_TRUE(fetch_capabilities_request_.has_value());
    EXPECT_TRUE(fetch_capabilities_request_.value().completion);
    fetch_capabilities_request_.value().completion(capabilities, error);
    fetch_capabilities_request_.reset();
  }

 protected:
  void FetchCapabilities(
      NSArray* capabilities,
      ChromeIdentity* identity,
      ChromeIdentityCapabilitiesFetchCompletionBlock completion) override {
    EXPECT_FALSE(fetch_capabilities_request_.has_value());
    FetchCapabilitiesRequest request;
    request.capabilities = capabilities;
    request.identity = identity;
    request.completion = completion;
    fetch_capabilities_request_ = request;
  }

 private:
  absl::optional<FetchCapabilitiesRequest> fetch_capabilities_request_;
};

class ChromeIdentityServiceTest : public PlatformTest {
 public:
  ChromeIdentityServiceTest() = default;
  ~ChromeIdentityServiceTest() override = default;

 protected:
  ChromeIdentityCapabilityResult FetchCanOfferExtendedSyncPromos(
      ChromeIdentity* identity,
      int capability_value) {
    __block ChromeIdentityCapabilityResult fetched_capability_result;
    service_.CanOfferExtendedSyncPromos(
        identity, ^(ChromeIdentityCapabilityResult result) {
          fetched_capability_result = result;
        });
    EXPECT_NSEQ(@[ @"accountcapabilities/gi2tklldmfya" ],
                service_.fetch_capabilities_request().capabilities);
    EXPECT_EQ(identity, service_.fetch_capabilities_request().identity);

    NSNumber* capability_val_number = [NSNumber numberWithInt:capability_value];
    service_.RunFinishCapabilitiesCompletion(
        @{@"accountcapabilities/gi2tklldmfya" : capability_val_number},
        /*error=*/nil);
    return fetched_capability_result;
  }

  TestChromeIdentityService service_;
};

TEST_F(ChromeIdentityServiceTest, CanOfferExtendedSyncPromos) {
  FakeChromeIdentity* identity =
      [FakeChromeIdentity identityWithEmail:@"foo@bar.com"
                                     gaiaID:@"foo_bar_id"
                                       name:@"Foo"];

  EXPECT_EQ(ChromeIdentityCapabilityResult::kFalse,
            FetchCanOfferExtendedSyncPromos(identity,
                                            /*capability_value=*/0));

  EXPECT_EQ(ChromeIdentityCapabilityResult::kTrue,
            FetchCanOfferExtendedSyncPromos(identity,
                                            /*capability_value=*/1));

  EXPECT_EQ(ChromeIdentityCapabilityResult::kUnknown,
            FetchCanOfferExtendedSyncPromos(identity,
                                            /*capability_value=*/2));
}

TEST_F(ChromeIdentityServiceTest, CanOfferExtendedSyncPromos_Histogram) {
  FakeChromeIdentity* identity =
      [FakeChromeIdentity identityWithEmail:@"foo@bar.com"
                                     gaiaID:@"foo_bar_id"
                                       name:@"Foo"];

  base::HistogramTester histogramTester;
  base::ScopedMockClockOverride clock;
  service_.CanOfferExtendedSyncPromos(identity, /*callback=*/nil);
  clock.Advance(base::TimeDelta::FromMinutes(1));
  service_.RunFinishCapabilitiesCompletion(
      @{@"accountcapabilities/gi2tklldmfya" : @0}, /*error=*/nil);
  histogramTester.ExpectUniqueTimeSample(
      "Signin.AccountCapabilities.GetFromSystemLibraryDuration",
      base::TimeDelta::FromMinutes(1),
      /*expected_bucket_count=*/1);
}

}  // namespace
}  // namespace ios
