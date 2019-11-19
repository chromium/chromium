// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/address_tracker_linux.h"

#include <linux/if.h>

#include <memory>
#include <unordered_set>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/spin_wait.h"
#include "base/test/task_environment.h"
#include "base/threading/simple_thread.h"
#include "net/base/ip_address.h"
#include "testing/gtest/include/gtest/gtest.h"

#ifndef IFA_F_HOMEADDRESS
#define IFA_F_HOMEADDRESS 0x10
#endif

namespace net {
namespace internal {
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

typedef std::vector<char> Buffer;

class AddressTrackerLinuxTest : public testing::Test {
 protected:
  AddressTrackerLinuxTest() = default;

  void InitializeAddressTracker(bool tracking) {
    if (tracking) {
      tracker_.reset(
          new AddressTrackerLinux(base::DoNothing(), base::DoNothing(),
                                  base::DoNothing(), ignored_interfaces_));
    } else {
      tracker_.reset(new AddressTrackerLinux());
    }
    original_get_interface_name_ = tracker_->get_interface_name_;
    tracker_->get_interface_name_ = TestGetInterfaceName;
  }

  bool HandleAddressMessage(const Buffer& buf) {
    Buffer writable_buf = buf;
    bool address_changed = false;
    bool link_changed = false;
    bool tunnel_changed = false;
    tracker_->HandleMessage(&writable_buf[0], buf.size(),
                           &address_changed, &link_changed, &tunnel_changed);
    EXPECT_FALSE(link_changed);
    return address_changed;
  }

  bool HandleLinkMessage(const Buffer& buf) {
    Buffer writable_buf = buf;
    bool address_changed = false;
    bool link_changed = false;
    bool tunnel_changed = false;
    tracker_->HandleMessage(&writable_buf[0], buf.size(),
                           &address_changed, &link_changed, &tunnel_changed);
    EXPECT_FALSE(address_changed);
    return link_changed;
  }

  bool HandleTunnelMessage(const Buffer& buf) {
    Buffer writable_buf = buf;
    bool address_changed = false;
    bool link_changed = false;
    bool tunnel_changed = false;
    tracker_->HandleMessage(&writable_buf[0], buf.size(),
                           &address_changed, &link_changed, &tunnel_changed);
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
};

namespace {

class NetlinkMessage {
 public:
  explicit NetlinkMessage(uint16_t type) : buffer_(NLMSG_HDRLEN) {
    header()->nlmsg_type = type;
    Align();
  }

  void AddPayload(const void* data, size_t length) {
    CHECK_EQ(static_cast<size_t>(NLMSG_HDRLEN),
             buffer_.size()) << "Payload must be added first";
    Append(data, length);
    Align();
  }

  void AddAttribute(uint16_t type, const void* data, size_t length) {
    struct nlattr attr;
    attr.nla_len = NLA_HDRLEN + length;
    attr.nla_type = type;
    Append(&attr, sizeof(attr));
    Align();
    Append(data, length);
    Align();
  }

  void AppendTo(Buffer* output) const {
    CHECK_EQ(NLMSG_ALIGN(output->size()), output->size());
    output->reserve(output->size() + NLMSG_LENGTH(buffer_.size()));
    output->insert(output->end(), buffer_.begin(), buffer_.end());
  }

 private:
  void Append(const void* data, size_t length) {
    const char* chardata = reinterpret_cast<const char*>(data);
    buffer_.insert(buffer_.end(), chardata, chardata + length);
  }

  void Align() {
    header()->nlmsg_len = buffer_.size();
    buffer_.insert(buffer_.end(), NLMSG_ALIGN(buffer_.size()) - buffer_.size(),
                   0);
    CHECK(NLMSG_OK(header(), buffer_.size()));
  }

  struct nlmsghdr* header() {
    return reinterpret_cast<struct nlmsghdr*>(&buffer_[0]);
  }

  Buffer buffer_;
};

#define INFINITY_LIFE_TIME 0xFFFFFFFF

void MakeAddrMessageWithCacheInfo(uint16_t type,
                                  uint8_t flags,
                                  uint8_t family,
                                  int index,
                                  const IPAddress& address,
                                  const IPAddress& local,
                                  uint32_t preferred_lifetime,
                                  Buffer* output) {
  NetlinkMessage nlmsg(type);
  struct ifaddrmsg msg = {};
  msg.ifa_family = family;
  msg.ifa_flags = flags;
  msg.ifa_index = index;
  nlmsg.AddPayload(&msg, sizeof(msg));
  if (address.size())
    nlmsg.AddAttribute(IFA_ADDRESS, address.bytes().data(), address.size());
  if (local.size())
    nlmsg.AddAttribute(IFA_LOCAL, local.bytes().data(), local.size());
  struct ifa_cacheinfo cache_info = {};
  cache_info.ifa_prefered = preferred_lifetime;
  cache_info.ifa_valid = INFINITY_LIFE_TIME;
  nlmsg.AddAttribute(IFA_CACHEINFO, &cache_info, sizeof(cache_info));
  nlmsg.AppendTo(output);
}

void MakeAddrMessage(uint16_t type,
                     uint8_t flags,
                     uint8_t family,
                     int index,
                     const IPAddress& address,
                     const IPAddress& local,
                     Buffer* output) {
  MakeAddrMessageWithCacheInfo(type, flags, family, index, address, local,
                               INFINITY_LIFE_TIME, output);
}

void MakeLinkMessage(uint16_t type,
                     uint32_t flags,
                     uint32_t index,
                     Buffer* output) {
  NetlinkMessage nlmsg(type);
  struct ifinfomsg msg = {};
  msg.ifi_index = index;
  msg.ifi_flags = flags;
  nlmsg.AddPayload(&msg, sizeof(msg));
  output->clear();
  nlmsg.AppendTo(output);
}

// Creates a netlink message generated by wireless_send_event. These events
// should be ignored.
void MakeWirelessLinkMessage(uint16_t type,
                             uint32_t flags,
                             uint32_t index,
                             Buffer* output) {
  NetlinkMessage nlmsg(type);
  struct ifinfomsg msg = {};
  msg.ifi_index = index;
  msg.ifi_flags = flags;
  msg.ifi_change = 0;
  nlmsg.AddPayload(&msg, sizeof(msg));
  char data[8] = {0};
  nlmsg.AddAttribute(IFLA_WIRELESS, data, sizeof(data));
  output->clear();
  nlmsg.AppendTo(output);
}

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

  Buffer buffer;
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

  Buffer buffer;
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

  Buffer buffer;
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

  Buffer buffer;
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

  Buffer buffer;
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

  Buffer buffer;
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
  nlmsg.AddPayload(&msg, sizeof(msg));
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

  Buffer buffer;

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

  Buffer buffer;

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

  Buffer buffer;
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

  Buffer buffer;
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

  Buffer buffer;

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
    EXPECT_NE((const char*)NULL, original_get_interface_name_(i, buf));
  }
}

TEST_F(AddressTrackerLinuxTest, NonTrackingMode) {
  InitializeAddressTracker(false);

  const IPAddress kEmpty;
  const IPAddress kAddr0(kAddress0);

  Buffer buffer;
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
    EXPECT_TRUE(done_.TimedWait(base::TimeDelta::FromSeconds(5)));
    thread_.Join();
  }

 private:
  AddressTrackerLinux* const tracker_;
  base::WaitableEvent done_;
  base::DelegateSimpleThread thread_;
};

TEST_F(AddressTrackerLinuxTest, BroadcastInit) {
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

}  // namespace internal
}  // namespace net
