// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_ADDRESS_LIST_H_
#define NET_BASE_ADDRESS_LIST_H_

#include <stdint.h>

#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/values.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"

struct addrinfo;

namespace base {
class Value;
}

namespace net {

class IPAddress;

class NET_EXPORT AddressList {
 public:
  AddressList();
  AddressList(const AddressList&);
  AddressList& operator=(const AddressList&);
  AddressList(AddressList&&);
  AddressList& operator=(AddressList&&);
  ~AddressList();

  // Creates an address list for a single IP endpoint.
  explicit AddressList(const IPEndPoint& endpoint);

  // Creates an address list for a single IP endpoint and a list of DNS aliases.
  AddressList(const IPEndPoint& endpoint, std::vector<std::string> aliases);

  // Creates an address list for a list of IP endpoints.
  explicit AddressList(std::vector<IPEndPoint> endpoints);

  static AddressList CreateFromIPAddress(const IPAddress& address,
                                         uint16_t port);

  static AddressList CreateFromIPAddressList(const IPAddressList& addresses,
                                             std::vector<std::string> aliases);

  // Copies the data from `head` and the chained list into an AddressList.
  static AddressList CreateFromAddrinfo(const struct addrinfo* head);

  // Returns a copy of `list` with port on each element set to |port|.
  static AddressList CopyWithPort(const AddressList& list, uint16_t port);

  bool operator==(const AddressList& other) const {
    return std::tie(endpoints_, dns_aliases_) ==
           std::tie(other.endpoints_, other.dns_aliases_);
  }
  bool operator!=(const AddressList& other) const { return !(*this == other); }

  // Sets the first entry of `dns_aliases_` to the literal of the first IP
  // address on the list. Assumes that `dns_aliases_` is empty.
  void SetDefaultCanonicalName();

  // The alias chain in no particular order.
  const std::vector<std::string>& dns_aliases() const { return dns_aliases_; }

  void SetDnsAliases(std::vector<std::string> aliases);

  void AppendDnsAliases(std::vector<std::string> aliases);

  // Creates a value representation of the address list, appropriate for
  // inclusion in a NetLog.
  base::Value::Dict NetLogParams() const;

  // Deduplicates the stored addresses while otherwise preserving their order.
  void Deduplicate();

  using iterator = std::vector<IPEndPoint>::iterator;
  using const_iterator = std::vector<IPEndPoint>::const_iterator;

  size_t size() const { return endpoints_.size(); }
  bool empty() const { return endpoints_.empty(); }
  void clear() { endpoints_.clear(); }
  void reserve(size_t count) { endpoints_.reserve(count); }
  size_t capacity() const { return endpoints_.capacity(); }
  IPEndPoint& operator[](size_t index) { return endpoints_[index]; }
  const IPEndPoint& operator[](size_t index) const { return endpoints_[index]; }
  IPEndPoint& front() { return endpoints_.front(); }
  const IPEndPoint& front() const { return endpoints_.front(); }
  IPEndPoint& back() { return endpoints_.back(); }
  const IPEndPoint& back() const { return endpoints_.back(); }
  void push_back(const IPEndPoint& val) { endpoints_.push_back(val); }

  template <typename InputIt>
  void insert(iterator pos, InputIt first, InputIt last) {
    endpoints_.insert(pos, first, last);
  }
  iterator begin() { return endpoints_.begin(); }
  const_iterator begin() const { return endpoints_.begin(); }
  iterator end() { return endpoints_.end(); }
  const_iterator end() const { return endpoints_.end(); }

  const std::vector<net::IPEndPoint>& endpoints() const { return endpoints_; }
  std::vector<net::IPEndPoint>& endpoints() { return endpoints_; }

 private:
  std::vector<IPEndPoint> endpoints_;

  // In no particular order.
  std::vector<std::string> dns_aliases_;
};

}  // namespace net

#endif  // NET_BASE_ADDRESS_LIST_H_
