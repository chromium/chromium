// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_HOST_RESOLVER_NAT64_TASK_H_
#define NET_DNS_HOST_RESOLVER_NAT64_TASK_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_manager.h"
#include "net/dns/public/dns_query_type.h"

namespace net {

class HostResolverInternalResult;

// Representation of a single HostResolverImpl::Job task to convert an IPv4
// address literal to an IPv4-Embedded IPv6 according to rfc6052.
// https://www.rfc-editor.org/rfc/rfc6052
// When a DNS64 is not found returns the original IPv4 address.
// Destruction cancels the task and prevents any callbacks from being invoked.
class HostResolverNat64Task {
 public:
  using CallbackType =
      base::OnceCallback<void(std::unique_ptr<HostResolverInternalResult>)>;

  HostResolverNat64Task(std::string_view hostname,
                        NetworkAnonymizationKey network_anonymization_key,
                        NetLogWithSource net_log,
                        ResolveContext* resolve_context,
                        base::WeakPtr<HostResolverManager> resolver);

  HostResolverNat64Task(const HostResolverNat64Task&) = delete;
  HostResolverNat64Task& operator=(const HostResolverNat64Task&) = delete;

  ~HostResolverNat64Task();

  // Should only be called once.
  void Start(CallbackType completion_callback);

 private:
  const std::string hostname_;
  const NetworkAnonymizationKey network_anonymization_key_;
  NetLogWithSource net_log_;
  const raw_ptr<ResolveContext> resolve_context_;
  CallbackType completion_callback_;
  base::WeakPtr<HostResolverManager> resolver_;

  SEQUENCE_CHECKER(sequence_checker_);

  int DoResolve();
  int DoResolveComplete(int result);
  int DoSynthesizeToIpv6();

  void OnIOComplete(int result);
  int DoLoop(int result);

  enum class State {
    kResolve,
    kResolveComplete,
    kSynthesizeToIpv6,
    kStateNone,
  };

  State next_state_ = State::kStateNone;

  std::unique_ptr<HostResolver::ResolveHostRequest> request_ipv4onlyarpa_;
  std::unique_ptr<HostResolverInternalResult> result_;

  base::WeakPtrFactory<HostResolverNat64Task> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_DNS_HOST_RESOLVER_NAT64_TASK_H_
