// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_ADDRESS_INFO_H_
#define NET_DNS_ADDRESS_INFO_H_

#include <memory>
#include <string>
#include <tuple>

#include "base/macros.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "net/base/address_family.h"
#include "net/base/net_export.h"
#include "net/base/sys_addrinfo.h"

namespace net {

class AddressList;
class AddrInfoGetter;

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
    const addrinfo* ai_;
  };

  // Constructors
  using AddressInfoAndResult = std::
      tuple<base::Optional<AddressInfo>, int /* err */, int /* os_error */>;
  // Invokes AddrInfoGetter with provided |host| and |hints|. If |getter| is
  // null, the system's getaddrinfo will be invoked. (A non-null |getter| is
  // primarily for tests).
  static AddressInfoAndResult Get(
      const std::string& host,
      const addrinfo& hints,
      std::unique_ptr<AddrInfoGetter> getter = nullptr);

  AddressInfo(AddressInfo&& other);
  AddressInfo& operator=(AddressInfo&& other);
  ~AddressInfo();

  // Accessors
  const_iterator begin() const;
  const_iterator end() const;

  // Methods
  base::Optional<std::string> GetCanonicalName() const;
  bool IsAllLocalhostOfOneFamily() const;
  AddressList CreateAddressList() const;

 private:
  // Constructors
  AddressInfo(addrinfo* ai, std::unique_ptr<AddrInfoGetter> getter);

  // Data.
  addrinfo* ai_;  // Never null (except after move)
  std::unique_ptr<AddrInfoGetter> getter_;

  DISALLOW_COPY_AND_ASSIGN(AddressInfo);
};

// Encapsulates calls to getaddrinfo and freeaddrinfo for tests.
class NET_EXPORT_PRIVATE AddrInfoGetter {
 public:
  AddrInfoGetter();
  // Virtual for tests.
  virtual ~AddrInfoGetter();
  virtual addrinfo* getaddrinfo(const std::string& host,
                                const addrinfo* hints,
                                int* out_os_error);
  virtual void freeaddrinfo(addrinfo* ai);

  DISALLOW_COPY_AND_ASSIGN(AddrInfoGetter);
};

}  // namespace net

#endif  // NET_DNS_ADDRESS_INFO_H_
