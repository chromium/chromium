// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CERT_VERIFIER_CERT_VERIFIER_SERVICE_H_
#define SERVICES_CERT_VERIFIER_CERT_VERIFIER_SERVICE_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_proc.h"
#include "net/log/net_log_with_source.h"
#include "services/cert_verifier/cert_net_url_loader/cert_net_fetcher_url_loader.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/cpp/network_service_buildflags.h"
#include "services/network/public/mojom/cert_verifier_service.mojom.h"
#include "services/network/public/mojom/cert_verifier_service_updater.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

#if BUILDFLAG(IS_CT_SUPPORTED)
#include "services/network/public/mojom/network_context.mojom-forward.h"
#endif

namespace net {
class ChromeRootStoreData;
}

// Defines an implementation of a Cert Verifier Service to be queried by network
// service or others.
namespace cert_verifier {

class CertVerifierServiceFactoryImpl;

namespace internal {

// This class will delete itself upon disconnection of its Mojo receiver.
class CertVerifierServiceImpl : public mojom::CertVerifierService,
                                public mojom::CertVerifierServiceUpdater,
                                public net::CertVerifier::Observer {
 public:
  // Create a CertVerifierServiceImpl that will verify certificates requested
  // through `service_receiver` using `verifier`. The verifier parameters may
  // be updated through `updater_receiver` as well as the public methods such
  // as `UpdateVerifierData`. Observers will be notified of updates through
  // `client`.
  // `cert_net_fetcher` may be used for network requests during verification,
  // once `EnableNetworkAccess` has been called.
  // `instance_params` is the initial set of InstanceParams used by `verifier`.
  // If `wait_for_additional_certificates` is true, `Verify` calls will be
  // blocked until the first `UpdateAdditionalCertificates` call. If
  // `wait_for_component_data` is true, `Verify` calls will be blocked until
  // `OnComponentDataLoaded` is called. If both are true, verifications will be
  // blocked until both conditions are satisfied.
  explicit CertVerifierServiceImpl(
      std::unique_ptr<net::CertVerifierWithUpdatableProc> verifier,
      mojo::PendingReceiver<mojom::CertVerifierService> service_receiver,
      mojo::PendingReceiver<mojom::CertVerifierServiceUpdater> updater_receiver,
      mojo::PendingRemote<mojom::CertVerifierServiceClient> client,
      scoped_refptr<CertNetFetcherURLLoader> cert_net_fetcher,
      net::CertVerifyProc::InstanceParams instance_params,
      bool wait_for_additional_certificates,
      bool wait_for_component_data);

  // mojom::CertVerifierService implementation:
  void Verify(const net::CertVerifier::RequestParams& params,
              const net::NetLogSource& net_log_source,
              mojo::PendingRemote<mojom::CertVerifierRequest>
                  cert_verifier_request) override;
  void Verify2QwacBinding(
      const std::string& binding,
      const std::string& hostname,
      const scoped_refptr<net::X509Certificate>& tls_cert,
      const net::NetLogSource& net_log_source,
      base::OnceCallback<void(const scoped_refptr<net::X509Certificate>&)>
          callback) override;
  void SetConfig(const net::CertVerifier::Config& config) override;
  void EnableNetworkAccess(
      mojo::PendingRemote<network::mojom::URLLoaderFactory>,
      mojo::PendingRemote<mojom::URLLoaderFactoryConnector> reconnector)
      override;

  // mojom::CertVerifierServiceUpdater implementation:
  void UpdateAdditionalCertificates(
      mojom::AdditionalCertificatesPtr additional_certificates) override;
  void WaitUntilNextUpdateForTesting(
      WaitUntilNextUpdateForTestingCallback callback) override;
#if BUILDFLAG(IS_CT_SUPPORTED)
  void SetCTPolicy(network::mojom::CTPolicyPtr ct_policy) override;
#endif

  // Set a pointer to the CertVerifierServiceFactory so that it may be notified
  // when we are deleted.
  void SetCertVerifierServiceFactory(
      base::WeakPtr<cert_verifier::CertVerifierServiceFactoryImpl>
          service_factory_impl);

  // Update the wrapped verifier with CRLSet and ChromeRootStoreData.
  void UpdateVerifierData(const net::CertVerifyProc::ImplParams& impl_params);

  // Called when all initial component data that the factory was waiting for has
  // finished loading.
  void OnComponentDataLoaded();

 private:
  struct QueuedCertVerifyRequest {
    QueuedCertVerifyRequest();
    ~QueuedCertVerifyRequest();
    QueuedCertVerifyRequest(QueuedCertVerifyRequest&&);
    QueuedCertVerifyRequest& operator=(QueuedCertVerifyRequest&& other);

    net::CertVerifier::RequestParams params;
    net::NetLogSource net_log_source;
    mojo::PendingRemote<mojom::CertVerifierRequest> cert_verifier_request;
  };

  ~CertVerifierServiceImpl() override;

  // CertVerifier::Observer methods:
  void OnCertVerifierChanged() override;

  void OnDisconnectFromService();

  bool IsWaiting() const;
  void MaybeUpdateVerifierAndRunQueuedRequests();

  void VerifyHelper(
      const net::CertVerifier::RequestParams& params,
      const net::NetLogSource& net_log_source,
      mojo::PendingRemote<mojom::CertVerifierRequest> cert_verifier_request);

  net::CertVerifyProc::InstanceParams instance_params_;
  std::unique_ptr<net::CertVerifierWithUpdatableProc> verifier_;
  mojo::Receiver<mojom::CertVerifierService> service_receiver_;
  mojo::Receiver<mojom::CertVerifierServiceUpdater> updater_receiver_;
  mojo::Remote<mojom::CertVerifierServiceClient> client_;
  scoped_refptr<CertNetFetcherURLLoader> cert_net_fetcher_;
  base::WeakPtr<cert_verifier::CertVerifierServiceFactoryImpl>
      service_factory_impl_;
  // Requests are queued while either of these is true.
  bool waiting_for_additional_certificates_;
  bool waiting_for_component_data_;
  bool pending_verifier_data_update_ = false;
  bool enable_network_access_called_ = false;
  std::vector<QueuedCertVerifyRequest> queued_requests_;
  WaitUntilNextUpdateForTestingCallback update_complete_callback_;
};

}  // namespace internal
}  // namespace cert_verifier

#endif  // SERVICES_CERT_VERIFIER_CERT_VERIFIER_SERVICE_H_
