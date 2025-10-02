// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/url_search_params_view.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/to_vector.h"
#include "net/base/url_unescape_iterator.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace net {

namespace {

std::string Unescape(std::string_view view) {
  const auto unescaped_range = MakeUrlUnescapeRange(view);
  std::string decoded(std::ranges::begin(unescaped_range),
                      std::ranges::end(unescaped_range));
  return decoded;
}

}  // namespace

UrlSearchParamsView::UrlSearchParamsView(const GURL& url) {
  for (auto it = QueryIterator(url); !it.IsAtEnd(); it.Advance()) {
    // Keys are actively unescaped and copied during construction. Values are
    // not copied, and are lazily unescaped on use.
    params_.emplace_back(Unescape(it.GetKey()), it.GetValue());
  }
}

UrlSearchParamsView::~UrlSearchParamsView() = default;

bool UrlSearchParamsView::operator==(const UrlSearchParamsView& other) const =
    default;

void UrlSearchParamsView::Sort() {
  if (params_.size() <= 1u) {
    // No sort needed, so avoid doing any work.
    return;
  }

  // Note: the standard specifies sorting by UTF-16 code unit. Here we are
  // sorting by UTF-8 code unit, which will give a different order in some edge
  // cases, but because we only care about normalizing the order, and not the
  // actual order itself, it doesn't matter.
  std::ranges::stable_sort(params_, std::less<>(), &KeyValue::unescaped_key);
}

void UrlSearchParamsView::DeleteAllWithNames(
    const base::flat_set<std::string>& names) {
  absl::erase_if(params_, [&names](const KeyValue& key_value) {
    return names.contains(key_value.unescaped_key);
  });
}

void UrlSearchParamsView::DeleteAllExceptWithNames(
    const base::flat_set<std::string>& names) {
  absl::erase_if(params_, [&names](const KeyValue& key_value) {
    return !names.contains(key_value.unescaped_key);
  });
}

std::vector<std::pair<std::string, std::string>>
UrlSearchParamsView::GetDecodedParamsForTesting() const {
  return base::ToVector(params_, [](const KeyValue& key_value) {
    return std::pair(key_value.unescaped_key,
                     Unescape(key_value.escaped_value));
  });
}

bool UrlSearchParamsView::KeyValue::operator==(const KeyValue& other) const {
  return unescaped_key == other.unescaped_key &&
         EqualsAfterUrlDecoding(escaped_value, other.escaped_value);
}

}  // namespace net
