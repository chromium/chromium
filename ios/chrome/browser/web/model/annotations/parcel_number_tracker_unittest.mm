// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/annotations/parcel_number_tracker.h"

#import "base/test/scoped_feature_list.h"
#import "ios/web/common/features.h"
#import "ios/web/public/annotations/custom_text_checking_result.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

class ParcelNumberTrackerTest : public PlatformTest {
 public:
  ParcelNumberTracker tracker_;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ParcelNumberTrackerTest, TestNumberDetect) {
  scoped_feature_list_.Reset();

  std::vector<web::TextAnnotation> results;
  NSRange range = NSMakeRange(0, 3);

  EXPECT_FALSE(tracker_.HasNewTrackingNumbers());

  // Add a number for another carrier
  NSTextCheckingResult* match =
      [CustomTextCheckingResult parcelCheckingResultWithRange:range
                                                      carrier:4
                                                carrierNumber:@"123456789"];
  results.emplace_back(web::ConvertMatchToAnnotation(@"123456789", range, match,
                                                     @"TRACKING_NUMBER"));
  tracker_.ProcessAnnotations(results);

  // Expect that the tracking number has been removed.
  EXPECT_EQ(0ul, results.size());
  // and that the tracking numbers is available.
  EXPECT_TRUE(tracker_.HasNewTrackingNumbers());
  NSArray<CustomTextCheckingResult*>* new_tracking_numbers =
      tracker_.GetNewTrackingNumbers();
  EXPECT_EQ(1ul, new_tracking_numbers.count);
  EXPECT_EQ(new_tracking_numbers[0].carrier, 4);
  EXPECT_EQ(new_tracking_numbers[0].carrierNumber, @"123456789");

  // Expect the tracking number has been removed.
  EXPECT_FALSE(tracker_.HasNewTrackingNumbers());
  new_tracking_numbers = tracker_.GetNewTrackingNumbers();
  EXPECT_EQ(0ul, new_tracking_numbers.count);

  // Add and process a carrier.
  match = [CustomTextCheckingResult carrierCheckingResultWithRange:range
                                                           carrier:2];
  results.emplace_back(
      web::ConvertMatchToAnnotation(@"UPS", range, match, @"CARRIER"));
  tracker_.ProcessAnnotations(results);

  // Expect that the carrier has been removed.
  EXPECT_EQ(0ul, results.size());
  // And that no tracking number is available.
  EXPECT_FALSE(tracker_.HasNewTrackingNumbers());

  // See the same number again.
  match = [CustomTextCheckingResult parcelCheckingResultWithRange:range
                                                          carrier:4
                                                    carrierNumber:@"123456789"];
  results.emplace_back(web::ConvertMatchToAnnotation(@"123456789", range, match,
                                                     @"TRACKING_NUMBER"));
  tracker_.ProcessAnnotations(results);
  // Expect its' not a new number.
  EXPECT_FALSE(tracker_.HasNewTrackingNumbers());

  // Add two tracking numbers and an address.
  match = [CustomTextCheckingResult parcelCheckingResultWithRange:range
                                                          carrier:2
                                                    carrierNumber:@"abcdefg"];
  results.emplace_back(web::ConvertMatchToAnnotation(@"abcdefg", range, match,
                                                     @"TRACKING_NUMBER"));
  match = [CustomTextCheckingResult parcelCheckingResultWithRange:range
                                                          carrier:2
                                                    carrierNumber:@"hijklmn"];
  results.emplace_back(web::ConvertMatchToAnnotation(@"hijklmn", range, match,
                                                     @"TRACKING_NUMBER"));
  match = [NSTextCheckingResult addressCheckingResultWithRange:range
                                                    components:@{}];
  results.emplace_back(web::ConvertMatchToAnnotation(@"8 Rue de Londre", range,
                                                     match, @"ADDRESS"));
  tracker_.ProcessAnnotations(results);

  // Expect the address was left untouched.
  EXPECT_EQ(1ul, results.size());

  // And that there are two new tracking numbers.
  EXPECT_TRUE(tracker_.HasNewTrackingNumbers());
  new_tracking_numbers = tracker_.GetNewTrackingNumbers();
  EXPECT_EQ(2ul, new_tracking_numbers.count);
  EXPECT_EQ(new_tracking_numbers[0].carrier, 2);
  EXPECT_EQ(new_tracking_numbers[0].carrierNumber, @"abcdefg");
  EXPECT_EQ(new_tracking_numbers[1].carrier, 2);
  EXPECT_EQ(new_tracking_numbers[1].carrierNumber, @"hijklmn");
  EXPECT_FALSE(tracker_.HasNewTrackingNumbers());
}

TEST_F(ParcelNumberTrackerTest, TestImprovedCarrierAndNumberDetect) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      {web::features::kEnableNewParcelTrackingNumberDetection}, {});

  std::vector<web::TextAnnotation> results;
  NSRange range = NSMakeRange(0, 3);

  EXPECT_FALSE(tracker_.HasNewTrackingNumbers());

  // Add and process a carrier.
  NSTextCheckingResult* match =
      [CustomTextCheckingResult carrierCheckingResultWithRange:range carrier:2];
  results.emplace_back(
      web::ConvertMatchToAnnotation(@"UPS", range, match, @"CARRIER"));
  tracker_.ProcessAnnotations(results);

  // Expect that the carrier has been removed.
  EXPECT_EQ(0ul, results.size());
  // And that no tracking numbers are available.
  EXPECT_FALSE(tracker_.HasNewTrackingNumbers());

  // Add a number for another carrier
  match = [CustomTextCheckingResult parcelCheckingResultWithRange:range
                                                          carrier:4
                                                    carrierNumber:@"123456789"];
  results.emplace_back(web::ConvertMatchToAnnotation(@"123456789", range, match,
                                                     @"TRACKING_NUMBER"));
  tracker_.ProcessAnnotations(results);

  // Expect that the tracking number has been removed.
  EXPECT_EQ(0ul, results.size());
  // and that no matching tracking numbers are available.
  EXPECT_FALSE(tracker_.HasNewTrackingNumbers());

  // Add a matching carrier.
  match = [CustomTextCheckingResult carrierCheckingResultWithRange:range
                                                           carrier:4];
  results.emplace_back(
      web::ConvertMatchToAnnotation(@"USPS", range, match, @"CARRIER"));
  tracker_.ProcessAnnotations(results);

  // Expect that matching tracking numbers are available.
  EXPECT_TRUE(tracker_.HasNewTrackingNumbers());
  // and that calling the check twice doesn't change the result.
  EXPECT_TRUE(tracker_.HasNewTrackingNumbers());
  NSArray<CustomTextCheckingResult*>* new_tracking_numbers =
      tracker_.GetNewTrackingNumbers();
  EXPECT_EQ(1ul, new_tracking_numbers.count);
  EXPECT_EQ(new_tracking_numbers[0].carrier, 4);
  EXPECT_EQ(new_tracking_numbers[0].carrierNumber, @"123456789");

  // Expect the tracking number has been removed.
  EXPECT_FALSE(tracker_.HasNewTrackingNumbers());
  new_tracking_numbers = tracker_.GetNewTrackingNumbers();
  EXPECT_EQ(0ul, new_tracking_numbers.count);

  // See the same number again.
  match = [CustomTextCheckingResult parcelCheckingResultWithRange:range
                                                          carrier:4
                                                    carrierNumber:@"123456789"];
  results.emplace_back(web::ConvertMatchToAnnotation(@"123456789", range, match,
                                                     @"TRACKING_NUMBER"));
  tracker_.ProcessAnnotations(results);
  // Expect its' not a new number.
  EXPECT_FALSE(tracker_.HasNewTrackingNumbers());

  match = [CustomTextCheckingResult carrierCheckingResultWithRange:range
                                                           carrier:4];
  results.emplace_back(
      web::ConvertMatchToAnnotation(@"USPS", range, match, @"CARRIER"));
  tracker_.ProcessAnnotations(results);
  EXPECT_FALSE(tracker_.HasNewTrackingNumbers());

  // Add two tracking numbers for first carrier and an address.
  match = [CustomTextCheckingResult parcelCheckingResultWithRange:range
                                                          carrier:2
                                                    carrierNumber:@"abcdefg"];
  results.emplace_back(web::ConvertMatchToAnnotation(@"abcdefg", range, match,
                                                     @"TRACKING_NUMBER"));
  match = [CustomTextCheckingResult parcelCheckingResultWithRange:range
                                                          carrier:2
                                                    carrierNumber:@"hijklmn"];
  results.emplace_back(web::ConvertMatchToAnnotation(@"hijklmn", range, match,
                                                     @"TRACKING_NUMBER"));
  match = [NSTextCheckingResult addressCheckingResultWithRange:range
                                                    components:@{}];
  results.emplace_back(web::ConvertMatchToAnnotation(@"8 Rue de Londre", range,
                                                     match, @"ADDRESS"));
  tracker_.ProcessAnnotations(results);

  // Expect the address was left untouched.
  EXPECT_EQ(1ul, results.size());

  // And that there are two new tracking numbers.
  EXPECT_TRUE(tracker_.HasNewTrackingNumbers());
  new_tracking_numbers = tracker_.GetNewTrackingNumbers();
  EXPECT_EQ(2ul, new_tracking_numbers.count);
  EXPECT_EQ(new_tracking_numbers[0].carrier, 2);
  EXPECT_EQ(new_tracking_numbers[0].carrierNumber, @"abcdefg");
  EXPECT_EQ(new_tracking_numbers[1].carrier, 2);
  EXPECT_EQ(new_tracking_numbers[1].carrierNumber, @"hijklmn");
  EXPECT_FALSE(tracker_.HasNewTrackingNumbers());
}

TEST_F(ParcelNumberTrackerTest, TestAnyCarrierAndNumberDetect) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      {web::features::kEnableNewParcelTrackingNumberDetection}, {});

  std::vector<web::TextAnnotation> results;
  NSRange range = NSMakeRange(0, 3);

  EXPECT_FALSE(tracker_.HasNewTrackingNumbers());

  // Add a number for carrier 4.
  NSTextCheckingResult* match =
      [CustomTextCheckingResult parcelCheckingResultWithRange:range
                                                      carrier:4
                                                carrierNumber:@"123456789"];
  results.emplace_back(web::ConvertMatchToAnnotation(@"123456789", range, match,
                                                     @"TRACKING_NUMBER"));
  tracker_.ProcessAnnotations(results);

  // Expect that the tracking number has been removed.
  EXPECT_EQ(0ul, results.size());
  // and that no matching tracking numbers are available.
  EXPECT_FALSE(tracker_.HasNewTrackingNumbers());

  // Add an any-carrier carrier.
  match = [CustomTextCheckingResult carrierCheckingResultWithRange:range
                                                           carrier:0];
  results.emplace_back(web::ConvertMatchToAnnotation(@"Tracking Number", range,
                                                     match, @"CARRIER"));
  tracker_.ProcessAnnotations(results);

  // Expect that the tracking number is available.
  EXPECT_TRUE(tracker_.HasNewTrackingNumbers());
  NSArray<CustomTextCheckingResult*>* new_tracking_numbers =
      tracker_.GetNewTrackingNumbers();
  EXPECT_EQ(1ul, new_tracking_numbers.count);
  EXPECT_EQ(new_tracking_numbers[0].carrier, 4);
  EXPECT_EQ(new_tracking_numbers[0].carrierNumber, @"123456789");

  // Expect the tracking number has been removed.
  EXPECT_FALSE(tracker_.HasNewTrackingNumbers());
  new_tracking_numbers = tracker_.GetNewTrackingNumbers();
  EXPECT_EQ(0ul, new_tracking_numbers.count);
}
