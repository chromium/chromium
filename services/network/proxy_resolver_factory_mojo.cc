// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/proxy_resolver_factory_mojo.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/task_runner.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/ip_address.h"
#include "net/base/load_states.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
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
class NetworkIsolationKey;
}

namespace network {

namespace {

base::Value NetLogErrorParams(int line_number, const std::string& message) {
  base::DictionaryValue dict;
  dict.SetInteger("line_number", line_number);
  dict.SetString("message", message);
  return std::move(dict);
}

// Implementation for myIpAddress() and myIpAddressEx() that is expected to run
// on a worker thread. Will notify |client| on completion.
void DoMyIpAddressOnWorker(
    bool is_ex,
    mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
        client_remote) {
  // Resolve the list of IP addresses.
  std::vector<net::IPAddress> my_ip_addresses =
      is_ex ? PacMyIpAddressEx() : PacMyIpAddress();

  mojo::Remote<proxy_resolver::mojom::HostResolverRequestClient> client(
      std::move(client_remote));

  // TODO(eroman): Note that this code always returns a success response (with
  // loopback) rather than passing forward the error. This is to ensure that the
  // response gets cached on the proxy resolver process side, since this layer
  // here does not currently do any caching or de-duplication. This should be
  // cleaned up once the interfaces are refactored. Lastly note that for
  // myIpAddress() this doesn't change the final result. However for
  // myIpAddressEx() it means we return 127.0.0.1 rather than empty string.
  if (my_ip_addresses.empty())
    my_ip_addresses.push_back(net::IPAddress::IPv4Localhost());

  client->ReportResult(net::OK, my_ip_addresses);
}

// A mixin that forwards logging to (Bound)NetLog and ProxyResolverErrorObserver
// and DNS requests to a MojoHostResolverImpl, which is implemented in terms of
// a HostResolver, or myIpAddress[Ex]() which is implemented by //net.
template <typename ClientInterface>
class ClientMixin : public ClientInterface {
 public:
  ClientMixin(net::HostResolver* host_resolver,
              net::ProxyResolverErrorObserver* error_observer,
              net::NetLog* net_log,
              const net::NetLogWithSource& net_log_with_source)
      : host_resolver_(host_resolver, net_log_with_source),
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
      const net::NetworkIsolationKey& network_isolation_key,
      mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
          client) override {
    bool is_ex = operation == net::ProxyResolveDnsOperation::DNS_RESOLVE_EX ||
                 operation == net::ProxyResolveDnsOperation::MY_IP_ADDRESS_EX;

    if (operation == net::ProxyResolveDnsOperation::MY_IP_ADDRESS ||
        operation == net::ProxyResolveDnsOperation::MY_IP_ADDRESS_EX) {
      GetMyIpAddressTaskRuner()->PostTask(
          FROM_HERE,
          base::BindOnce(&DoMyIpAddressOnWorker, is_ex, std::move(client)));
    } else {
      // Request was for dnsResolve() or dnsResolveEx().
      host_resolver_.Resolve(hostname, network_isolation_key, is_ex,
                             std::move(client));
    }
  }

 protected:
  // TODO(eroman): This doesn't track being blocked in myIpAddress(Ex) handler.
  bool dns_request_in_progress() {
    return host_resolver_.request_in_progress();
  }

  // Returns a task runner used to run the code for myIpAddress[Ex].
  static scoped_refptr<base::TaskRunner> GetMyIpAddressTaskRuner() {
    // TODO(eroman): While these tasks are expected to normally run quickly,
    // it would be prudent to enforce a bound on outstanding tasks, and maybe
    // de-duplication of requests.
    //
    // However the better place to focus on is de-duplication and caching on the
    // proxy service side (which currently caches but doesn't de-duplicate).
    return base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN,
         base::TaskPriority::USER_VISIBLE});
  }

 private:
  MojoHostResolverImpl host_resolver_;
  net::ProxyResolverErrorObserver* const error_observer_;
  net::NetLog* const net_log_;
  const net::NetLogWithSource net_log_with_source_;
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
  ~ProxyResolverMojo() override;

  // ProxyResolver implementation:
  int GetProxyForURL(const GURL& url,
                     const net::NetworkIsolationKey& network_isolation_key,
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

  net::HostResolver* host_resolver_;

  std::unique_ptr<net::ProxyResolverErrorObserver> error_observer_;

  net::NetLog* net_log_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolverMojo);
};

class ProxyResolverMojo::Job
    : public ProxyResolver::Request,
      public ClientMixin<proxy_resolver::mojom::ProxyResolverRequestClient> {
 public:
  Job(ProxyResolverMojo* resolver,
      const GURL& url,
      const net::NetworkIsolationKey& network_isolation_key,
      net::ProxyInfo* results,
      net::CompletionOnceCallback callback,
      const net::NetLogWithSource& net_log);
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
  net::ProxyInfo* results_;
  net::CompletionOnceCallback callback_;

  SEQUENCE_CHECKER(sequence_checker_);
  mojo::Receiver<proxy_resolver::mojom::ProxyResolverRequestClient> receiver_{
      this};

  DISALLOW_COPY_AND_ASSIGN(Job);
};

ProxyResolverMojo::Job::Job(
    ProxyResolverMojo* resolver,
    const GURL& url,
    const net::NetworkIsolationKey& network_isolation_key,
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
      url_, network_isolation_key, receiver_.BindNewPipeAndPassRemote());
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
  std::move(callback).Run(result);
}

void ProxyResolverMojo::Job::ReportResult(int32_t error,
                                          const net::ProxyInfo& proxy_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "ProxyResolverMojo::Job::ReportResult: " << error;

  if (error == net::OK) {
    *results_ = proxy_info;
    DVLOG(1) << "Servers: " << results_->ToPacString();
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
    const net::NetworkIsolationKey& network_isolation_key,
    net::ProxyInfo* results,
    net::CompletionOnceCallback callback,
    std::unique_ptr<Request>* request,
    const net::NetLogWithSource& net_log) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!mojo_proxy_resolver_remote_)
    return net::ERR_PAC_SCRIPT_TERMINATED;

  *request = std::make_unique<Job>(this, url, network_isolation_key, results,
                                   std::move(callback), net_log);

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

  void OnMojoDisconnect() { ReportResult(net::ERR_PAC_SCRIPT_TERMINATED); }

 private:
  void ReportResult(int32_t error) override {
    // Prevent any other messages arriving unexpectedly, in the case |this|
    // isn't destroyed immediately.
    receiver_.reset();

    if (error == net::OK) {
      *resolver_ = std::make_unique<ProxyResolverMojo>(
          std::move(resolver_remote_), factory_->host_resolver_,
          std::move(error_observer_), factory_->net_log_);
    }
    std::move(callback_).Run(error);
  }

  ProxyResolverFactoryMojo* const factory_;
  std::unique_ptr<net::ProxyResolver>* resolver_;
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
