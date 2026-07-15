// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_WEB_REQUEST_WEB_REQUEST_FILTER_H_
#define EXTENSIONS_COMMON_API_WEB_REQUEST_WEB_REQUEST_FILTER_H_

#include <string>
#include <vector>

#include "extensions/buildflags/buildflags.h"
#include "extensions/common/api/web_request/web_request_resource_type.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace base {
class DictValue;
}  // namespace base

namespace extensions {

// The URL schemes a `webRequest.RequestFilter` URL pattern can match.
inline constexpr int kWebRequestFilterValidSchemes =
    URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS |
    URLPattern::SCHEME_FTP | URLPattern::SCHEME_FILE |
    URLPattern::SCHEME_EXTENSION | URLPattern::SCHEME_WS |
    URLPattern::SCHEME_WSS | URLPattern::SCHEME_UUID_IN_PACKAGE;

// Keys of the webRequest.RequestFilter dictionary.
inline constexpr char kRequestFilterUrlsKey[] = "urls";
inline constexpr char kRequestFilterTypesKey[] = "types";
inline constexpr char kRequestFilterTabIdKey[] = "tabId";
inline constexpr char kRequestFilterWindowIdKey[] = "windowId";
inline constexpr char kRequestFilterOptionsKey[] = "_options";

// The parsed matching constraints of a webRequest.RequestFilter, shared by the
// browser-side RequestFilter (which inherits from it) and the renderer-side
// matcher.
struct WebRequestParsedFilter {
  WebRequestParsedFilter();
  WebRequestParsedFilter(WebRequestParsedFilter&&);
  WebRequestParsedFilter& operator=(WebRequestParsedFilter&&);
  ~WebRequestParsedFilter();

  friend bool operator==(const WebRequestParsedFilter&,
                         const WebRequestParsedFilter&) = default;

  // Returns false if there was an error initializing. If it is a user error,
  // an error message is provided, otherwise the error is internal (and
  // unexpected).
  bool InitFromValue(const base::DictValue& value, std::string* error);

  URLPatternSet urls;
  std::vector<WebRequestResourceType> types;
  int tab_id = -1;
  int window_id = -1;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_WEB_REQUEST_WEB_REQUEST_FILTER_H_
