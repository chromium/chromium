// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_VIEW_ERROR_TRANSLATION_UTIL_H_
#define IOS_WEB_WEB_VIEW_ERROR_TRANSLATION_UTIL_H_

#import <Foundation/Foundation.h>

namespace web {

// Translates an CFNetwork error code to a net error code using `net_error_code`
// as an out-parameter.  `url` is URL which failed to load. Returns true if a
// valid translation was found.
bool GetNetErrorFromIOSErrorCode(NSInteger ios_error_code,
                                 int* net_error_code,
                                 NSURL* url);

// Translates an iOS-specific error into its net error equivalent and returns a
// copy of `error` with the translation as its final underlying error.  The
// underlying net error will have an error code of net::ERR_FAILED if no
// specific translation of the iOS error is found.
NSError* NetErrorFromError(NSError* error);

// Same as above but uses `net_error_code` for underlying error.
NSError* NetErrorFromError(NSError* error, int net_error_code);

}  // namespace web

#endif  // IOS_WEB_WEB_VIEW_ERROR_TRANSLATION_UTIL_H_
