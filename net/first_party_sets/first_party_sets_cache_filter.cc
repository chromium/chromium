// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/first_party_sets_cache_filter.h"

namespace net {

FirstPartySetsCacheFilter::MatchInfo::MatchInfo() = default;

FirstPartySetsCacheFilter::MatchInfo::MatchInfo(
    const FirstPartySetsCacheFilter::MatchInfo& other) = default;

FirstPartySetsCacheFilter::MatchInfo::MatchInfo::~MatchInfo() = default;

bool FirstPartySetsCacheFilter::MatchInfo::operator==(
    const FirstPartySetsCacheFilter::MatchInfo& other) const = default;

FirstPartySetsCacheFilter::FirstPartySetsCacheFilter() = default;
FirstPartySetsCacheFilter::FirstPartySetsCacheFilter(
    base::flat_map<net::SchemefulSite, int64_t> filter,
    int64_t browser_run_id)
    : filter_(std::move(filter)), browser_run_id_(std::move(browser_run_id)) {
  CHECK(browser_run_id != 0 || filter_.empty());
}

FirstPartySetsCacheFilter::FirstPartySetsCacheFilter(
    FirstPartySetsCacheFilter&& other) = default;
FirstPartySetsCacheFilter& FirstPartySetsCacheFilter::operator=(
    FirstPartySetsCacheFilter&& other) = default;

FirstPartySetsCacheFilter::~FirstPartySetsCacheFilter() = default;

bool FirstPartySetsCacheFilter::operator==(
    const FirstPartySetsCacheFilter& other) const = default;

FirstPartySetsCacheFilter FirstPartySetsCacheFilter::Clone() const {
  return FirstPartySetsCacheFilter(filter_, browser_run_id_);
}

FirstPartySetsCacheFilter::MatchInfo FirstPartySetsCacheFilter::GetMatchInfo(
    const net::SchemefulSite& site) const {
  FirstPartySetsCacheFilter::MatchInfo res;
  if (browser_run_id_ > 0) {
    res.browser_run_id = browser_run_id_;
    if (const auto it = filter_.find(site); it != filter_.end())
      res.clear_at_run_id = it->second;
  }
  return res;
}

}  // namespace net
