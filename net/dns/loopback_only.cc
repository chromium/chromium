// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/dns/loopback_only.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_interfaces.h"
#include "net/base/sys_addrinfo.h"

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include <net/if.h>
#if BUILDFLAG(IS_ANDROID)
#include "net/android/network_library.h"
#else  // BUILDFLAG(IS_ANDROID)
#include <ifaddrs.h>
#endif  // BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_LINUX)
#include <linux/rtnetlink.h>
#include "net/base/address_map_linux.h"
#include "net/base/address_tracker_linux.h"
#include "net/base/network_interfaces_linux.h"
#endif

namespace net {

namespace {

#if (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)) || BUILDFLAG(IS_FUCHSIA)
bool HaveOnlyLoopbackAddressesUsingGetifaddrs() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  struct ifaddrs* interface_addr = nullptr;
  int rv = getifaddrs(&interface_addr);
  if (rv != 0) {
    DVPLOG(1) << "getifaddrs() failed";
    return false;
  }

  bool result = true;
  for (struct ifaddrs* interface = interface_addr; interface != nullptr;
       interface = interface->ifa_next) {
    if (!(IFF_UP & interface->ifa_flags)) {
      continue;
    }
    if (IFF_LOOPBACK & interface->ifa_flags) {
      continue;
    }
    const struct sockaddr* addr = interface->ifa_addr;
    if (!addr) {
      continue;
    }
    if (addr->sa_family == AF_INET6) {
      // Safe cast since this is AF_INET6.
      const struct sockaddr_in6* addr_in6 =
          reinterpret_cast<const struct sockaddr_in6*>(addr);
      const struct in6_addr* sin6_addr = &addr_in6->sin6_addr;
      if (IN6_IS_ADDR_LOOPBACK(sin6_addr) || IN6_IS_ADDR_LINKLOCAL(sin6_addr)) {
        continue;
      }
    }
    if (addr->sa_family != AF_INET6 && addr->sa_family != AF_INET) {
      continue;
    }

    result = false;
    break;
  }
  freeifaddrs(interface_addr);
  return result;
}
#endif  // (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)) ||
        // BUILDFLAG(IS_FUCHSIA)

// This implementation will always be posted to a thread pool.
bool HaveOnlyLoopbackAddressesSlow() {
#if BUILDFLAG(IS_WIN)
  // TODO(wtc): implement with the GetAdaptersAddresses function.
  NOTIMPLEMENTED();
  return false;
#elif BUILDFLAG(IS_ANDROID)
  return android::HaveOnlyLoopbackAddresses();
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  return HaveOnlyLoopbackAddressesUsingGetifaddrs();
#endif  // defined(various platforms)
}

#if BUILDFLAG(IS_LINUX)
// This implementation can run on the main thread as it will not block.
bool HaveOnlyLoopbackAddressesFast(AddressMapOwnerLinux* address_map_owner) {
  // The AddressMapOwnerLinux has already cached all the information necessary
  // to determine if only loopback addresses exist.
  AddressMapOwnerLinux::AddressMap address_map =
      address_map_owner->GetAddressMap();
  std::unordered_set<int> online_links = address_map_owner->GetOnlineLinks();
  for (const auto& [address, ifaddrmsg] : address_map) {
    // If there is an online link that isn't loopback or IPv6 link-local, return
    // false.
    // `online_links` shouldn't ever contain a loopback address, but keep the
    // check as it is clearer and harmless.
    //
    // NOTE(2023-05-26): `online_links` only contains links with *both*
    // IFF_LOWER_UP and IFF_UP, which is stricter than the
    // HaveOnlyLoopbackAddressesUsingGetifaddrs() check above. LOWER_UP means
    // the physical link layer is up and IFF_UP means the interface is
    // administratively up. This new behavior might even be desirable, but if
    // this causes issues it will need to be reverted.
    if (online_links.contains(ifaddrmsg.ifa_index) && !address.IsLoopback() &&
        !(address.IsIPv6() && address.IsLinkLocal())) {
      return false;
    }
  }

  return true;
}
#endif  // BUILDFLAG(IS_LINUX)

}  // namespace

void RunHaveOnlyLoopbackAddressesJob(
    base::OnceCallback<void(bool)> finished_cb) {
#if BUILDFLAG(IS_LINUX)
  // On Linux, this check can be fast if it accesses only network information
  // that's cached by NetworkChangeNotifier, so there's no need to post this
  // task to a thread pool. If HaveOnlyLoopbackAddressesFast() *is* posted to a
  // different thread, it can cause a TSAN error when also setting a mock
  // NetworkChangeNotifier in tests. So it's important to not run off the main
  // thread if using cached, global information.
  AddressMapOwnerLinux* address_map_owner =
      NetworkChangeNotifier::GetAddressMapOwner();
  if (address_map_owner) {
    // Post `finished_cb` to avoid the bug-prone sometimes-synchronous behavior,
    // which is only useful in latency-sensitive situations.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(finished_cb),
                       HaveOnlyLoopbackAddressesFast(address_map_owner)));
    return;
  }
#endif  // BUILDFLAG(IS_LINUX)

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&HaveOnlyLoopbackAddressesSlow), std::move(finished_cb));
}

}  // namespace net
