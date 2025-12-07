// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_GROUP_BY_ORIGIN_KEY_H_
#define CONTENT_BROWSER_INTEREST_GROUP_GROUP_BY_ORIGIN_KEY_H_

#include <cstdint>
#include <map>

#include "base/containers/flat_set.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "url/origin.h"

namespace content {

class SingleStorageInterestGroup;

// This class helps assign integer IDs to things that can go together in
// group-by-origin mode within an auction.
class CONTENT_EXPORT GroupByOriginKeyMapper {
 public:
  GroupByOriginKeyMapper();
  ~GroupByOriginKeyMapper();

  // Returns the appropriate group-by-origin ID to use for the interest group
  // `ig`. 0 if this group and the auction config do not use that execution
  // mode.
  size_t LookupGroupByOriginId(
      const SingleStorageInterestGroup& ig,
      const blink::InterestGroup::ExecutionMode execution_mode);

 private:
  struct Key {
    Key();
    Key(Key&&);
    Key(const Key&) = delete;
    ~Key();

    Key& operator=(Key&&);
    Key& operator=(const Key&) = delete;

    url::Origin joining_origin;
    url::Origin bidding_origin;
    base::flat_set<url::Origin> view_and_click_counts_providers;

    friend bool operator==(const Key&, const Key&) = default;
    friend auto operator<=>(const Key&, const Key&) = default;
  };

  std::map<Key, size_t> key_ids_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_GROUP_BY_ORIGIN_KEY_H_
