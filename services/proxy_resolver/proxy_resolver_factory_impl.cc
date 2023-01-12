// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver/proxy_resolver_factory_impl.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/net_errors.h"
#include "net/proxy_resolution/proxy_resolver_factory.h"
#include "services/proxy_resolver/mojo_proxy_resolver_v8_tracing_bindings.h"
#include "services/proxy_resolver/proxy_resolver_impl.h"
#include "services/proxy_resolver/proxy_resolver_v8_tracing.h"

namespace proxy_resolver {

class ProxyResolverFactoryImpl::Job {
 public:
  Job(ProxyResolverFactoryImpl* parent,
      const scoped_refptr<net::PacFileData>& pac_script,
      ProxyResolverV8TracingFactory* proxy_resolver_factory,
      mojo::PendingReceiver<mojom::ProxyResolver> receiver,
      mojo::PendingRemote<mojom::ProxyResolverFactoryRequestClient> client);

  Job(const Job&) = delete;
  Job& operator=(const Job&) = delete;

  ~Job();

 private:
  void OnDisconnect();
  void OnProxyResolverCreated(int error);

  const raw_ptr<ProxyResolverFactoryImpl> parent_;
  std::unique_ptr<ProxyResolverV8Tracing> proxy_resolver_impl_;
  mojo::PendingReceiver<mojom::ProxyResolver> proxy_receiver_;
  raw_ptr<ProxyResolverV8TracingFactory> factory_;
  std::unique_ptr<net::ProxyResolverFactory::Request> request_;
  mojo::Remote<mojom::ProxyResolverFactoryRequestClient> remote_client_;
};

ProxyResolverFactoryImpl::Job::Job(
    ProxyResolverFactoryImpl* factory,
    const scoped_refptr<net::PacFileData>& pac_script,
    ProxyResolverV8TracingFactory* proxy_resolver_factory,
    mojo::PendingReceiver<mojom::ProxyResolver> receiver,
    mojo::PendingRemote<mojom::ProxyResolverFactoryRequestClient> client)
    : parent_(factory),
      proxy_receiver_(std::move(receiver)),
      factory_(proxy_resolver_factory),
      remote_client_(std::move(client)) {
  remote_client_.set_disconnect_handler(base::BindOnce(
      &ProxyResolverFactoryImpl::Job::OnDisconnect, base::Unretained(this)));
  factory_->CreateProxyResolverV8Tracing(
      pac_script,
      std::make_unique<MojoProxyResolverV8TracingBindings<
          mojom::ProxyResolverFactoryRequestClient>>(remote_client_.get()),
      &proxy_resolver_impl_,
      base::BindOnce(&ProxyResolverFactoryImpl::Job::OnProxyResolverCreated,
                     base::Unretained(this)),
      &request_);
}

ProxyResolverFactoryImpl::Job::~Job() = default;

void ProxyResolverFactoryImpl::Job::OnDisconnect() {
  remote_client_->ReportResult(net::ERR_PAC_SCRIPT_TERMINATED);
  parent_->RemoveJob(this);
}

void ProxyResolverFactoryImpl::Job::OnProxyResolverCreated(int error) {
  if (error == net::OK) {
    parent_->AddResolver(
        std::make_unique<ProxyResolverImpl>(std::move(proxy_resolver_impl_)),
        std::move(proxy_receiver_));
  }
  remote_client_->ReportResult(error);
  parent_->RemoveJob(this);
}

ProxyResolverFactoryImpl::ProxyResolverFactoryImpl(
    mojo::PendingReceiver<mojom::ProxyResolverFactory> receiver)
    : ProxyResolverFactoryImpl(std::move(receiver),
                               ProxyResolverV8TracingFactory::Create()) {}

void ProxyResolverFactoryImpl::AddResolver(
    std::unique_ptr<mojom::ProxyResolver> resolver,
    mojo::PendingReceiver<mojom::ProxyResolver> receiver) {
  resolvers_.Add(std::move(resolver), std::move(receiver));
}

ProxyResolverFactoryImpl::ProxyResolverFactoryImpl(
    mojo::PendingReceiver<mojom::ProxyResolverFactory> receiver,
    std::unique_ptr<ProxyResolverV8TracingFactory> proxy_resolver_factory)
    : proxy_resolver_impl_factory_(std::move(proxy_resolver_factory)),
      receiver_(this, std::move(receiver)) {}

ProxyResolverFactoryImpl::~ProxyResolverFactoryImpl() = default;

void ProxyResolverFactoryImpl::CreateResolver(
    const std::string& pac_script,
    mojo::PendingReceiver<mojom::ProxyResolver> receiver,
    mojo::PendingRemote<mojom::ProxyResolverFactoryRequestClient> client) {
  // The Job will call RemoveJob on |this| when either the create request
  // finishes or |receiver| or |client| encounters a connection error.
  std::unique_ptr<Job> job =
      std::make_unique<Job>(this, net::PacFileData::FromUTF8(pac_script),
                            proxy_resolver_impl_factory_.get(),
                            std::move(receiver), std::move(client));
  Job* job_ptr = job.get();
  jobs_[job_ptr] = std::move(job);
}

void ProxyResolverFactoryImpl::RemoveJob(Job* job) {
  size_t erased_count = jobs_.erase(job);
  DCHECK_EQ(1U, erased_count);
}

}  // namespace proxy_resolver
