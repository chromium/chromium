// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_ADDRESS_INFO_H_
#define NET_DNS_ADDRESS_INFO_H_

#include <memory>
#include <optional>
#include <string>
#include <tuple>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "net/base/address_family.h"
#include "net/base/net_export.h"
#include "net/base/network_handle.h"
#include "net/base/sys_addrinfo.h"

namespace net {

class AddressList;
class AddrInfoGetter;

using FreeAddrInfoFunc = void (*)(addrinfo*);

// AddressInfo -- this encapsulates the system call to getaddrinfo and the
// data structure that it populates and returns.
class NET_EXPORT_PRIVATE AddressInfo {
 public:
  // Types
  class NET_EXPORT_PRIVATE const_iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = const addrinfo;
    using difference_type = std::ptrdiff_t;
    using pointer = const addrinfo*;
    using reference = const addrinfo&;

    const_iterator(const const_iterator& other) = default;
    explicit const_iterator(const addrinfo* ai);
    bool operator!=(const const_iterator& o) const;
    const_iterator& operator++();  // prefix
    const addrinfo* operator->() const;
    const addrinfo& operator*() const;

   private:
    // Owned by AddressInfo.
    raw_ptr<const addrinfo> ai_;
  };

  // Constructors
  using AddressInfoAndResult =
      std::tuple<std::optional<AddressInfo>, int /* err */, int /* os_error */>;
  // Invokes AddrInfoGetter with provided `host` and `hints`. If `getter` is
  // null, the system's getaddrinfo will be invoked. (A non-null `getter` is
  // primarily for tests).
  // `network` is an optional parameter, when specified (!=
  // handles::kInvalidNetworkHandle) the lookup will be performed specifically
  // for `network` (currently only supported on Android platforms).
  static AddressInfoAndResult Get(
      const std::string& host,
      const addrinfo& hints,
      std::unique_ptr<AddrInfoGetter> getter = nullptr,
      handles::NetworkHandle network = handles::kInvalidNetworkHandle);

  AddressInfo(const AddressInfo&) = delete;
  AddressInfo& operator=(const AddressInfo&) = delete;

  AddressInfo(AddressInfo&& other);
  AddressInfo& operator=(AddressInfo&& other);

  ~AddressInfo();

  // Accessors
  const_iterator begin() const;
  const_iterator end() const;

  // Methods
  std::optional<std::string> GetCanonicalName() const;
  bool IsAllLocalhostOfOneFamily() const;
  AddressList CreateAddressList() const;

 private:
  // Constructors
  AddressInfo(std::unique_ptr<addrinfo, FreeAddrInfoFunc> ai,
              std::unique_ptr<AddrInfoGetter> getter);

  // Data.
  std::unique_ptr<addrinfo, FreeAddrInfoFunc>
      ai_;  // Never null (except after move)
  std::unique_ptr<AddrInfoGetter> getter_;
};

// Encapsulates calls to getaddrinfo and freeaddrinfo for tests.
class NET_EXPORT_PRIVATE AddrInfoGetter {
 public:
  AddrInfoGetter();

  AddrInfoGetter(const AddrInfoGetter&) = delete;
  AddrInfoGetter& operator=(const AddrInfoGetter&) = delete;

  // Virtual for tests.
  virtual ~AddrInfoGetter();
  virtual std::unique_ptr<addrinfo, FreeAddrInfoFunc> getaddrinfo(
      const std::string& host,
      const addrinfo* hints,
      int* out_os_error,
      handles::NetworkHandle network);
};

}  // namespace net

#endif  // NET_DNS_ADDRESS_INFO_H_
