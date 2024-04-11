// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_reduce_accept_language_controller_delegate.h"

#include <string_view>

#include "base/strings/string_split.h"
#include "content/public/common/origin_util.h"
#include "net/http/http_util.h"
#include "url/gurl.h"

namespace content {

namespace {

std::string GetFirstLanguage(std::string_view language_list) {
  auto end = language_list.find(",");
  return std::string(language_list.substr(0, end));
}

}  // namespace

MockReduceAcceptLanguageControllerDelegate::
    MockReduceAcceptLanguageControllerDelegate(const std::string& languages,
                                               bool is_incognito)
    : is_incognito_(is_incognito) {
  // In incognito mode return only the first language.
  std::string accept_languages_str = net::HttpUtil::ExpandLanguageList(
      is_incognito_ ? GetFirstLanguage(languages) : languages);
  user_accept_languages_ = base::SplitString(
      accept_languages_str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
}

MockReduceAcceptLanguageControllerDelegate::
    ~MockReduceAcceptLanguageControllerDelegate() = default;

std::optional<std::string>
MockReduceAcceptLanguageControllerDelegate::GetReducedLanguage(
    const url::Origin& origin) {
  const auto& iter = reduce_accept_language_map_.find(origin);
  if (iter != reduce_accept_language_map_.end()) {
    return std::make_optional(iter->second);
  }
  return std::nullopt;
}

std::vector<std::string>
MockReduceAcceptLanguageControllerDelegate::GetUserAcceptLanguages() const {
  return user_accept_languages_;
}

void MockReduceAcceptLanguageControllerDelegate::PersistReducedLanguage(
    const url::Origin& origin,
    const std::string& language) {
  if (!origin.GetURL().SchemeIsHTTPOrHTTPS()) {
    return;
  }
  reduce_accept_language_map_[origin] = language;
}

void MockReduceAcceptLanguageControllerDelegate::ClearReducedLanguage(
    const url::Origin& origin) {
  if (!origin.GetURL().SchemeIsHTTPOrHTTPS()) {
    return;
  }
  reduce_accept_language_map_.erase(origin);
}

void MockReduceAcceptLanguageControllerDelegate::SetUserAcceptLanguages(
    const std::string& languages) {
  std::string accept_languages_str =
      net::HttpUtil::ExpandLanguageList(languages);
  user_accept_languages_ =
      base::SplitString(accept_languages_str, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
}

}  // namespace content
