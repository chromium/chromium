// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include <iterator>
#include <memory>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "net/base/address_family.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/base/request_priority.h"
#include "net/dns/context_host_resolver.h"
#include "net/dns/fuzzed_host_resolver_util.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_proc.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/host_resolver_source.h"
#include "net/log/net_log.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/net_buildflags.h"

namespace {

const char* kHostNames[] = {"foo", "foo.com",   "a.foo.com",
                            "bar", "localhost", "localhost6"};

class DnsRequest {
 public:
  DnsRequest(net::HostResolver* host_resolver,
             FuzzedDataProvider* data_provider,
             std::vector<std::unique_ptr<DnsRequest>>* dns_requests)
      : host_resolver_(host_resolver),
        data_provider_(data_provider),
        dns_requests_(dns_requests) {}

  DnsRequest(const DnsRequest&) = delete;
  DnsRequest& operator=(const DnsRequest&) = delete;

  ~DnsRequest() = default;

  // Creates and starts a DNS request using fuzzed parameters. If the request
  // doesn't complete synchronously, adds it to |dns_requests|.
  static void CreateRequest(
      net::HostResolver* host_resolver,
      FuzzedDataProvider* data_provider,
      std::vector<std::unique_ptr<DnsRequest>>* dns_requests) {
    auto dns_request = std::make_unique<DnsRequest>(
        host_resolver, data_provider, dns_requests);

    if (dns_request->Start() == net::ERR_IO_PENDING)
      dns_requests->push_back(std::move(dns_request));
  }

  // If |dns_requests| is non-empty, waits for a randomly chosen one of the
  // requests to complete and removes it from |dns_requests|.
  static void WaitForRequestComplete(
      FuzzedDataProvider* data_provider,
      std::vector<std::unique_ptr<DnsRequest>>* dns_requests) {
    if (dns_requests->empty())
      return;
    uint32_t index = data_provider->ConsumeIntegralInRange<uint32_t>(
        0, dns_requests->size() - 1);

    // Remove the request from the list before waiting on it - this prevents one
    // of the other callbacks from deleting the callback being waited on.
    std::unique_ptr<DnsRequest> request = std::move((*dns_requests)[index]);
    dns_requests->erase(dns_requests->begin() + index);
    request->WaitUntilDone();
  }

  // If |dns_requests| is non-empty, attempts to cancel a randomly chosen one of
  // them and removes it from |dns_requests|. If the one it picks is already
  // complete, just removes it from the list.
  static void CancelRequest(
      net::HostResolver* host_resolver,
      FuzzedDataProvider* data_provider,
      std::vector<std::unique_ptr<DnsRequest>>* dns_requests) {
    if (dns_requests->empty())
      return;
    uint32_t index = data_provider->ConsumeIntegralInRange<uint32_t>(
        0, dns_requests->size() - 1);
    auto request = dns_requests->begin() + index;
    (*request)->Cancel();
    dns_requests->erase(request);
  }

 private:
  void OnCallback(int result) {
    CHECK_NE(net::ERR_IO_PENDING, result);

    request_.reset();

    // Remove |this| from |dns_requests| and take ownership of it, if it wasn't
    // already removed from the vector. It may have been removed if this is in a
    // WaitForRequest call, in which case, do nothing.
    std::unique_ptr<DnsRequest> self;
    for (auto request = dns_requests_->begin(); request != dns_requests_->end();
         ++request) {
      if (request->get() != this)
        continue;
      self = std::move(*request);
      dns_requests_->erase(request);
      break;
    }

    while (true) {
      bool done = false;
      switch (data_provider_->ConsumeIntegralInRange(0, 2)) {
        case 0:
          // Quit on 0, or when no data is left.
          done = true;
          break;
        case 1:
          CreateRequest(host_resolver_, data_provider_, dns_requests_);
          break;
        case 2:
          CancelRequest(host_resolver_, data_provider_, dns_requests_);
          break;
      }

      if (done)
        break;
    }

    if (run_loop_)
      run_loop_->Quit();
  }

  // Starts the DNS request, using a fuzzed set of parameters.
  int Start() {
    net::HostResolver::ResolveHostParameters parameters;

    auto query_types_it = net::kDnsQueryTypes.cbegin();
    std::advance(query_types_it, data_provider_->ConsumeIntegralInRange<size_t>(
                                     0, net::kDnsQueryTypes.size() - 1));
    parameters.dns_query_type = query_types_it->first;

    parameters.initial_priority = static_cast<net::RequestPriority>(
        data_provider_->ConsumeIntegralInRange<int32_t>(net::MINIMUM_PRIORITY,
                                                        net::MAXIMUM_PRIORITY));

    parameters.source =
        data_provider_->PickValueInArray(net::kHostResolverSources);
#if !BUILDFLAG(ENABLE_MDNS)
    while (parameters.source == net::HostResolverSource::MULTICAST_DNS) {
      parameters.source =
          data_provider_->PickValueInArray(net::kHostResolverSources);
    }
#endif  // !BUILDFLAG(ENABLE_MDNS)

    parameters.cache_usage =
        data_provider_->ConsumeBool()
            ? net::HostResolver::ResolveHostParameters::CacheUsage::ALLOWED
            : net::HostResolver::ResolveHostParameters::CacheUsage::DISALLOWED;

    // `include_canonical_name` only allowed for address queries and only when
    // the system resolver can be used.
    if (net::IsAddressType(parameters.dns_query_type) &&
        parameters.source != net::HostResolverSource::DNS &&
        parameters.source != net::HostResolverSource::MULTICAST_DNS) {
      parameters.include_canonical_name = data_provider_->ConsumeBool();
    }

    if (!IsParameterCombinationAllowed(parameters)) {
      return net::ERR_FAILED;
    }

    const char* hostname = data_provider_->PickValueInArray(kHostNames);
    request_ = host_resolver_->CreateRequest(
        net::HostPortPair(hostname, 80), net::NetworkAnonymizationKey(),
        net::NetLogWithSource(), parameters);
    int rv = request_->Start(
        base::BindOnce(&DnsRequest::OnCallback, base::Unretained(this)));
    if (rv != net::ERR_IO_PENDING)
      request_.reset();
    return rv;
  }

  // Waits until the request is done, if it isn't done already.
  void WaitUntilDone() {
    CHECK(!run_loop_);
    if (request_) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
      run_loop_.reset();
    }
  }

  // Some combinations of request parameters are disallowed and expected to
  // DCHECK. Returns whether or not |parameters| represents one of those cases.
  static bool IsParameterCombinationAllowed(
      net::HostResolver::ResolveHostParameters parameters) {
    // SYSTEM requests only support address types.
    if (parameters.source == net::HostResolverSource::SYSTEM &&
        !net::IsAddressType(parameters.dns_query_type)) {
      return false;
    }

    // Multiple parameters disallowed for mDNS requests.
    if (parameters.source == net::HostResolverSource::MULTICAST_DNS &&
        (parameters.include_canonical_name || parameters.loopback_only ||
         parameters.cache_usage !=
             net::HostResolver::ResolveHostParameters::CacheUsage::ALLOWED ||
         parameters.dns_query_type == net::DnsQueryType::HTTPS)) {
      return false;
    }

    return true;
  }

  // Cancel the request, if not already completed. Otherwise, does nothing.
  void Cancel() { request_.reset(); }

  raw_ptr<net::HostResolver> host_resolver_;
  raw_ptr<FuzzedDataProvider> data_provider_;
  raw_ptr<std::vector<std::unique_ptr<DnsRequest>>> dns_requests_;

  // Non-null only while running.
  std::unique_ptr<net::HostResolver::ResolveHostRequest> request_;
  net::AddressList address_list_;

  std::unique_ptr<base::RunLoop> run_loop_;
};

class FuzzerEnvironment {
 public:
  FuzzerEnvironment() {
    net::SetSystemDnsResolutionTaskRunnerForTesting(  // IN-TEST
        base::SequencedTaskRunner::GetCurrentDefault());
  }
  ~FuzzerEnvironment() = default;
};

void EnsureInitFuzzerEnvironment() {
  static FuzzerEnvironment init_environment;
}

}  // namespace

// Fuzzer for HostResolverImpl. Fuzzes using both the system resolver and
// built-in DNS client paths.
//
// TODO(mmenke): Add coverage for things this does not cover:
//     * Out of order completion, particularly for the platform resolver path.
//     * Simulate network changes, including both enabling and disabling the
//     async resolver while lookups are active as a result of the change.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  {
    FuzzedDataProvider data_provider(data, size);

    EnsureInitFuzzerEnvironment();

    // Including an observer; even though the recorded results aren't currently
    // used, it'll ensure the netlogging code is fuzzed as well.
    net::RecordingNetLogObserver net_log_observer;

    net::HostResolver::ManagerOptions options;
    options.max_concurrent_resolves =
        data_provider.ConsumeIntegralInRange(1, 8);
    options.insecure_dns_client_enabled = data_provider.ConsumeBool();
    bool enable_caching = data_provider.ConsumeBool();
    std::unique_ptr<net::ContextHostResolver> host_resolver =
        net::CreateFuzzedContextHostResolver(options, net::NetLog::Get(),
                                             &data_provider, enable_caching);

    std::vector<std::unique_ptr<DnsRequest>> dns_requests;
    bool done = false;
    while (!done) {
      switch (data_provider.ConsumeIntegralInRange(0, 3)) {
        case 0:
          // Quit on 0, or when no data is left.
          done = true;
          break;
        case 1:
          DnsRequest::CreateRequest(host_resolver.get(), &data_provider,
                                    &dns_requests);
          break;
        case 2:
          DnsRequest::WaitForRequestComplete(&data_provider, &dns_requests);
          break;
        case 3:
          DnsRequest::CancelRequest(host_resolver.get(), &data_provider,
                                    &dns_requests);
          break;
      }
    }
  }

  // Clean up any pending tasks, after deleting everything.
  base::RunLoop().RunUntilIdle();

  return 0;
}
