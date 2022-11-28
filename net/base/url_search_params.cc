// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/url_search_params.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/cxx20_erase_vector.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/url_canon.h"
#include "url/url_canon_internal.h"
#include "url/url_util.h"

namespace net {
namespace {

// The notion of unescaping used in the application/x-www-form-urlencoded
// parser. https://url.spec.whatwg.org/#concept-urlencoded-parser
std::string UnescapePercentEncodedUrl(base::StringPiece input) {
  std::string result(input);
  // Replace any 0x2B (+) with 0x20 (SP).
  for (char& c : result) {
    if (c == '+')
      c = ' ';
  }
  // Run UTF-8 decoding without BOM on the percent-decoding.
  url::RawCanonOutputT<char16_t> canon_output;
  url::DecodeURLEscapeSequences(result.data(), result.size(),
                                url::DecodeURLMode::kUTF8, &canon_output);
  return base::UTF16ToUTF8(
      base::StringPiece16(canon_output.data(), canon_output.length()));
}

}  // namespace

UrlSearchParams::UrlSearchParams(const GURL& url) {
  for (auto it = QueryIterator(url); !it.IsAtEnd(); it.Advance()) {
    // Use unescaped keys and values in order to mitigate potentially different
    // representations for query search params names/values.
    // E.g. a space character might be encoded as '+' or as "%20". A character
    // might be encoded as a character or as its percent encoded
    // representation (e.g. ?%63=2 should be the same as ?c=2). E.g. ぁ would be
    // percent encoded as %E3%81%81. Unescapes the given `key` and `value`
    // using URL escaping rules.
    params_.emplace_back(UnescapePercentEncodedUrl(it.GetKey()),
                         UnescapePercentEncodedUrl(it.GetValue()));
  }
}

UrlSearchParams::~UrlSearchParams() = default;

void UrlSearchParams::Sort() {
  // Note: since query is ASCII and we've Unescaped the keys already,
  // the URL equivalence under No-Vary-Search conditions using the normal string
  // comparison should be enough.
  std::stable_sort(params_.begin(), params_.end(),
                   [](const std::pair<std::string, std::string>& a,
                      const std::pair<std::string, std::string>& b) {
                     return a.first < b.first;
                   });
}

void UrlSearchParams::DeleteAllWithNames(
    const base::flat_set<std::string>& names) {
  base::EraseIf(params_,
                [&](const auto& pair) { return names.contains(pair.first); });
}

void UrlSearchParams::DeleteAllExceptWithNames(
    const base::flat_set<std::string>& names) {
  base::EraseIf(params_,
                [&](const auto& pair) { return !names.contains(pair.first); });
}

const std::vector<std::pair<std::string, std::string>>&
UrlSearchParams::params() const {
  return params_;
}

}  // namespace net
