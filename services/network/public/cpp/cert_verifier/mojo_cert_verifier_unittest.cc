// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cert_verifier/mojo_cert_verifier.h"

#include <map>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/log/net_log_with_source.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/mojom/cert_verifier_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace cert_verifier {
namespace {
const int kExpectedNetError = net::ERR_CERT_INVALID;
const unsigned int kExpectedCertStatus = net::CERT_STATUS_INVALID;

scoped_refptr<net::X509Certificate> GetTestCert() {
  return net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
}

net::CertVerifier::RequestParams GetDummyParams() {
  return net::CertVerifier::RequestParams(GetTestCert(), "example.com", 0,
                                          /*ocsp_response=*/std::string(),
                                          /*sct_list=*/std::string());
}

mojo::PendingRemote<network::mojom::URLLoaderFactory>
CreateUnconnectedURLLoaderFactory() {
  mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory;
  // Bind the factory, but don't bother connecting it.
  ignore_result(url_loader_factory.InitWithNewPipeAndPassReceiver());
  return url_loader_factory;
}

class MojoCertVerifierTest : public PlatformTest {
 public:
  MojoCertVerifierTest()
      : dummy_cv_service_(this),
        cv_service_receiver_(&dummy_cv_service_),
        mojo_cert_verifier_(
            cv_service_receiver_.BindNewPipeAndPassRemote(),
            CreateUnconnectedURLLoaderFactory(),
            base::BindRepeating(&MojoCertVerifierTest::ReconnectCb,
                                base::Unretained(this))) {
    // Any Mojo requests in the MojoCertVerifier constructor should run here.
    mojo_cert_verifier_.FlushForTesting();
  }

  class DummyCVService final : public mojom::CertVerifierService {
   public:
    explicit DummyCVService(MojoCertVerifierTest* test) : test_(test) {}
    void Verify(const net::CertVerifier::RequestParams& params,
                mojo::PendingRemote<mojom::CertVerifierRequest>
                    cert_verifier_request) override {
      ASSERT_FALSE(test_->received_requests_.count(params));
      test_->received_requests_[params].Bind(std::move(cert_verifier_request));
    }

    void SetConfig(const net::CertVerifier::Config& config) override {
      config_ = config;
    }

    void EnableNetworkAccess(
        mojo::PendingRemote<network::mojom::URLLoaderFactory>
            url_loader_factory,
        mojo::PendingRemote<mojom::URLLoaderFactoryConnector> reconnector)
        override {
      reconnector_.Bind(std::move(reconnector));
    }

    const net::CertVerifier::Config* config() const { return &config_; }
    mojom::URLLoaderFactoryConnector* reconnector() {
      return reconnector_.get();
    }

   private:
    MojoCertVerifierTest* test_;

    net::CertVerifier::Config config_;
    mojo::Remote<mojom::URLLoaderFactoryConnector> reconnector_;
  };

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

  MojoCertVerifier* mojo_cert_verifier() { return &mojo_cert_verifier_; }

  net::NetLogWithSource* net_log_with_source() {
    return &empty_net_log_with_source_;
  }

  DummyCVService* dummy_cv_service() { return &dummy_cv_service_; }

  void RespondToRequest(const net::CertVerifier::RequestParams& params) {
    ASSERT_TRUE(received_requests_.count(params));
    net::CertVerifyResult result;
    result.cert_status = kExpectedCertStatus;
    received_requests_[params]->Complete(result, kExpectedNetError);
  }

  bool DidRequestDisconnect(const net::CertVerifier::RequestParams& params) {
    if (!received_requests_.count(params)) {
      ADD_FAILURE();
      return false;
    }
    return !received_requests_[params].is_connected();
  }

  void DisconnectRequest(const net::CertVerifier::RequestParams& params) {
    ASSERT_TRUE(received_requests_.count(params));
    received_requests_[params].reset();
  }

  void SimulateCVServiceDisconnect() { cv_service_receiver_.reset(); }

  void ReconnectCb(mojo::PendingReceiver<network::mojom::URLLoaderFactory>
                       mojo_cert_verifier) {
    if (reconnect_cb_)
      reconnect_cb_.Run(std::move(mojo_cert_verifier));
  }

  void SetReconnectCb(MojoCertVerifier::ReconnectURLLoaderFactory cb) {
    reconnect_cb_ = std::move(cb);
  }

 private:
  std::map<net::CertVerifier::RequestParams,
           mojo::Remote<mojom::CertVerifierRequest>>
      received_requests_;

  base::test::TaskEnvironment task_environment_;

  MojoCertVerifierTest::DummyCVService dummy_cv_service_;
  mojo::Receiver<mojom::CertVerifierService> cv_service_receiver_;

  MojoCertVerifier mojo_cert_verifier_;

  MojoCertVerifier::ReconnectURLLoaderFactory reconnect_cb_;

  net::NetLogWithSource empty_net_log_with_source_;
};
}  // namespace

TEST_F(MojoCertVerifierTest, BasicVerify) {
  net::CertVerifier::RequestParams dummy_params = GetDummyParams();

  auto cert_verify_result = std::make_unique<net::CertVerifyResult>();
  net::TestCompletionCallback test_cb;
  std::unique_ptr<net::CertVerifier::Request> request;

  int net_error = mojo_cert_verifier()->Verify(
      dummy_params, cert_verify_result.get(), test_cb.callback(), &request,
      *net_log_with_source());
  ASSERT_EQ(net_error, net::ERR_IO_PENDING);

  // Handle Mojo request
  task_environment()->RunUntilIdle();

  RespondToRequest(dummy_params);

  net_error = test_cb.WaitForResult();
  EXPECT_EQ(net_error, kExpectedNetError);
  EXPECT_EQ(cert_verify_result->cert_status, kExpectedCertStatus);
}

// Same as BasicVerify, except we disconnect the Remote<CertVerifierRequest>
// right after responding, to test that we don't accidentally handle both the
// normal response and the disconnection.
TEST_F(MojoCertVerifierTest, BasicVerifyAndRequestDisconnection) {
  net::CertVerifier::RequestParams dummy_params = GetDummyParams();

  auto cert_verify_result = std::make_unique<net::CertVerifyResult>();
  net::TestCompletionCallback test_cb;
  std::unique_ptr<net::CertVerifier::Request> request;

  int net_error = mojo_cert_verifier()->Verify(
      dummy_params, cert_verify_result.get(), test_cb.callback(), &request,
      *net_log_with_source());
  ASSERT_EQ(net_error, net::ERR_IO_PENDING);

  // Handle Mojo request
  task_environment()->RunUntilIdle();

  RespondToRequest(dummy_params);
  // Disconnect the request right after responding to it
  DisconnectRequest(dummy_params);

  net_error = test_cb.WaitForResult();
  EXPECT_EQ(net_error, kExpectedNetError);
  EXPECT_EQ(cert_verify_result->cert_status, kExpectedCertStatus);
}

TEST_F(MojoCertVerifierTest, CVRequestDeletionCausesDisconnect) {
  net::CertVerifier::RequestParams dummy_params = GetDummyParams();

  auto cert_verify_result = std::make_unique<net::CertVerifyResult>();
  auto verify_cb = base::BindRepeating([](int net_error) {
    // Should never get called as the request will be cancelled on the
    // MojoCertVerifier side.
    GTEST_FAIL();
  });
  std::unique_ptr<net::CertVerifier::Request> request;

  int net_error = mojo_cert_verifier()->Verify(
      dummy_params, cert_verify_result.get(), std::move(verify_cb), &request,
      *net_log_with_source());
  ASSERT_EQ(net_error, net::ERR_IO_PENDING);

  // Reset our cert verifier request to cause a disconnection.
  request.reset();

  task_environment()->RunUntilIdle();

  EXPECT_TRUE(DidRequestDisconnect(dummy_params));
}

TEST_F(MojoCertVerifierTest, HandlesMojomCVRequestDisconnectionAsCancellation) {
  net::CertVerifier::RequestParams dummy_params = GetDummyParams();

  auto cert_verify_result = std::make_unique<net::CertVerifyResult>();
  net::TestCompletionCallback test_cb;
  std::unique_ptr<net::CertVerifier::Request> request;

  int net_error = mojo_cert_verifier()->Verify(
      dummy_params, cert_verify_result.get(), test_cb.callback(), &request,
      *net_log_with_source());
  ASSERT_EQ(net_error, net::ERR_IO_PENDING);

  // Handle Mojo request
  task_environment()->RunUntilIdle();

  DisconnectRequest(dummy_params);

  net_error = test_cb.WaitForResult();
  // Should abort if cancelled on the CertVerifierService side.
  EXPECT_EQ(net_error, net::ERR_ABORTED);
  EXPECT_EQ(cert_verify_result->cert_status, kExpectedCertStatus);
}

TEST_F(MojoCertVerifierTest, IgnoresCVServiceDisconnection) {
  net::CertVerifier::RequestParams dummy_params = GetDummyParams();

  auto cert_verify_result = std::make_unique<net::CertVerifyResult>();
  net::TestCompletionCallback test_cb;
  std::unique_ptr<net::CertVerifier::Request> request;

  int net_error = mojo_cert_verifier()->Verify(
      dummy_params, cert_verify_result.get(), test_cb.callback(), &request,
      *net_log_with_source());
  ASSERT_EQ(net_error, net::ERR_IO_PENDING);

  // Handle Mojo request
  task_environment()->RunUntilIdle();

  SimulateCVServiceDisconnect();
  RespondToRequest(dummy_params);

  net_error = test_cb.WaitForResult();
  EXPECT_EQ(net_error, kExpectedNetError);
  EXPECT_EQ(cert_verify_result->cert_status, kExpectedCertStatus);
}

TEST_F(MojoCertVerifierTest, SendsConfig) {
  ASSERT_FALSE(dummy_cv_service()->config()->disable_symantec_enforcement);

  net::CertVerifier::Config config;
  config.disable_symantec_enforcement = true;

  mojo_cert_verifier()->SetConfig(config);
  task_environment()->RunUntilIdle();

  ASSERT_TRUE(dummy_cv_service()->config()->disable_symantec_enforcement);
}

TEST_F(MojoCertVerifierTest, ReconnectorCallsCb) {
  base::RunLoop run_loop;
  SetReconnectCb(base::BindLambdaForTesting(
      [&](mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
        run_loop.Quit();
      }));

  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      dummy_url_loader_factory_remote;
  // Simulate a remote CertVerifierService trying to reconnect after
  // disconnection. This should call the callback given to the MojoCertVerifier
  // on construction.
  dummy_cv_service()->reconnector()->CreateURLLoaderFactory(
      dummy_url_loader_factory_remote.InitWithNewPipeAndPassReceiver());

  run_loop.Run();
}

}  // namespace cert_verifier
