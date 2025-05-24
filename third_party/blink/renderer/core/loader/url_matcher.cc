// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/url_matcher.h"

#include <string_view>

#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

UrlMatcher::UrlMatcher(const std::string_view& encoded_url_list_string) {
  ParseFieldTrialParam(encoded_url_list_string);
}

UrlMatcher::~UrlMatcher() = default;

bool UrlMatcher::Match(const KURL& url) const {
  scoped_refptr<const SecurityOrigin> origin = SecurityOrigin::Create(url);
  for (const auto& it : url_list_) {
    // TODO(sisidovski): IsSameOriginWith is more strict but we skip the port
    // number check in order to avoid hardcoding port numbers to corresponding
    // WPT test suites. To check port numbers, we need to set them to the
    // allowlist which is passed by Chrome launch flag or Finch params. But,
    // WPT server could have multiple ports, and it's difficult to expect which
    // ports are available and set to the feature params before starting the
    // test. That will affect the test reliability.
    if ((origin.get()->Protocol() == it.first->Protocol() &&
         origin.get()->Host() == it.first->Host())) {
      // AllowList could only have domain info. In that case the matcher neither
      // cares path nor query strings.
      if (!it.second.has_value())
        return true;
      // Otherwise check if the path or query contains the string.
      if (url.GetPath().ToString().Contains(it.second.value()) ||
          url.Query().ToString().Contains(it.second.value())) {
        return true;
      }
    }
  }

  return false;
}

void UrlMatcher::ParseFieldTrialParam(
    const std::string_view& encoded_url_list_string) {
  Vector<String> parsed_strings;
  String::FromUTF8(encoded_url_list_string)
      .Split(",", /*allow_empty_entries=*/false, parsed_strings);
  Vector<String> site_info;
  for (const auto& it : parsed_strings) {
    it.Split("|", /*allow_empty_entries=*/false, site_info);
    DCHECK_LE(site_info.size(), 2u)
        << "Got unexpected format that UrlMatcher cannot handle: " << it;
    std::optional<String> match_string;
    if (site_info.size() == 2u)
      match_string = site_info[1];
    url_list_.push_back(std::make_pair(
        SecurityOrigin::CreateFromString(site_info[0]), match_string));
  }
}
}  // namespace blink
