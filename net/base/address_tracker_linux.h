// Copyright 2012 The Chromium Authors
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

#include "base/compiler_specific.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "net/base/address_map_linux.h"
#include "net/base/ip_address.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"

namespace net::test {
class AddressTrackerLinuxTest;
}

namespace net::internal {

// Keeps track of network interface addresses using rtnetlink. Used by
// NetworkChangeNotifier to provide signals to registered IPAddressObservers.
//
// In tracking mode, this class should mostly be used on a single sequence,
// except GetAddressMap() and GetOnlineLinks() (AddressMapOwnerLinux overrides)
// which can be called on any thread. The main sequence should be able to block
// (e.g. use a base::SequencedTaskRunner with base::MayBlock()).
//
// In non-tracking mode this should be used on a single thread.
class NET_EXPORT_PRIVATE AddressTrackerLinux : public AddressMapOwnerLinux {
 public:
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
  //
  // |blocking_thread_runner| is the sequence on which this AddressTrackerLinux
  // will run. The AddressTrackerLinux can block in tracking mode and so it
  // should run on a sequence that can block, e.g. a base::SequencedTaskRunner
  // with base::MayBlock(). If nullptr, SetDiffCallback() cannot be used off of
  // the AddressTrackerLinux's sequence.
  AddressTrackerLinux(const base::RepeatingClosure& address_callback,
                      const base::RepeatingClosure& link_callback,
                      const base::RepeatingClosure& tunnel_callback,
                      const std::unordered_set<std::string>& ignored_interfaces,
                      scoped_refptr<base::SequencedTaskRunner>
                          blocking_thread_runner = nullptr);
  ~AddressTrackerLinux() override;

  // In tracking mode, it starts watching the system configuration for
  // changes. The current thread must have a MessageLoopForIO. In
  // non-tracking mode, once Init() returns, a snapshot of the system
  // configuration is available through GetOnlineLinks() and
  // GetAddressMap().
  void Init();

  // Same as Init(), except instead of creating and binding a netlink socket,
  // this AddressTrackerLinux will send and receive messages from |fd|.
  void InitWithFdForTesting(base::ScopedFD fd);

  // AddressMapOwnerLinux implementation (callable on any thread):
  AddressMap GetAddressMap() const override;
  // Returns set of interface indices for online interfaces.
  std::unordered_set<int> GetOnlineLinks() const override;

  AddressTrackerLinux* GetAddressTrackerLinux() override;

  // This returns the current AddressMap and set of online links, and atomically
  // starts recording diffs to those structures. This can be called on any
  // thread, and must be called called before SetDiffCallback() below. Available
  // only in tracking mode.
  std::pair<AddressMap, std::unordered_set<int>>
  GetInitialDataAndStartRecordingDiffs();

  // Called after GetInitialDataAndStartRecordingDiffs().
  //
  // Whenever the AddressMap or the set of online links (returned by the above
  // two methods) changes, this callback is called on AddressTrackerLinux's
  // sequence. On the first call, |diff_callback| is called synchronously with
  // the set of diffs that have been built since
  // GetInitialDataAndStartRecordingDiffs() was called. If there are none,
  // |diff_callback| won't be called.
  //
  // This is only available in tracking mode. It can be called on any thread,
  // but it will post a task to the AddressTrackerLinux's sequence and therefore
  // will finish asynchronously. The caller MUST ENSURE that the
  // AddressTrackerLinux is not deleted until this task finishes.
  // This also requires |sequenced_task_runner_| to be set by the
  // AddressTrackerLinux constructor above.
  //
  // Note that other threads may see updated AddressMaps by calling
  // GetAddressMap() before |diff_callback| is ever called.
  void SetDiffCallback(DiffCallback diff_callback);

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
  friend class net::test::AddressTrackerLinuxTest;
  FRIEND_TEST_ALL_PREFIXES(AddressTrackerLinuxNetlinkTest,
                           TestInitializeTwoTrackers);
  FRIEND_TEST_ALL_PREFIXES(AddressTrackerLinuxNetlinkTest,
                           TestInitializeTwoTrackersInPidNamespaces);
  friend int ChildProcessInitializeTrackerForTesting();

  // In tracking mode, holds |lock| while alive. In non-tracking mode,
  // enforces single-threaded access.
  class SCOPED_LOCKABLE AddressTrackerAutoLock {
   public:
    AddressTrackerAutoLock(const AddressTrackerLinux& tracker, base::Lock& lock)
        EXCLUSIVE_LOCK_FUNCTION(lock);
    AddressTrackerAutoLock(const AddressTrackerAutoLock&) = delete;
    AddressTrackerAutoLock& operator=(const AddressTrackerAutoLock&) = delete;
    ~AddressTrackerAutoLock() UNLOCK_FUNCTION();

   private:
    const raw_ref<const AddressTrackerLinux> tracker_;
    const raw_ref<base::Lock> lock_;
  };

  // A function that returns the name of an interface given the interface index
  // in |interface_index|. |ifname| should be a buffer of size IFNAMSIZ. The
  // function should return a pointer to |ifname|.
  typedef char* (*GetInterfaceNameFunction)(int interface_index, char* ifname);

  // Retrieves a dump of the current AddressMap and set of online links as part
  // of initialization. Expects |netlink_fd_| to exist already.
  void DumpInitialAddressesAndWatch();

  // Sets |*address_changed| to indicate whether |address_map_| changed and
  // sets |*link_changed| to indicate if |online_links_| changed and sets
  // |*tunnel_changed| to indicate if |online_links_| changed with regards to a
  // tunnel interface while reading messages from |netlink_fd_|.
  //
  // If |address_map_| has changed and |address_map_diff_| is not nullopt,
  // |*address_map_diff_| is populated with the changes to the AddressMap.
  // Similarly, if |online_links_| has changed and |online_links_diff_| is not
  // nullopt, |*online_links_diff| is populated with the changes to the set of
  // online links.
  void ReadMessages(bool* address_changed,
                    bool* link_changed,
                    bool* tunnel_changed);

  // Sets |*address_changed| to true if |address_map_| changed, sets
  // |*link_changed| to true if |online_links_| changed, sets |*tunnel_changed|
  // to true if |online_links_| changed with regards to a tunnel interface while
  // reading the message from |buffer|.
  //
  // If |address_map_| has changed and |address_map_diff_| is not nullopt,
  // |*address_map_diff_| is populated with the changes to the AddressMap.
  // Similarly, if |online_links_| has changed and |online_links_diff_| is not
  // nullopt, |*online_links_diff| is populated with the changes to the set of
  // online links.
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

  // Passes |address_map_diff_| and |online_links_diff_| to |diff_callback_| as
  // arguments, and then clears them.
  void RunDiffCallback();

  // Used by AddressTrackerLinuxTest, returns the number of threads waiting
  // for |connection_type_initialized_cv_|.
  int GetThreadsWaitingForConnectionTypeInitForTesting();

  // Used by AddressTrackerLinuxNetlinkTest, returns true iff `Init` succeeded.
  // Undefined for non-tracking mode.
  bool DidTrackingInitSucceedForTesting() const;

  AddressMapDiff& address_map_diff_for_testing() {
    AddressTrackerAutoLock lock(*this, address_map_lock_);
    return address_map_diff_.value();
  }
  OnlineLinksDiff& online_links_diff_for_testing() {
    AddressTrackerAutoLock lock(*this, online_links_lock_);
    return online_links_diff_.value();
  }

  // Gets the name of an interface given the interface index |interface_index|.
  // May return empty string if it fails but should not return NULL. This is
  // overridden by tests.
  GetInterfaceNameFunction get_interface_name_;

  DiffCallback diff_callback_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::RepeatingClosure address_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::RepeatingClosure link_callback_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::RepeatingClosure tunnel_callback_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Note that |watcher_| must be inactive when |netlink_fd_| is closed.
  base::ScopedFD netlink_fd_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<base::FileDescriptorWatcher::Controller> watcher_
      GUARDED_BY_CONTEXT(sequence_checker_);

  mutable base::Lock address_map_lock_;
  AddressMap address_map_ GUARDED_BY(address_map_lock_);
  std::optional<AddressMapDiff> address_map_diff_;

  // Set of interface indices for links that are currently online.
  mutable base::Lock online_links_lock_ ACQUIRED_AFTER(address_map_lock_);
  std::unordered_set<int> online_links_ GUARDED_BY(online_links_lock_);
  std::optional<OnlineLinksDiff> online_links_diff_;

  // Set of interface names that should be ignored.
  const std::unordered_set<std::string> ignored_interfaces_;

  base::Lock connection_type_lock_;
  bool connection_type_initialized_ GUARDED_BY(connection_type_lock_) = false;
  base::ConditionVariable connection_type_initialized_cv_;
  NetworkChangeNotifier::ConnectionType current_connection_type_ GUARDED_BY(
      connection_type_lock_) = NetworkChangeNotifier::CONNECTION_NONE;
  int threads_waiting_for_connection_type_initialization_
      GUARDED_BY(connection_type_lock_) = 0;

  const bool tracking_;

  // This can be set by the tracking constructor.
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  // This SequenceChecker is still useful so instance variables above can be
  // marked GUARDED_BY_CONTEXT(sequence_checker_).
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AddressTrackerLinux> weak_ptr_factory_{this};
};

}  // namespace net::internal

#endif  // NET_BASE_ADDRESS_TRACKER_LINUX_H_
