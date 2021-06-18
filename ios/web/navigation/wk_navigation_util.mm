// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/wk_navigation_util.h"

#include <algorithm>

#include "base/json/json_writer.h"
#include "base/mac/bundle_locations.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "ios/web/common/features.h"
#import "ios/web/navigation/crw_error_page_helper.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/web_client.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"
#include "url/url_constants.h"

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
const char kOriginalUrlKey[] = "for";
NSString* const kReferrerHeaderName = @"Referer";

int GetSafeItemRange(int last_committed_item_index,
                     int item_count,
                     int* offset,
                     int* size) {
  int max_session_size = kMaxSessionSize;
  if (base::FeatureList::IsEnabled(features::kReduceSessionSize)) {
    if (@available(iOS 14.0, *)) {
      // IOS.MetricKit.ForegroundExitData is supported starting from iOS 14, and
      // it's the only good metric to track effect of the session size on OOM
      // crashes.
      max_session_size = base::GetFieldTrialParamByFeatureAsInt(
          features::kReduceSessionSize, "session-size", kMaxSessionSize);
      max_session_size = MIN(max_session_size, kMaxSessionSize);
      max_session_size = MAX(max_session_size, 40);
    }
  }

  *size = std::min(max_session_size, item_count);
  *offset = std::min(last_committed_item_index - max_session_size / 2,
                     item_count - max_session_size);
  *offset = std::max(*offset, 0);
  return last_committed_item_index - *offset;
}

bool IsWKInternalUrl(const GURL& url) {
  return (!base::FeatureList::IsEnabled(web::features::kUseJSForErrorPage) &&
          IsPlaceholderUrl(url)) ||
         IsRestoreSessionUrl(url);
}

bool IsWKInternalUrl(NSURL* url) {
  return (!base::FeatureList::IsEnabled(web::features::kUseJSForErrorPage) &&
          IsPlaceholderUrl(url)) ||
         IsRestoreSessionUrl(url);
}

bool URLNeedsUserAgentType(const GURL& url) {
  if (web::GetWebClient()->IsAppSpecificURL(url))
    return false;

  if (!base::FeatureList::IsEnabled(web::features::kUseJSForErrorPage) &&
      url.SchemeIs(url::kAboutScheme) && IsPlaceholderUrl(url)) {
    return !web::GetWebClient()->IsAppSpecificURL(
        ExtractUrlFromPlaceholderUrl(url));
  }

  if (url.SchemeIs(url::kAboutScheme))
    return false;

  if (url.SchemeIs(url::kFileScheme) && IsRestoreSessionUrl(url))
    return true;

  if (url.SchemeIs(url::kFileScheme) &&
      base::FeatureList::IsEnabled(web::features::kUseJSForErrorPage) &&
      [CRWErrorPageHelper isErrorPageFileURL:url]) {
    return true;
  }

  if (url.SchemeIs(url::kFileScheme))
    return false;

  return true;
}

GURL GetRestoreSessionBaseUrl() {
  std::string restore_session_resource_path = base::SysNSStringToUTF8(
      [base::mac::FrameworkBundle() pathForResource:@"restore_session"
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
  base::Value restored_urls(base::Value::Type::LIST);
  base::Value restored_titles(base::Value::Type::LIST);
  for (int i = first_restored_item_offset;
       i < new_size + first_restored_item_offset; i++) {
    NavigationItem* item = items[i].get();
    restored_urls.Append(item->GetURL().spec());
    restored_titles.Append(item->GetTitle());
  }
  base::Value session(base::Value::Type::DICTIONARY);
  int committed_item_offset = new_last_committed_item_index + 1 - new_size;
  session.SetKey("offset", base::Value(committed_item_offset));
  session.SetKey("urls", std::move(restored_urls));
  session.SetKey("titles", std::move(restored_titles));

  std::string session_json;
  base::JSONWriter::Write(session, &session_json);
  std::string ref =
      kRestoreSessionSessionHashPrefix +
      net::EscapeQueryParamValue(session_json, false /* use_plus */);
  GURL::Replacements replacements;
  replacements.SetRefStr(ref);
  *first_index = first_restored_item_offset;
  *url = GetRestoreSessionBaseUrl().ReplaceComponents(replacements);
}

bool IsRestoreSessionUrl(const GURL& url) {
  return url.SchemeIsFile() && url.path() == GetRestoreSessionBaseUrl().path();
}

bool IsRestoreSessionUrl(NSURL* url) {
  return
      [url.scheme isEqual:@"file"] &&
      [url.path
          isEqual:base::SysUTF8ToNSString(GetRestoreSessionBaseUrl().path())];
}

GURL CreateRedirectUrl(const GURL& target_url) {
  GURL::Replacements replacements;
  std::string ref =
      kRestoreSessionTargetUrlHashPrefix +
      net::EscapeQueryParamValue(target_url.spec(), false /* use_plus */);
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
    *target_url = GURL(net::UnescapeBinaryURLComponent(encoded_target_url));
  }

  return success;
}

bool IsPlaceholderUrl(const GURL& url) {
  DCHECK(!base::FeatureList::IsEnabled(web::features::kUseJSForErrorPage));
  return url.IsAboutBlank() && base::StartsWith(url.query(), kOriginalUrlKey,
                                                base::CompareCase::SENSITIVE);
}

bool IsPlaceholderUrl(NSURL* url) {
  DCHECK(!base::FeatureList::IsEnabled(web::features::kUseJSForErrorPage));
  // about:blank NSURLs don't have nil host and query, so use absolute string
  // matching.
  return [url.scheme isEqual:@"about"] &&
         ([url.absoluteString hasPrefix:@"about:blank?for="] ||
          [url.absoluteString hasPrefix:@"about://blank?for="]);
}

GURL CreatePlaceholderUrlForUrl(const GURL& original_url) {
  DCHECK(!base::FeatureList::IsEnabled(web::features::kUseJSForErrorPage));
  if (!original_url.is_valid())
    return GURL::EmptyGURL();

  GURL placeholder_url = net::AppendQueryParameter(
      GURL(url::kAboutBlankURL), kOriginalUrlKey, original_url.spec());
  DCHECK(placeholder_url.is_valid());
  return placeholder_url;
}

GURL ExtractUrlFromPlaceholderUrl(const GURL& url) {
  DCHECK(!base::FeatureList::IsEnabled(web::features::kUseJSForErrorPage));
  std::string value;
  if (IsPlaceholderUrl(url) &&
      net::GetValueForKeyInQuery(url, kOriginalUrlKey, &value)) {
    GURL decoded_url(value);
    if (decoded_url.is_valid())
      return decoded_url;
  }
  return GURL::EmptyGURL();
}

}  // namespace wk_navigation_util
}  // namespace web
