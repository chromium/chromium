// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_HOST_RESOLVER_MDNS_TASK_H_
#define NET_DNS_HOST_RESOLVER_MDNS_TASK_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "net/base/completion_once_callback.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mdns_client.h"

namespace net {

// Representation of a single HostResolverImpl::Job task to resolve the hostname
// using multicast DNS transactions.  Destruction cancels the task and prevents
// any callbacks from being invoked.
class HostResolverMdnsTask {
 public:
  // |mdns_client| must outlive |this|.
  HostResolverMdnsTask(
      MDnsClient* mdns_client,
      const std::string& hostname,
      const std::vector<HostResolver::DnsQueryType>& query_types);
  ~HostResolverMdnsTask();

  // Starts the task. |completion_callback| will be called asynchronously with
  // results.
  //
  // Should only be called once.
  void Start(CompletionOnceCallback completion_callback);

  const AddressList& result_addresses() { return result_addresses_; }

 private:
  class Transaction;

  void CheckCompletion(bool post_needed);
  void CompleteWithResult(int result, bool post_needed);

  MDnsClient* const mdns_client_;

  const std::string hostname_;

  AddressList result_addresses_;
  std::vector<Transaction> transactions_;

  CompletionOnceCallback completion_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<HostResolverMdnsTask> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HostResolverMdnsTask);
};

}  // namespace net

#endif  // NET_DNS_HOST_RESOLVER_MDNS_TASK_H_
