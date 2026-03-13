// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_FEATURES_WINDOW_ERROR_FEATURES_H_
#define IOS_WEB_JS_FEATURES_WINDOW_ERROR_FEATURES_H_

#import <string>

#import "base/feature_list.h"

namespace web {

// Feature flag to filter or completely disable JavaScript error reports.
// When this feature is disabled, all JavaScript error reports are uploads.
// If enabled, reports will be filtered by the value of the `regex` feature
// param and only reported if they do NOT match the regex. Reporting can be
// completely disabled by using a regex which matches all strings.
BASE_DECLARE_FEATURE(kIOSJavaScriptErrorReportSignatureFilter);

// Returns whether an error report should be uploaded given the associated error
// report identifier. `signature` must be of the format
// "${ERROR_MESSAGE} (${API_NAME})" to allow for filtering on the API name or
// error message contents.
bool AllowUploadOfJavaScriptErrorReportWithSignature(
    std::string_view signature);

// This function is the same as above, but does not cache the regular expression
// to allow for testing multiple regex params.
bool AllowUploadOfJavaScriptErrorReportWithSignatureForTesting(
    std::string_view signature);

}  // namespace web

#endif  // IOS_WEB_JS_FEATURES_WINDOW_ERROR_FEATURES_H_
