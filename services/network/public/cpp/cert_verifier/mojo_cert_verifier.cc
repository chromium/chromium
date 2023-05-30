// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cert_verifier/mojo_cert_verifier.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/cert_verifier_service.mojom.h"

namespace cert_verifier {
namespace {
class CertVerifierRequestImpl : public mojom::CertVerifierRequest,
                                public net::CertVerifier::Request {
 public:
  CertVerifierRequestImpl(
      mojo::PendingReceiver<mojom::CertVerifierRequest> receiver,
      scoped_refptr<net::X509Certificate> cert,
      net::CertVerifyResult* verify_result,
      net::CompletionOnceCallback callback,
      const net::NetLogWithSource& net_log)
      : receiver_(this, std::move(receiver)),
        cert_(cert),
        cert_verify_result_(verify_result),
        completion_callback_(std::move(callback)),
        net_log_(net_log) {
    // Use of base::Unretained safe because |this| owns |receiver_|, so the
    // callback will never be called once |this| is deleted.
    receiver_.set_disconnect_handler(base::BindOnce(
        &CertVerifierRequestImpl::DisconnectRequest, base::Unretained(this)));
  }

  // This will cancel the request by destroying |receiver_|, disconnecting the
  // Mojo pipe.
  ~CertVerifierRequestImpl() override = default;

  void Complete(const net::CertVerifyResult& result,
                int32_t net_error) override {
    *cert_verify_result_ = result;
    // Complete should not be called more than once.
    DCHECK(completion_callback_);
    // We've received our result and no longer need to know about
    // disconnections.
    receiver_.set_disconnect_handler(base::OnceClosure());
    std::move(completion_callback_).Run(net_error);
  }

  void DisconnectRequest() {
    // The CertVerifierRequest disconnected.
    DCHECK(completion_callback_);
    *cert_verify_result_ = net::CertVerifyResult();
    cert_verify_result_->verified_cert = cert_;
    cert_verify_result_->cert_status = net::CERT_STATUS_INVALID;
    std::move(completion_callback_).Run(net::ERR_ABORTED);
  }

 private:
  mojo::Receiver<mojom::CertVerifierRequest> receiver_;
  // Certificate being verified.
  scoped_refptr<net::X509Certificate> cert_;
  // Out parameter for the result.
  raw_ptr<net::CertVerifyResult> cert_verify_result_;
  // Callback to call once the result is available.
  net::CompletionOnceCallback completion_callback_;

  const net::NetLogWithSource net_log_;
};
}  // namespace

class MojoCertVerifier::MojoReconnector
    : public mojom::URLLoaderFactoryConnector {
 public:
  MojoReconnector(
      mojo::PendingReceiver<mojom::URLLoaderFactoryConnector> receiver,
      base::RepeatingCallback<void(
          mojo::PendingReceiver<network::mojom::URLLoaderFactory>)> reconnector)
      : receiver_(this, std::move(receiver)),
        reconnector_(std::move(reconnector)) {}

  // mojom::URLLoaderFactoryConnector implementation:
  void CreateURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>
          url_loader_factory) override {
    reconnector_.Run(std::move(url_loader_factory));
  }

 private:
  mojo::Receiver<mojom::URLLoaderFactoryConnector> receiver_;
  base::RepeatingCallback<void(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>)>
      reconnector_;
};

MojoCertVerifier::MojoCertVerifier(
    mojo::PendingRemote<mojom::CertVerifierService> mojo_cert_verifier,
    mojo::PendingReceiver<mojom::CertVerifierServiceClient> client_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory,
    base::RepeatingCallback<void(
        mojo::PendingReceiver<network::mojom::URLLoaderFactory>)> reconnector)
    : mojo_cert_verifier_(std::move(mojo_cert_verifier)),
      client_receiver_(this, std::move(client_receiver)) {
  mojo::PendingRemote<mojom::URLLoaderFactoryConnector> reconnector_remote;

  reconnector_ = std::make_unique<MojoReconnector>(
      reconnector_remote.InitWithNewPipeAndPassReceiver(),
      std::move(reconnector));
  mojo_cert_verifier_->EnableNetworkAccess(std::move(url_loader_factory),
                                           std::move(reconnector_remote));
}

MojoCertVerifier::~MojoCertVerifier() = default;

int MojoCertVerifier::Verify(
    const net::CertVerifier::RequestParams& params,
    net::CertVerifyResult* verify_result,
    net::CompletionOnceCallback callback,
    std::unique_ptr<net::CertVerifier::Request>* out_req,
    const net::NetLogWithSource& net_log) {
  DVLOG(3) << "MojoCertVerifier asked to validate hostname: "
           << params.hostname();
  mojo::PendingRemote<mojom::CertVerifierRequest> cert_verifier_request;
  auto cert_verifier_receiver =
      cert_verifier_request.InitWithNewPipeAndPassReceiver();
  mojo_cert_verifier_->Verify(params, net_log.source(),
                              std::move(cert_verifier_request));
  *out_req = std::make_unique<CertVerifierRequestImpl>(
      std::move(cert_verifier_receiver), params.certificate(), verify_result,
      std::move(callback), net_log);

  return net::ERR_IO_PENDING;
}

void MojoCertVerifier::SetConfig(const net::CertVerifier::Config& config) {
  mojo_cert_verifier_->SetConfig(config);
}

void MojoCertVerifier::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MojoCertVerifier::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MojoCertVerifier::OnCertVerifierChanged() {
  for (Observer& observer : observers_) {
    observer.OnCertVerifierChanged();
  }
}

void MojoCertVerifier::FlushForTesting() {
  mojo_cert_verifier_.FlushForTesting();
}
}  // namespace cert_verifier
