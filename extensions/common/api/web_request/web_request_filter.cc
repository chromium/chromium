// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/web_request/web_request_filter.h"

#include <string>

#include "base/values.h"
#include "extensions/common/api/web_request/web_request_resource_type.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/url_pattern.h"

namespace extensions {

namespace {

constexpr char kInvalidRequestFilterUrl[] = "'*' is not a valid URL pattern.";

}  // namespace

WebRequestParsedFilter::WebRequestParsedFilter() = default;
WebRequestParsedFilter::WebRequestParsedFilter(WebRequestParsedFilter&&) =
    default;
WebRequestParsedFilter& WebRequestParsedFilter::operator=(
    WebRequestParsedFilter&&) = default;
WebRequestParsedFilter::~WebRequestParsedFilter() = default;

bool WebRequestParsedFilter::InitFromValue(const base::DictValue& value,
                                           std::string* error) {
  if (!value.Find(kRequestFilterUrlsKey)) {
    return false;
  }

  for (const auto dict_item : value) {
    if (dict_item.first == kRequestFilterUrlsKey &&
        dict_item.second.is_list()) {
      for (const auto& item : dict_item.second.GetList()) {
        std::string url;
        URLPattern pattern(kWebRequestFilterValidSchemes);
        if (item.is_string()) {
          url = item.GetString();
        }

        // Parse will fail on an empty url, so we don't need to distinguish
        // between `item` not being a string and `item` being an empty string.
        if (url.empty() ||
            pattern.Parse(url) != URLPattern::ParseResult::kSuccess) {
          *error =
              ErrorUtils::FormatErrorMessage(kInvalidRequestFilterUrl, url);
          return false;
        }
        urls.AddPattern(pattern);
      }
    } else if (dict_item.first == kRequestFilterTypesKey &&
               dict_item.second.is_list()) {
      for (const auto& type : dict_item.second.GetList()) {
        std::string type_str;
        if (type.is_string()) {
          type_str = type.GetString();
        }
        types.push_back(WebRequestResourceType::OTHER);
        if (type_str.empty() ||
            !ParseWebRequestResourceType(type_str, &types.back())) {
          return false;
        }
      }
    } else if (dict_item.first == kRequestFilterTabIdKey &&
               dict_item.second.is_int()) {
      tab_id = dict_item.second.GetInt();
    } else if (dict_item.first == kRequestFilterWindowIdKey &&
               dict_item.second.is_int()) {
      window_id = dict_item.second.GetInt();
    } else if (dict_item.first == kRequestFilterOptionsKey) {
      // The renderer-side bindings inject an "_options" key into the
      // filter to pass along some extra information (like `extraInfo` and
      // `webViewInstanceId`). We ignore it here, as it's not a part of the
      // RequestFilter.
      continue;
    } else {
      return false;
    }
  }
  return true;
}

}  // namespace extensions
