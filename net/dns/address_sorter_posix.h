// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_ADDRESS_SORTER_POSIX_H_
#define NET_DNS_ADDRESS_SORTER_POSIX_H_

#include <map>
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "net/base/ip_address.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/address_sorter.h"
#include "net/socket/datagram_client_socket.h"

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
    AddressScope scope = SCOPE_UNDEFINED;
    unsigned label = 0;

    // Values from the OS, matter only if more than one source address is used.
    size_t prefix_length = 0;
    bool deprecated = false;  // vs. preferred RFC4862
    bool home = false;        // vs. care-of RFC6275
    bool native = false;
  };

  typedef std::map<IPAddress, SourceAddressInfo> SourceAddressMap;

  explicit AddressSorterPosix(ClientSocketFactory* socket_factory);

  AddressSorterPosix(const AddressSorterPosix&) = delete;
  AddressSorterPosix& operator=(const AddressSorterPosix&) = delete;

  ~AddressSorterPosix() override;

  // AddressSorter:
  void Sort(const std::vector<IPEndPoint>& endpoints,
            CallbackType callback) const override;

 private:
  friend class AddressSorterPosixTest;
  class SortContext;

  // NetworkChangeNotifier::IPAddressObserver:
  void OnIPAddressChanged() override;
  // Fills |info| with values for |address| from policy tables.
  void FillPolicy(const IPAddress& address, SourceAddressInfo* info) const;

  void FinishedSort(SortContext* sort_context) const;

  // Mutable to allow using default values for source addresses which were not
  // found in most recent OnIPAddressChanged.
  mutable SourceAddressMap source_map_;

  raw_ptr<ClientSocketFactory> socket_factory_;
  PolicyTable precedence_table_;
  PolicyTable label_table_;
  PolicyTable ipv4_scope_table_;

  // SortContext stores data for an outstanding Sort() that is completing
  // asynchronously. Mutable to allow pushing a new SortContext when Sort is
  // called. Since Sort can be called multiple times, a container is necessary
  // to track different SortContexts.
  mutable std::set<std::unique_ptr<SortContext>, base::UniquePtrComparator>
      sort_contexts_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace net

#endif  // NET_DNS_ADDRESS_SORTER_POSIX_H_
