// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_ANNOTATIONS_CUSTOM_TEXT_CHECKING_RESULT_H_
#define IOS_WEB_PUBLIC_ANNOTATIONS_CUSTOM_TEXT_CHECKING_RESULT_H_

#import <Foundation/Foundation.h>

// Custom NSTextCheckingResult types.
uint64_t const TCTextCheckingTypeParcelTracking = 1ULL << 32;
uint64_t const TCTextCheckingTypeMeasurement = 1ULL << 33;
uint64_t const TCTextCheckingTypeCarrier = 1ULL << 34;

// Custom NSTextCheckingResult class adding TextClassifier custom types.
@interface CustomTextCheckingResult : NSTextCheckingResult

@property(readonly, copy) NSMeasurement* measurement;
@property(readonly) int carrier;
@property(readonly, copy) NSString* carrierNumber;

/* Methods for creating instances of the various types of results. */
+ (NSTextCheckingResult*)measurementCheckingResultWithRange:(NSRange)range
                                                measurement:
                                                    (NSMeasurement*)measurement;
+ (NSTextCheckingResult*)parcelCheckingResultWithRange:(NSRange)range
                                               carrier:(int)carrier
                                         carrierNumber:(NSString*)carrierNumber;
+ (NSTextCheckingResult*)carrierCheckingResultWithRange:(NSRange)range
                                                carrier:(int)carrier;
@end

#endif  // IOS_WEB_PUBLIC_ANNOTATIONS_CUSTOM_TEXT_CHECKING_RESULT_H_
