// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_DOES_URL_MATCH_FILTER_H_
#define NET_BASE_DOES_URL_MATCH_FILTER_H_

#include <string>

#include "base/containers/flat_set.h"
#include "net/base/net_export.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

enum class UrlFilterType {
  kTrueIfMatches,
  kFalseIfMatches,
};

// A utility function to determine if a given |url| matches a set of origins or
// domains.
// `filter_type` indicates if we should return true or false for a match.
// `origins` set of url::Origins to match against.
// `domains` set of strings representing registrable domains, IP addresses or
// local hostnames to match against.
// Returns true if |url| matches any of the origins or domains, and
// filter_type == kTrueIfMatches, or |url| doesn't match any of the origins or
// domains and filter_type == kFalseIfMatches.
NET_EXPORT bool DoesUrlMatchFilter(UrlFilterType filter_type,
                                   const base::flat_set<url::Origin>& origins,
                                   const base::flat_set<std::string>& domains,
                                   const GURL& url);

}  // namespace net

#endif  // NET_BASE_DOES_URL_MATCH_FILTER_H_
