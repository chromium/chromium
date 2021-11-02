// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/mojo_host_resolver_impl.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/dns_query_type.h"

namespace network {

// Handles host resolution for a single request and sends a response when done.
// Also detects connection errors for HostResolverRequestClient and cancels the
// outstanding resolve request. Owned by MojoHostResolverImpl.
class MojoHostResolverImpl::Job {
 public:
  Job(MojoHostResolverImpl* resolver_service,
      net::HostResolver* resolver,
      const std::string& hostname,
      const net::NetworkIsolationKey& network_isolation_key,
      bool is_ex,
      const net::NetLogWithSource& net_log,
      mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
          client);
  ~Job();

  void set_iter(std::list<Job>::iterator iter) { iter_ = iter; }

  void Start();

 private:
  // Completion callback for the HostResolver::Resolve request.
  void OnResolveDone(int result);

  // Mojo disconnect handler.
  void OnMojoDisconnect();

  MojoHostResolverImpl* resolver_service_;
  // This Job's iterator in |resolver_service_|, so the Job may be removed on
  // completion.
  std::list<Job>::iterator iter_;
  mojo::Remote<proxy_resolver::mojom::HostResolverRequestClient> client_;
  const std::string hostname_;
  std::unique_ptr<net::HostResolver::ResolveHostRequest> request_;
  base::ThreadChecker thread_checker_;
};

MojoHostResolverImpl::MojoHostResolverImpl(net::HostResolver* resolver,
                                           const net::NetLogWithSource& net_log)
    : resolver_(resolver), net_log_(net_log) {}

MojoHostResolverImpl::~MojoHostResolverImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void MojoHostResolverImpl::Resolve(
    const std::string& hostname,
    const net::NetworkIsolationKey& network_isolation_key,
    bool is_ex,
    mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
        client) {
  DCHECK(thread_checker_.CalledOnValidThread());

  pending_jobs_.emplace_front(this, resolver_, hostname, network_isolation_key,
                              is_ex, net_log_, std::move(client));
  auto job = pending_jobs_.begin();
  job->set_iter(job);
  job->Start();
}

void MojoHostResolverImpl::DeleteJob(std::list<Job>::iterator job) {
  DCHECK(thread_checker_.CalledOnValidThread());
  pending_jobs_.erase(job);
}

MojoHostResolverImpl::Job::Job(
    MojoHostResolverImpl* resolver_service,
    net::HostResolver* resolver,
    const std::string& hostname,
    const net::NetworkIsolationKey& network_isolation_key,
    bool is_ex,
    const net::NetLogWithSource& net_log,
    mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
        client)
    : resolver_service_(resolver_service),
      client_(std::move(client)),
      hostname_(hostname) {
  client_.set_disconnect_handler(base::BindOnce(
      &MojoHostResolverImpl::Job::OnMojoDisconnect, base::Unretained(this)));

  net::HostResolver::ResolveHostParameters parameters;
  if (!is_ex)
    parameters.dns_query_type = net::DnsQueryType::A;
  request_ =
      resolver->CreateRequest(net::HostPortPair(hostname_, 0),
                              network_isolation_key, net_log, parameters);
}

void MojoHostResolverImpl::Job::Start() {
  // The caller is responsible for setting up |iter_|.
  DCHECK_EQ(this, &*iter_);

  DVLOG(1) << "Resolve " << hostname_;
  int result = request_->Start(base::BindOnce(
      &MojoHostResolverImpl::Job::OnResolveDone, base::Unretained(this)));

  if (result != net::ERR_IO_PENDING)
    OnResolveDone(result);
}

MojoHostResolverImpl::Job::~Job() = default;

void MojoHostResolverImpl::Job::OnResolveDone(int result) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::vector<net::IPAddress> result_addresses;
  if (request_->GetAddressResults()) {
    for (const auto& endpoint :
         request_->GetAddressResults().value().endpoints()) {
      result_addresses.push_back(endpoint.address());
    }
  }

  request_.reset();
  DVLOG(1) << "Resolved " << hostname_ << " with error " << result << " and "
           << result_addresses.size() << " results!";
  for (const auto& address : result_addresses) {
    DVLOG(1) << address.ToString();
  }
  client_->ReportResult(result, result_addresses);

  resolver_service_->DeleteJob(iter_);
}

void MojoHostResolverImpl::Job::OnMojoDisconnect() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // |resolver_service_| should always outlive us.
  DCHECK(resolver_service_);
  DVLOG(1) << "Disconnection on request for " << hostname_;
  resolver_service_->DeleteJob(iter_);
}

}  // namespace network
