// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/common/annotations_utils.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"

namespace web {

// Annotation keys for annotation.
static const char kAnnotationsTextKey[] = "text";
static const char kAnnotationsStartKey[] = "start";
static const char kAnnotationsEndKey[] = "end";
static const char kAnnotationsTypeKey[] = "type";
NSString* const kMailtoPrefixUrl = @"mailto:";

web::TextAnnotation ConvertMatchToAnnotation(NSString* source,
                                             NSRange range,
                                             NSTextCheckingResult* data,
                                             NSString* type) {
  base::Value::Dict dict;
  NSString* start = [source substringWithRange:range];
  dict.Set(kAnnotationsStartKey, base::Value(static_cast<int>(range.location)));
  dict.Set(kAnnotationsEndKey,
           base::Value(static_cast<int>(range.location + range.length)));
  dict.Set(kAnnotationsTextKey, base::Value(base::SysNSStringToUTF8(start)));
  dict.Set(kAnnotationsTypeKey, base::Value(base::SysNSStringToUTF8(type)));
  return std::make_pair(std::move(dict), data);
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

}  // namespace web
