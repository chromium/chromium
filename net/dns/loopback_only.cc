// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/loopback_only.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
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

}  // namespace

void RunHaveOnlyLoopbackAddressesJob(
    base::OnceCallback<void(bool)> finished_cb) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&HaveOnlyLoopbackAddressesSlow), std::move(finished_cb));
}

}  // namespace net
