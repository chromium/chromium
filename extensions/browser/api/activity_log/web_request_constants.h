// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used when describing request modifications via the WebRequest API
// in the activity log.

#ifndef EXTENSIONS_BROWSER_API_ACTIVITY_LOG_WEB_REQUEST_CONSTANTS_H_
#define EXTENSIONS_BROWSER_API_ACTIVITY_LOG_WEB_REQUEST_CONSTANTS_H_

namespace activity_log_web_request_constants {

// Keys used in the dictionary summarizing an EventResponseDelta for the
// extension activity log.
extern const char kCancelKey[];
extern const char kNewUrlKey[];
extern const char kModifiedRequestHeadersKey[];
extern const char kDeletedRequestHeadersKey[];
extern const char kAddedRequestHeadersKey[];
extern const char kDeletedResponseHeadersKey[];
extern const char kAuthCredentialsKey[];
extern const char kResponseCookieModificationsKey[];

// Keys and values used for describing cookie modifications.
extern const char kCookieModificationTypeKey[];
extern const char kCookieModificationAdd[];
extern const char kCookieModificationEdit[];
extern const char kCookieModificationRemove[];
extern const char kCookieFilterNameKey[];
extern const char kCookieFilterDomainKey[];
extern const char kCookieModNameKey[];
extern const char kCookieModDomainKey[];

}  // namespace activity_log_web_request_constants

#endif  // EXTENSIONS_BROWSER_API_ACTIVITY_LOG_WEB_REQUEST_CONSTANTS_H_
