// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/wk_navigation_util.h"

#import <algorithm>

#import "base/apple/bundle_locations.h"
#import "base/json/json_writer.h"
#import "base/metrics/field_trial_params.h"
#import "base/strings/escape.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "ios/web/common/features.h"
#import "ios/web/navigation/crw_error_page_helper.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/web_client.h"
#import "net/base/url_util.h"
#import "url/url_constants.h"

namespace web {
namespace wk_navigation_util {

// Session restoration algorithms uses pushState calls to restore back forward
// navigation list. WKWebView does not allow pushing more than 100 items per
// 30 seconds. Limiting max session size to 75 will allow web pages to use push
// state calls.
const int kMaxSessionSize = 75;

const char kRestoreSessionSessionHashPrefix[] = "session=";
const char kRestoreSessionTargetUrlHashPrefix[] = "targetUrl=";
NSString* const kReferrerHeaderName = @"Referer";

int GetSafeItemRange(int last_committed_item_index,
                     int item_count,
                     int* offset,
                     int* size) {
  *size = std::min(kMaxSessionSize, item_count);
  *offset = std::min(last_committed_item_index - kMaxSessionSize / 2,
                     item_count - kMaxSessionSize);
  *offset = std::max(*offset, 0);
  return last_committed_item_index - *offset;
}

bool IsWKInternalUrl(const GURL& url) {
  return IsRestoreSessionUrl(url);
}

bool IsWKInternalUrl(NSURL* url) {
  return IsRestoreSessionUrl(url);
}

bool URLNeedsUserAgentType(const GURL& url) {
  if (web::GetWebClient()->IsAppSpecificURL(url))
    return false;

  if (url.SchemeIs(url::kAboutScheme))
    return false;

  if (url.SchemeIs(url::kFileScheme) && IsRestoreSessionUrl(url))
    return true;

  if (url.SchemeIs(url::kFileScheme) &&
      [CRWErrorPageHelper isErrorPageFileURL:url]) {
    return true;
  }

  if (url.SchemeIs(url::kFileScheme))
    return false;

  return true;
}

bool IsRestoreSessionUrl(const GURL& url) {
  return false;
}

bool IsRestoreSessionUrl(NSURL* url) {
  return false;
}

bool ExtractTargetURL(const GURL& restore_session_url, GURL* target_url) {
  // TODO:(crbug.com/40276021): Cleanup this unused function.
  return false;
}

}  // namespace wk_navigation_util
}  // namespace web
