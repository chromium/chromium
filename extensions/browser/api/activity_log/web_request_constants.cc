// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used when describing request modifications via the WebRequest API
// in the activity log.

#include "extensions/browser/api/activity_log/web_request_constants.h"

namespace activity_log_web_request_constants {

// Keys used in the dictionary summarizing an EventResponseDelta for the
// extension activity log.
const char kCancelKey[] = "cancel";
const char kNewUrlKey[] = "new_url";
const char kModifiedRequestHeadersKey[] = "modified_request_headers";
const char kDeletedRequestHeadersKey[] = "deleted_request_headers";
const char kAddedRequestHeadersKey[] = "added_request_headers";
const char kDeletedResponseHeadersKey[] = "deleted_response_headers";
const char kAuthCredentialsKey[] = "auth_credentials";
const char kResponseCookieModificationsKey[] = "response_cookie_modifications";

// Keys and values used for describing cookie modifications.
const char kCookieModificationTypeKey[] = "type";
const char kCookieModificationAdd[] = "ADD";
const char kCookieModificationEdit[] = "EDIT";
const char kCookieModificationRemove[] = "REMOVE";
const char kCookieFilterNameKey[] = "filter_name";
const char kCookieFilterDomainKey[] = "filter_domain";
const char kCookieModNameKey[] = "mod_name";
const char kCookieModDomainKey[] = "mod_domain";

}  // namespace activity_log_web_request_constants
