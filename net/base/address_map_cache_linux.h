// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_ADDRESS_MAP_CACHE_LINUX_H_
#define NET_BASE_ADDRESS_MAP_CACHE_LINUX_H_

#include <map>
#include <string>
#include <unordered_set>

#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "net/base/address_map_linux.h"
#include "net/base/net_export.h"

namespace net {

// This caches AddressMap and the set of online links (see AddressMapOwnerLinux)
// so AddressTrackerLinux doesn't need to always be running in every process.
// This class is thread-safe.
class NET_EXPORT AddressMapCacheLinux : public AddressMapOwnerLinux {
 public:
  AddressMapCacheLinux();

  AddressMapCacheLinux(const AddressMapCacheLinux&) = delete;
  AddressMapCacheLinux& operator=(const AddressMapCacheLinux&) = delete;

  ~AddressMapCacheLinux() override;

  // AddressMapOwnerLinux implementation:
  AddressMap GetAddressMap() const override;
  std::unordered_set<int> GetOnlineLinks() const override;

  AddressMapCacheLinux* GetAddressMapCacheLinux() override;

  // Sets `cached_address_map_` and `cached_online_links_`. This should normally
  // only be used to set the initial AddressMap and set of online links.
  void SetCachedInfo(AddressMap address_map,
                     std::unordered_set<int> online_links);

  // Takes the diffs and applies them (atomically) to `cached_address_map_` and
  // `cached_online_links_`.
  // Once this method returns, calls on other threads to GetAddressMap() and
  // GetOnlineLinks() that happen-after this call should return the updated
  // data.
  void ApplyDiffs(const AddressMapDiff& addr_diff,
                  const OnlineLinksDiff& links_diff);

 private:
  mutable base::Lock lock_;
  AddressMap cached_address_map_ GUARDED_BY(lock_);
  std::unordered_set<int> cached_online_links_ GUARDED_BY(lock_);
};

}  // namespace net

#endif  // NET_BASE_ADDRESS_MAP_CACHE_LINUX_H_
