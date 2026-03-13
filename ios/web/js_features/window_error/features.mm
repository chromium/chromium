// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/window_error/features.h"

#import <Foundation/Foundation.h>

#import "base/metrics/field_trial_params.h"
#import "base/strings/sys_string_conversions.h"

namespace web {

BASE_FEATURE(kIOSJavaScriptErrorReportSignatureFilter,
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<std::string>
    kIOSJavaScriptErrorReportMessageFilterParam{
        &kIOSJavaScriptErrorReportSignatureFilter,
        /*name=*/"regex",
        /*default_value=*/"[\\s\\S]*"};

bool AllowUploadOfJavaScriptErrorReportWithSignature(
    NSRegularExpression* regex,
    std::string_view signature) {
  // Do not filter messages if filter feature is disabled.
  if (!base::FeatureList::IsEnabled(kIOSJavaScriptErrorReportSignatureFilter)) {
    return true;
  }

  if (!regex) {
    // disable all reporting if the regular expression string could not be
    // parsed.
    return false;
  }

  NSString* ns_signature = base::SysUTF8ToNSString(signature);
  NSTextCheckingResult* first_match =
      [regex firstMatchInString:ns_signature
                        options:0
                          range:NSMakeRange(0, [ns_signature length])];
  if (first_match) {
    // Do not upload reports matching regular expression.
    return false;
  }

  return true;
}

bool AllowUploadOfJavaScriptErrorReportWithSignature(
    std::string_view signature) {
  static NSRegularExpression* regex;
  static dispatch_once_t once_token;
  dispatch_once(&once_token, ^{
    NSString* regex_pattern = base::SysUTF8ToNSString(
        kIOSJavaScriptErrorReportMessageFilterParam.Get());
    regex = [NSRegularExpression regularExpressionWithPattern:regex_pattern
                                                      options:0
                                                        error:nil];
  });

  return AllowUploadOfJavaScriptErrorReportWithSignature(regex, signature);
}

bool AllowUploadOfJavaScriptErrorReportWithSignatureForTesting(
    std::string_view signature) {
  NSString* regex_pattern = base::SysUTF8ToNSString(
      kIOSJavaScriptErrorReportMessageFilterParam.Get());
  NSRegularExpression* regex =
      [NSRegularExpression regularExpressionWithPattern:regex_pattern
                                                options:0
                                                  error:nil];

  return AllowUploadOfJavaScriptErrorReportWithSignature(regex, signature);
}

}  // namespace web
