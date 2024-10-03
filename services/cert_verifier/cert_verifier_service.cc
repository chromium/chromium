// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/cert_verifier/cert_verifier_service.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/completion_once_callback.h"
#include "net/base/ip_address.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/x509_util.h"
#include "services/cert_verifier/cert_net_url_loader/cert_net_fetcher_url_loader.h"
#include "services/cert_verifier/cert_verifier_service_factory.h"
#include "services/network/public/mojom/cert_verifier_service.mojom.h"

namespace cert_verifier {
namespace internal {

namespace {

// Owns everything once |Verify()| is called on the underlying CertVerifier.
// Handles disconnection of the remote cert verification request gracefully.
class CertVerifyResultHelper {
 public:
  CertVerifyResultHelper() = default;

  void Initialize(mojo::PendingRemote<mojom::CertVerifierRequest> request,
                  std::unique_ptr<net::CertVerifyResult> result) {
    request_.Bind(std::move(request));
    result_ = std::move(result);

    // base::Unretained is safe because |request_| is owned by this object, so
    // the disconnect handler cannot be called after this object is destroyed.
    request_.set_disconnect_handler(base::BindOnce(
        &CertVerifyResultHelper::DisconnectRequest, base::Unretained(this)));
  }

  // This member function is meant to be wrapped in a OnceCallback that owns
  // |this|, and passed to |CertVerifier::Verify()|.
  void CompleteCertVerifierRequest(int net_error) {
    DCHECK(request_.is_bound());
    DCHECK(local_request_);
    DCHECK(result_);

    request_->Complete(*result_, net_error);

    // After returning from this function, |this| will be deleted.
  }

  void DisconnectRequest() {
    DCHECK(request_.is_bound());
    DCHECK(local_request_);
    DCHECK(result_);

    // |request_| disconnected. At this point we should delete our
    // |local_request_| to pass on the "cancellation message" to the underlying
    // cert verifier. Deleting |local_request_| will also delete |this|.
    // Deleting |local_request_| also guarantees that
    // CompleteCertVerifierRequest() will never get called.
    local_request_.reset();
  }

  std::unique_ptr<net::CertVerifier::Request>* local_request() {
    return &local_request_;
  }

 private:
  mojo::Remote<mojom::CertVerifierRequest> request_;
  std::unique_ptr<net::CertVerifyResult> result_;
  std::unique_ptr<net::CertVerifier::Request> local_request_;
};

void ReconnectURLLoaderFactory(
    mojo::Remote<mojom::URLLoaderFactoryConnector>* reconnector,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  (*reconnector)->CreateURLLoaderFactory(std::move(receiver));
}

}  // namespace

CertVerifierServiceImpl::CertVerifierServiceImpl(
    std::unique_ptr<net::CertVerifierWithUpdatableProc> verifier,
    mojo::PendingReceiver<mojom::CertVerifierService> service_receiver,
    mojo::PendingReceiver<mojom::CertVerifierServiceUpdater> updater_receiver,
    mojo::PendingRemote<mojom::CertVerifierServiceClient> client,
    scoped_refptr<CertNetFetcherURLLoader> cert_net_fetcher,
    net::CertVerifyProc::InstanceParams instance_params,
    bool wait_for_update)
    : instance_params_(std::move(instance_params)),
      verifier_(std::move(verifier)),
      service_receiver_(this, std::move(service_receiver)),
      updater_receiver_(this, std::move(updater_receiver)),
      client_(std::move(client)),
      cert_net_fetcher_(std::move(cert_net_fetcher)),
      waiting_for_update_(wait_for_update) {
  // base::Unretained is safe because |this| owns |receiver_|, so deleting
  // |this| will prevent |receiver_| from calling this callback.
  service_receiver_.set_disconnect_handler(
      base::BindRepeating(&CertVerifierServiceImpl::OnDisconnectFromService,
                          base::Unretained(this)));
  verifier_->AddObserver(this);
  if (waiting_for_update_) {
    wait_start_time_ = base::TimeTicks::Now();
  }
}

// Note: this object owns the underlying CertVerifier, which owns all of the
// callbacks passed to their Verify methods. These callbacks own the
// mojo::Remote<CertVerifierRequest> objects, so destroying this object cancels
// the verifications and all the callbacks.
CertVerifierServiceImpl::~CertVerifierServiceImpl() {
  verifier_->RemoveObserver(this);
  if (cert_net_fetcher_)
    cert_net_fetcher_->Shutdown();
}

void CertVerifierServiceImpl::SetConfig(
    const net::CertVerifier::Config& config) {
  verifier_->SetConfig(config);
}

void CertVerifierServiceImpl::EnableNetworkAccess(
    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory,
    mojo::PendingRemote<mojom::URLLoaderFactoryConnector> reconnector) {
  if (cert_net_fetcher_) {
    auto reconnect_cb =
        std::make_unique<mojo::Remote<mojom::URLLoaderFactoryConnector>>(
            std::move(reconnector));
    cert_net_fetcher_->SetURLLoaderFactoryAndReconnector(
        std::move(url_loader_factory),
        base::BindRepeating(&ReconnectURLLoaderFactory,
                            base::Owned(std::move(reconnect_cb))));
  }
}

void CertVerifierServiceImpl::UpdateAdditionalCertificates(
    mojom::AdditionalCertificatesPtr additional_certificates) {
  UpdateCertVerifierInstanceParams(additional_certificates, &instance_params_);
  // TODO(hchao, mattm): figure out what to do if the CertVerifierServiceFactory
  // is destroyed before the CertVerifierService (or if this is not possible in
  // normal circumstances, add a DCHECK or CHECK here).
  verifier_->UpdateVerifyProcData(cert_net_fetcher_,
                                  service_factory_impl_->get_impl_params(),
                                  instance_params_);
  if (waiting_for_update_) {
    base::UmaHistogramTimes("Net.CertVerifier.TimeUntilReady",
                            base::TimeTicks::Now() - wait_start_time_);
    base::UmaHistogramCounts100("Net.CertVerifier.QueuedRequestsWhenReady",
                                queued_requests_.size());
  }
  waiting_for_update_ = false;

  // Empty queue if necessary
  for (auto& queued_request : queued_requests_) {
    VerifyHelper(queued_request.params, queued_request.net_log_source,
                 std::move(queued_request.cert_verifier_request));
  }

  queued_requests_.clear();
}

void CertVerifierServiceImpl::SetCertVerifierServiceFactory(
    base::WeakPtr<cert_verifier::CertVerifierServiceFactoryImpl>
        service_factory_impl) {
  service_factory_impl_ = std::move(service_factory_impl);
}

void CertVerifierServiceImpl::UpdateVerifierData(
    const net::CertVerifyProc::ImplParams& impl_params) {
  verifier_->UpdateVerifyProcData(cert_net_fetcher_, impl_params,
                                  instance_params_);
}

void CertVerifierServiceImpl::Verify(
    const net::CertVerifier::RequestParams& params,
    const net::NetLogSource& net_log_source,
    mojo::PendingRemote<mojom::CertVerifierRequest> cert_verifier_request) {
  DVLOG(3) << "Received certificate validation request for hostname: "
           << params.hostname();

  if (waiting_for_update_) {
    DVLOG(3)
        << "initial cert update not received, queueing request for hostname: "
        << params.hostname();
    QueuedCertVerifyRequest queued_request;
    queued_request.params = std::move(params);
    queued_request.net_log_source = std::move(net_log_source);
    queued_request.cert_verifier_request = std::move(cert_verifier_request);

    queued_requests_.push_back(std::move(queued_request));
  } else {
    VerifyHelper(params, net_log_source, std::move(cert_verifier_request));
  }
}

void CertVerifierServiceImpl::VerifyHelper(
    const net::CertVerifier::RequestParams& params,
    const net::NetLogSource& net_log_source,
    mojo::PendingRemote<mojom::CertVerifierRequest> cert_verifier_request) {
  DVLOG(3) << "Running certificate validation request for hostname: "
           << params.hostname();
  auto result = std::make_unique<net::CertVerifyResult>();

  auto result_helper = std::make_unique<CertVerifyResultHelper>();

  CertVerifyResultHelper* result_helper_ptr = result_helper.get();

  // It's okay for this callback to delete |result_helper| and its
  // |local_request_| variable, as CertVerifier::Verify allows it, even though
  // in MultiThreadedCertVerifier, |local_request_| will in turn own this
  // callback. |local_request_| gives up ownership of the callback before
  // calling it.
  net::CompletionOnceCallback callback =
      base::BindOnce(&CertVerifyResultHelper::CompleteCertVerifierRequest,
                     std::move(result_helper));

  // Note it is valid to pass a unbound NetLogWithSource into
  // CertVerifier::Verify. If that occurs the net_log_source.id passed through
  // mojo will be kInvalidId and the NetLogWithSource::Make below will in turn
  // create an unbound NetLogWithSource.
  int net_err = verifier_->Verify(
      params, result.get(), std::move(callback),
      result_helper_ptr->local_request(),
      net::NetLogWithSource::Make(net::NetLog::Get(), net_log_source));
  if (net_err == net::ERR_IO_PENDING) {
    // If this request is to be completely asynchronously, give the callback
    // ownership of our mojom::CertVerifierRequest and net::CertVerifyResult.
    result_helper_ptr->Initialize(std::move(cert_verifier_request),
                                  std::move(result));
  } else {
    // If we already finished, then we can just complete the request
    // immediately.
    mojo::Remote<mojom::CertVerifierRequest> remote(
        std::move(cert_verifier_request));
    remote->Complete(*result, net_err);
  }
}

void CertVerifierServiceImpl::OnCertVerifierChanged() {
  client_->OnCertVerifierChanged();
}

void CertVerifierServiceImpl::OnDisconnectFromService() {
  if (service_factory_impl_) {
    service_factory_impl_->RemoveService(this);
  }
  delete this;
}

CertVerifierServiceImpl::QueuedCertVerifyRequest::QueuedCertVerifyRequest() =
    default;
CertVerifierServiceImpl::QueuedCertVerifyRequest::~QueuedCertVerifyRequest() =
    default;

CertVerifierServiceImpl::QueuedCertVerifyRequest::QueuedCertVerifyRequest(
    CertVerifierServiceImpl::QueuedCertVerifyRequest&&) = default;
CertVerifierServiceImpl::QueuedCertVerifyRequest&
CertVerifierServiceImpl::QueuedCertVerifyRequest::operator=(
    CertVerifierServiceImpl::QueuedCertVerifyRequest&& other) = default;

}  // namespace internal
}  // namespace cert_verifier
