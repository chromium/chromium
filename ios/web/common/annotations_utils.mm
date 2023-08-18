// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/common/annotations_utils.h"

#import "base/apple/foundation_util.h"
#import "base/logging.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/web/common/features.h"
#import "ios/web/public/annotations/custom_text_checking_result.h"

namespace web {

// Annotation keys for annotation.
static const char kAnnotationsTextKey[] = "text";
static const char kAnnotationsStartKey[] = "start";
static const char kAnnotationsEndKey[] = "end";
static const char kAnnotationsTypeKey[] = "type";
static const char kAnnotationsDataKey[] = "data";
NSString* const kMailtoPrefixUrl = @"mailto:";

NSString* EncodeNSTextCheckingResultData(NSTextCheckingResult* match) {
  NSMutableDictionary* dict = [[NSMutableDictionary alloc] init];

  if (match.resultType == NSTextCheckingTypeDate) {
    [dict setObject:@"date" forKey:@"type"];
    if (match.date) {
      [dict setObject:match.date forKey:@"date"];
    }
    if (match.duration) {
      [dict setObject:[NSNumber numberWithDouble:match.duration]
               forKey:@"duration"];
    }
    if (match.timeZone) {
      [dict setObject:match.timeZone forKey:@"timeZone"];
    }
  } else if (match.resultType == NSTextCheckingTypeAddress) {
    [dict setObject:@"address" forKey:@"type"];
    if (match.addressComponents) {
      [dict setObject:match.addressComponents forKey:@"addressComponents"];
    }
  } else if (match.resultType == NSTextCheckingTypePhoneNumber) {
    [dict setObject:@"phoneNumber" forKey:@"type"];
    if (match.phoneNumber) {
      [dict setObject:match.phoneNumber forKey:@"phoneNumber"];
    }
  } else if (IsNSTextCheckingResultEmail(match)) {
    [dict setObject:@"email" forKey:@"type"];
    if (match.URL) {
      [dict setObject:match.URL.resourceSpecifier forKey:@"email"];
    }
  } else if (match.resultType == TCTextCheckingTypeParcelTracking) {
    CustomTextCheckingResult* custom_match =
        base::apple::ObjCCastStrict<CustomTextCheckingResult>(match);
    [dict setObject:@"parcel" forKey:@"type"];
    if (custom_match.carrier) {
      [dict setObject:[NSNumber numberWithInt:custom_match.carrier]
               forKey:@"carrier"];
    }
    if (custom_match.carrierNumber) {
      [dict setObject:custom_match.carrierNumber forKey:@"carrierNumber"];
    }
  } else if (match.resultType == TCTextCheckingTypeMeasurement) {
    CustomTextCheckingResult* custom_match =
        base::apple::ObjCCastStrict<CustomTextCheckingResult>(match);
    [dict setObject:@"measurement" forKey:@"type"];
    if (custom_match.measurement) {
      [dict setObject:custom_match.measurement forKey:@"measurement"];
    }
  }

  NSError* error = nil;
  NSData* data = [NSKeyedArchiver archivedDataWithRootObject:dict
                                       requiringSecureCoding:NO
                                                       error:&error];

  if (!data || error) {
    DLOG(ERROR) << "Error serializing data: "
                << base::SysNSStringToUTF8([error description]);
    return nil;
  }

  return [data base64EncodedStringWithOptions:0];
}

NSTextCheckingResult* DecodeNSTextCheckingResultData(NSString* base64_data) {
  NSData* data = [[NSData alloc] initWithBase64EncodedString:base64_data
                                                     options:0];

  NSError* error = nil;
  NSKeyedUnarchiver* unarchiver =
      [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:&error];
  if (!unarchiver || error) {
    DLOG(ERROR) << "Error deserializing data: "
                << base::SysNSStringToUTF8([error description]);
    return nil;
  }

  unarchiver.requiresSecureCoding = NO;
  NSMutableDictionary* dict =
      [unarchiver decodeObjectForKey:NSKeyedArchiveRootObjectKey];

  NSRange range;
  NSString* type = dict[@"type"];
  if ([type isEqualToString:@"date"]) {
    NSDate* date = dict[@"date"];
    NSNumber* number = dict[@"duration"];
    NSTimeInterval duration = number.doubleValue;
    NSTimeZone* timeZone = dict[@"timeZone"];
    return [NSTextCheckingResult dateCheckingResultWithRange:range
                                                        date:date
                                                    timeZone:timeZone
                                                    duration:duration];
  } else if ([type isEqualToString:@"address"]) {
    NSDictionary* components = dict[@"addressComponents"];
    return [NSTextCheckingResult addressCheckingResultWithRange:range
                                                     components:components];
  } else if ([type isEqualToString:@"phoneNumber"]) {
    NSString* phoneNumber = dict[@"phoneNumber"];
    return
        [NSTextCheckingResult phoneNumberCheckingResultWithRange:range
                                                     phoneNumber:phoneNumber];
  } else if ([type isEqualToString:@"email"]) {
    NSString* email = dict[@"email"];
    return MakeNSTextCheckingResultEmail(email, range);
  } else if ([type isEqualToString:@"parcel"]) {
    NSNumber* number = dict[@"carrier"];
    int carrier = number.intValue;
    NSString* carrierNumber = dict[@"carrierNumber"];
    return
        [CustomTextCheckingResult parcelCheckingResultWithRange:range
                                                        carrier:carrier
                                                  carrierNumber:carrierNumber];
  } else if ([type isEqualToString:@"parcel"]) {
    NSMeasurement* measurement = dict[@"measurement"];
    return [CustomTextCheckingResult
        measurementCheckingResultWithRange:range
                               measurement:measurement];
  }
  return nil;
}

base::Value::Dict ConvertMatchToAnnotation(NSString* source,
                                           NSRange range,
                                           NSString* data,
                                           NSString* type) {
  base::Value::Dict dict;
  NSString* start = [source substringWithRange:range];
  dict.Set(kAnnotationsStartKey, base::Value(static_cast<int>(range.location)));
  dict.Set(kAnnotationsEndKey,
           base::Value(static_cast<int>(range.location + range.length)));
  dict.Set(kAnnotationsTextKey, base::Value(base::SysNSStringToUTF8(start)));
  dict.Set(kAnnotationsTypeKey, base::Value(base::SysNSStringToUTF8(type)));
  dict.Set(kAnnotationsDataKey, base::Value(base::SysNSStringToUTF8(data)));
  return dict;
}

bool IsNSTextCheckingResultEmail(NSTextCheckingResult* result) {
  return result.resultType == NSTextCheckingTypeLink &&
         [result.URL.scheme isEqualToString:@"mailto"];
}

NSTextCheckingResult* MakeNSTextCheckingResultEmail(NSString* email,
                                                    NSRange range) {
  NSString* mailto_email = [kMailtoPrefixUrl stringByAppendingString:email];
  NSURL* email_url = [[NSURL alloc] initWithString:mailto_email];
  return [NSTextCheckingResult linkCheckingResultWithRange:range URL:email_url];
}

bool WebPageAnnotationsEnabled() {
  return base::FeatureList::IsEnabled(web::features::kEnableEmails) ||
         base::FeatureList::IsEnabled(web::features::kEnablePhoneNumbers) ||
         base::FeatureList::IsEnabled(web::features::kOneTapForMaps) ||
         base::FeatureList::IsEnabled(web::features::kEnableMeasurements);
}

}  // namespace web
