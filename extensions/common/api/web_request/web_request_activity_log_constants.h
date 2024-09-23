// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used when describing request modifications via the WebRequest API
// in the activity log.

#ifndef EXTENSIONS_COMMON_API_WEB_REQUEST_WEB_REQUEST_ACTIVITY_LOG_CONSTANTS_H_
#define EXTENSIONS_COMMON_API_WEB_REQUEST_WEB_REQUEST_ACTIVITY_LOG_CONSTANTS_H_

namespace web_request_activity_log_constants {

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

}  // namespace web_request_activity_log_constants

#endif  // EXTENSIONS_COMMON_API_WEB_REQUEST_WEB_REQUEST_ACTIVITY_LOG_CONSTANTS_H_
