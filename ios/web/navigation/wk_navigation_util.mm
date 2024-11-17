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

bool URLNeedsUserAgentType(const GURL& url) {
  if (web::GetWebClient()->IsAppSpecificURL(url))
    return false;

  if (url.SchemeIs(url::kAboutScheme))
    return false;

  if (url.SchemeIs(url::kFileScheme) &&
      [CRWErrorPageHelper isErrorPageFileURL:url]) {
    return true;
  }

  if (url.SchemeIs(url::kFileScheme))
    return false;

  return true;
}

}  // namespace wk_navigation_util
}  // namespace web
