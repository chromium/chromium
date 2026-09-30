// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_CHANGE_NOTIFIER_LINUX_H_
#define NET_BASE_NETWORK_CHANGE_NOTIFIER_LINUX_H_

#include <atomic>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/files/scoped_file.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace base {
class SequencedTaskRunner;
struct OnTaskRunnerDeleter;
}  // namespace base

namespace dbus {
class Bus;
}  // namespace dbus

namespace net {

class NET_EXPORT_PRIVATE NetworkChangeNotifierLinux
    : public NetworkChangeNotifier {
 public:
  enum class PortalOnlineState {
    kUnknown,
    kOnline,
    kOffline,
  };

  class PortalMonitor;

  // Creates the object mostly like normal, but the AddressTrackerLinux will use
  // |netlink_fd| instead of creating and binding its own netlink socket.
  static std::unique_ptr<NetworkChangeNotifierLinux> CreateWithSocketForTesting(
      const absl::flat_hash_set<std::string>& ignored_interfaces,
      base::ScopedFD netlink_fd);

  // Creates the object for testing the D-Bus NetworkMonitor portal with |bus|,
  // leaving netlink uninitialized so it deterministically reports
  // CONNECTION_NONE.
  static std::unique_ptr<NetworkChangeNotifierLinux> CreateWithBusForTesting(
      dbus::Bus* bus);

  // Creates NetworkChangeNotifierLinux with a list of ignored interfaces.
  // |ignored_interfaces| is the list of interfaces to ignore. An ignored
  // interface will not trigger IP address or connection type notifications.
  // NOTE: Only ignore interfaces not used to connect to the internet. Adding
  // interfaces used to connect to the internet can cause critical network
  // changed signals to be lost allowing incorrect stale state to persist.
  explicit NetworkChangeNotifierLinux(
      const absl::flat_hash_set<std::string>& ignored_interfaces);

  // Note: |bus| is passed as a raw pointer rather than scoped_refptr<dbus::Bus>
  // so that dbus::Bus can remain forward-declared here without requiring
  // #include "dbus/bus.h" or #if BUILDFLAG(USE_DBUS) in this header;
  // PortalMonitor::Create immediately wraps |bus| in a scoped_refptr. If
  // non-null, |bus| must have the current thread as its origin thread.
  NetworkChangeNotifierLinux(
      const absl::flat_hash_set<std::string>& ignored_interfaces,
      dbus::Bus* bus);

  // This constructor can leave the BlockingThreadObjects uninitialized (causing
  // netlink to deterministically report CONNECTION_NONE unless
  // InitBlockingThreadObjectsForTesting is called). This is useful in tests
  // that want to mock the netlink or D-Bus portal dependencies.
  NetworkChangeNotifierLinux(
      const absl::flat_hash_set<std::string>& ignored_interfaces,
      bool initialize_blocking_thread_objects,
      dbus::Bus* bus,
      base::PassKey<NetworkChangeNotifierLinux>);

  NetworkChangeNotifierLinux(const NetworkChangeNotifierLinux&) = delete;
  NetworkChangeNotifierLinux& operator=(const NetworkChangeNotifierLinux&) =
      delete;

  ~NetworkChangeNotifierLinux() override;

  static NetworkChangeCalculatorParams NetworkChangeCalculatorParamsLinux();

 private:
  class BlockingThreadObjects;

  // Initializes BlockingThreadObjects, but AddressTrackerLinux will listen to
  // |netlink_fd| rather than the kernel.
  void InitBlockingThreadObjectsForTesting(base::ScopedFD netlink_fd);

  void OnPortalOnlineStateChanged(PortalOnlineState state);
  void OnPortalConnectionCostChanged(ConnectionCost cost);

  // NetworkChangeNotifier:
  ConnectionType GetCurrentConnectionType() const override;
  ConnectionCost GetCurrentConnectionCost() override;

  AddressMapOwnerLinux* GetAddressMapOwnerInternal() override;

  // |blocking_thread_objects_| will live on this runner.
  scoped_refptr<base::SequencedTaskRunner> blocking_thread_runner_;
  // A collection of objects that must live on blocking sequences. These objects
  // listen for notifications and relay the notifications to the registered
  // observers without posting back to the thread the object was created on.
  std::unique_ptr<BlockingThreadObjects, base::OnTaskRunnerDeleter>
      blocking_thread_objects_;

  std::atomic<ConnectionCost> last_computed_connection_cost_{
      ConnectionCost::CONNECTION_COST_UNKNOWN};

  std::unique_ptr<PortalMonitor> portal_monitor_;
  base::WeakPtrFactory<NetworkChangeNotifierLinux> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_BASE_NETWORK_CHANGE_NOTIFIER_LINUX_H_
