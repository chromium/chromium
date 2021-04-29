// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_ADDRESS_SORTER_POSIX_H_
#define NET_DNS_ADDRESS_SORTER_POSIX_H_

#include <map>
#include <vector>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/address_sorter.h"

namespace net {

class ClientSocketFactory;

// This implementation uses explicit policy to perform the sorting. It is not
// thread-safe and always completes synchronously.
class NET_EXPORT_PRIVATE AddressSorterPosix
    : public AddressSorter,
      public NetworkChangeNotifier::IPAddressObserver {
 public:
  // Generic policy entry.
  struct PolicyEntry {
    // IPv4 addresses must be mapped to IPv6.
    unsigned char prefix[IPAddress::kIPv6AddressSize];
    unsigned prefix_length;
    unsigned value;
  };

  typedef std::vector<PolicyEntry> PolicyTable;

  enum AddressScope {
    SCOPE_UNDEFINED = 0,
    SCOPE_NODELOCAL = 1,
    SCOPE_LINKLOCAL = 2,
    SCOPE_SITELOCAL = 5,
    SCOPE_ORGLOCAL = 8,
    SCOPE_GLOBAL = 14,
  };

  struct SourceAddressInfo {
    // Values read from policy tables.
    AddressScope scope;
    unsigned label;

    // Values from the OS, matter only if more than one source address is used.
    size_t prefix_length;
    bool deprecated;  // vs. preferred RFC4862
    bool home;        // vs. care-of RFC6275
    bool native;
  };

  typedef std::map<IPAddress, SourceAddressInfo> SourceAddressMap;

  explicit AddressSorterPosix(ClientSocketFactory* socket_factory);
  ~AddressSorterPosix() override;

  // AddressSorter:
  void Sort(const AddressList& list, CallbackType callback) const override;

 private:
  friend class AddressSorterPosixTest;

  // NetworkChangeNotifier::IPAddressObserver:
  void OnIPAddressChanged() override;

  // Fills |info| with values for |address| from policy tables.
  void FillPolicy(const IPAddress& address, SourceAddressInfo* info) const;

  // Mutable to allow using default values for source addresses which were not
  // found in most recent OnIPAddressChanged.
  mutable SourceAddressMap source_map_;

  ClientSocketFactory* socket_factory_;
  PolicyTable precedence_table_;
  PolicyTable label_table_;
  PolicyTable ipv4_scope_table_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(AddressSorterPosix);
};

}  // namespace net

#endif  // NET_DNS_ADDRESS_SORTER_POSIX_H_
