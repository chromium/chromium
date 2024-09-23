// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_nat64_task.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/address_list.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_manager.h"
#include "net/dns/public/dns_query_type.h"

namespace net {

HostResolverNat64Task::HostResolverNat64Task(
    std::string_view hostname,
    NetworkAnonymizationKey network_anonymization_key,
    NetLogWithSource net_log,
    ResolveContext* resolve_context,
    base::WeakPtr<HostResolverManager> resolver)
    : hostname_(hostname),
      network_anonymization_key_(std::move(network_anonymization_key)),
      net_log_(std::move(net_log)),
      resolve_context_(resolve_context),
      resolver_(std::move(resolver)) {}

HostResolverNat64Task::~HostResolverNat64Task() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void HostResolverNat64Task::Start(base::OnceClosure completion_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!completion_closure_);

  completion_closure_ = std::move(completion_closure);

  next_state_ = State::kResolve;
  int rv = DoLoop(OK);
  if (rv != ERR_IO_PENDING) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(completion_closure_));
  }
}

HostCache::Entry HostResolverNat64Task::GetResults() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!completion_closure_);
  return results_;
}

int HostResolverNat64Task::DoLoop(int result) {
  DCHECK_NE(next_state_, State::kStateNone);
  int rv = result;
  do {
    State state = next_state_;
    next_state_ = State::kStateNone;
    switch (state) {
      case State::kResolve:
        DCHECK_EQ(OK, rv);
        rv = DoResolve();
        break;
      case State::kResolveComplete:
        rv = DoResolveComplete(rv);
        break;
      case State::kSynthesizeToIpv6:
        DCHECK_EQ(OK, rv);
        rv = DoSynthesizeToIpv6();
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != State::kStateNone);
  return rv;
}

int HostResolverNat64Task::DoResolve() {
  next_state_ = State::kResolveComplete;
  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::AAAA;

  if (!resolver_) {
    return ERR_FAILED;
  }

  request_ipv4onlyarpa_ = resolver_->CreateRequest(
      HostPortPair("ipv4only.arpa", 80), network_anonymization_key_, net_log_,
      parameters, resolve_context_);

  return request_ipv4onlyarpa_->Start(base::BindOnce(
      &HostResolverNat64Task::OnIOComplete, weak_ptr_factory_.GetWeakPtr()));
}

int HostResolverNat64Task::DoResolveComplete(int result) {
  // If not under DNS64 and resolving ipv4only.arpa fails, return the original
  // IPv4 address.
  if (result != OK || request_ipv4onlyarpa_->GetEndpointResults()->empty()) {
    IPAddress ipv4_address;
    bool is_ip = ipv4_address.AssignFromIPLiteral(hostname_);
    DCHECK(is_ip);
    std::set<std::string> aliases;
    results_ =
        HostCache::Entry(OK, {IPEndPoint(ipv4_address, 0)}, std::move(aliases),
                         HostCache::Entry::SOURCE_UNKNOWN);
    return OK;
  }

  next_state_ = State::kSynthesizeToIpv6;
  return OK;
}

int HostResolverNat64Task::DoSynthesizeToIpv6() {
  IPAddress ipv4_address;
  bool is_ip = ipv4_address.AssignFromIPLiteral(hostname_);
  DCHECK(is_ip);

  IPAddress ipv4onlyarpa_AAAA_address;

  std::vector<IPEndPoint> converted_addresses;

  for (const auto& endpoints : *request_ipv4onlyarpa_->GetEndpointResults()) {
    for (const auto& ip_endpoint : endpoints.ip_endpoints) {
      ipv4onlyarpa_AAAA_address = ip_endpoint.address();

      Dns64PrefixLength pref64_length =
          ExtractPref64FromIpv4onlyArpaAAAA(ipv4onlyarpa_AAAA_address);

      IPAddress converted_address = ConvertIPv4ToIPv4EmbeddedIPv6(
          ipv4_address, ipv4onlyarpa_AAAA_address, pref64_length);

      IPEndPoint converted_ip_endpoint(converted_address, 0);
      if (!base::Contains(converted_addresses, converted_ip_endpoint)) {
        converted_addresses.push_back(std::move(converted_ip_endpoint));
      }
    }
  }

  std::set<std::string> aliases;

  if (converted_addresses.empty()) {
    converted_addresses = {IPEndPoint(ipv4_address, 0)};
  }

  results_ = HostCache::Entry(OK, converted_addresses, std::move(aliases),
                              HostCache::Entry::SOURCE_UNKNOWN);
  return OK;
}

void HostResolverNat64Task::OnIOComplete(int result) {
  result = DoLoop(result);
  if (result != ERR_IO_PENDING)
    std::move(completion_closure_).Run();
}

}  // namespace net
