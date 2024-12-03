// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/address_tracker_linux.h"

#include <linux/if.h>
#include <linux/rtnetlink.h>
#include <sched.h>

#include <memory>
#include <unordered_set>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/multiprocess_test.h"
#include "base/test/spin_wait.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/simple_thread.h"
#include "build/build_config.h"
#include "net/base/address_map_cache_linux.h"
#include "net/base/address_map_linux.h"
#include "net/base/address_tracker_linux_test_util.h"
#include "net/base/ip_address.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

#ifndef IFA_F_HOMEADDRESS
#define IFA_F_HOMEADDRESS 0x10
#endif

using net::internal::AddressTrackerLinux;

namespace net::test {
namespace {

const int kTestInterfaceEth = 1;
const int kTestInterfaceWifi = 2;
const int kTestInterfaceTun = 123;
const int kTestInterfaceAp = 456;

const char kIgnoredInterfaceName[] = "uap0";

char* TestGetInterfaceName(int interface_index, char* buf) {
  if (interface_index == kTestInterfaceEth) {
    snprintf(buf, IFNAMSIZ, "%s", "eth0");
  } else if (interface_index == kTestInterfaceTun) {
    snprintf(buf, IFNAMSIZ, "%s", "tun0");
  } else if (interface_index == kTestInterfaceAp) {
    snprintf(buf, IFNAMSIZ, "%s", kIgnoredInterfaceName);
  } else {
    snprintf(buf, IFNAMSIZ, "%s", "");
  }
  return buf;
}

}  // namespace

class AddressTrackerLinuxTest : public testing::Test {
 protected:
  AddressTrackerLinuxTest() = default;

  void InitializeAddressTracker(bool tracking) {
    tracking_ = tracking;
    if (tracking) {
      tracker_ = std::make_unique<AddressTrackerLinux>(
          base::DoNothing(), base::DoNothing(), base::DoNothing(),
          ignored_interfaces_);
#if BUILDFLAG(IS_LINUX)
      const auto& [address_map, online_links] =
          tracker_->GetInitialDataAndStartRecordingDiffs();
      address_map_cache_.SetCachedInfo(address_map, online_links);
#endif  // BUILDFLAG(IS_LINUX)
    } else {
      tracker_ = std::make_unique<AddressTrackerLinux>();
    }
    original_get_interface_name_ = tracker_->get_interface_name_;
    tracker_->get_interface_name_ = TestGetInterfaceName;
  }

  bool HandleAddressMessage(const NetlinkBuffer& buf) {
    NetlinkBuffer writable_buf = buf;
    bool address_changed = false;
    bool link_changed = false;
    bool tunnel_changed = false;
    tracker_->HandleMessage(&writable_buf[0], buf.size(), &address_changed,
                            &link_changed, &tunnel_changed);
    UpdateCache();
    EXPECT_FALSE(link_changed);
    return address_changed;
  }

  bool HandleLinkMessage(const NetlinkBuffer& buf) {
    NetlinkBuffer writable_buf = buf;
    bool address_changed = false;
    bool link_changed = false;
    bool tunnel_changed = false;
    tracker_->HandleMessage(&writable_buf[0], buf.size(), &address_changed,
                            &link_changed, &tunnel_changed);
    UpdateCache();
    EXPECT_FALSE(address_changed);
    return link_changed;
  }

  bool HandleTunnelMessage(const NetlinkBuffer& buf) {
    NetlinkBuffer writable_buf = buf;
    bool address_changed = false;
    bool link_changed = false;
    bool tunnel_changed = false;
    AddressMapOwnerLinux::AddressMapDiff address_map_diff_;
    AddressMapOwnerLinux::OnlineLinksDiff online_links_diff_;
    tracker_->HandleMessage(&writable_buf[0], buf.size(), &address_changed,
                            &link_changed, &tunnel_changed);
    UpdateCache();
    EXPECT_FALSE(address_changed);
    return tunnel_changed;
  }

  AddressTrackerLinux::AddressMap GetAddressMap() {
    return tracker_->GetAddressMap();
  }

  const std::unordered_set<int> GetOnlineLinks() const {
    return tracker_->GetOnlineLinks();
  }

  void IgnoreInterface(const std::string& interface_name) {
    ignored_interfaces_.insert(interface_name);
  }

  int GetThreadsWaitingForConnectionTypeInit() {
    return tracker_->GetThreadsWaitingForConnectionTypeInitForTesting();
  }

  std::unordered_set<std::string> ignored_interfaces_;
  std::unique_ptr<AddressTrackerLinux> tracker_;
  AddressTrackerLinux::GetInterfaceNameFunction original_get_interface_name_;

 private:
  // Checks that applying the generated diff to `address_map_cache_` results in
  // the same AddressMap and set of online links that `tracker_` maintains.
  void UpdateCache() {
    if (!tracking_) {
      return;
    }
#if BUILDFLAG(IS_LINUX)
    address_map_cache_.ApplyDiffs(tracker_->address_map_diff_for_testing(),
                                  tracker_->online_links_diff_for_testing());
    EXPECT_EQ(address_map_cache_.GetAddressMap(), tracker_->GetAddressMap());
    EXPECT_EQ(address_map_cache_.GetOnlineLinks(), tracker_->GetOnlineLinks());
    tracker_->address_map_diff_for_testing().clear();
    tracker_->online_links_diff_for_testing().clear();
#endif  // BUILDFLAG(IS_LINUX)
  }

#if BUILDFLAG(IS_LINUX)
  AddressMapCacheLinux address_map_cache_;
#endif
  bool tracking_;
};

namespace {

const unsigned char kAddress0[] = { 127, 0, 0, 1 };
const unsigned char kAddress1[] = { 10, 0, 0, 1 };
const unsigned char kAddress2[] = { 192, 168, 0, 1 };
const unsigned char kAddress3[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                    0, 0, 0, 1 };

TEST_F(AddressTrackerLinuxTest, NewAddress) {
  InitializeAddressTracker(true);

  const IPAddress kEmpty;
  const IPAddress kAddr0(kAddress0);
  const IPAddress kAddr1(kAddress1);
  const IPAddress kAddr2(kAddress2);
  const IPAddress kAddr3(kAddress3);

  NetlinkBuffer buffer;
  MakeAddrMessage(RTM_NEWADDR, IFA_F_TEMPORARY, AF_INET, kTestInterfaceEth,
                  kAddr0, kEmpty, &buffer);
  EXPECT_TRUE(HandleAddressMessage(buffer));
  AddressTrackerLinux::AddressMap map = GetAddressMap();
  EXPECT_EQ(1u, map.size());
  EXPECT_EQ(1u, map.count(kAddr0));
  EXPECT_EQ(IFA_F_TEMPORARY, map[kAddr0].ifa_flags);

  buffer.clear();
  MakeAddrMessage(RTM_NEWADDR, IFA_F_HOMEADDRESS, AF_INET, kTestInterfaceEth,
                  kAddr1, kAddr2, &buffer);
  EXPECT_TRUE(HandleAddressMessage(buffer));
  map = GetAddressMap();
  EXPECT_EQ(2u, map.size());
  EXPECT_EQ(1u, map.count(kAddr0));
  EXPECT_EQ(1u, map.count(kAddr2));
  EXPECT_EQ(IFA_F_HOMEADDRESS, map[kAddr2].ifa_flags);

  buffer.clear();
  MakeAddrMessage(RTM_NEWADDR, 0, AF_INET6, kTestInterfaceEth, kEmpty, kAddr3,
                  &buffer);
  EXPECT_TRUE(HandleAddressMessage(buffer));
  map = GetAddressMap();
  EXPECT_EQ(3u, map.size());
  EXPECT_EQ(1u, map.count(kAddr3));
}

TEST_F(AddressTrackerLinuxTest, NewAddressChange) {
  InitializeAddressTracker(true);

  const IPAddress kEmpty;
  const IPAddress kAddr0(kAddress0);

  NetlinkBuffer buffer;
  MakeAddrMessage(RTM_NEWADDR, IFA_F_TEMPORARY, AF_INET, kTestInterfaceEth,
                  kAddr0, kEmpty, &buffer);
  EXPECT_TRUE(HandleAddressMessage(buffer));
  AddressTrackerLinux::AddressMap map = GetAddressMap();
  EXPECT_EQ(1u, map.size());
  EXPECT_EQ(1u, map.count(kAddr0));
  EXPECT_EQ(IFA_F_TEMPORARY, map[kAddr0].ifa_flags);

  buffer.clear();
  MakeAddrMessage(RTM_NEWADDR, IFA_F_HOMEADDRESS, AF_INET, kTestInterfaceEth,
                  kAddr0, kEmpty, &buffer);
  EXPECT_TRUE(HandleAddressMessage(buffer));
  map = GetAddressMap();
  EXPECT_EQ(1u, map.size());
  EXPECT_EQ(1u, map.count(kAddr0));
  EXPECT_EQ(IFA_F_HOMEADDRESS, map[kAddr0].ifa_flags);

  // Both messages in one buffer.
  buffer.clear();
  MakeAddrMessage(RTM_NEWADDR, IFA_F_TEMPORARY, AF_INET, kTestInterfaceEth,
                  kAddr0, kEmpty, &buffer);
  MakeAddrMessage(RTM_NEWADDR, IFA_F_HOMEADDRESS, AF_INET, kTestInterfaceEth,
                  kAddr0, kEmpty, &buffer);
  EXPECT_TRUE(HandleAddressMessage(buffer));
  map = GetAddressMap();
  EXPECT_EQ(1u, map.size());
  EXPECT_EQ(IFA_F_HOMEADDRESS, map[kAddr0].ifa_flags);
}

TEST_F(AddressTrackerLinuxTest, NewAddressDuplicate) {
  InitializeAddressTracker(true);

  const IPAddress kAddr0(kAddress0);

  NetlinkBuffer buffer;
  MakeAddrMessage(RTM_NEWADDR, IFA_F_TEMPORARY, AF_INET, kTestInterfaceEth,
                  kAddr0, kAddr0, &buffer);
  EXPECT_TRUE(HandleAddressMessage(buffer));
  AddressTrackerLinux::AddressMap map = GetAddressMap();
  EXPECT_EQ(1u, map.size());
  EXPECT_EQ(1u, map.count(kAddr0));
  EXPECT_EQ(IFA_F_TEMPORARY, map[kAddr0].ifa_flags);

  EXPECT_FALSE(HandleAddressMessage(buffer));
  map = GetAddressMap();
  EXPECT_EQ(1u, map.size());
  EXPECT_EQ(IFA_F_TEMPORARY, map[kAddr0].ifa_flags);
}

TEST_F(AddressTrackerLinuxTest, DeleteAddress) {
  InitializeAddressTracker(true);

  const IPAddress kEmpty;
  const IPAddress kAddr0(kAddress0);
  const IPAddress kAddr1(kAddress1);
  const IPAddress kAddr2(kAddress2);

  NetlinkBuffer buffer;
  MakeAddrMessage(RTM_NEWADDR, 0, AF_INET, kTestInterfaceEth, kAddr0, kEmpty,
                  &buffer);
  MakeAddrMessage(RTM_NEWADDR, 0, AF_INET, kTestInterfaceEth, kAddr1, kAddr2,
                  &buffer);
  EXPECT_TRUE(HandleAddressMessage(buffer));
  AddressTrackerLinux::AddressMap map = GetAddressMap();
  EXPECT_EQ(2u, map.size());

  buffer.clear();
  MakeAddrMessage(RTM_DELADDR, 0, AF_INET, kTestInterfaceEth, kEmpty, kAddr0,
                  &buffer);
  EXPECT_TRUE(HandleAddressMessage(buffer));
  map = GetAddressMap();
  EXPECT_EQ(1u, map.size());
  EXPECT_EQ(0u, map.count(kAddr0));
  EXPECT_EQ(1u, map.count(kAddr2));

  buffer.clear();
  MakeAddrMessage(RTM_DELADDR, 0, AF_INET, kTestInterfaceEth, kAddr2, kAddr1,
                  &buffer);
  // kAddr1 does not exist in the map.
  EXPECT_FALSE(HandleAddressMessage(buffer));
  map = GetAddressMap();
  EXPECT_EQ(1u, map.size());

  buffer.clear();
  MakeAddrMessage(RTM_DELADDR, 0, AF_INET, kTestInterfaceEth, kAddr2, kEmpty,
                  &buffer);
  EXPECT_TRUE(HandleAddressMessage(buffer));
  map = GetAddressMap();
  EXPECT_EQ(0u, map.size());
}

TEST_F(AddressTrackerLinuxTest, DeprecatedLifetime) {
  InitializeAddressTracker(true);

  const IPAddress kEmpty;
  const IPAddress kAddr3(kAddress3);

  NetlinkBuffer buffer;
  MakeAddrMessage(RTM_NEWADDR, 0, AF_INET6, kTestInterfaceEth, kEmpty, kAddr3,
                  &buffer);
  EXPECT_TRUE(HandleAddressMessage(buffer));
  AddressTrackerLinux::AddressMap map = GetAddressMap();
  EXPECT_EQ(1u, map.size());
  EXPECT_EQ(1u, map.count(kAddr3));
  EXPECT_EQ(0, map[kAddr3].ifa_flags);

  // Verify 0 preferred lifetime implies deprecated.
  buffer.clear();
  MakeAddrMessageWithCacheInfo(RTM_NEWADDR, 0, AF_INET6, kTestInterfaceEth,
                               kEmpty, kAddr3, 0, &buffer);
  EXPECT_TRUE(HandleAddressMessage(buffer));
  map = GetAddressMap();
  EXPECT_EQ(1u, map.size());
  EXPECT_EQ(IFA_F_DEPRECATED, map[kAddr3].ifa_flags);

  // Verify properly flagged message doesn't imply change.
  buffer.clear();
  MakeAddrMessageWithCacheInfo(RTM_NEWADDR, IFA_F_DEPRECATED, AF_INET6,
                               kTestInterfaceEth, kEmpty, kAddr3, 0, &buffer);
  EXPECT_FALSE(HandleAddressMessage(buffer));
  map = GetAddressMap();
  EXPECT_EQ(1u, map.size());
  EXPECT_EQ(IFA_F_DEPRECATED, map[kAddr3].ifa_flags);

  // Verify implied deprecated doesn't imply change.
  buffer.clear();
  MakeAddrMessageWithCacheInfo(RTM_NEWADDR, 0, AF_INET6, kTestInterfaceEth,
                               kEmpty, kAddr3, 0, &buffer);
  EXPECT_FALSE(HandleAddressMessage(buffer));
  map = GetAddressMap();
  EXPECT_EQ(1u, map.size());
  EXPECT_EQ(IFA_F_DEPRECATED, map[kAddr3].ifa_flags);
}

TEST_F(AddressTrackerLinuxTest, IgnoredMessage) {
  InitializeAddressTracker(true);

  const IPAddress kEmpty;
  const IPAddress kAddr0(kAddress0);
  const IPAddress kAddr3(kAddress3);

  NetlinkBuffer buffer;
  // Ignored family.
  MakeAddrMessage(RTM_NEWADDR, 0, AF_UNSPEC, kTestInterfaceEth, kAddr3, kAddr0,
                  &buffer);
  // No address.
  MakeAddrMessage(RTM_NEWADDR, 0, AF_INET, kTestInterfaceEth, kEmpty, kEmpty,
                  &buffer);
  // Ignored type.
  MakeAddrMessage(RTM_DELROUTE, 0, AF_INET6, kTestInterfaceEth, kAddr3, kEmpty,
                  &buffer);
  EXPECT_FALSE(HandleAddressMessage(buffer));
  EXPECT_TRUE(GetAddressMap().empty());

  // Valid message after ignored messages.
  NetlinkMessage nlmsg(RTM_NEWADDR);
  struct ifaddrmsg msg = {};
  msg.ifa_family = AF_INET;
  nlmsg.AddPayload(msg);
  // Ignored attribute.
  struct ifa_cacheinfo cache_info = {};
  nlmsg.AddAttribute(IFA_CACHEINFO, &cache_info, sizeof(cache_info));
  nlmsg.AddAttribute(IFA_ADDRESS, kAddr0.bytes().data(), kAddr0.size());
  nlmsg.AppendTo(&buffer);

  EXPECT_TRUE(HandleAddressMessage(buffer));
  EXPECT_EQ(1u, GetAddressMap().size());
}

TEST_F(AddressTrackerLinuxTest, AddInterface) {
  InitializeAddressTracker(true);

  NetlinkBuffer buffer;

  // Ignores loopback.
  MakeLinkMessage(RTM_NEWLINK,
                  IFF_LOOPBACK | IFF_UP | IFF_LOWER_UP | IFF_RUNNING,
                  kTestInterfaceEth, &buffer);
  EXPECT_FALSE(HandleLinkMessage(buffer));
  EXPECT_TRUE(GetOnlineLinks().empty());

  // Ignores not IFF_LOWER_UP.
  MakeLinkMessage(RTM_NEWLINK, IFF_UP | IFF_RUNNING, kTestInterfaceEth,
                  &buffer);
  EXPECT_FALSE(HandleLinkMessage(buffer));
  EXPECT_TRUE(GetOnlineLinks().empty());

  // Ignores deletion.
  MakeLinkMessage(RTM_DELLINK, IFF_UP | IFF_LOWER_UP | IFF_RUNNING,
                  kTestInterfaceEth, &buffer);
  EXPECT_FALSE(HandleLinkMessage(buffer));
  EXPECT_TRUE(GetOnlineLinks().empty());

  // Verify success.
  MakeLinkMessage(RTM_NEWLINK, IFF_UP | IFF_LOWER_UP | IFF_RUNNING,
                  kTestInterfaceEth, &buffer);
  EXPECT_TRUE(HandleLinkMessage(buffer));
  EXPECT_EQ(1u, GetOnlineLinks().count(kTestInterfaceEth));
  EXPECT_EQ(1u, GetOnlineLinks().size());

  // Ignores redundant enables.
  MakeLinkMessage(RTM_NEWLINK, IFF_UP | IFF_LOWER_UP | IFF_RUNNING,
                  kTestInterfaceEth, &buffer);
  EXPECT_FALSE(HandleLinkMessage(buffer));
  EXPECT_EQ(1u, GetOnlineLinks().count(kTestInterfaceEth));
  EXPECT_EQ(1u, GetOnlineLinks().size());

  // Ignores messages from wireless_send_event.
  MakeWirelessLinkMessage(RTM_NEWLINK, IFF_UP | IFF_LOWER_UP | IFF_RUNNING,
                          kTestInterfaceWifi, &buffer);
  EXPECT_FALSE(HandleLinkMessage(buffer));
  EXPECT_EQ(0u, GetOnlineLinks().count(kTestInterfaceWifi));
  EXPECT_EQ(1u, GetOnlineLinks().size());

  // Verify adding another online device (e.g. VPN) is considered a change.
  MakeLinkMessage(RTM_NEWLINK, IFF_UP | IFF_LOWER_UP | IFF_RUNNING, 2, &buffer);
  EXPECT_TRUE(HandleLinkMessage(buffer));
  EXPECT_EQ(1u, GetOnlineLinks().count(kTestInterfaceEth));
  EXPECT_EQ(1u, GetOnlineLinks().count(2));
  EXPECT_EQ(2u, GetOnlineLinks().size());
}

TEST_F(AddressTrackerLinuxTest, RemoveInterface) {
  InitializeAddressTracker(true);

  NetlinkBuffer buffer;

  // Should disappear when not IFF_LOWER_UP.
  MakeLinkMessage(RTM_NEWLINK, IFF_UP | IFF_LOWER_UP | IFF_RUNNING,
                  kTestInterfaceEth, &buffer);
  EXPECT_TRUE(HandleLinkMessage(buffer));
  EXPECT_FALSE(GetOnlineLinks().empty());
  MakeLinkMessage(RTM_NEWLINK, IFF_UP | IFF_RUNNING, kTestInterfaceEth,
                  &buffer);
  EXPECT_TRUE(HandleLinkMessage(buffer));
  EXPECT_TRUE(GetOnlineLinks().empty());

  // Ignores redundant disables.
  MakeLinkMessage(RTM_NEWLINK, IFF_UP | IFF_RUNNING, kTestInterfaceEth,
                  &buffer);
  EXPECT_FALSE(HandleLinkMessage(buffer));
  EXPECT_TRUE(GetOnlineLinks().empty());

  // Ignores deleting down interfaces.
  MakeLinkMessage(RTM_DELLINK, IFF_UP | IFF_RUNNING, kTestInterfaceEth,
                  &buffer);
  EXPECT_FALSE(HandleLinkMessage(buffer));
  EXPECT_TRUE(GetOnlineLinks().empty());

  // Should disappear when deleted.
  MakeLinkMessage(RTM_NEWLINK, IFF_UP | IFF_LOWER_UP | IFF_RUNNING,
                  kTestInterfaceEth, &buffer);
  EXPECT_TRUE(HandleLinkMessage(buffer));
  EXPECT_FALSE(GetOnlineLinks().empty());
  MakeLinkMessage(RTM_DELLINK, IFF_UP | IFF_LOWER_UP | IFF_RUNNING,
                  kTestInterfaceEth, &buffer);
  EXPECT_TRUE(HandleLinkMessage(buffer));
  EXPECT_TRUE(GetOnlineLinks().empty());

  // Ignores messages from wireless_send_event.
  MakeLinkMessage(RTM_NEWLINK, IFF_UP | IFF_LOWER_UP | IFF_RUNNING,
                  kTestInterfaceWifi, &buffer);
  EXPECT_TRUE(HandleLinkMessage(buffer));
  EXPECT_FALSE(GetOnlineLinks().empty());
  MakeWirelessLinkMessage(RTM_NEWLINK, IFF_UP | IFF_LOWER_UP,
                          kTestInterfaceWifi, &buffer);
  EXPECT_FALSE(HandleLinkMessage(buffer));
  EXPECT_FALSE(GetOnlineLinks().empty());
  MakeLinkMessage(RTM_NEWLINK, IFF_UP | IFF_RUNNING, kTestInterfaceWifi,
                  &buffer);
  EXPECT_TRUE(HandleLinkMessage(buffer));
  EXPECT_TRUE(GetOnlineLinks().empty());
}

TEST_F(AddressTrackerLinuxTest, IgnoreInterface) {
  IgnoreInterface(kIgnoredInterfaceName);
  InitializeAddressTracker(true);

  NetlinkBuffer buffer;
  const IPAddress kEmpty;
  const IPAddress kAddr0(kAddress0);

  // Verify online links and address map has been not been updated
  MakeAddrMessage(RTM_NEWADDR, IFA_F_TEMPORARY, AF_INET, kTestInterfaceAp,
                  kAddr0, kEmpty, &buffer);
  EXPECT_FALSE(HandleAddressMessage(buffer));
  AddressTrackerLinux::AddressMap map = GetAddressMap();
  EXPECT_EQ(0u, map.size());
  EXPECT_EQ(0u, map.count(kAddr0));
  MakeLinkMessage(RTM_NEWLINK, IFF_UP | IFF_LOWER_UP | IFF_RUNNING,
                  kTestInterfaceAp, &buffer);
  EXPECT_FALSE(HandleLinkMessage(buffer));
  EXPECT_EQ(0u, GetOnlineLinks().count(kTestInterfaceAp));
  EXPECT_EQ(0u, GetOnlineLinks().size());
}

TEST_F(AddressTrackerLinuxTest, IgnoreInterface_NonIgnoredInterface) {
  IgnoreInterface(kIgnoredInterfaceName);
  InitializeAddressTracker(true);

  NetlinkBuffer buffer;
  const IPAddress kEmpty;
  const IPAddress kAddr0(kAddress0);

  // Verify eth0 is not ignored when only uap0 is ignored
  MakeAddrMessage(RTM_NEWADDR, IFA_F_TEMPORARY, AF_INET, kTestInterfaceEth,
                  kAddr0, kEmpty, &buffer);
  EXPECT_TRUE(HandleAddressMessage(buffer));
  AddressTrackerLinux::AddressMap map = GetAddressMap();
  EXPECT_EQ(1u, map.size());
  EXPECT_EQ(1u, map.count(kAddr0));
  MakeLinkMessage(RTM_NEWLINK, IFF_UP | IFF_LOWER_UP | IFF_RUNNING,
                  kTestInterfaceEth, &buffer);
  EXPECT_TRUE(HandleLinkMessage(buffer));
  EXPECT_EQ(1u, GetOnlineLinks().count(kTestInterfaceEth));
  EXPECT_EQ(1u, GetOnlineLinks().size());
}

TEST_F(AddressTrackerLinuxTest, TunnelInterface) {
  InitializeAddressTracker(true);

  NetlinkBuffer buffer;

  // Ignores without "tun" prefixed name.
  MakeLinkMessage(RTM_NEWLINK,
                  IFF_UP | IFF_LOWER_UP | IFF_RUNNING | IFF_POINTOPOINT,
                  kTestInterfaceEth, &buffer);
  EXPECT_FALSE(HandleTunnelMessage(buffer));

  // Verify success.
  MakeLinkMessage(RTM_NEWLINK,
                  IFF_UP | IFF_LOWER_UP | IFF_RUNNING | IFF_POINTOPOINT,
                  kTestInterfaceTun, &buffer);
  EXPECT_TRUE(HandleTunnelMessage(buffer));

  // Ignores redundant enables.
  MakeLinkMessage(RTM_NEWLINK,
                  IFF_UP | IFF_LOWER_UP | IFF_RUNNING | IFF_POINTOPOINT,
                  kTestInterfaceTun, &buffer);
  EXPECT_FALSE(HandleTunnelMessage(buffer));

  // Ignores deleting without "tun" prefixed name.
  MakeLinkMessage(RTM_DELLINK,
                  IFF_UP | IFF_LOWER_UP | IFF_RUNNING | IFF_POINTOPOINT,
                  0, &buffer);
  EXPECT_FALSE(HandleTunnelMessage(buffer));

  // Verify successful deletion
  MakeLinkMessage(RTM_DELLINK,
                  IFF_UP | IFF_LOWER_UP | IFF_RUNNING | IFF_POINTOPOINT,
                  kTestInterfaceTun, &buffer);
  EXPECT_TRUE(HandleTunnelMessage(buffer));

  // Ignores redundant deletions.
  MakeLinkMessage(RTM_DELLINK,
                  IFF_UP | IFF_LOWER_UP | IFF_RUNNING | IFF_POINTOPOINT,
                  kTestInterfaceTun, &buffer);
  EXPECT_FALSE(HandleTunnelMessage(buffer));
}

// Check AddressTrackerLinux::get_interface_name_ original implementation
// doesn't crash or return NULL.
TEST_F(AddressTrackerLinuxTest, GetInterfaceName) {
  InitializeAddressTracker(true);

  for (int i = 0; i < 10; i++) {
    char buf[IFNAMSIZ] = {0};
    EXPECT_NE((const char*)nullptr, original_get_interface_name_(i, buf));
  }
}

TEST_F(AddressTrackerLinuxTest, NonTrackingMode) {
  InitializeAddressTracker(false);

  const IPAddress kEmpty;
  const IPAddress kAddr0(kAddress0);

  NetlinkBuffer buffer;
  MakeAddrMessage(RTM_NEWADDR, IFA_F_TEMPORARY, AF_INET, kTestInterfaceEth,
                  kAddr0, kEmpty, &buffer);
  EXPECT_TRUE(HandleAddressMessage(buffer));
  AddressTrackerLinux::AddressMap map = GetAddressMap();
  EXPECT_EQ(1u, map.size());
  EXPECT_EQ(1u, map.count(kAddr0));
  EXPECT_EQ(IFA_F_TEMPORARY, map[kAddr0].ifa_flags);

  MakeLinkMessage(RTM_NEWLINK, IFF_UP | IFF_LOWER_UP | IFF_RUNNING, 1, &buffer);
  EXPECT_TRUE(HandleLinkMessage(buffer));
  EXPECT_EQ(1u, GetOnlineLinks().count(1));
  EXPECT_EQ(1u, GetOnlineLinks().size());
}

TEST_F(AddressTrackerLinuxTest, NonTrackingModeInit) {
#if BUILDFLAG(IS_ANDROID)
  // Calling Init() on Android P+ isn't supported.
  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SDK_VERSION_P)
    return;
#endif
  AddressTrackerLinux tracker;
  tracker.Init();
}

class GetCurrentConnectionTypeRunner
    : public base::DelegateSimpleThread::Delegate {
 public:
  explicit GetCurrentConnectionTypeRunner(AddressTrackerLinux* tracker,
                                          const std::string& thread_name)
      : tracker_(tracker),
        done_(base::WaitableEvent::ResetPolicy::MANUAL,
              base::WaitableEvent::InitialState::NOT_SIGNALED),
        thread_(this, thread_name) {}
  ~GetCurrentConnectionTypeRunner() override = default;

  void Run() override {
    tracker_->GetCurrentConnectionType();
    done_.Signal();
  }

  void Start() {
    thread_.Start();
  }

  void VerifyCompletes() {
    EXPECT_TRUE(done_.TimedWait(base::Seconds(5)));
    thread_.Join();
  }

 private:
  const raw_ptr<AddressTrackerLinux> tracker_;
  base::WaitableEvent done_;
  base::DelegateSimpleThread thread_;
};

TEST_F(AddressTrackerLinuxTest, BroadcastInit) {
#if BUILDFLAG(IS_ANDROID)
  // Calling Init() on Android P+ isn't supported.
  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SDK_VERSION_P)
    return;
#endif
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);
  InitializeAddressTracker(true);

  GetCurrentConnectionTypeRunner runner1(tracker_.get(), "waiter_thread_1");
  GetCurrentConnectionTypeRunner runner2(tracker_.get(), "waiter_thread_2");

  runner1.Start();
  runner2.Start();

  SPIN_FOR_1_SECOND_OR_UNTIL_TRUE(
      GetThreadsWaitingForConnectionTypeInit() == 2);

  tracker_->Init();

  runner1.VerifyCompletes();
  runner2.VerifyCompletes();
}

TEST_F(AddressTrackerLinuxTest, TunnelInterfaceName) {
  EXPECT_TRUE(AddressTrackerLinux::IsTunnelInterfaceName("tun0"));
  EXPECT_FALSE(AddressTrackerLinux::IsTunnelInterfaceName("wlan0"));
}

}  // namespace
}  // namespace net::test

namespace net::internal {

// This is a regression test for https://crbug.com/1224428.
//
// This test initializes two instances of `AddressTrackerLinux` in the same
// process. The test will fail if the implementation reuses the value of
// `sockaddr_nl::nl_pid`.
//
// Note: consumers generally should not need to create two tracking instances of
// `AddressTrackerLinux` in the same process.
TEST(AddressTrackerLinuxNetlinkTest, TestInitializeTwoTrackers) {
#if BUILDFLAG(IS_ANDROID)
  // Calling Init() on Android P+ isn't supported.
  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SDK_VERSION_P)
    return;
#endif
  base::test::TaskEnvironment task_env(
      base::test::TaskEnvironment::MainThreadType::IO);
  AddressTrackerLinux tracker1(base::DoNothing(), base::DoNothing(),
                               base::DoNothing(), {});
  AddressTrackerLinux tracker2(base::DoNothing(), base::DoNothing(),
                               base::DoNothing(), {});
  tracker1.Init();
  tracker2.Init();
  EXPECT_TRUE(tracker1.DidTrackingInitSucceedForTesting());
  EXPECT_TRUE(tracker2.DidTrackingInitSucceedForTesting());
}

// These tests use `base::LaunchOptions::clone_flags` for fine-grained control
// over the clone syscall, but the field is only defined on Linux and ChromeOS.
// Unfortunately, this means these tests do not have coverage on Android.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// These tests require specific flag values defined in <sched.h>.
#if defined(CLONE_NEWUSER) && defined(CLONE_NEWPID)

namespace {
const char* const kSwitchParentWriteFd = "addresstrackerlinux_parent_write_fd";
const char* const kSwitchReadFd = "addresstrackerlinux_read_fd";

enum IPCMessage {
  // Sent from child to parent once the child has initialized its tracker.
  kChildInitializedAndWaiting,
  // Sent from child to parent when it was unable to initialize its tracker.
  kChildFailed,
  // Sent from parent to child when all children are permitted to exit.
  kChildMayExit,
};

base::File GetSwitchValueFile(const base::CommandLine* command_line,
                              std::string_view name) {
  std::string value = command_line->GetSwitchValueASCII(name);
  int fd;
  CHECK(base::StringToInt(value, &fd));
  return base::File(fd);
}
}  // namespace

// This is a regression test for https://crbug.com/1224428.
//
// This test creates multiple concurrent `AddressTrackerLinux` instances in
// separate processes, each in their own PID namespaces.
TEST(AddressTrackerLinuxNetlinkTest, TestInitializeTwoTrackersInPidNamespaces) {
  // This test initializes `kNumChildren` instances of `AddressTrackerLinux` in
  // tracking mode, each in their own child process running in a PID namespace.
  // The test will fail if the implementation reuses the value of
  // `sockaddr_nl::nl_pid`.
  //
  // The child processes use pipes to synchronize. Each child initializes a
  // tracker, sends a message to the parent, and waits for the parent to
  // respond, indicating that all children are done setting up. This ensures
  // that the tracker objects have overlapping lifetimes, and thus that the
  // underlying netlink sockets have overlapping lifetimes. This coexistence is
  // necessary, but not sufficient, for a `sockaddr_nl::nl_pid` value collision.
  constexpr size_t kNumChildren = 2;

  base::ScopedFD parent_read_fd, parent_write_fd;
  ASSERT_TRUE(base::CreatePipe(&parent_read_fd, &parent_write_fd));

  struct Child {
    base::ScopedFD read_fd;
    base::ScopedFD write_fd;
    base::Process process;
  } children[kNumChildren];

  for (Child& child : children) {
    ASSERT_TRUE(base::CreatePipe(&child.read_fd, &child.write_fd));

    // Since the child process will wipe its address space by calling execvp, we
    // must share the file descriptors via its command line.
    base::CommandLine command_line(
        base::GetMultiProcessTestChildBaseCommandLine());
    command_line.AppendSwitchASCII(kSwitchParentWriteFd,
                                   base::NumberToString(parent_write_fd.get()));
    command_line.AppendSwitchASCII(kSwitchReadFd,
                                   base::NumberToString(child.read_fd.get()));

    base::LaunchOptions options;
    // Indicate that the child process requires these file descriptors.
    // Otherwise, they will be closed. See `base::CloseSuperfluousFds`.
    options.fds_to_remap = {{child.read_fd.get(), child.read_fd.get()},
                            {parent_write_fd.get(), parent_write_fd.get()}};
    // Clone into a new PID namespace. Making it a new user namespace as well to
    // skirt the CAP_SYS_ADMIN requirement.
    options.clone_flags = CLONE_NEWPID | CLONE_NEWUSER;

    child.process = base::SpawnMultiProcessTestChild(
        "ChildProcessInitializeTrackerForTesting", command_line, options);
  }

  // Wait for all children to finish initializing their tracking
  // AddressTrackerLinuxes.
  base::File parent_reader(std::move(parent_read_fd));
  for (const Child& child : children) {
    ASSERT_TRUE(child.process.IsValid());

    uint8_t message[] = {0};
    ASSERT_TRUE(parent_reader.ReadAtCurrentPosAndCheck(message));
    ASSERT_EQ(message[0], kChildInitializedAndWaiting);
  }

  // Tell children to exit and wait for them to exit.
  for (Child& child : children) {
    base::File child_writer(std::move(child.write_fd));
    const uint8_t kMessage[] = {kChildMayExit};
    ASSERT_TRUE(child_writer.WriteAtCurrentPosAndCheck(kMessage));

    int exit_code = 0;
    ASSERT_TRUE(child.process.WaitForExit(&exit_code));
    ASSERT_EQ(exit_code, 0);
  }
}

MULTIPROCESS_TEST_MAIN(ChildProcessInitializeTrackerForTesting) {
  base::test::TaskEnvironment task_env(
      base::test::TaskEnvironment::MainThreadType::IO);

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  base::File reader = GetSwitchValueFile(command_line, kSwitchReadFd);
  base::File parent_writer =
      GetSwitchValueFile(command_line, kSwitchParentWriteFd);

  // Initialize an `AddressTrackerLinux` in tracking mode and ensure that it
  // created a netlink socket.
  AddressTrackerLinux tracker(base::DoNothing(), base::DoNothing(),
                              base::DoNothing(), {});
  tracker.Init();
  if (!tracker.DidTrackingInitSucceedForTesting()) {
    const uint8_t kMessage[] = {kChildFailed};
    parent_writer.WriteAtCurrentPosAndCheck(kMessage);
    return 1;
  }

  // Signal to the parent that we have initialized the tracker.
  const uint8_t kMessage[] = {kChildInitializedAndWaiting};
  if (!parent_writer.WriteAtCurrentPosAndCheck(kMessage))
    return 1;

  // Block until the parent says all children have initialized their trackers.
  uint8_t message[] = {0};
  if (!reader.ReadAtCurrentPosAndCheck(message) || message[0] != kChildMayExit)
    return 1;
  return 0;
}

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#endif  // defined(CLONE_NEWUSER) && defined(CLONE_NEWPID)

}  // namespace net::internal
