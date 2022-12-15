// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

#import "base/run_loop.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_mock_clock_override.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/signin/capabilities_dict.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using signin_metrics::FetchAccountCapabilitiesFromSystemLibraryResult;

namespace ios {
namespace {

// A wrapper for the API to fetch Capability results for a specific Capability
// type.
using CapabilityFetcherBlock = void (^)(ChromeIdentityCapabilityResult*);

class TestChromeIdentityService : public ChromeIdentityService {
 public:
  struct FetchCapabilitiesRequest {
    NSArray* capabilities;
    id<SystemIdentity> identity;
    ChromeIdentityCapabilitiesFetchCompletionBlock completion;
  };

  TestChromeIdentityService() = default;
  ~TestChromeIdentityService() override = default;

  // Defines available capabilities that can be set under test.
  // Sets the capability `kCanOfferExtendedChromeSyncPromosCapabilityName` under
  // test.
  void SetCapabilityUnderTestCanOfferExtendedSyncPromos(
      id<SystemIdentity> identity) {
    SetCapabilityUnderTest(
        @(kCanOfferExtendedChromeSyncPromosCapabilityName),
        ^(ChromeIdentityCapabilityResult* fetched_capability_result) {
          CanOfferExtendedSyncPromos(identity,
                                     ^(ChromeIdentityCapabilityResult result) {
                                       *fetched_capability_result = result;
                                     });
        });
  }

  // Sets the capability `kIsSubjectToParentalControlsCapabilityName` under
  // test.
  void SetCapabilityUnderTestIsSubjectToParentalControls(
      id<SystemIdentity> identity) {
    SetCapabilityUnderTest(
        @(kIsSubjectToParentalControlsCapabilityName),
        ^(ChromeIdentityCapabilityResult* fetched_capability_result) {
          IsSubjectToParentalControls(identity,
                                      ^(ChromeIdentityCapabilityResult result) {
                                        *fetched_capability_result = result;
                                      });
        });
  }

  // Retrieves the capability tribool result for the capability under test
  // with the specified delay.
  ChromeIdentityCapabilityResult FetchCapability(NSNumber* capability_value,
                                                 NSError* error) {
    base::HistogramTester histogramTester;
    base::ScopedMockClockOverride clock;
    ChromeIdentityCapabilityResult result;
    capability_fetcher_block_(&result);

    clock.Advance(base::Minutes(1));
    // Capability result is set after completion.
    RunFinishCapabilitiesCompletion(capability_value, error);

    histogramTester.ExpectUniqueTimeSample(
        "Signin.AccountCapabilities.GetFromSystemLibraryDuration",
        base::Minutes(1),
        /*expected_bucket_count=*/1);

    return result;
  }

 protected:
  void FetchCapabilities(
      id<SystemIdentity> identity,
      NSArray<NSString*>* capabilities,
      ChromeIdentityCapabilitiesFetchCompletionBlock completion) override {
    EXPECT_FALSE(fetch_capabilities_request_.has_value());
    FetchCapabilitiesRequest request;
    request.capabilities = capabilities;
    request.identity = identity;
    request.completion = completion;
    fetch_capabilities_request_ = request;
  }

 private:
  // Sets the capability and its corresponding fetcher in the test service.
  void SetCapabilityUnderTest(NSString* capability_name,
                              CapabilityFetcherBlock capability_fetcher_block) {
    capability_name_ = capability_name;
    capability_fetcher_block_ = capability_fetcher_block;
  }

  void RunFinishCapabilitiesCompletion(NSNumber* capability_value,
                                       NSError* error) {
    CapabilitiesDict* capabilities =
        capability_value ? @{capability_name_ : capability_value} : nil;
    EXPECT_TRUE(fetch_capabilities_request_.has_value());
    EXPECT_TRUE(fetch_capabilities_request_.value().completion);
    fetch_capabilities_request_.value().completion(capabilities, error);
    fetch_capabilities_request_.reset();
  }

  absl::optional<FetchCapabilitiesRequest> fetch_capabilities_request_;
  NSString* capability_name_ = nil;
  CapabilityFetcherBlock capability_fetcher_block_ = nil;
};

class ChromeIdentityServiceTest : public PlatformTest {
 public:
  ChromeIdentityServiceTest() {
    identity_ = [FakeSystemIdentity identityWithEmail:@"foo@bar.com"
                                               gaiaID:@"foo_bar_id"
                                                 name:@"Foo"];
  }
  ~ChromeIdentityServiceTest() override = default;

 protected:
  // Checks that the defined capability values correspond to expected
  // ChromeIdentityCapabilityResult.
  void CheckChromeIdentityCapabilityResult() {
    {
      base::HistogramTester histogramTester;
      EXPECT_EQ(ChromeIdentityCapabilityResult::kFalse,
                service_.FetchCapability(/*capability_value=*/@0, nil));
      histogramTester.ExpectUniqueSample(
          "Signin.AccountCapabilities.GetFromSystemLibraryResult",
          FetchAccountCapabilitiesFromSystemLibraryResult::kSuccess, 1);
    }
    {
      base::HistogramTester histogramTester;
      EXPECT_EQ(ChromeIdentityCapabilityResult::kTrue,
                service_.FetchCapability(/*capability_value=*/@1, nil));
      histogramTester.ExpectUniqueSample(
          "Signin.AccountCapabilities.GetFromSystemLibraryResult",
          FetchAccountCapabilitiesFromSystemLibraryResult::kSuccess, 1);
    }
    {
      base::HistogramTester histogramTester;
      EXPECT_EQ(ChromeIdentityCapabilityResult::kUnknown,
                service_.FetchCapability(/*capability_value=*/@2, nil));
      histogramTester.ExpectUniqueSample(
          "Signin.AccountCapabilities.GetFromSystemLibraryResult",
          FetchAccountCapabilitiesFromSystemLibraryResult::kSuccess, 1);
    }
  }

  // Checks that a missing capability maps to the kUnknown capability result.
  void CheckMissingCapability() {
    base::HistogramTester histogramTester;
    EXPECT_EQ(ChromeIdentityCapabilityResult::kUnknown,
              service_.FetchCapability(/*capability_value=*/nil, nil));
    histogramTester.ExpectUniqueSample(
        "Signin.AccountCapabilities.GetFromSystemLibraryResult",
        FetchAccountCapabilitiesFromSystemLibraryResult::
            kErrorMissingCapability,
        1);
  }

  // Checks that an out of range capability value maps to the kUnknown
  // capability result.
  void CheckCapabilityValueOutOfRange() {
    base::HistogramTester histogramTester;
    EXPECT_EQ(ChromeIdentityCapabilityResult::kUnknown,
              service_.FetchCapability(/*capability_value=*/@100, nil));
    histogramTester.ExpectUniqueSample(
        "Signin.AccountCapabilities.GetFromSystemLibraryResult",
        FetchAccountCapabilitiesFromSystemLibraryResult::kErrorUnexpectedValue,
        1);
  }

  // Checks that an error in fetching capabilities maps to the kUnknown
  // capability result.
  void CheckCapabilityFetcherWithError() {
    NSError* error = [NSError errorWithDomain:@"test" code:-100 userInfo:nil];
    {
      base::HistogramTester histogramTester;
      EXPECT_EQ(ChromeIdentityCapabilityResult::kUnknown,
                service_.FetchCapability(
                    /*capability_value=*/nil, error));
      histogramTester.ExpectUniqueSample(
          "Signin.AccountCapabilities.GetFromSystemLibraryResult",
          FetchAccountCapabilitiesFromSystemLibraryResult::kErrorGeneric, 1);
    }
    {
      base::HistogramTester histogramTester;
      EXPECT_EQ(ChromeIdentityCapabilityResult::kUnknown,
                service_.FetchCapability(/*capability_value=*/@0, error));
      histogramTester.ExpectUniqueSample(
          "Signin.AccountCapabilities.GetFromSystemLibraryResult",
          FetchAccountCapabilitiesFromSystemLibraryResult::kErrorGeneric, 1);
    }
    {
      base::HistogramTester histogramTester;
      EXPECT_EQ(ChromeIdentityCapabilityResult::kUnknown,
                service_.FetchCapability(
                    /*capability_value=*/@1, error));
      histogramTester.ExpectUniqueSample(
          "Signin.AccountCapabilities.GetFromSystemLibraryResult",
          FetchAccountCapabilitiesFromSystemLibraryResult::kErrorGeneric, 1);
    }
  }

  FakeSystemIdentity* identity_;
  TestChromeIdentityService service_;
};

// Checks that the capability CanOfferExtendedChromeSyncPromos maps
// capability values to their corresponding ChromeIdentityCapabilityResult.
TEST_F(ChromeIdentityServiceTest,
       CanOfferExtendedSyncPromos_CheckChromeIdentityCapabilityResult) {
  service_.SetCapabilityUnderTestCanOfferExtendedSyncPromos(identity_);
  CheckChromeIdentityCapabilityResult();
}

// Checks that the capability CanOfferExtendedSyncPromos correctly handles
// missing capabilities as kUnknown.
TEST_F(ChromeIdentityServiceTest,
       CanOfferExtendedSyncPromos_CheckMissingCapability) {
  service_.SetCapabilityUnderTestCanOfferExtendedSyncPromos(identity_);
  CheckMissingCapability();
}

// Checks that the capability CanOfferExtendedSyncPromos correctly handles
// out of range capability values as kUnknown.
TEST_F(ChromeIdentityServiceTest,
       CanOfferExtendedSyncPromos_CheckCapabilityValueOutOfRange) {
  service_.SetCapabilityUnderTestCanOfferExtendedSyncPromos(identity_);
  CheckCapabilityValueOutOfRange();
}

// Checks that the capability CanOfferExtendedSyncPromos correctly handles
// errors in the capability fetcher as capability value kUnknown.
TEST_F(ChromeIdentityServiceTest,
       CanOfferExtendedSyncPromos_CheckCapabilityFetcherWithError) {
  service_.SetCapabilityUnderTestCanOfferExtendedSyncPromos(identity_);
  CheckCapabilityFetcherWithError();
}

// Checks that the capability IsSubjectToParentalControls maps
// capability values to their corresponding ChromeIdentityCapabilityResult.
TEST_F(ChromeIdentityServiceTest,
       IsSubjectToParentalControls_CheckChromeIdentityCapabilityResult) {
  service_.SetCapabilityUnderTestIsSubjectToParentalControls(identity_);
  CheckChromeIdentityCapabilityResult();
}

// Checks that the capability IsSubjectToParentalControls correctly handles
// missing capabilities as kUnknown.
TEST_F(ChromeIdentityServiceTest,
       IsSubjectToParentalControls_CheckMissingCapability) {
  service_.SetCapabilityUnderTestIsSubjectToParentalControls(identity_);
  CheckMissingCapability();
}

// Checks that the capability IsSubjectToParentalControls correctly handles
// out of range capability values as kUnknown.
TEST_F(ChromeIdentityServiceTest,
       IsSubjectToParentalControls_CheckCapabilityValueOutOfRange) {
  service_.SetCapabilityUnderTestIsSubjectToParentalControls(identity_);
  CheckCapabilityValueOutOfRange();
}

// Checks that the capability IsSubjectToParentalControls correctly handles
// errors in the capability fetcher as capability value kUnknown.
TEST_F(ChromeIdentityServiceTest,
       IsSubjectToParentalControls_CheckCapabilityFetcherWithError) {
  service_.SetCapabilityUnderTestIsSubjectToParentalControls(identity_);
  CheckCapabilityFetcherWithError();
}

}  // namespace
}  // namespace ios
