// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/address_info.h"

#include "base/logging.h"
#include "base/sys_byteorder.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/sys_addrinfo.h"

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
    std::unique_ptr<AddrInfoGetter> getter) {
  if (getter == nullptr)
    getter = std::make_unique<AddrInfoGetter>();
  int err = OK;
  int os_error = 0;
  addrinfo* ai = getter->getaddrinfo(host, &hints, &os_error);

  if (!ai) {
    err = ERR_NAME_NOT_RESOLVED;

    // If the call to getaddrinfo() failed because of a system error, report
    // it separately from ERR_NAME_NOT_RESOLVED.
#if defined(OS_WIN)
    if (os_error != WSAHOST_NOT_FOUND && os_error != WSANO_DATA)
      err = ERR_NAME_RESOLUTION_FAILED;
#elif defined(OS_ANDROID)
    // Workaround for Android's getaddrinfo leaving ai==nullptr without an
    // error.
    // http://crbug.com/134142
    err = ERR_NAME_NOT_RESOLVED;
#elif defined(OS_POSIX) && !defined(OS_FREEBSD)
    if (os_error != EAI_NONAME && os_error != EAI_NODATA)
      err = ERR_NAME_RESOLUTION_FAILED;
#endif

    return AddressInfoAndResult(base::Optional<AddressInfo>(), err, os_error);
  }

  return AddressInfoAndResult(
      base::Optional<AddressInfo>(AddressInfo(ai, std::move(getter))), OK, 0);
}

AddressInfo::AddressInfo(AddressInfo&& other)
    : ai_(other.ai_), getter_(std::move(other.getter_)) {
  other.ai_ = nullptr;
}

AddressInfo& AddressInfo::operator=(AddressInfo&& other) {
  ai_ = other.ai_;
  other.ai_ = nullptr;
  getter_ = std::move(other.getter_);
  return *this;
}

AddressInfo::~AddressInfo() {
  if (ai_)
    getter_->freeaddrinfo(ai_);
}

//// public methods

AddressInfo::const_iterator AddressInfo::begin() const {
  return const_iterator(ai_);
}

AddressInfo::const_iterator AddressInfo::end() const {
  return const_iterator(nullptr);
}

base::Optional<std::string> AddressInfo::GetCanonicalName() const {
  return (ai_->ai_canonname != nullptr)
             ? base::Optional<std::string>(std::string(ai_->ai_canonname))
             : base::Optional<std::string>();
}

bool AddressInfo::IsAllLocalhostOfOneFamily() const {
  bool saw_v4_localhost = false;
  bool saw_v6_localhost = false;
  const auto* ai = ai_;
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
        NOTREACHED();
        return false;
    }
  }

  return saw_v4_localhost != saw_v6_localhost;
}

AddressList AddressInfo::CreateAddressList() const {
  AddressList list;
  auto canonical_name = GetCanonicalName();
  if (canonical_name)
    list.set_canonical_name(*canonical_name);
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

AddressInfo::AddressInfo(addrinfo* ai, std::unique_ptr<AddrInfoGetter> getter)
    : ai_(ai), getter_(std::move(getter)) {}

//// AddrInfoGetter

AddrInfoGetter::AddrInfoGetter() = default;
AddrInfoGetter::~AddrInfoGetter() = default;

addrinfo* AddrInfoGetter::getaddrinfo(const std::string& host,
                                      const addrinfo* hints,
                                      int* out_os_error) {
  addrinfo* ai;
  *out_os_error = ::getaddrinfo(host.c_str(), nullptr, hints, &ai);

  if (*out_os_error) {
#if defined(OS_WIN)
    *out_os_error = WSAGetLastError();
#endif
    return nullptr;
  }

  return ai;
}

void AddrInfoGetter::freeaddrinfo(addrinfo* ai) {
  ::freeaddrinfo(ai);
}

}  // namespace net
