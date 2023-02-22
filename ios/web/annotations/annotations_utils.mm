// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/annotations/annotations_utils.h"

#import "base/logging.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/web/common/annotations_utils.h"
#import "ios/web/common/features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace annotations {

NSString* EncodeNSTextCheckingResultData(NSTextCheckingResult* match) {
  return web::EncodeNSTextCheckingResultData(match);
}

NSTextCheckingResult* DecodeNSTextCheckingResultData(NSString* base64_data) {
  return web::DecodeNSTextCheckingResultData(base64_data);
}

base::Value::Dict ConvertMatchToAnnotation(NSString* source,
                                           NSRange range,
                                           NSString* data,
                                           NSString* type) {
  return web::ConvertMatchToAnnotation(source, range, data, type);
}

bool IsNSTextCheckingResultEmail(NSTextCheckingResult* result) {
  return web::IsNSTextCheckingResultEmail(result);
}

NSTextCheckingResult* MakeNSTextCheckingResultEmail(NSString* email,
                                                    NSRange range) {
  return web::MakeNSTextCheckingResultEmail(email, range);
}

bool WebPageAnnotationsEnabled() {
  return web::WebPageAnnotationsEnabled();
}

}  // namespace annotations
}  // namespace web
