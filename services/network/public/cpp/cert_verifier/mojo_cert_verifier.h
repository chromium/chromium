// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CERT_VERIFIER_MOJO_CERT_VERIFIER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CERT_VERIFIER_MOJO_CERT_VERIFIER_H_

#include "base/functional/callback_forward.h"
#include "base/observer_list.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/log/net_log_with_source.h"
#include "services/network/public/mojom/cert_verifier_service.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace cert_verifier {

// Implementation of net::CertVerifier that proxies across a Mojo interface to
// verify certificates.
class MojoCertVerifier : public net::CertVerifier,
                         public mojom::CertVerifierServiceClient {
 public:
  using ReconnectURLLoaderFactory = base::RepeatingCallback<void(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>)>;

  // The remote CertNetFetcher will use |url_loader_factory| for fetches. If
  // |url_loader_factory| disconnects it will use |reconnector| to try to
  // connect a new URLLoaderFactory.
  MojoCertVerifier(
      mojo::PendingRemote<mojom::CertVerifierService> mojo_cert_verifier,
      mojo::PendingReceiver<mojom::CertVerifierServiceClient> client_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory,
      ReconnectURLLoaderFactory reconnector);
  ~MojoCertVerifier() override;

  // net::CertVerifier implementation:
  int Verify(const net::CertVerifier::RequestParams& params,
             net::CertVerifyResult* verify_result,
             net::CompletionOnceCallback callback,
             std::unique_ptr<net::CertVerifier::Request>* out_req,
             const net::NetLogWithSource& net_log) override;
  void SetConfig(const net::CertVerifier::Config& config) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // mojom::CertVerifierServiceClient implementation:
  void OnCertVerifierChanged() override;

  // Flushes the underlying Mojo pipe.
  void FlushForTesting();

 private:
  class MojoReconnector;

  mojo::Remote<mojom::CertVerifierService> mojo_cert_verifier_;
  mojo::Receiver<mojom::CertVerifierServiceClient> client_receiver_;
  std::unique_ptr<MojoReconnector> reconnector_;
  base::ObserverList<Observer> observers_;
};

}  // namespace cert_verifier

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CERT_VERIFIER_MOJO_CERT_VERIFIER_H_
