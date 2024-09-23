// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FIRST_PARTY_SETS_FIRST_PARTY_SETS_CACHE_FILTER_H_
#define NET_FIRST_PARTY_SETS_FIRST_PARTY_SETS_CACHE_FILTER_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "net/base/net_export.h"
#include "net/base/schemeful_site.h"

namespace mojo {
template <typename DataViewType, typename T>
struct StructTraits;
}  // namespace mojo
namespace network::mojom {
class FirstPartySetsCacheFilterDataView;
}  // namespace network::mojom

namespace net {

// This class stores the First-Party Sets configuration to filter cache access
// for a request in the given network context.
class NET_EXPORT FirstPartySetsCacheFilter {
 public:
  // This struct bundles together the info needed to filter cache for a request.
  struct NET_EXPORT MatchInfo {
    MatchInfo();
    MatchInfo(const MatchInfo& other);
    ~MatchInfo();
    bool operator==(const MatchInfo& other) const;

    // Stores the ID used to check whether cache should be bypassed. Only not
    // null if the request site matches the filter; nullopt if don't match.
    std::optional<int64_t> clear_at_run_id;
    // The ID used to mark the new cache. It should be either a positive number
    // or nullopt.
    std::optional<int64_t> browser_run_id;
  };

  // The default cache filter is no-op.
  FirstPartySetsCacheFilter();
  explicit FirstPartySetsCacheFilter(
      base::flat_map<net::SchemefulSite, int64_t> filter,
      int64_t browser_run_id);

  FirstPartySetsCacheFilter(FirstPartySetsCacheFilter&& other);
  FirstPartySetsCacheFilter& operator=(FirstPartySetsCacheFilter&& other);

  ~FirstPartySetsCacheFilter();

  bool operator==(const FirstPartySetsCacheFilter& other) const;

  FirstPartySetsCacheFilter Clone() const;

  MatchInfo GetMatchInfo(const SchemefulSite& site) const;

 private:
  // mojo (de)serialization needs access to private details.
  friend struct mojo::StructTraits<
      network::mojom::FirstPartySetsCacheFilterDataView,
      FirstPartySetsCacheFilter>;

  // The filter used to bypass cache. The key is site may be bypassed for
  // cache access, the value indicates the browser run of which the site
  // was marked to be cleared.
  base::flat_map<net::SchemefulSite, int64_t> filter_;

  // The id of the current browser run, to mark the cache entry when persisting.
  // The cache filter should be no-op if this is 0.
  // TODO(crbug.com/40489779): Make this optional.
  int64_t browser_run_id_ = 0;
};

}  // namespace net

#endif  // NET_FIRST_PARTY_SETS_FIRST_PARTY_SETS_CACHE_FILTER_H_
