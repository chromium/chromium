// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_origin_prober.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "content/browser/preloading/prefetch/prefetch_canary_checker.h"
#include "content/browser/preloading/prefetch/prefetch_dns_prober.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/host_port_pair.h"
#include "net/base/isolation_info.h"
#include "net/base/network_anonymization_key.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/public/mojom/tls_socket.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/origin.h"

namespace content {
namespace {

net::NetworkTrafficAnnotationTag GetProbingTrafficAnnotation() {
  return net::DefineNetworkTrafficAnnotation("speculation_rules_prefetch_probe",
                                             R"(
        semantics {
          sender: "Speculation Rules Prefetch Probe"
          description:
            "Verifies the end to end connection between the browser and the "
            "origin site that the user is currently navigating to. This is "
            "done during a navigation that was previously prefetched over a "
            "proxy to check that the site is not blocked by middleboxes. "
            "Such prefetches will be used to fetch render-blocking "
            "content before being navigated by the user without impacting "
            "privacy."
          trigger:
            "Used only when this feature and speculation rules feature are "
            "enabled."
          data: "None."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control this via a setting specific to each content "
            "embedder."
          policy_exception_justification: "Not implemented."
      })");
}

void TLSDropHandler(base::OnceClosure ui_only_callback) {
  GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(ui_only_callback));
}

class TLSProber {
 public:
  TLSProber(const GURL& url,
            PrefetchOriginProber::OnProbeResultCallback callback)
      : url_(url), callback_(std::move(callback)) {}
  ~TLSProber() { DCHECK(!callback_); }

  network::mojom::NetworkContext::CreateTCPConnectedSocketCallback
  GetOnTCPConnectedCallback() {
    network::mojom::NetworkContext::CreateTCPConnectedSocketCallback
        tcp_handler = base::BindOnce(&TLSProber::OnTCPConnected,
                                     weak_factory_.GetWeakPtr());

    return mojo::WrapCallbackWithDropHandler(std::move(tcp_handler),
                                             GetDropHandler());
  }

  mojo::PendingReceiver<network::mojom::TCPConnectedSocket>
  GetTCPSocketReceiver() {
    return tcp_socket_.BindNewPipeAndPassReceiver();
  }

 private:
  void OnTCPConnected(int result,
                      const std::optional<net::IPEndPoint>& local_addr,
                      const std::optional<net::IPEndPoint>& peer_adder,
                      mojo::ScopedDataPipeConsumerHandle receive_stream,
                      mojo::ScopedDataPipeProducerHandle send_stream) {
    if (result != net::OK) {
      HandleFailure();
      return;
    }

    network::mojom::TCPConnectedSocket::UpgradeToTLSCallback tls_handler =
        base::BindOnce(&TLSProber::OnUpgradeToTLS, weak_factory_.GetWeakPtr());

    tcp_socket_->UpgradeToTLS(
        net::HostPortPair::FromURL(url_), /*options=*/nullptr,
        net::MutableNetworkTrafficAnnotationTag(GetProbingTrafficAnnotation()),
        tls_socket_.BindNewPipeAndPassReceiver(),
        /*observer=*/mojo::NullRemote(),
        mojo::WrapCallbackWithDropHandler(std::move(tls_handler),
                                          GetDropHandler()));
  }

  void OnUpgradeToTLS(int result,
                      mojo::ScopedDataPipeConsumerHandle receive_stream,
                      mojo::ScopedDataPipeProducerHandle send_stream,
                      const std::optional<net::SSLInfo>& ssl_info) {
    std::move(callback_).Run(result == net::OK
                                 ? PrefetchProbeResult::kTLSProbeSuccess
                                 : PrefetchProbeResult::kTLSProbeFailure);
    delete this;
  }

  base::OnceClosure GetDropHandler() {
    // The drop handler is not guaranteed to be run on the original thread. Use
    // the anon method above to fix that.
    return base::BindOnce(
        &TLSDropHandler,
        base::BindOnce(&TLSProber::HandleFailure, weak_factory_.GetWeakPtr()));
  }

  void HandleFailure() {
    std::move(callback_).Run(PrefetchProbeResult::kTLSProbeFailure);
    delete this;
  }

  // The URL of the resource being probed. Only the host:port is used.
  const GURL url_;

  // The callback to run when the probe is complete.
  PrefetchOriginProber::OnProbeResultCallback callback_;

  // Mojo sockets. We only test that both can be connected.
  mojo::Remote<network::mojom::TCPConnectedSocket> tcp_socket_;
  mojo::Remote<network::mojom::TLSClientSocket> tls_socket_;

  base::WeakPtrFactory<TLSProber> weak_factory_{this};
};

}  // namespace

PrefetchOriginProber::PrefetchOriginProber(BrowserContext* browser_context,
                                           const GURL& dns_canary_check_url,
                                           const GURL& tls_canary_check_url)
    : browser_context_(browser_context) {
  // Check if probing is enabled

  PrefetchCanaryChecker::RetryPolicy retry_policy;
  retry_policy.max_retries = PrefetchCanaryCheckRetries();

  if (PrefetchTLSCanaryCheckEnabled()) {
    tls_canary_checker_ = PrefetchCanaryChecker::MakePrefetchCanaryChecker(
        browser_context_, PrefetchCanaryChecker::CheckType::kTLS,
        tls_canary_check_url, retry_policy, PrefetchCanaryCheckTimeout(),
        PrefetchCanaryCheckCacheLifetime());
  }

  dns_canary_checker_ = PrefetchCanaryChecker::MakePrefetchCanaryChecker(
      browser_context_, PrefetchCanaryChecker::CheckType::kDNS,
      dns_canary_check_url, retry_policy, PrefetchCanaryCheckTimeout(),
      PrefetchCanaryCheckCacheLifetime());
}

PrefetchOriginProber::~PrefetchOriginProber() = default;

void PrefetchOriginProber::RunCanaryChecksIfNeeded() const {
  if (!PrefetchProbingEnabled() || !PrefetchCanaryCheckEnabled())
    return;

  if (dns_canary_checker_)
    dns_canary_checker_->RunChecksIfNeeded();
  if (tls_canary_checker_)
    tls_canary_checker_->RunChecksIfNeeded();
}

bool PrefetchOriginProber::ShouldProbeOrigins() const {
  if (!PrefetchProbingEnabled())
    return false;
  if (!PrefetchCanaryCheckEnabled() || !dns_canary_checker_)
    return true;

  // We call CanaryCheckSuccessful on all enabled canary checks to make sure
  // their cache gets refreshed if necessary.
  bool dns_success =
      dns_canary_checker_->CanaryCheckSuccessful().value_or(false);
  bool tls_success = true;
  if (tls_canary_checker_)
    tls_success = tls_canary_checker_->CanaryCheckSuccessful().value_or(false);

  // If either check has failed or not completed in time, then probe.
  return !dns_success || !tls_success;
}

void PrefetchOriginProber::Probe(const GURL& url,
                                 OnProbeResultCallback callback) {
  // If canary checks are disabled, or if the TLS canary check is enabled and
  // failed (or did not complete), do TLS probing.
  bool also_do_tls_connect = !PrefetchCanaryCheckEnabled() ||
      (tls_canary_checker_ &&
       !tls_canary_checker_->CanaryCheckSuccessful().value_or(false));

  StartDNSResolution(url, std::move(callback), also_do_tls_connect);
}

void PrefetchOriginProber::StartDNSResolution(const GURL& url,
                                              OnProbeResultCallback callback,
                                              bool also_do_tls_connect) {
  net::NetworkAnonymizationKey nak =
      net::IsolationInfo::CreateForInternalRequest(url::Origin::Create(url))
          .network_anonymization_key();

  network::mojom::ResolveHostParametersPtr resolve_host_parameters =
      network::mojom::ResolveHostParameters::New();
  // This action is navigation-blocking, so use the highest priority.
  resolve_host_parameters->initial_priority = net::RequestPriority::HIGHEST;

  mojo::PendingRemote<network::mojom::ResolveHostClient> client_remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<PrefetchDNSProber>(base::BindOnce(
          &PrefetchOriginProber::OnDNSResolved, weak_factory_.GetWeakPtr(), url,
          std::move(callback), also_do_tls_connect)),
      client_remote.InitWithNewPipeAndPassReceiver());

  // TODO(crbug.com/40235854): Consider passing a SchemeHostPort to trigger
  // HTTPS DNS resource record query.
  browser_context_->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                        net::HostPortPair::FromURL(url)),
                    nak, std::move(resolve_host_parameters),
                    std::move(client_remote));
}

void PrefetchOriginProber::OnDNSResolved(
    const GURL& url,
    OnProbeResultCallback callback,
    bool also_do_tls_connect,
    int net_error,
    const std::optional<net::AddressList>& resolved_addresses) {
  bool successful = net_error == net::OK && resolved_addresses &&
                    !resolved_addresses->empty();

  // A TLS connection needs the resolved addresses, so it also fails here.
  if (!successful) {
    std::move(callback).Run(PrefetchProbeResult::kDNSProbeFailure);
    return;
  }

  if (!also_do_tls_connect) {
    std::move(callback).Run(PrefetchProbeResult::kDNSProbeSuccess);
    return;
  }

  DoTLSProbeAfterDNSResolution(url, std::move(callback), *resolved_addresses);
}

void PrefetchOriginProber::DoTLSProbeAfterDNSResolution(
    const GURL& url,
    OnProbeResultCallback callback,
    const net::AddressList& addresses) {
  DCHECK(!addresses.empty());

  std::unique_ptr<TLSProber> prober =
      std::make_unique<TLSProber>(url, std::move(callback));

  browser_context_->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->CreateTCPConnectedSocket(
          /*local_addr=*/std::nullopt, addresses,
          /*tcp_connected_socket_options=*/nullptr,
          net::MutableNetworkTrafficAnnotationTag(
              GetProbingTrafficAnnotation()),
          prober->GetTCPSocketReceiver(),
          /*observer=*/mojo::NullRemote(), prober->GetOnTCPConnectedCallback());

  // |prober| manages its own lifetime, using the mojo pipes.
  prober.release();
}

}  // namespace content
