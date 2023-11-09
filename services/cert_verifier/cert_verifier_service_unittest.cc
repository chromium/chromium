// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/cert_verifier/cert_verifier_service.h"

#include <stdint.h>

#include <iterator>
#include <memory>
#include <string>
#include <tuple>

#include "base/containers/adapters.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/log/net_log.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/cert_verifier/cert_verifier_service_factory.h"
#include "services/network/public/mojom/cert_verifier_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {
class CertNetFetcher;
class ChromeRootStoreData;
}  // namespace net

namespace cert_verifier {
namespace {
const int kExpectedNetError = net::ERR_CERT_INVALID;
const unsigned int kExpectedCertStatus = net::CERT_STATUS_INVALID;

scoped_refptr<net::X509Certificate> GetTestCert() {
  return net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
}

class DummyCertVerifier : public net::CertVerifierWithUpdatableProc {
 public:
  struct DummyRequest : public net::CertVerifier::Request {
    ~DummyRequest() override {
      if (cancel_cb)
        std::move(cancel_cb).Run();
    }

    net::CompletionOnceCallback callback;  // Must outlive `verify_result`.
    raw_ptr<net::CertVerifyResult> verify_result;
    base::OnceClosure cancel_cb;
    base::WeakPtrFactory<DummyRequest> weak_ptr_factory_{this};
  };

  ~DummyCertVerifier() override {
    for (base::WeakPtr<DummyRequest>& dummy_req : dummy_request_ptrs) {
      if (dummy_req)
        dummy_req->callback.Reset();
    }
  }

  // CertVerifier implementation
  int Verify(const net::CertVerifier::RequestParams& params,
             net::CertVerifyResult* verify_result,
             net::CompletionOnceCallback callback,
             std::unique_ptr<Request>* out_req,
             const net::NetLogWithSource& net_log) override {
    request_netlogs_[params] = net_log;

    if (sync_response_params_.find(params) != sync_response_params_.end()) {
      verify_result->cert_status = kExpectedCertStatus;
      verify_result->verified_cert = params.certificate();
      return kExpectedNetError;
    }

    // Don't currently handle requests with same params for simplicity.
    EXPECT_TRUE(dummy_requests_.find(params) == dummy_requests_.end());
    auto dummy_req = std::make_unique<DummyRequest>();
    dummy_request_ptrs.push_back(dummy_req->weak_ptr_factory_.GetWeakPtr());
    dummy_req->verify_result = verify_result;
    dummy_req->callback = std::move(callback);
    dummy_req->cancel_cb = base::BindLambdaForTesting([this, params]() {
      ASSERT_TRUE(cancelled_requests_.find(params) ==
                  cancelled_requests_.end());
      cancelled_requests_.insert(std::move(params));
    });
    dummy_requests_[params] = dummy_req.get();
    *out_req = std::move(dummy_req);
    return net::ERR_IO_PENDING;
  }
  void SetConfig(const Config& config) override { config_ = config; }
  void AddObserver(Observer* observer) override {
    EXPECT_FALSE(observer_);
    observer_ = observer;
  }
  void RemoveObserver(Observer* observer) override { observer_ = nullptr; }
  void UpdateVerifyProcData(
      scoped_refptr<net::CertNetFetcher> cert_net_fetcher,
      const net::CertVerifyProcFactory::ImplParams& impl_params) override {
    ADD_FAILURE() << "not handled";
  }

  void RespondToRequest(const net::CertVerifier::RequestParams& params) {
    auto it = dummy_requests_.find(params);
    ASSERT_FALSE(it == dummy_requests_.end());
    DummyRequest* req = it->second;
    dummy_requests_.erase(it);
    req->cancel_cb.Reset();
    req->verify_result->cert_status = kExpectedCertStatus;
    req->verify_result->verified_cert = params.certificate();
    std::move(req->callback).Run(kExpectedNetError);
  }

  void ShouldRespondSynchronouslyToParams(
      const net::CertVerifier::RequestParams& params) {
    sync_response_params_.insert(params);
  }

  bool WasRequestCancelled(const net::CertVerifier::RequestParams& params) {
    return cancelled_requests_.find(params) != cancelled_requests_.end();
  }

  void ExpectReceivedNetlogSource(
      const net::CertVerifier::RequestParams& params,
      const net::NetLogSource& net_log_source) {
    ASSERT_TRUE(request_netlogs_.count(params));
    EXPECT_EQ(net_log_source, request_netlogs_[params].source());
  }

  const net::CertVerifier::Config* config() const { return &config_; }

  CertVerifier::Observer* GetObserver() { return observer_; }

 private:
  std::map<net::CertVerifier::RequestParams, DummyRequest*> dummy_requests_;
  std::set<net::CertVerifier::RequestParams> sync_response_params_;
  std::set<net::CertVerifier::RequestParams> cancelled_requests_;
  // Keep WeakPtr's to all the DummyRequests in case we need to reset the
  // callbacks if |this| is destructed;
  std::vector<base::WeakPtr<DummyRequest>> dummy_request_ptrs;
  std::map<net::CertVerifier::RequestParams, net::NetLogWithSource>
      request_netlogs_;

  net::CertVerifier::Config config_;
  raw_ptr<CertVerifier::Observer> observer_ = nullptr;
};

struct DummyCVServiceRequest : public mojom::CertVerifierRequest {
  void Complete(const net::CertVerifyResult& result_param,
                int32_t net_error_param) override {
    is_completed = true;
    result = result_param;
    net_error = net_error_param;
  }

  bool is_completed = false;
  net::CertVerifyResult result;
  int net_error;
};

class CertVerifierServiceTest : public PlatformTest,
                                public mojom::CertVerifierServiceClient {
 public:
  struct RequestInfo {
    RequestInfo(net::CertVerifier::RequestParams request_params_p,
                const net::NetLogSource& net_log_source_p,
                std::unique_ptr<DummyCVServiceRequest> dummy_cv_request_p,
                std::unique_ptr<mojo::Receiver<mojom::CertVerifierRequest>>
                    cv_request_receiver_p)
        : request_params(std::move(request_params_p)),
          net_log_source(net_log_source_p),
          dummy_cv_request(std::move(dummy_cv_request_p)),
          cv_request_receiver(std::move(cv_request_receiver_p)) {}
    net::CertVerifier::RequestParams request_params;
    net::NetLogSource net_log_source;
    std::unique_ptr<DummyCVServiceRequest> dummy_cv_request;
    std::unique_ptr<mojo::Receiver<mojom::CertVerifierRequest>>
        cv_request_receiver;
  };

  // Wraps a DummyCertVerifier with a CertVerifierServiceImpl.
  CertVerifierServiceTest()
      : cv_service_client_(this), dummy_cv_(new DummyCertVerifier) {
    // NOTE: CertVerifierServiceImpl is self-deleting.
    (void)new internal::CertVerifierServiceImpl(
        base::WrapUnique(dummy_cv_.get()),
        cv_service_remote_.BindNewPipeAndPassReceiver(),
        cv_service_client_.BindNewPipeAndPassRemote(),
        /*cert_net_fetcher=*/nullptr);
  }

  void SetUp() override { ASSERT_TRUE(GetTestCert()); }

  void TestCompletions(int num_simultaneous, bool sync) {
    std::vector<RequestInfo> request_infos;
    base::TimeTicks time_zero = base::TimeTicks::Now();
    for (int i = 0; i < num_simultaneous; i++) {
      std::string hostname = "www" + base::NumberToString(i) + ".example.com";
      net::CertVerifier::RequestParams dummy_params(
          GetTestCert(), hostname, 0,
          /*ocsp_response=*/std::string(),
          /*sct_list=*/std::string());

      if (sync) {
        dummy_cv_->ShouldRespondSynchronouslyToParams(dummy_params);
      }

      // Perform a verification request using the Remote<CertVerifierService>,
      // which forwards to the CertVerifierServiceImpl.
      auto cv_service_req = std::make_unique<DummyCVServiceRequest>();
      auto cv_request_receiver =
          std::make_unique<mojo::Receiver<mojom::CertVerifierRequest>>(
              cv_service_req.get());

      request_infos.emplace_back(
          std::move(dummy_params),
          net::NetLogSource(net::NetLogSourceType::CERT_VERIFIER_JOB, 1234 + i,
                            time_zero + base::Seconds(i)),
          std::move(cv_service_req), std::move(cv_request_receiver));
    }

    for (RequestInfo& info : request_infos) {
      cv_service_remote_->Verify(
          info.request_params, info.net_log_source,
          info.cv_request_receiver->BindNewPipeAndPassRemote());
    }

    // Handle async Mojo request.
    task_environment_.RunUntilIdle();

    if (!sync) {  // For fun, complete the requests in reverse order.

      for (auto& info : base::Reversed(request_infos)) {
        ASSERT_FALSE(info.dummy_cv_request->is_completed);
        dummy_cv()->RespondToRequest(info.request_params);
      }

      // FlushForTesting() so the CertVerifierServiceImpl Mojo response is
      // handled.
      cv_service_remote().FlushForTesting();
    }

    for (RequestInfo& info : request_infos) {
      // Check the CertVerifierService finished the request.
      ASSERT_TRUE(info.dummy_cv_request->is_completed);
      ASSERT_EQ(info.dummy_cv_request->net_error, kExpectedNetError);
      ASSERT_EQ(info.dummy_cv_request->result.cert_status, kExpectedCertStatus);
      dummy_cv()->ExpectReceivedNetlogSource(info.request_params,
                                             info.net_log_source);
    }
  }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }
  mojo::Remote<mojom::CertVerifierService>& cv_service_remote() {
    return cv_service_remote_;
  }
  DummyCertVerifier* dummy_cv() { return dummy_cv_; }
  void set_dummy_cv(DummyCertVerifier* cv) { dummy_cv_ = cv; }

  unsigned cv_service_client_changed_count() const {
    return cv_service_client_changed_count_;
  }

  // mojom::CertVerifierServiceClient implementation:
  void OnCertVerifierChanged() override { cv_service_client_changed_count_++; }

 private:
  base::test::TaskEnvironment task_environment_;

  mojo::Remote<mojom::CertVerifierService> cv_service_remote_;
  mojo::Receiver<mojom::CertVerifierServiceClient> cv_service_client_;
  unsigned cv_service_client_changed_count_ = 0;
  raw_ptr<DummyCertVerifier> dummy_cv_;
};
}  // namespace

TEST_F(CertVerifierServiceTest, TestSingleCompletion) {
  TestCompletions(1, false);
}

TEST_F(CertVerifierServiceTest, TestMultipleSimultaneousCompletions) {
  TestCompletions(5, false);
}

TEST_F(CertVerifierServiceTest, TestSingleSyncCompletion) {
  TestCompletions(1, true);
}

TEST_F(CertVerifierServiceTest, TestMultipleSimultaneousSyncCompletions) {
  TestCompletions(5, true);
}

TEST_F(CertVerifierServiceTest, TestInvalidIntermediate) {
  auto leaf = GetTestCert();

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(
      net::x509_util::CreateCryptoBuffer(base::StringPiece("F")));

  scoped_refptr<net::X509Certificate> test_cert =
      net::X509Certificate::CreateFromBuffer(bssl::UpRef(leaf->cert_buffer()),
                                             std::move(intermediates));
  ASSERT_TRUE(test_cert);

  net::CertVerifier::RequestParams dummy_params(test_cert, "example.com", 0,
                                                /*ocsp_response=*/std::string(),
                                                /*sct_list=*/std::string());

  // Perform a verification request using the Remote<CertVerifierService>,
  // which forwards to the CertVerifierServiceImpl.
  DummyCVServiceRequest cv_service_req;
  mojo::Receiver<mojom::CertVerifierRequest> cv_request_receiver(
      &cv_service_req);

  cv_service_remote()->Verify(
      dummy_params,
      net::NetLogSource(net::NetLogSourceType::CERT_VERIFIER_JOB,
                        /*id=*/1234, base::TimeTicks::Now()),
      cv_request_receiver.BindNewPipeAndPassRemote());

  // Handle async Mojo request.
  cv_service_remote().FlushForTesting();

  ASSERT_FALSE(cv_service_req.is_completed);
  ASSERT_FALSE(cv_service_req.result.verified_cert);
  dummy_cv()->RespondToRequest(dummy_params);

  // FlushForTesting() so the CertVerifierServiceImpl Mojo response is
  // handled.
  cv_service_remote().FlushForTesting();

  // Request should have completed (should not be rejected at deserialization).
  ASSERT_TRUE(cv_service_req.is_completed);
  EXPECT_EQ(cv_service_req.net_error, kExpectedNetError);
  // Check that the invalid intermediate can be successfully round-tripped.
  ASSERT_TRUE(cv_service_req.result.verified_cert);
  EXPECT_TRUE(test_cert->EqualsIncludingChain(
      cv_service_req.result.verified_cert.get()));
}

TEST_F(CertVerifierServiceTest, TestRequestDisconnectionCancelsCVRequest) {
  net::CertVerifier::RequestParams dummy_params(GetTestCert(), "example.com", 0,
                                                /*ocsp_response=*/std::string(),
                                                /*sct_list=*/std::string());

  // Perform a verification request using the Remote<CertVerifierService>,
  // which forwards to the CertVerifierServiceImpl.
  DummyCVServiceRequest cv_service_req;
  mojo::Receiver<mojom::CertVerifierRequest> cv_request_receiver(
      &cv_service_req);

  cv_service_remote()->Verify(
      dummy_params,
      net::NetLogSource(net::NetLogSourceType::CERT_VERIFIER_JOB, 1234,
                        base::TimeTicks::Now()),
      cv_request_receiver.BindNewPipeAndPassRemote());

  // Handle async Mojo request.
  cv_service_remote().FlushForTesting();

  ASSERT_FALSE(dummy_cv()->WasRequestCancelled(dummy_params));
  // Disconnect our receiver.
  cv_request_receiver.reset();

  // Run mojo disconnection request.
  task_environment()->RunUntilIdle();

  ASSERT_TRUE(dummy_cv()->WasRequestCancelled(dummy_params));
}

TEST_F(CertVerifierServiceTest, TestCVServiceDisconnection) {
  net::CertVerifier::RequestParams dummy_params(GetTestCert(), "example.com", 0,
                                                /*ocsp_response=*/std::string(),
                                                /*sct_list=*/std::string());
  bool disconnected = false;

  // Perform a verification request using the Remote<CertVerifierService>,
  // which forwards to the CertVerifierServiceImpl.
  DummyCVServiceRequest cv_service_req;
  mojo::Receiver<mojom::CertVerifierRequest> cv_request_receiver(
      &cv_service_req);

  cv_service_remote()->Verify(
      dummy_params,
      net::NetLogSource(net::NetLogSourceType::CERT_VERIFIER_JOB, 1234,
                        base::TimeTicks::Now()),
      cv_request_receiver.BindNewPipeAndPassRemote());

  // Make sure we observe disconnection.
  cv_request_receiver.set_disconnect_handler(
      base::BindLambdaForTesting([&]() { disconnected = true; }));

  // Disconnect our receiver.
  set_dummy_cv(nullptr);
  cv_service_remote().reset();
  task_environment()->RunUntilIdle();

  ASSERT_FALSE(cv_service_req.is_completed);
  ASSERT_TRUE(disconnected);
}

// Check that calling SetConfig() on the Mojo interface results in a SetConfig
// call to the underlying net::CertVerifier.
TEST_F(CertVerifierServiceTest, StoresConfig) {
  ASSERT_FALSE(dummy_cv()->config()->disable_symantec_enforcement);

  net::CertVerifier::Config config;
  config.disable_symantec_enforcement = true;

  cv_service_remote()->SetConfig(config);
  cv_service_remote().FlushForTesting();

  ASSERT_TRUE(dummy_cv()->config()->disable_symantec_enforcement);
}

// CertVerifierService should register an Observer on the underlying
// CertVerifier and when that observer is notified, it should proxy the
// notifications to the CertVerifierServiceClient.
TEST_F(CertVerifierServiceTest, ObserverIsRegistered) {
  ASSERT_TRUE(dummy_cv()->GetObserver());

  EXPECT_EQ(cv_service_client_changed_count(), 0u);

  dummy_cv()->GetObserver()->OnCertVerifierChanged();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(cv_service_client_changed_count(), 1u);

  dummy_cv()->GetObserver()->OnCertVerifierChanged();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(cv_service_client_changed_count(), 2u);
}

}  // namespace cert_verifier
