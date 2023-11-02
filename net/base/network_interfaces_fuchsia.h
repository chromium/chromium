// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_INTERFACES_FUCHSIA_H_
#define NET_BASE_NETWORK_INTERFACES_FUCHSIA_H_

#include <fuchsia/net/interfaces/cpp/fidl.h>

#include <vector>

#include "net/base/network_change_notifier.h"
#include "net/base/network_interfaces.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {
namespace internal {

// Move-only wrapper for fuchsia::net::interface::Properties that guarantees
// that its inner |properties_| are valid and complete properties as reported by
// |VerifyCompleteInterfaceProperties|.
class InterfaceProperties final {
 public:
  // Creates an |InterfaceProperties| if |properties| are valid complete
  // properties as reported by |VerifyCompleteInterfaceProperties|.
  static absl::optional<InterfaceProperties> VerifyAndCreate(
      fuchsia::net::interfaces::Properties properties);
  InterfaceProperties(InterfaceProperties&& interface);
  InterfaceProperties& operator=(InterfaceProperties&& interface);
  ~InterfaceProperties();

  // Updates this instance with the values set in |properties|.
  // Fields not set in |properties| retain their previous values.
  // Returns false if the |properties| has a missing or mismatched |id| field,
  // or if any field set in |properties| has an invalid value (e.g. addresses of
  // unknown types).
  bool Update(fuchsia::net::interfaces::Properties properties);

  // Appends the NetworkInterfaces for this interface to |interfaces|.
  void AppendNetworkInterfaces(NetworkInterfaceList* interfaces) const;

  // Returns true if the interface is online and it has either an IPv4 default
  // route and a non-link-local address, or an IPv6 default route and a global
  // address.
  bool IsPubliclyRoutable() const;

  bool HasAddresses() const { return !properties_.addresses().empty(); }
  uint64_t id() const { return properties_.id(); }
  bool online() const { return properties_.online(); }
  const fuchsia::net::interfaces::DeviceClass& device_class() const {
    return properties_.device_class();
  }

 private:
  explicit InterfaceProperties(fuchsia::net::interfaces::Properties properties);

  fuchsia::net::interfaces::Properties properties_;
};

using ExistingInterfaceProperties =
    std::vector<std::pair<uint64_t, InterfaceProperties>>;

// Returns the //net ConnectionType for the supplied netstack interface
// description. Returns CONNECTION_NONE for loopback interfaces.
NetworkChangeNotifier::ConnectionType ConvertConnectionType(
    const fuchsia::net::interfaces::DeviceClass& device_class);

// Connects to the service via the process' ComponentContext, and connects the
// Watcher to the service.
fuchsia::net::interfaces::WatcherHandle ConnectInterfacesWatcher();

// Validates that |properties| contains all the required fields, returning
// |true| if so.
bool VerifyCompleteInterfaceProperties(
    const fuchsia::net::interfaces::Properties& properties);

// Consumes events describing existing interfaces from |watcher| and
// returns a vector of interface Id & properties pairs.  |watcher| must
// be a newly-connected Watcher channel.
// Returns absl::nullopt if any protocol error is encountered, e.g.
// |watcher| supplies an invalid event, or disconnects.
absl::optional<internal::ExistingInterfaceProperties> GetExistingInterfaces(
    const fuchsia::net::interfaces::WatcherSyncPtr& watcher);

}  // namespace internal
}  // namespace net

#endif  // NET_BASE_NETWORK_INTERFACES_FUCHSIA_H_
