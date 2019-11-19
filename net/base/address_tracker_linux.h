// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_ADDRESS_TRACKER_LINUX_H_
#define NET_BASE_ADDRESS_TRACKER_LINUX_H_

#include <sys/socket.h>  // Needed to include netlink.
// Mask superfluous definition of |struct net|. This is fixed in Linux 2.6.38.
#define net net_kernel
#include <linux/rtnetlink.h>
#undef net
#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_set>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "net/base/ip_address.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"

namespace net {
namespace internal {

// Keeps track of network interface addresses using rtnetlink. Used by
// NetworkChangeNotifier to provide signals to registered IPAddressObservers.
class NET_EXPORT_PRIVATE AddressTrackerLinux {
 public:
  typedef std::map<IPAddress, struct ifaddrmsg> AddressMap;

  // Non-tracking version constructor: it takes a snapshot of the
  // current system configuration. Once Init() returns, the
  // configuration is available through GetOnlineLinks() and
  // GetAddressMap().
  AddressTrackerLinux();

  // Tracking version constructor: it will run |address_callback| when
  // the AddressMap changes, |link_callback| when the list of online
  // links changes, and |tunnel_callback| when the list of online
  // tunnels changes.
  // |ignored_interfaces| is the list of interfaces to ignore.  Changes to an
  // ignored interface will not cause any callback to be run. An ignored
  // interface will not have entries in GetAddressMap() and GetOnlineLinks().
  // NOTE: Only ignore interfaces not used to connect to the internet. Adding
  // interfaces used to connect to the internet can cause critical network
  // changed signals to be lost allowing incorrect stale state to persist.
  AddressTrackerLinux(
      const base::Closure& address_callback,
      const base::Closure& link_callback,
      const base::Closure& tunnel_callback,
      const std::unordered_set<std::string>& ignored_interfaces);
  virtual ~AddressTrackerLinux();

  // In tracking mode, it starts watching the system configuration for
  // changes. The current thread must have a MessageLoopForIO. In
  // non-tracking mode, once Init() returns, a snapshot of the system
  // configuration is available through GetOnlineLinks() and
  // GetAddressMap().
  void Init();

  AddressMap GetAddressMap() const;

  // Returns set of interface indicies for online interfaces.
  std::unordered_set<int> GetOnlineLinks() const;

  // Implementation of NetworkChangeNotifierLinux::GetCurrentConnectionType().
  // Safe to call from any thread, but will block until Init() has completed.
  NetworkChangeNotifier::ConnectionType GetCurrentConnectionType();

  // Returns the name for the interface with interface index |interface_index|.
  // |buf| should be a pointer to an array of size IFNAMSIZ. The returned
  // pointer will point to |buf|. This function acts like if_indextoname which
  // cannot be used as net/if.h cannot be mixed with linux/if.h. We'll stick
  // with exclusively talking to the kernel and not the C library.
  static char* GetInterfaceName(int interface_index, char* buf);

  // Does |name| refer to a tunnel interface?
  static bool IsTunnelInterfaceName(const char* name);

 private:
  friend class AddressTrackerLinuxTest;

  // In tracking mode, holds |lock| while alive. In non-tracking mode,
  // enforces single-threaded access.
  class AddressTrackerAutoLock {
   public:
    AddressTrackerAutoLock(const AddressTrackerLinux& tracker,
                           base::Lock& lock);
    ~AddressTrackerAutoLock();

   private:
    const AddressTrackerLinux& tracker_;
    base::Lock& lock_;
    DISALLOW_COPY_AND_ASSIGN(AddressTrackerAutoLock);
  };

  // A function that returns the name of an interface given the interface index
  // in |interface_index|. |ifname| should be a buffer of size IFNAMSIZ. The
  // function should return a pointer to |ifname|.
  typedef char* (*GetInterfaceNameFunction)(int interface_index, char* ifname);

  // Sets |*address_changed| to indicate whether |address_map_| changed and
  // sets |*link_changed| to indicate if |online_links_| changed and sets
  // |*tunnel_changed| to indicate if |online_links_| changed with regards to a
  // tunnel interface while reading messages from |netlink_fd_|.
  void ReadMessages(bool* address_changed,
                    bool* link_changed,
                    bool* tunnel_changed);

  // Sets |*address_changed| to true if |address_map_| changed, sets
  // |*link_changed| to true if |online_links_| changed, sets |*tunnel_changed|
  // to true if |online_links_| changed with regards to a tunnel interface while
  // reading the message from |buffer|.
  void HandleMessage(const char* buffer,
                     int length,
                     bool* address_changed,
                     bool* link_changed,
                     bool* tunnel_changed);

  // Call when some part of initialization failed; forces online and unblocks.
  void AbortAndForceOnline();

  // Called by |watcher_| when |netlink_fd_| can be read without blocking.
  void OnFileCanReadWithoutBlocking();

  // Does |interface_index| refer to a tunnel interface?
  bool IsTunnelInterface(int interface_index) const;

  // Is interface with index |interface_index| in list of ignored interfaces?
  bool IsInterfaceIgnored(int interface_index) const;

  // Updates current_connection_type_ based on the network list.
  void UpdateCurrentConnectionType();

  // Used by AddressTrackerLinuxTest, returns the number of threads waiting
  // for |connection_type_initialized_cv_|.
  int GetThreadsWaitingForConnectionTypeInitForTesting();

  // Gets the name of an interface given the interface index |interface_index|.
  // May return empty string if it fails but should not return NULL. This is
  // overridden by tests.
  GetInterfaceNameFunction get_interface_name_;

  base::Closure address_callback_;
  base::Closure link_callback_;
  base::Closure tunnel_callback_;

  // Note that |watcher_| must be inactive when |netlink_fd_| is closed.
  base::ScopedFD netlink_fd_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> watcher_;

  mutable base::Lock address_map_lock_;
  AddressMap address_map_;

  // Set of interface indices for links that are currently online.
  mutable base::Lock online_links_lock_;
  std::unordered_set<int> online_links_;

  // Set of interface names that should be ignored.
  const std::unordered_set<std::string> ignored_interfaces_;

  base::Lock connection_type_lock_;
  bool connection_type_initialized_;
  base::ConditionVariable connection_type_initialized_cv_;
  NetworkChangeNotifier::ConnectionType current_connection_type_;
  bool tracking_;
  int threads_waiting_for_connection_type_initialization_;

  // Used to verify single-threaded access in non-tracking mode.
  base::ThreadChecker thread_checker_;
};

}  // namespace internal
}  // namespace net

#endif  // NET_BASE_ADDRESS_TRACKER_LINUX_H_
