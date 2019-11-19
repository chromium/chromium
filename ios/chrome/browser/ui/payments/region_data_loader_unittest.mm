// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/region_data_loader.h"

#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/geo/test_region_data_loader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kQuebecCode[] = "QC";
const char kOntarioCode[] = "ON";
const char kQuebec[] = "Quebec";
const char kOntario[] = "Ontario";
}  // namespace

class PaymentRequestRegionDataLoaderTest : public PlatformTest {
 protected:
  PaymentRequestRegionDataLoaderTest() {}
  ~PaymentRequestRegionDataLoaderTest() override {}

  autofill::TestRegionDataLoader autofill_region_data_loader_;
};

// Tests that the two regions returned by the source are correctly returned.
TEST_F(PaymentRequestRegionDataLoaderTest, SourceSuccess) {
  // Mock the consumer.
  id consumer =
      [OCMockObject mockForProtocol:@protocol(RegionDataLoaderConsumer)];
  [[consumer expect]
      regionDataLoaderDidSucceedWithRegions:[OCMArg checkWithBlock:^BOOL(
                                                        id value) {
        NSArray<RegionData*>* data =
            base::mac::ObjCCastStrict<NSArray<RegionData*>>(value);
        return [data[0].regionCode
                   isEqualToString:base::SysUTF8ToNSString(kQuebecCode)] &&
               [data[0].regionName
                   isEqualToString:base::SysUTF8ToNSString(kQuebec)] &&
               [data[1].regionCode
                   isEqualToString:base::SysUTF8ToNSString(kOntarioCode)] &&
               [data[1].regionName
                   isEqualToString:base::SysUTF8ToNSString(kOntario)];
      }]];

  RegionDataLoader region_data_loader(consumer);
  region_data_loader.LoadRegionData("some country",
                                    &autofill_region_data_loader_);

  std::vector<std::pair<std::string, std::string>> regions;
  regions.push_back(std::make_pair(kQuebecCode, kQuebec));
  regions.push_back(std::make_pair(kOntarioCode, kOntario));
  autofill_region_data_loader_.SendAsynchronousData(regions);

  EXPECT_OCMOCK_VERIFY(consumer);
}

// Tests that the source emptiness/failure is properly handled.
TEST_F(PaymentRequestRegionDataLoaderTest, SourceFailure) {
  // Mock the consumer.
  id consumer =
      [OCMockObject mockForProtocol:@protocol(RegionDataLoaderConsumer)];
  [[consumer expect] regionDataLoaderDidSucceedWithRegions:@[]];

  RegionDataLoader region_data_loader(consumer);
  region_data_loader.LoadRegionData("some country",
                                    &autofill_region_data_loader_);

  autofill_region_data_loader_.SendAsynchronousData(
      std::vector<std::pair<std::string, std::string>>());

  EXPECT_OCMOCK_VERIFY(consumer);
}
