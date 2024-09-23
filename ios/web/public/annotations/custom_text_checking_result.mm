// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/annotations/custom_text_checking_result.h"

@implementation CustomTextCheckingResult {
  uint64_t _customResultType;
  NSRange _customRange;

  NSMeasurement* _measurement;
  int _carrier;
  NSString* _carrierNumber;
}

- (NSTextCheckingType)resultType {
  return _customResultType;
}
- (NSRange)range {
  return _customRange;
}

- (NSMeasurement*)measurement {
  return _measurement;
}
- (int)carrier {
  return _carrier;
}
- (NSString*)carrierNumber {
  return _carrierNumber;
}

+ (NSTextCheckingResult*)measurementCheckingResultWithRange:(NSRange)range
                                                measurement:(NSMeasurement*)
                                                                measurement {
  CustomTextCheckingResult* result = [[CustomTextCheckingResult alloc] init];
  if (result) {
    result->_customResultType = TCTextCheckingTypeMeasurement;
    result->_customRange = range;
    result->_measurement = measurement;
  }
  return result;
}

+ (NSTextCheckingResult*)parcelCheckingResultWithRange:(NSRange)range
                                               carrier:(int)carrier
                                         carrierNumber:
                                             (NSString*)carrierNumber {
  CustomTextCheckingResult* result = [[CustomTextCheckingResult alloc] init];
  if (result) {
    result->_customResultType = TCTextCheckingTypeParcelTracking;
    result->_customRange = range;
    result->_carrier = carrier;
    result->_carrierNumber = carrierNumber;
  }
  return result;
}

+ (NSTextCheckingResult*)carrierCheckingResultWithRange:(NSRange)range
                                                carrier:(int)carrier {
  CustomTextCheckingResult* result = [[CustomTextCheckingResult alloc] init];
  if (result) {
    result->_customResultType = TCTextCheckingTypeCarrier;
    result->_customRange = range;
    result->_carrier = carrier;
  }
  return result;
}

@end
