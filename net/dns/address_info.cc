// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/dns/address_info.h"

#include <memory>
#include <optional>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/sys_byteorder.h"
#include "build/build_config.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/sys_addrinfo.h"

#if BUILDFLAG(IS_ANDROID)
#include "net/android/network_library.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace net {

namespace {

const addrinfo* Next(const addrinfo* ai) {
  return ai->ai_next;
}

}  // namespace

//// iterator

AddressInfo::const_iterator::const_iterator(const addrinfo* ai) : ai_(ai) {}

bool AddressInfo::const_iterator::operator!=(
    const AddressInfo::const_iterator& o) const {
  return ai_ != o.ai_;
}

AddressInfo::const_iterator& AddressInfo::const_iterator::operator++() {
  ai_ = Next(ai_);
  return *this;
}

const addrinfo* AddressInfo::const_iterator::operator->() const {
  return ai_;
}

const addrinfo& AddressInfo::const_iterator::operator*() const {
  return *ai_;
}

//// constructors

AddressInfo::AddressInfoAndResult AddressInfo::Get(
    const std::string& host,
    const addrinfo& hints,
    std::unique_ptr<AddrInfoGetter> getter,
    handles::NetworkHandle network) {
  if (getter == nullptr)
    getter = std::make_unique<AddrInfoGetter>();
  int err = OK;
  int os_error = 0;
  std::unique_ptr<addrinfo, FreeAddrInfoFunc> ai =
      getter->getaddrinfo(host, &hints, &os_error, network);

  if (!ai) {
    err = ERR_NAME_NOT_RESOLVED;

    // If the call to getaddrinfo() failed because of a system error, report
    // it separately from ERR_NAME_NOT_RESOLVED.
#if BUILDFLAG(IS_WIN)
    if (os_error != WSAHOST_NOT_FOUND && os_error != WSANO_DATA)
      err = ERR_NAME_RESOLUTION_FAILED;
#elif BUILDFLAG(IS_ANDROID)
    // Workaround for Android's getaddrinfo leaving ai==nullptr without an
    // error.
    // http://crbug.com/134142
    err = ERR_NAME_NOT_RESOLVED;
#elif BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_FREEBSD)
    if (os_error != EAI_NONAME && os_error != EAI_NODATA)
      err = ERR_NAME_RESOLUTION_FAILED;
#endif

    return AddressInfoAndResult(std::optional<AddressInfo>(), err, os_error);
  }

  return AddressInfoAndResult(
      std::optional<AddressInfo>(AddressInfo(std::move(ai), std::move(getter))),
      OK, 0);
}

AddressInfo::AddressInfo(AddressInfo&& other) = default;

AddressInfo& AddressInfo::operator=(AddressInfo&& other) = default;

AddressInfo::~AddressInfo() = default;

//// public methods

AddressInfo::const_iterator AddressInfo::begin() const {
  return const_iterator(ai_.get());
}

AddressInfo::const_iterator AddressInfo::end() const {
  return const_iterator(nullptr);
}

std::optional<std::string> AddressInfo::GetCanonicalName() const {
  return (ai_->ai_canonname != nullptr)
             ? std::optional<std::string>(std::string(ai_->ai_canonname))
             : std::optional<std::string>();
}

bool AddressInfo::IsAllLocalhostOfOneFamily() const {
  bool saw_v4_localhost = false;
  bool saw_v6_localhost = false;
  const auto* ai = ai_.get();
  for (; ai != nullptr; ai = Next(ai)) {
    switch (ai->ai_family) {
      case AF_INET: {
        const struct sockaddr_in* addr_in =
            reinterpret_cast<struct sockaddr_in*>(ai->ai_addr);
        if ((base::NetToHost32(addr_in->sin_addr.s_addr) & 0xff000000) ==
            0x7f000000)
          saw_v4_localhost = true;
        else
          return false;
        break;
      }
      case AF_INET6: {
        const struct sockaddr_in6* addr_in6 =
            reinterpret_cast<struct sockaddr_in6*>(ai->ai_addr);
        if (IN6_IS_ADDR_LOOPBACK(&addr_in6->sin6_addr))
          saw_v6_localhost = true;
        else
          return false;
        break;
      }
      default:
        return false;
    }
  }

  return saw_v4_localhost != saw_v6_localhost;
}

AddressList AddressInfo::CreateAddressList() const {
  AddressList list;
  auto canonical_name = GetCanonicalName();
  if (canonical_name) {
    std::vector<std::string> aliases({*canonical_name});
    list.SetDnsAliases(std::move(aliases));
  }
  for (auto&& ai : *this) {
    IPEndPoint ipe;
    // NOTE: Ignoring non-INET* families.
    if (ipe.FromSockAddr(ai.ai_addr, ai.ai_addrlen))
      list.push_back(ipe);
    else
      DLOG(WARNING) << "Unknown family found in addrinfo: " << ai.ai_family;
  }
  return list;
}

//// private methods

AddressInfo::AddressInfo(std::unique_ptr<addrinfo, FreeAddrInfoFunc> ai,
                         std::unique_ptr<AddrInfoGetter> getter)
    : ai_(std::move(ai)), getter_(std::move(getter)) {}

//// AddrInfoGetter

AddrInfoGetter::AddrInfoGetter() = default;
AddrInfoGetter::~AddrInfoGetter() = default;

std::unique_ptr<addrinfo, FreeAddrInfoFunc> AddrInfoGetter::getaddrinfo(
    const std::string& host,
    const addrinfo* hints,
    int* out_os_error,
    handles::NetworkHandle network) {
  addrinfo* ai;
  // We wrap freeaddrinfo() in a lambda just in case some operating systems use
  // a different signature for it.
  FreeAddrInfoFunc deleter = [](addrinfo* ai) { ::freeaddrinfo(ai); };

  std::unique_ptr<addrinfo, FreeAddrInfoFunc> rv = {nullptr, deleter};

  if (network != handles::kInvalidNetworkHandle) {
    // Currently, only Android supports lookups for a specific network.
#if BUILDFLAG(IS_ANDROID)
    *out_os_error = android::GetAddrInfoForNetwork(network, host.c_str(),
                                                   nullptr, hints, &ai);
#elif BUILDFLAG(IS_WIN)
    *out_os_error = WSAEOPNOTSUPP;
    return rv;
#else
    errno = ENOSYS;
    *out_os_error = EAI_SYSTEM;
    return rv;
#endif  // BUILDFLAG(IS_ANDROID)
  } else {
    *out_os_error = ::getaddrinfo(host.c_str(), nullptr, hints, &ai);
  }

  if (*out_os_error) {
#if BUILDFLAG(IS_WIN)
    *out_os_error = WSAGetLastError();
#endif
    return rv;
  }

  rv.reset(ai);
  return rv;
}

}  // namespace net
