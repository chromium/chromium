// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_ADDRESS_MAP_LINUX_H_
#define NET_BASE_ADDRESS_MAP_LINUX_H_

#include <map>
#include <optional>
#include <unordered_set>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "net/base/ip_address.h"
#include "net/base/net_export.h"

struct ifaddrmsg;

namespace net {

class AddressMapCacheLinux;
namespace internal {
class AddressTrackerLinux;
}

// Various components of //net need to access a real-time-updated AddressMap
// (see comments below). For example, AddressSorterPosix (used in DNS
// resolution) and GetNetworkList() (used in many places).
// The methods defined in this interface should be safe to call from any thread.
class NET_EXPORT AddressMapOwnerLinux {
 public:
  // A map from net::IPAddress to netlink's ifaddrmsg, which includes
  // information about the network interface that the IP address is associated
  // with (e.g. interface index).
  using AddressMap = std::map<IPAddress, struct ifaddrmsg>;

  // Represents a diff between one AddressMap and a new one. IPAddresses that
  // map to std::nullopt have been deleted from the map, and IPAddresses that
  // map to non-nullopt have been added or updated.
  using AddressMapDiff =
      base::flat_map<IPAddress, std::optional<struct ifaddrmsg>>;
  // Represents a diff between one set of online links and new one. Interface
  // indices that map to true are newly online and indices that map to false are
  // newly offline.
  using OnlineLinksDiff = base::flat_map<int, bool>;
  // A callback for diffs, to be used by AddressTrackerLinux.
  using DiffCallback =
      base::RepeatingCallback<void(const AddressMapDiff& addr_diff,
                                   const OnlineLinksDiff&)>;

  AddressMapOwnerLinux() = default;

  AddressMapOwnerLinux(const AddressMapOwnerLinux&) = delete;
  AddressMapOwnerLinux& operator=(const AddressMapOwnerLinux&) = delete;

  virtual ~AddressMapOwnerLinux() = default;

  // These functions can be called on any thread. Implementations should use
  // locking if necessary.

  // Returns the current AddressMap.
  virtual AddressMap GetAddressMap() const = 0;
  // Returns set of interface indices for online interfaces.
  virtual std::unordered_set<int> GetOnlineLinks() const = 0;

  // These are the concrete implementations of AddressMapOwnerLinux and these
  // functions serve as an ad-hoc dynamic cast to the concrete implementation,
  // so this base class is not polluted with methods that end up unimplemented
  // in one subclass.
  virtual internal::AddressTrackerLinux* GetAddressTrackerLinux();
  virtual AddressMapCacheLinux* GetAddressMapCacheLinux();
};

}  // namespace net

#endif  // NET_BASE_ADDRESS_MAP_LINUX_H_
