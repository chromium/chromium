// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/cert_verifier/cert_verifier_service_factory.h"

#include <cstddef>
#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/mojom/cert_verifier_service.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace cert_verifier {
namespace {

struct DummyCVServiceRequest : public mojom::CertVerifierRequest {
  explicit DummyCVServiceRequest(base::RepeatingClosure on_finish)
      : on_finish_(std::move(on_finish)) {}
  void Complete(const net::CertVerifyResult& result_param,
                int32_t net_error_param) override {
    is_completed = true;
    result = result_param;
    net_error = net_error_param;
    std::move(on_finish_).Run();
  }

  base::RepeatingClosure on_finish_;
  bool is_completed = false;
  net::CertVerifyResult result;
  int net_error;
};
}  // namespace

TEST(CertVerifierServiceFactoryTest, GetNewCertVerifier) {
  base::test::TaskEnvironment task_environment;

  base::FilePath certs_dir = net::GetTestCertsDirectory();
  scoped_refptr<net::X509Certificate> test_cert(
      net::ImportCertFromFile(certs_dir, "ok_cert.pem"));
  ASSERT_NE(nullptr, test_cert.get());

  mojo::Remote<mojom::CertVerifierServiceFactory> cv_service_factory_remote;
  CertVerifierServiceFactoryImpl cv_service_factory_impl(
      cv_service_factory_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<mojom::CertVerifierService> cv_service_remote;
  network::mojom::CertVerifierCreationParamsPtr cv_creation_params =
      network::mojom::CertVerifierCreationParams::New();

  cv_service_factory_remote->GetNewCertVerifier(
      cv_service_remote.BindNewPipeAndPassReceiver(),
      std::move(cv_creation_params));

  base::RunLoop request_completed_run_loop;
  DummyCVServiceRequest dummy_cv_service_req(
      request_completed_run_loop.QuitClosure());
  mojo::Receiver<mojom::CertVerifierRequest> dummy_cv_service_req_receiver(
      &dummy_cv_service_req);

  cv_service_remote->Verify(
      net::CertVerifier::RequestParams(test_cert, "www.example.com", 0,
                                       /*ocsp_response=*/std::string(),
                                       /*sct_list=*/std::string()),
      dummy_cv_service_req_receiver.BindNewPipeAndPassRemote());

  request_completed_run_loop.Run();
  ASSERT_EQ(dummy_cv_service_req.net_error, net::ERR_CERT_AUTHORITY_INVALID);
  ASSERT_TRUE(dummy_cv_service_req.result.cert_status &
              net::CERT_STATUS_AUTHORITY_INVALID);
}

}  // namespace cert_verifier
