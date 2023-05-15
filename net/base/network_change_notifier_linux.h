// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_CHANGE_NOTIFIER_LINUX_H_
#define NET_BASE_NETWORK_CHANGE_NOTIFIER_LINUX_H_

#include <memory>
#include <unordered_set>

#include "base/compiler_specific.h"
#include "base/files/scoped_file.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/pass_key.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"

namespace base {
class SequencedTaskRunner;
struct OnTaskRunnerDeleter;
}  // namespace base

namespace net {

class NET_EXPORT_PRIVATE NetworkChangeNotifierLinux
    : public NetworkChangeNotifier {
 public:
  // Creates the object mostly like normal, but the AddressTrackerLinux will use
  // |netlink_fd| instead of creating and binding its own netlink socket.
  static std::unique_ptr<NetworkChangeNotifierLinux> CreateWithSocketForTesting(
      const std::unordered_set<std::string>& ignored_interfaces,
      base::ScopedFD netlink_fd);

  // Creates NetworkChangeNotifierLinux with a list of ignored interfaces.
  // |ignored_interfaces| is the list of interfaces to ignore. An ignored
  // interface will not trigger IP address or connection type notifications.
  // NOTE: Only ignore interfaces not used to connect to the internet. Adding
  // interfaces used to connect to the internet can cause critical network
  // changed signals to be lost allowing incorrect stale state to persist.
  explicit NetworkChangeNotifierLinux(
      const std::unordered_set<std::string>& ignored_interfaces);

  // This constructor can leave the BlockingThreadObjects uninitialized. This
  // is useful in tests that want to mock the netlink dependency of
  // AddressTrackerLinux. The PassKey makes this essentially a private
  // constructor.
  NetworkChangeNotifierLinux(
      const std::unordered_set<std::string>& ignored_interfaces,
      bool initialize_blocking_thread_objects,
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

  // NetworkChangeNotifier:
  ConnectionType GetCurrentConnectionType() const override;

  AddressMapOwnerLinux* GetAddressMapOwnerInternal() override;

  // |blocking_thread_objects_| will live on this runner.
  scoped_refptr<base::SequencedTaskRunner> blocking_thread_runner_;
  // A collection of objects that must live on blocking sequences. These objects
  // listen for notifications and relay the notifications to the registered
  // observers without posting back to the thread the object was created on.
  std::unique_ptr<BlockingThreadObjects, base::OnTaskRunnerDeleter>
      blocking_thread_objects_;
};

}  // namespace net

#endif  // NET_BASE_NETWORK_CHANGE_NOTIFIER_LINUX_H_
