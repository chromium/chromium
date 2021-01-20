// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_HOST_RESOLVER_MDNS_TASK_H_
#define NET_DNS_HOST_RESOLVER_MDNS_TASK_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mdns_client.h"
#include "net/dns/public/dns_query_type.h"

namespace net {

class RecordParsed;

// Representation of a single HostResolverImpl::Job task to resolve the hostname
// using multicast DNS transactions.  Destruction cancels the task and prevents
// any callbacks from being invoked.
class HostResolverMdnsTask {
 public:
  // |mdns_client| must outlive |this|.
  HostResolverMdnsTask(MDnsClient* mdns_client,
                       const std::string& hostname,
                       const std::vector<DnsQueryType>& query_types);
  ~HostResolverMdnsTask();

  // Starts the task. |completion_closure| will be called asynchronously.
  //
  // Should only be called once.
  void Start(base::OnceClosure completion_closure);

  // Results only available after invocation of the completion closure.
  HostCache::Entry GetResults() const;

  static HostCache::Entry ParseResult(int error,
                                      DnsQueryType query_type,
                                      const RecordParsed* parsed,
                                      const std::string& expected_hostname);

 private:
  class Transaction;

  void CheckCompletion(bool post_needed);
  void Complete(bool post_needed);

  MDnsClient* const mdns_client_;

  const std::string hostname_;

  std::vector<Transaction> transactions_;

  base::OnceClosure completion_closure_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<HostResolverMdnsTask> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HostResolverMdnsTask);
};

}  // namespace net

#endif  // NET_DNS_HOST_RESOLVER_MDNS_TASK_H_
