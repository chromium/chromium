// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/annotations/parcel_number_tracker.h"

#import "ios/web/common/features.h"

/**
 * Tracks parcels number found so far in the page and those that have been
 * passed on to the Parcel Tracking service.
 */
@interface AnnotationsParcelProvider : NSObject
@property(nonatomic, strong)
    NSMutableArray<CustomTextCheckingResult*>* trackingNumbers;
@property(nonatomic, strong) NSMutableSet<NSString*>* uniqueParcelNumbers;
@end

@implementation AnnotationsParcelProvider
- (instancetype)init {
  self = [super init];
  if (self) {
    _trackingNumbers = [NSMutableArray array];
    _uniqueParcelNumbers = [NSMutableSet set];
  }
  return self;
}
@end

ParcelNumberTracker::ParcelNumberTracker() {
  parcel_tracking_numbers_ = [NSMutableDictionary dictionary];
  parcel_carriers_ = [NSMutableSet set];
  any_carrier_id_ = [[NSNumber alloc] initWithInt:0];
}

ParcelNumberTracker::~ParcelNumberTracker() {
  parcel_tracking_numbers_ = nil;
  parcel_carriers_ = nil;
}

void ParcelNumberTracker::ProcessAnnotations(
    std::vector<web::TextAnnotation>& annotations_list) {
  for (auto annotation = annotations_list.begin();
       annotation != annotations_list.end();) {
    NSTextCheckingResult* match = annotation->second;
    if (!match || (match.resultType != TCTextCheckingTypeParcelTracking &&
                   match.resultType != TCTextCheckingTypeCarrier)) {
      annotation++;
      continue;
    }
    CustomTextCheckingResult* custom_match =
        static_cast<CustomTextCheckingResult*>(match);

    if (match.resultType == TCTextCheckingTypeCarrier) {
      NSNumber* carrier = [[NSNumber alloc] initWithInt:custom_match.carrier];
      [parcel_carriers_ addObject:carrier];
      // Remove the carrier from annotations_list to prevent decorating it.
      annotation = annotations_list.erase(annotation);
    } else if (match.resultType == TCTextCheckingTypeParcelTracking) {
      NSNumber* carrier = [[NSNumber alloc] initWithInt:custom_match.carrier];
      AnnotationsParcelProvider* parcel =
          [parcel_tracking_numbers_ objectForKey:carrier];
      if (!parcel) {
        parcel = [[AnnotationsParcelProvider alloc] init];
        [parcel_tracking_numbers_ setObject:parcel forKey:carrier];
      }
      // Avoid adding duplicates to `trackingNumbers`.
      if (![parcel.uniqueParcelNumbers
              containsObject:[custom_match carrierNumber]]) {
        [parcel.uniqueParcelNumbers addObject:[custom_match carrierNumber]];
        [parcel.trackingNumbers addObject:custom_match];
      }
      // Remove the parcel from annotations_list to prevent decorating it.
      annotation = annotations_list.erase(annotation);
    } else {
      annotation++;
    }
  }
}

// Returns true if any parcels are ready to be dispatched to parcel tracking
// system.
bool ParcelNumberTracker::HasNewTrackingNumbers() {
  for (NSNumber* key in parcel_tracking_numbers_) {
    AnnotationsParcelProvider* parcel =
        [parcel_tracking_numbers_ objectForKey:key];
    if (base::FeatureList::IsEnabled(
            web::features::kEnableNewParcelTrackingNumberDetection) &&
        ![parcel_carriers_ containsObject:key] &&
        ![parcel_carriers_ containsObject:any_carrier_id_]) {
      continue;
    }
    if (parcel.trackingNumbers.count) {
      return true;
    }
  }
  return false;
}

// Returns the array of parcels ready to be dispatched to parcel tracking
// system and removes them from the match wait list.
NSArray<CustomTextCheckingResult*>*
ParcelNumberTracker::GetNewTrackingNumbers() {
  NSMutableArray<CustomTextCheckingResult*>* trackingNumbers =
      [NSMutableArray array];
  for (NSNumber* key in parcel_tracking_numbers_) {
    AnnotationsParcelProvider* parcel =
        [parcel_tracking_numbers_ objectForKey:key];
    if (base::FeatureList::IsEnabled(
            web::features::kEnableNewParcelTrackingNumberDetection) &&
        ![parcel_carriers_ containsObject:key] &&
        ![parcel_carriers_ containsObject:any_carrier_id_]) {
      continue;
    }
    [trackingNumbers addObjectsFromArray:parcel.trackingNumbers];
    [parcel.trackingNumbers removeAllObjects];
  }
  return trackingNumbers;
}
