// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_error_codes.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/to_string.h"

NSString* const kTTCErrorDomain = @"TTCErrorDomain";

namespace {

bool IsValidErrorCodeValue(NSInteger code) {
  return (code >= static_cast<NSInteger>(ttc::ErrorCode::kUnknown) &&
          code <=
              static_cast<NSInteger>(ttc::ErrorCode::kMaxServerErrorCode)) ||
         (code >= static_cast<NSInteger>(
                      ttc::ErrorCode::kOptimizationGuideUnavailable) &&
          code <= static_cast<NSInteger>(ttc::ErrorCode::kAudioUnknownError));
}

}  // namespace

NSError* CreateTTCError(ttc::ErrorCode error_code) {
  NSString* description = base::SysUTF8ToNSString(base::ToString(error_code));
  return [NSError
      errorWithDomain:kTTCErrorDomain
                 code:static_cast<NSInteger>(error_code)
             userInfo:description ? @{NSLocalizedDescriptionKey : description}
                                  : nil];
}

bool IsFatalTTCError(NSError* error) {
  if (!error) {
    return false;
  }
  if ([error.domain isEqualToString:kTTCErrorDomain] &&
      IsValidErrorCodeValue(error.code)) {
    return ttc::IsFatal(static_cast<ttc::ErrorCode>(error.code));
  }
  return ttc::IsFatal(ttc::ErrorCode::kUnknown);
}
