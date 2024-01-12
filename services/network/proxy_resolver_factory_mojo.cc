// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/proxy_resolver_factory_mojo.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/base/ip_address.h"
#include "net/base/load_states.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/pac_file_data.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolve_dns_operation.h"
#include "net/proxy_resolution/proxy_resolver.h"
#include "net/proxy_resolution/proxy_resolver_error_observer.h"
#include "services/network/mojo_host_resolver_impl.h"
#include "services/network/proxy_auto_config_library.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"

namespace net {
class NetworkAnonymizationKey;
}

namespace network {

namespace {

base::Value::Dict NetLogErrorParams(int line_number,
                                    const std::string& message) {
  return base::Value::Dict()
      .Set("line_number", line_number)
      .Set("message", message);
}

// A mixin that forwards logging to (Bound)NetLog and ProxyResolverErrorObserver
// and DNS requests to a MojoHostResolverImpl, which is implemented in terms of
// a HostResolver, or myIpAddress[Ex]() which is implemented by MyIpAddressImpl.
template <typename ClientInterface>
class ClientMixin : public ClientInterface {
 public:
  ClientMixin(net::HostResolver* host_resolver,
              net::ProxyResolverErrorObserver* error_observer,
              net::NetLog* net_log,
              const net::NetLogWithSource& net_log_with_source)
      : host_resolver_(host_resolver, net_log_with_source),
        my_ip_address_impl_(std::make_unique<MyIpAddressImpl>(
            MyIpAddressImpl::Mode::kMyIpAddress)),
        my_ip_address_impl_ex_(std::make_unique<MyIpAddressImpl>(
            MyIpAddressImpl::Mode::kMyIpAddressEx)),
        error_observer_(error_observer),
        net_log_(net_log),
        net_log_with_source_(net_log_with_source) {}

  // Overridden from ClientInterface:
  void Alert(const std::string& message) override {
    net_log_with_source_.AddEventWithStringParams(
        net::NetLogEventType::PAC_JAVASCRIPT_ALERT, "message", message);
    if (net_log_)
      net_log_->AddGlobalEntryWithStringParams(
          net::NetLogEventType::PAC_JAVASCRIPT_ALERT, "message", message);
  }

  void OnError(int32_t line_number, const std::string& message) override {
    net_log_with_source_.AddEvent(
        net::NetLogEventType::PAC_JAVASCRIPT_ERROR,
        [&] { return NetLogErrorParams(line_number, message); });
    if (net_log_)
      net_log_->AddGlobalEntry(net::NetLogEventType::PAC_JAVASCRIPT_ERROR, [&] {
        return NetLogErrorParams(line_number, message);
      });
    if (error_observer_) {
      error_observer_->OnPACScriptError(line_number,
                                        base::UTF8ToUTF16(message));
    }
  }

  // TODO(eroman): Split the client interfaces so ResolveDns() does not also
  // carry the myIpAddress(Ex) requests.
  void ResolveDns(
      const std::string& hostname,
      net::ProxyResolveDnsOperation operation,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
          client) override {
    if (operation == net::ProxyResolveDnsOperation::MY_IP_ADDRESS) {
      my_ip_address_impl_->AddRequest(std::move(client));
    } else if (operation == net::ProxyResolveDnsOperation::MY_IP_ADDRESS_EX) {
      my_ip_address_impl_ex_->AddRequest(std::move(client));
    } else {
      bool is_ex = operation == net::ProxyResolveDnsOperation::DNS_RESOLVE_EX;
      // Request was for dnsResolve() or dnsResolveEx().
      host_resolver_.Resolve(hostname, network_anonymization_key, is_ex,
                             std::move(client));
    }
  }

 protected:
  void DetachMixin() {
    error_observer_ = nullptr;
    net_log_ = nullptr;
  }

  // TODO(eroman): This doesn't track being blocked in myIpAddress(Ex) handler.
  bool dns_request_in_progress() {
    return host_resolver_.request_in_progress();
  }

 private:
  // Handles DNS queries and owns the Remote<HostResolverRequestClient>'s.
  MojoHostResolverImpl host_resolver_;
  // Handles myIpAddress() queries and also owns the
  // Remote<HostResolverRequestClient>'s.
  std::unique_ptr<MyIpAddressImpl> my_ip_address_impl_;
  std::unique_ptr<MyIpAddressImpl> my_ip_address_impl_ex_;

  raw_ptr<net::ProxyResolverErrorObserver> error_observer_;
  raw_ptr<net::NetLog> net_log_;
  const net::NetLogWithSource net_log_with_source_;

  base::WeakPtrFactory<ClientMixin> weak_ptr_factory_{this};
};

// Implementation of ProxyResolver that connects to a Mojo service to evaluate
// PAC scripts. This implementation only knows about Mojo services, and
// therefore that service may live in or out of process.
//
// This implementation reports disconnections from the Mojo service (i.e. if the
// service is out-of-process and that process crashes) using the error code
// ERR_PAC_SCRIPT_TERMINATED.
class ProxyResolverMojo : public net::ProxyResolver {
 public:
  // Constructs a ProxyResolverMojo that connects to a mojo proxy resolver
  // implementation using |resolver_remote|. The implementation uses
  // |host_resolver| as the DNS resolver, using |host_resolver_binding| to
  // communicate with it.
  ProxyResolverMojo(
      mojo::PendingRemote<proxy_resolver::mojom::ProxyResolver> resolver_remote,
      net::HostResolver* host_resolver,
      std::unique_ptr<net::ProxyResolverErrorObserver> error_observer,
      net::NetLog* net_log);

  ProxyResolverMojo(const ProxyResolverMojo&) = delete;
  ProxyResolverMojo& operator=(const ProxyResolverMojo&) = delete;

  ~ProxyResolverMojo() override;

  // ProxyResolver implementation:
  int GetProxyForURL(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      net::ProxyInfo* results,
      net::CompletionOnceCallback callback,
      std::unique_ptr<Request>* request,
      const net::NetLogWithSource& net_log) override;

 private:
  class Job;

  SEQUENCE_CHECKER(sequence_checker_);

  // Mojo disconnect handler.
  void OnMojoDisconnect();

  // Connection to the Mojo proxy resolver.
  mojo::Remote<proxy_resolver::mojom::ProxyResolver>
      mojo_proxy_resolver_remote_;

  raw_ptr<net::HostResolver> host_resolver_;

  std::unique_ptr<net::ProxyResolverErrorObserver> error_observer_;

  raw_ptr<net::NetLog> net_log_;
};

class ProxyResolverMojo::Job
    : public ProxyResolver::Request,
      public ClientMixin<proxy_resolver::mojom::ProxyResolverRequestClient> {
 public:
  Job(ProxyResolverMojo* resolver,
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      net::ProxyInfo* results,
      net::CompletionOnceCallback callback,
      const net::NetLogWithSource& net_log);

  Job(const Job&) = delete;
  Job& operator=(const Job&) = delete;

  ~Job() override;

  // Returns the LoadState of this job.
  net::LoadState GetLoadState() override;

 private:
  // Mojo disconnection handler.
  void OnMojoDisconnect();

  // Overridden from proxy_resolver::mojom::ProxyResolverRequestClient:
  void ReportResult(int32_t error, const net::ProxyInfo& proxy_info) override;

  // Completes a request with a result code.
  void CompleteRequest(int result);

  const GURL url_;
  raw_ptr<net::ProxyInfo> results_;
  net::CompletionOnceCallback callback_;

  SEQUENCE_CHECKER(sequence_checker_);
  mojo::Receiver<proxy_resolver::mojom::ProxyResolverRequestClient> receiver_{
      this};
};

ProxyResolverMojo::Job::Job(
    ProxyResolverMojo* resolver,
    const GURL& url,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    net::ProxyInfo* results,
    net::CompletionOnceCallback callback,
    const net::NetLogWithSource& net_log)
    : ClientMixin<proxy_resolver::mojom::ProxyResolverRequestClient>(
          resolver->host_resolver_,
          resolver->error_observer_.get(),
          resolver->net_log_,
          net_log),
      url_(url),
      results_(results),
      callback_(std::move(callback)) {
  resolver->mojo_proxy_resolver_remote_->GetProxyForUrl(
      url_, network_anonymization_key, receiver_.BindNewPipeAndPassRemote());
  receiver_.set_disconnect_handler(base::BindOnce(
      &ProxyResolverMojo::Job::OnMojoDisconnect, base::Unretained(this)));
}

ProxyResolverMojo::Job::~Job() {}

net::LoadState ProxyResolverMojo::Job::GetLoadState() {
  return dns_request_in_progress() ? net::LOAD_STATE_RESOLVING_HOST_IN_PAC_FILE
                                   : net::LOAD_STATE_RESOLVING_PROXY_FOR_URL;
}

void ProxyResolverMojo::Job::OnMojoDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "ProxyResolverMojo::Job::OnMojoDisconnect";
  CompleteRequest(net::ERR_PAC_SCRIPT_TERMINATED);
}

void ProxyResolverMojo::Job::CompleteRequest(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  net::CompletionOnceCallback callback = std::move(callback_);
  receiver_.reset();
  // Clear raw pointers from the mojo mixin --- the callback may clean up stuff,
  // and it can't be invoked without `receiver_` connected anyway.
  DetachMixin();
  std::move(callback).Run(result);
}

void ProxyResolverMojo::Job::ReportResult(int32_t error,
                                          const net::ProxyInfo& proxy_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "ProxyResolverMojo::Job::ReportResult: " << error;

  if (error == net::OK) {
    *results_ = proxy_info;
    DVLOG(1) << "Servers: " << results_->ToDebugString();
  }

  CompleteRequest(error);
}

ProxyResolverMojo::ProxyResolverMojo(
    mojo::PendingRemote<proxy_resolver::mojom::ProxyResolver> resolver_remote,
    net::HostResolver* host_resolver,
    std::unique_ptr<net::ProxyResolverErrorObserver> error_observer,
    net::NetLog* net_log)
    : mojo_proxy_resolver_remote_(std::move(resolver_remote)),
      host_resolver_(host_resolver),
      error_observer_(std::move(error_observer)),
      net_log_(net_log) {
  mojo_proxy_resolver_remote_.set_disconnect_handler(base::BindOnce(
      &ProxyResolverMojo::OnMojoDisconnect, base::Unretained(this)));
}

ProxyResolverMojo::~ProxyResolverMojo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ProxyResolverMojo::OnMojoDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "ProxyResolverMojo::OnMojoDisconnect";

  // Disconnect from the Mojo proxy resolver service.
  mojo_proxy_resolver_remote_.reset();
}

int ProxyResolverMojo::GetProxyForURL(
    const GURL& url,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    net::ProxyInfo* results,
    net::CompletionOnceCallback callback,
    std::unique_ptr<Request>* request,
    const net::NetLogWithSource& net_log) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!mojo_proxy_resolver_remote_)
    return net::ERR_PAC_SCRIPT_TERMINATED;

  *request = std::make_unique<Job>(this, url, network_anonymization_key,
                                   results, std::move(callback), net_log);

  return net::ERR_IO_PENDING;
}

}  // namespace

// A Job to create a ProxyResolver instance.
//
// Note: a Job instance is not tied to a particular resolve request, and hence
// there is no per-request logging to be done (any netlog events are only sent
// globally) so this always uses an empty NetLogWithSource.
class ProxyResolverFactoryMojo::Job
    : public ClientMixin<
          proxy_resolver::mojom::ProxyResolverFactoryRequestClient>,
      public ProxyResolverFactory::Request {
 public:
  Job(ProxyResolverFactoryMojo* factory,
      const scoped_refptr<net::PacFileData>& pac_script,
      std::unique_ptr<net::ProxyResolver>* resolver,
      net::CompletionOnceCallback callback,
      std::unique_ptr<net::ProxyResolverErrorObserver> error_observer)
      : ClientMixin<proxy_resolver::mojom::ProxyResolverFactoryRequestClient>(
            factory->host_resolver_,
            error_observer.get(),
            factory->net_log_,
            net::NetLogWithSource()),
        factory_(factory),
        resolver_(resolver),
        callback_(std::move(callback)),
        error_observer_(std::move(error_observer)) {
    factory_->mojo_proxy_factory_->CreateResolver(
        base::UTF16ToUTF8(pac_script->utf16()),
        resolver_remote_.InitWithNewPipeAndPassReceiver(),
        receiver_.BindNewPipeAndPassRemote());
    receiver_.set_disconnect_handler(
        base::BindOnce(&ProxyResolverFactoryMojo::Job::OnMojoDisconnect,
                       base::Unretained(this)));
  }

  ~Job() override {
    // Clear pointer to `error_observer_` our base class stores.
    DetachMixin();
  }

  void OnMojoDisconnect() { ReportResult(net::ERR_PAC_SCRIPT_TERMINATED); }

 private:
  void ReportResult(int32_t error) override {
    // Prevent any other messages arriving unexpectedly, in the case |this|
    // isn't destroyed immediately.
    receiver_.reset();

    // Clear raw pointers from the mojo mixin --- the callback may clean up
    // stuff, and it can't be invoked without `receiver_` connected anyway.
    DetachMixin();

    if (error == net::OK) {
      *resolver_ = std::make_unique<ProxyResolverMojo>(
          std::move(resolver_remote_), factory_->host_resolver_,
          std::move(error_observer_), factory_->net_log_);
    }
    std::move(callback_).Run(error);
  }

  const raw_ptr<ProxyResolverFactoryMojo> factory_;
  raw_ptr<std::unique_ptr<net::ProxyResolver>> resolver_;
  net::CompletionOnceCallback callback_;
  mojo::PendingRemote<proxy_resolver::mojom::ProxyResolver> resolver_remote_;
  mojo::Receiver<proxy_resolver::mojom::ProxyResolverFactoryRequestClient>
      receiver_{this};
  std::unique_ptr<net::ProxyResolverErrorObserver> error_observer_;
};

ProxyResolverFactoryMojo::ProxyResolverFactoryMojo(
    mojo::PendingRemote<proxy_resolver::mojom::ProxyResolverFactory>
        mojo_proxy_factory,
    net::HostResolver* host_resolver,
    const base::RepeatingCallback<
        std::unique_ptr<net::ProxyResolverErrorObserver>()>&
        error_observer_factory,
    net::NetLog* net_log)
    : ProxyResolverFactory(true),
      mojo_proxy_factory_(std::move(mojo_proxy_factory)),
      host_resolver_(host_resolver),
      error_observer_factory_(error_observer_factory),
      net_log_(net_log) {}

ProxyResolverFactoryMojo::~ProxyResolverFactoryMojo() = default;

int ProxyResolverFactoryMojo::CreateProxyResolver(
    const scoped_refptr<net::PacFileData>& pac_script,
    std::unique_ptr<net::ProxyResolver>* resolver,
    net::CompletionOnceCallback callback,
    std::unique_ptr<net::ProxyResolverFactory::Request>* request) {
  DCHECK(resolver);
  DCHECK(request);
  if (pac_script->type() != net::PacFileData::TYPE_SCRIPT_CONTENTS ||
      pac_script->utf16().empty()) {
    return net::ERR_PAC_SCRIPT_FAILED;
  }
  *request = std::make_unique<Job>(
      this, pac_script, resolver, std::move(callback),
      error_observer_factory_.is_null() ? nullptr
                                        : error_observer_factory_.Run());
  return net::ERR_IO_PENDING;
}

}  // namespace network
