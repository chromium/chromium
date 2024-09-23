// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_ANNOTATIONS_PARCEL_NUMBER_TRACKER_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_ANNOTATIONS_PARCEL_NUMBER_TRACKER_H_

#import <UIKit/UIKit.h>

#import <optional>

#import "ios/web/common/annotations_utils.h"
#import "ios/web/public/annotations/custom_text_checking_result.h"

@class AnnotationsParcelProvider;

/**
 * Class tracking discovered parcel tracking numbers in text.
 */
class ParcelNumberTracker {
 public:
  explicit ParcelNumberTracker();
  ~ParcelNumberTracker();

  // Records and removes parcels from `annotations_list`.
  void ProcessAnnotations(std::vector<web::TextAnnotation>& annotations_list);

  // Returns true if any parcels are ready to be dispatched to parcel tracking
  // system.
  bool HasNewTrackingNumbers();

  // Returns the array of parcels ready to be dispatched to parcel tracking
  // system and removes them from the match wait list.
  NSArray<CustomTextCheckingResult*>* GetNewTrackingNumbers();

 private:
  // All tracking numbers that have been detected so far on this page, keyed
  // by the carrier associated to the number's pattern.
  NSMutableDictionary<NSNumber*, AnnotationsParcelProvider*>*
      parcel_tracking_numbers_;

  // All carriers that have been detected (by name) so far on this page.
  NSMutableSet<NSNumber*>* parcel_carriers_;

  // Id (0) representing textual data found that is as good as having literal
  // carrier identity. For example "Order Number", "Tracking Number", etc.
  NSNumber* any_carrier_id_;
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_ANNOTATIONS_PARCEL_NUMBER_TRACKER_H_
