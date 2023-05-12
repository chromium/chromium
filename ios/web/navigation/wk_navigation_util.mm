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

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

GURL GetRestoreSessionBaseUrl() {
  std::string restore_session_resource_path = base::SysNSStringToUTF8(
      [base::apple::FrameworkBundle() pathForResource:@"restore_session"
                                               ofType:@"html"]);
  GURL::Replacements replacements;
  replacements.SetSchemeStr(url::kFileScheme);
  replacements.SetPathStr(restore_session_resource_path);
  return GURL(url::kAboutBlankURL).ReplaceComponents(replacements);
}

void CreateRestoreSessionUrl(
    int last_committed_item_index,
    const std::vector<std::unique_ptr<NavigationItem>>& items,
    GURL* url,
    int* first_index) {
  DCHECK(last_committed_item_index >= 0 &&
         last_committed_item_index < static_cast<int>(items.size()));

  int first_restored_item_offset = 0;
  int new_size = 0;
  int new_last_committed_item_index =
      GetSafeItemRange(last_committed_item_index, items.size(),
                       &first_restored_item_offset, &new_size);

  // The URLs and titles of the restored entries are stored in two separate
  // lists instead of a single list of objects to reduce the size of the JSON
  // string to be included in the query parameter.
  base::Value::List restored_urls;
  base::Value::List restored_titles;
  for (int i = first_restored_item_offset;
       i < new_size + first_restored_item_offset; i++) {
    NavigationItem* item = items[i].get();
    restored_urls.Append(item->GetURL().spec());
    restored_titles.Append(item->GetTitle());
  }
  base::Value::Dict session;
  int committed_item_offset = new_last_committed_item_index + 1 - new_size;
  session.Set("offset", committed_item_offset);
  session.Set("urls", std::move(restored_urls));
  session.Set("titles", std::move(restored_titles));

  std::string session_json;
  base::JSONWriter::Write(session, &session_json);
  std::string ref =
      kRestoreSessionSessionHashPrefix +
      base::EscapeQueryParamValue(session_json, false /* use_plus */);
  GURL::Replacements replacements;
  replacements.SetRefStr(ref);
  *first_index = first_restored_item_offset;
  *url = GetRestoreSessionBaseUrl().ReplaceComponents(replacements);
}

bool IsRestoreSessionUrl(const GURL& url) {
  return url.SchemeIsFile() && url.path() == GetRestoreSessionBaseUrl().path();
}

bool IsRestoreSessionUrl(NSURL* url) {
  return [url.scheme isEqualToString:@"file"] &&
         [url.path isEqualToString:base::SysUTF8ToNSString(
                                       GetRestoreSessionBaseUrl().path())];
}

GURL CreateRedirectUrl(const GURL& target_url) {
  GURL::Replacements replacements;
  std::string ref =
      kRestoreSessionTargetUrlHashPrefix +
      base::EscapeQueryParamValue(target_url.spec(), false /* use_plus */);
  replacements.SetRefStr(ref);
  return GetRestoreSessionBaseUrl().ReplaceComponents(replacements);
}

bool ExtractTargetURL(const GURL& restore_session_url, GURL* target_url) {
  DCHECK(IsRestoreSessionUrl(restore_session_url))
      << restore_session_url.possibly_invalid_spec()
      << " is not a restore session URL";
  std::string target_url_spec;
  bool success =
      restore_session_url.ref().find(kRestoreSessionTargetUrlHashPrefix) == 0;
  if (success) {
    std::string encoded_target_url = restore_session_url.ref().substr(
        strlen(kRestoreSessionTargetUrlHashPrefix));
    *target_url = GURL(base::UnescapeBinaryURLComponent(encoded_target_url));
  }

  return success;
}

}  // namespace wk_navigation_util
}  // namespace web
