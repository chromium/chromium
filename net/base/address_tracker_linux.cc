// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/address_tracker_linux.h"

#include <errno.h>
#include <linux/if.h>
#include <stdint.h>
#include <sys/ioctl.h>

#include "base/bind_helpers.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_current.h"
#include "base/optional.h"
#include "base/posix/eintr_wrapper.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/network_interfaces_linux.h"

namespace net {
namespace internal {

namespace {

// Some kernel functions such as wireless_send_event and rtnetlink_ifinfo_prep
// may send spurious messages over rtnetlink. RTM_NEWLINK messages where
// ifi_change == 0 and rta_type == IFLA_WIRELESS should be ignored.
bool IgnoreWirelessChange(const struct nlmsghdr* header,
                          const struct ifinfomsg* msg) {
  size_t length = IFLA_PAYLOAD(header);
  for (const struct rtattr* attr = IFLA_RTA(msg); RTA_OK(attr, length);
       attr = RTA_NEXT(attr, length)) {
    if (attr->rta_type == IFLA_WIRELESS && msg->ifi_change == 0)
      return true;
  }
  return false;
}

// Retrieves address from NETLINK address message.
// Sets |really_deprecated| for IPv6 addresses with preferred lifetimes of 0.
bool GetAddress(const struct nlmsghdr* header,
                IPAddress* out,
                bool* really_deprecated) {
  if (really_deprecated)
    *really_deprecated = false;
  const struct ifaddrmsg* msg =
      reinterpret_cast<struct ifaddrmsg*>(NLMSG_DATA(header));
  size_t address_length = 0;
  switch (msg->ifa_family) {
    case AF_INET:
      address_length = IPAddress::kIPv4AddressSize;
      break;
    case AF_INET6:
      address_length = IPAddress::kIPv6AddressSize;
      break;
    default:
      // Unknown family.
      return false;
  }
  // Use IFA_ADDRESS unless IFA_LOCAL is present. This behavior here is based on
  // getaddrinfo in glibc (check_pf.c). Judging from kernel implementation of
  // NETLINK, IPv4 addresses have only the IFA_ADDRESS attribute, while IPv6
  // have the IFA_LOCAL attribute.
  uint8_t* address = NULL;
  uint8_t* local = NULL;
  size_t length = IFA_PAYLOAD(header);
  for (const struct rtattr* attr =
           reinterpret_cast<const struct rtattr*>(IFA_RTA(msg));
       RTA_OK(attr, length);
       attr = RTA_NEXT(attr, length)) {
    switch (attr->rta_type) {
      case IFA_ADDRESS:
        DCHECK_GE(RTA_PAYLOAD(attr), address_length);
        address = reinterpret_cast<uint8_t*>(RTA_DATA(attr));
        break;
      case IFA_LOCAL:
        DCHECK_GE(RTA_PAYLOAD(attr), address_length);
        local = reinterpret_cast<uint8_t*>(RTA_DATA(attr));
        break;
      case IFA_CACHEINFO: {
        const struct ifa_cacheinfo *cache_info =
            reinterpret_cast<const struct ifa_cacheinfo*>(RTA_DATA(attr));
        if (really_deprecated)
          *really_deprecated = (cache_info->ifa_prefered == 0);
      } break;
      default:
        break;
    }
  }
  if (local)
    address = local;
  if (!address)
    return false;
  *out = IPAddress(address, address_length);
  return true;
}

}  // namespace

// static
char* AddressTrackerLinux::GetInterfaceName(int interface_index, char* buf) {
  memset(buf, 0, IFNAMSIZ);
  base::ScopedFD ioctl_socket = GetSocketForIoctl();
  if (!ioctl_socket.is_valid())
    return buf;

  struct ifreq ifr = {};
  ifr.ifr_ifindex = interface_index;

  if (ioctl(ioctl_socket.get(), SIOCGIFNAME, &ifr) == 0)
    strncpy(buf, ifr.ifr_name, IFNAMSIZ - 1);
  return buf;
}

AddressTrackerLinux::AddressTrackerLinux()
    : get_interface_name_(GetInterfaceName),
      address_callback_(base::DoNothing()),
      link_callback_(base::DoNothing()),
      tunnel_callback_(base::DoNothing()),
      netlink_fd_(-1),
      watcher_(FROM_HERE),
      ignored_interfaces_(),
      connection_type_initialized_(false),
      connection_type_initialized_cv_(&connection_type_lock_),
      current_connection_type_(NetworkChangeNotifier::CONNECTION_NONE),
      tracking_(false),
      threads_waiting_for_connection_type_initialization_(0) {}

AddressTrackerLinux::AddressTrackerLinux(
    const base::Closure& address_callback,
    const base::Closure& link_callback,
    const base::Closure& tunnel_callback,
    const std::unordered_set<std::string>& ignored_interfaces)
    : get_interface_name_(GetInterfaceName),
      address_callback_(address_callback),
      link_callback_(link_callback),
      tunnel_callback_(tunnel_callback),
      netlink_fd_(-1),
      watcher_(FROM_HERE),
      ignored_interfaces_(ignored_interfaces),
      connection_type_initialized_(false),
      connection_type_initialized_cv_(&connection_type_lock_),
      current_connection_type_(NetworkChangeNotifier::CONNECTION_NONE),
      tracking_(true),
      threads_waiting_for_connection_type_initialization_(0) {
  DCHECK(!address_callback.is_null());
  DCHECK(!link_callback.is_null());
}

AddressTrackerLinux::~AddressTrackerLinux() {
  CloseSocket();
}

void AddressTrackerLinux::Init() {
  netlink_fd_ = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
  if (netlink_fd_ < 0) {
    PLOG(ERROR) << "Could not create NETLINK socket";
    AbortAndForceOnline();
    return;
  }

  int rv;

  if (tracking_) {
    // Request notifications.
    struct sockaddr_nl addr = {};
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();
    // TODO(szym): Track RTMGRP_LINK as well for ifi_type,
    // http://crbug.com/113993
    addr.nl_groups =
        RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR | RTMGRP_NOTIFY | RTMGRP_LINK;
    rv = bind(
        netlink_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    if (rv < 0) {
      PLOG(ERROR) << "Could not bind NETLINK socket";
      AbortAndForceOnline();
      return;
    }
  }

  // Request dump of addresses.
  struct sockaddr_nl peer = {};
  peer.nl_family = AF_NETLINK;

  struct {
    struct nlmsghdr header;
    struct rtgenmsg msg;
  } request = {};

  request.header.nlmsg_len = NLMSG_LENGTH(sizeof(request.msg));
  request.header.nlmsg_type = RTM_GETADDR;
  request.header.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
  request.header.nlmsg_pid = getpid();
  request.msg.rtgen_family = AF_UNSPEC;

  rv = HANDLE_EINTR(sendto(netlink_fd_, &request, request.header.nlmsg_len,
                           0, reinterpret_cast<struct sockaddr*>(&peer),
                           sizeof(peer)));
  if (rv < 0) {
    PLOG(ERROR) << "Could not send NETLINK request";
    AbortAndForceOnline();
    return;
  }

  // Consume pending message to populate the AddressMap, but don't notify.
  // Sending another request without first reading responses results in EBUSY.
  bool address_changed;
  bool link_changed;
  bool tunnel_changed;
  ReadMessages(&address_changed, &link_changed, &tunnel_changed);

  // Request dump of link state
  request.header.nlmsg_type = RTM_GETLINK;

  rv = HANDLE_EINTR(sendto(netlink_fd_, &request, request.header.nlmsg_len, 0,
                           reinterpret_cast<struct sockaddr*>(&peer),
                           sizeof(peer)));
  if (rv < 0) {
    PLOG(ERROR) << "Could not send NETLINK request";
    AbortAndForceOnline();
    return;
  }

  // Consume pending message to populate links_online_, but don't notify.
  ReadMessages(&address_changed, &link_changed, &tunnel_changed);
  {
    AddressTrackerAutoLock lock(*this, connection_type_lock_);
    connection_type_initialized_ = true;
    connection_type_initialized_cv_.Broadcast();
  }

  if (tracking_) {
    rv = base::MessageLoopCurrentForIO::Get()->WatchFileDescriptor(
        netlink_fd_, true, base::MessagePumpForIO::WATCH_READ, &watcher_, this);
    if (rv < 0) {
      PLOG(ERROR) << "Could not watch NETLINK socket";
      AbortAndForceOnline();
      return;
    }
  }
}

void AddressTrackerLinux::AbortAndForceOnline() {
  CloseSocket();
  AddressTrackerAutoLock lock(*this, connection_type_lock_);
  current_connection_type_ = NetworkChangeNotifier::CONNECTION_UNKNOWN;
  connection_type_initialized_ = true;
  connection_type_initialized_cv_.Broadcast();
}

AddressTrackerLinux::AddressMap AddressTrackerLinux::GetAddressMap() const {
  AddressTrackerAutoLock lock(*this, address_map_lock_);
  return address_map_;
}

std::unordered_set<int> AddressTrackerLinux::GetOnlineLinks() const {
  AddressTrackerAutoLock lock(*this, online_links_lock_);
  return online_links_;
}

bool AddressTrackerLinux::IsInterfaceIgnored(int interface_index) const {
  if (ignored_interfaces_.empty())
    return false;

  char buf[IFNAMSIZ] = {0};
  const char* interface_name = get_interface_name_(interface_index, buf);
  return ignored_interfaces_.find(interface_name) != ignored_interfaces_.end();
}

NetworkChangeNotifier::ConnectionType
AddressTrackerLinux::GetCurrentConnectionType() {
  // http://crbug.com/125097
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  AddressTrackerAutoLock lock(*this, connection_type_lock_);
  // Make sure the initial connection type is set before returning.
  threads_waiting_for_connection_type_initialization_++;
  while (!connection_type_initialized_) {
    connection_type_initialized_cv_.Wait();
  }
  threads_waiting_for_connection_type_initialization_--;
  return current_connection_type_;
}

void AddressTrackerLinux::ReadMessages(bool* address_changed,
                                       bool* link_changed,
                                       bool* tunnel_changed) {
  *address_changed = false;
  *link_changed = false;
  *tunnel_changed = false;
  char buffer[4096];
  bool first_loop = true;
  {
    base::Optional<base::ScopedBlockingCall> blocking_call;
    if (tracking_) {
      // If the loop below takes a long time to run, a new thread should added
      // to the current thread pool to ensure forward progress of all tasks.
      base::AssertBlockingAllowedDeprecated();
      blocking_call.emplace(base::BlockingType::MAY_BLOCK);
    }

    for (;;) {
      int rv = HANDLE_EINTR(recv(netlink_fd_, buffer, sizeof(buffer),
                                 // Block the first time through loop.
                                 first_loop ? 0 : MSG_DONTWAIT));
      first_loop = false;
      if (rv == 0) {
        LOG(ERROR) << "Unexpected shutdown of NETLINK socket.";
        return;
      }
      if (rv < 0) {
        if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
          break;
        PLOG(ERROR) << "Failed to recv from netlink socket";
        return;
      }
      HandleMessage(buffer, rv, address_changed, link_changed, tunnel_changed);
    }
  }
  if (*link_changed || *address_changed)
    UpdateCurrentConnectionType();
}

void AddressTrackerLinux::HandleMessage(char* buffer,
                                        size_t length,
                                        bool* address_changed,
                                        bool* link_changed,
                                        bool* tunnel_changed) {
  DCHECK(buffer);
  for (struct nlmsghdr* header = reinterpret_cast<struct nlmsghdr*>(buffer);
       NLMSG_OK(header, length);
       header = NLMSG_NEXT(header, length)) {
    switch (header->nlmsg_type) {
      case NLMSG_DONE:
        return;
      case NLMSG_ERROR: {
        const struct nlmsgerr* msg =
            reinterpret_cast<struct nlmsgerr*>(NLMSG_DATA(header));
        LOG(ERROR) << "Unexpected netlink error " << msg->error << ".";
      } return;
      case RTM_NEWADDR: {
        IPAddress address;
        bool really_deprecated;
        struct ifaddrmsg* msg =
            reinterpret_cast<struct ifaddrmsg*>(NLMSG_DATA(header));
        if (IsInterfaceIgnored(msg->ifa_index))
          break;
        if (GetAddress(header, &address, &really_deprecated)) {
          AddressTrackerAutoLock lock(*this, address_map_lock_);
          // Routers may frequently (every few seconds) output the IPv6 ULA
          // prefix which can cause the linux kernel to frequently output two
          // back-to-back messages, one without the deprecated flag and one with
          // the deprecated flag but both with preferred lifetimes of 0. Avoid
          // interpretting this as an actual change by canonicalizing the two
          // messages by setting the deprecated flag based on the preferred
          // lifetime also.  http://crbug.com/268042
          if (really_deprecated)
            msg->ifa_flags |= IFA_F_DEPRECATED;
          // Only indicate change if the address is new or ifaddrmsg info has
          // changed.
          auto it = address_map_.find(address);
          if (it == address_map_.end()) {
            address_map_.insert(it, std::make_pair(address, *msg));
            *address_changed = true;
          } else if (memcmp(&it->second, msg, sizeof(*msg))) {
            it->second = *msg;
            *address_changed = true;
          }
        }
      } break;
      case RTM_DELADDR: {
        IPAddress address;
        const struct ifaddrmsg* msg =
            reinterpret_cast<struct ifaddrmsg*>(NLMSG_DATA(header));
        if (IsInterfaceIgnored(msg->ifa_index))
          break;
        if (GetAddress(header, &address, NULL)) {
          AddressTrackerAutoLock lock(*this, address_map_lock_);
          if (address_map_.erase(address))
            *address_changed = true;
        }
      } break;
      case RTM_NEWLINK: {
        const struct ifinfomsg* msg =
            reinterpret_cast<struct ifinfomsg*>(NLMSG_DATA(header));
        if (IsInterfaceIgnored(msg->ifi_index))
          break;
        if (IgnoreWirelessChange(header, msg)) {
          VLOG(2) << "Ignoring RTM_NEWLINK message";
          break;
        }
        if (!(msg->ifi_flags & IFF_LOOPBACK) && (msg->ifi_flags & IFF_UP) &&
            (msg->ifi_flags & IFF_LOWER_UP) && (msg->ifi_flags & IFF_RUNNING)) {
          AddressTrackerAutoLock lock(*this, online_links_lock_);
          if (online_links_.insert(msg->ifi_index).second) {
            *link_changed = true;
            if (IsTunnelInterface(msg->ifi_index))
              *tunnel_changed = true;
          }
        } else {
          AddressTrackerAutoLock lock(*this, online_links_lock_);
          if (online_links_.erase(msg->ifi_index)) {
            *link_changed = true;
            if (IsTunnelInterface(msg->ifi_index))
              *tunnel_changed = true;
          }
        }
      } break;
      case RTM_DELLINK: {
        const struct ifinfomsg* msg =
            reinterpret_cast<struct ifinfomsg*>(NLMSG_DATA(header));
        if (IsInterfaceIgnored(msg->ifi_index))
          break;
        AddressTrackerAutoLock lock(*this, online_links_lock_);
        if (online_links_.erase(msg->ifi_index)) {
          *link_changed = true;
          if (IsTunnelInterface(msg->ifi_index))
            *tunnel_changed = true;
        }
      } break;
      default:
        break;
    }
  }
}

void AddressTrackerLinux::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_EQ(netlink_fd_, fd);
  bool address_changed;
  bool link_changed;
  bool tunnel_changed;
  ReadMessages(&address_changed, &link_changed, &tunnel_changed);
  if (address_changed)
    address_callback_.Run();
  if (link_changed)
    link_callback_.Run();
  if (tunnel_changed)
    tunnel_callback_.Run();
}

void AddressTrackerLinux::OnFileCanWriteWithoutBlocking(int /* fd */) {}

void AddressTrackerLinux::CloseSocket() {
  if (netlink_fd_ >= 0 && IGNORE_EINTR(close(netlink_fd_)) < 0)
    PLOG(ERROR) << "Could not close NETLINK socket.";
  netlink_fd_ = -1;
}

bool AddressTrackerLinux::IsTunnelInterface(int interface_index) const {
  char buf[IFNAMSIZ] = {0};
  return IsTunnelInterfaceName(get_interface_name_(interface_index, buf));
}

// static
bool AddressTrackerLinux::IsTunnelInterfaceName(const char* name) {
  // Linux kernel drivers/net/tun.c uses "tun" name prefix.
  return strncmp(name, "tun", 3) == 0;
}

void AddressTrackerLinux::UpdateCurrentConnectionType() {
  AddressTrackerLinux::AddressMap address_map = GetAddressMap();
  std::unordered_set<int> online_links = GetOnlineLinks();

  // Strip out tunnel interfaces from online_links
  for (auto it = online_links.cbegin(); it != online_links.cend();) {
    if (IsTunnelInterface(*it)) {
      it = online_links.erase(it);
    } else {
      ++it;
    }
  }

  NetworkInterfaceList networks;
  NetworkChangeNotifier::ConnectionType type =
      NetworkChangeNotifier::CONNECTION_NONE;
  if (GetNetworkListImpl(&networks, 0, online_links, address_map,
                         get_interface_name_)) {
    type = NetworkChangeNotifier::ConnectionTypeFromInterfaceList(networks);
  } else {
    type = online_links.empty() ? NetworkChangeNotifier::CONNECTION_NONE
                                : NetworkChangeNotifier::CONNECTION_UNKNOWN;
  }

  AddressTrackerAutoLock lock(*this, connection_type_lock_);
  current_connection_type_ = type;
}

int AddressTrackerLinux::GetThreadsWaitingForConnectionTypeInitForTesting()
{
  AddressTrackerAutoLock lock(*this, connection_type_lock_);
  return threads_waiting_for_connection_type_initialization_;
}

AddressTrackerLinux::AddressTrackerAutoLock::AddressTrackerAutoLock(
    const AddressTrackerLinux& tracker,
    base::Lock& lock)
    : tracker_(tracker), lock_(lock) {
  if (tracker_.tracking_) {
    lock_.Acquire();
  } else {
    DCHECK(tracker_.thread_checker_.CalledOnValidThread());
  }
}

AddressTrackerLinux::AddressTrackerAutoLock::~AddressTrackerAutoLock() {
  if (tracker_.tracking_) {
    lock_.AssertAcquired();
    lock_.Release();
  }
}

}  // namespace internal
}  // namespace net
