// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/cert_verifier/cert_verifier_service_factory.h"

#include <cstddef>
#include <memory>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/log/net_log_with_source.h"
#include "net/test/cert_builder.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/mojom/cert_verifier_service.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
#include "mojo/public/cpp/base/big_buffer.h"
#include "net/cert/internal/trust_store_chrome.h"
#include "net/cert/pki/parse_name.h"
#include "net/cert/root_store_proto_lite/root_store.pb.h"
#include "net/der/input.h"
#endif

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
      /*params=*/nullptr,
      cv_service_factory_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<mojom::CertVerifierService> cv_service_remote;
  mojom::CertVerifierCreationParamsPtr cv_creation_params =
      mojom::CertVerifierCreationParams::New();

  cv_service_factory_remote->GetNewCertVerifier(
      cv_service_remote.BindNewPipeAndPassReceiver(),
      std::move(cv_creation_params));

  base::RunLoop request_completed_run_loop;
  DummyCVServiceRequest dummy_cv_service_req(
      request_completed_run_loop.QuitClosure());
  mojo::Receiver<mojom::CertVerifierRequest> dummy_cv_service_req_receiver(
      &dummy_cv_service_req);

  auto net_log(net::NetLogWithSource::Make(
      net::NetLog::Get(), net::NetLogSourceType::CERT_VERIFIER_JOB));
  cv_service_remote->Verify(
      net::CertVerifier::RequestParams(test_cert, "www.example.com", 0,
                                       /*ocsp_response=*/std::string(),
                                       /*sct_list=*/std::string()),
      static_cast<uint32_t>(net_log.source().type), net_log.source().id,
      net_log.source().start_time,
      dummy_cv_service_req_receiver.BindNewPipeAndPassRemote());

  request_completed_run_loop.Run();
  ASSERT_EQ(dummy_cv_service_req.net_error, net::ERR_CERT_AUTHORITY_INVALID);
  ASSERT_TRUE(dummy_cv_service_req.result.cert_status &
              net::CERT_STATUS_AUTHORITY_INVALID);
}

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
// Test that a new Cert verifier will use an updated Chrome Root Store if
// one was already passed into CertVerifierServiceFactory.
TEST(CertVerifierServiceFactoryTest, GetNewCertVerifierWithUpdatedRootStore) {
  // Create leaf and root certs.
  base::test::TaskEnvironment task_environment;
  auto [leaf, root] = net::CertBuilder::CreateSimpleChain2();

  base::Time now = base::Time::Now();
  leaf->SetValidity(now - base::Days(1), now + base::Days(1));

  // Create updated Chrome Root Store with just the root cert from above.
  chrome_root_store::RootStore root_store_proto;
  root_store_proto.set_version_major(net::CompiledChromeRootStoreVersion() + 1);
  chrome_root_store::TrustAnchor* anchor = root_store_proto.add_trust_anchors();
  anchor->set_der(root->GetDER());
  std::string proto_serialized;
  root_store_proto.SerializeToString(&proto_serialized);
  cert_verifier::mojom::ChromeRootStorePtr root_store_ptr =
      cert_verifier::mojom::ChromeRootStore::New(
          base::as_bytes(base::make_span(proto_serialized)));

  // Configure with Chrome Root Store enabled.
  mojom::CertVerifierServiceParamsPtr service_params =
      mojom::CertVerifierServiceParams::New();
  service_params->use_chrome_root_store = true;

  mojo::Remote<mojom::CertVerifierServiceFactory> cv_service_factory_remote;
  CertVerifierServiceFactoryImpl cv_service_factory_impl(
      std::move(service_params),
      cv_service_factory_remote.BindNewPipeAndPassReceiver());

  // Feed factory the new Chrome Root Store.
  cv_service_factory_impl.UpdateChromeRootStore(std::move(root_store_ptr));

  mojo::Remote<mojom::CertVerifierService> cv_service_remote;
  mojom::CertVerifierCreationParamsPtr cv_creation_params =
      mojom::CertVerifierCreationParams::New();

  cv_service_factory_remote->GetNewCertVerifier(
      cv_service_remote.BindNewPipeAndPassReceiver(),
      std::move(cv_creation_params));

  base::RunLoop request_completed_run_loop;
  DummyCVServiceRequest dummy_cv_service_req(
      request_completed_run_loop.QuitClosure());
  mojo::Receiver<mojom::CertVerifierRequest> dummy_cv_service_req_receiver(
      &dummy_cv_service_req);

  auto net_log(net::NetLogWithSource::Make(
      net::NetLog::Get(), net::NetLogSourceType::CERT_VERIFIER_JOB));
  cv_service_remote->Verify(
      net::CertVerifier::RequestParams(leaf->GetX509Certificate(),
                                       "www.example.com", 0,
                                       /*ocsp_response=*/std::string(),
                                       /*sct_list=*/std::string()),
      static_cast<uint32_t>(net_log.source().type), net_log.source().id,
      net_log.source().start_time,
      dummy_cv_service_req_receiver.BindNewPipeAndPassRemote());

  request_completed_run_loop.Run();
  ASSERT_EQ(dummy_cv_service_req.net_error, net::OK);
}

// Test that an existing CertVerifierService will use an updated Chrome Root
// Store if one is provided to the CertVerifierServiceFactory
TEST(CertVerifierServiceFactoryTest, UpdateExistingCertVerifierWithRootStore) {
  // Create leaf and root certs.
  base::test::TaskEnvironment task_environment;
  auto [leaf, root] = net::CertBuilder::CreateSimpleChain2();

  base::Time now = base::Time::Now();
  leaf->SetValidity(now - base::Days(1), now + base::Days(1));

  // Configure with Chrome Root Store enabled.
  mojom::CertVerifierServiceParamsPtr service_params =
      mojom::CertVerifierServiceParams::New();
  service_params->use_chrome_root_store = true;

  mojo::Remote<mojom::CertVerifierServiceFactory> cv_service_factory_remote;
  CertVerifierServiceFactoryImpl cv_service_factory_impl(
      std::move(service_params),
      cv_service_factory_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<mojom::CertVerifierService> cv_service_remote;
  mojom::CertVerifierCreationParamsPtr cv_creation_params =
      mojom::CertVerifierCreationParams::New();

  cv_service_factory_remote->GetNewCertVerifier(
      cv_service_remote.BindNewPipeAndPassReceiver(),
      std::move(cv_creation_params));

  // Try request, it should fail because we haven't updated the Root Store yet.
  {
    base::RunLoop request_completed_run_loop;
    DummyCVServiceRequest dummy_cv_service_req(
        request_completed_run_loop.QuitClosure());
    mojo::Receiver<mojom::CertVerifierRequest> dummy_cv_service_req_receiver(
        &dummy_cv_service_req);

    auto net_log(net::NetLogWithSource::Make(
        net::NetLog::Get(), net::NetLogSourceType::CERT_VERIFIER_JOB));
    cv_service_remote->Verify(
        net::CertVerifier::RequestParams(leaf->GetX509Certificate(),
                                         "www.example.com", 0,
                                         /*ocsp_response=*/std::string(),
                                         /*sct_list=*/std::string()),
        static_cast<uint32_t>(net_log.source().type), net_log.source().id,
        net_log.source().start_time,
        dummy_cv_service_req_receiver.BindNewPipeAndPassRemote());

    request_completed_run_loop.Run();
    ASSERT_EQ(dummy_cv_service_req.net_error, net::ERR_CERT_AUTHORITY_INVALID);
    ASSERT_TRUE(dummy_cv_service_req.result.cert_status &
                net::CERT_STATUS_AUTHORITY_INVALID);
  }

  // Create updated Chrome Root Store with just the root cert from above.
  chrome_root_store::RootStore root_store_proto;
  root_store_proto.set_version_major(net::CompiledChromeRootStoreVersion() + 1);
  chrome_root_store::TrustAnchor* anchor = root_store_proto.add_trust_anchors();
  anchor->set_der(root->GetDER());
  std::string proto_serialized;
  root_store_proto.SerializeToString(&proto_serialized);
  cert_verifier::mojom::ChromeRootStorePtr root_store_ptr =
      cert_verifier::mojom::ChromeRootStore::New(
          base::as_bytes(base::make_span(proto_serialized)));

  // Feed factory the new Chrome Root Store.
  cv_service_factory_impl.UpdateChromeRootStore(std::move(root_store_ptr));

  // Try request, it should succeed.
  {
    base::RunLoop request_completed_run_loop;
    DummyCVServiceRequest dummy_cv_service_req(
        request_completed_run_loop.QuitClosure());
    mojo::Receiver<mojom::CertVerifierRequest> dummy_cv_service_req_receiver(
        &dummy_cv_service_req);
    auto net_log(net::NetLogWithSource::Make(
        net::NetLog::Get(), net::NetLogSourceType::CERT_VERIFIER_JOB));
    cv_service_remote->Verify(
        net::CertVerifier::RequestParams(leaf->GetX509Certificate(),
                                         "www.example.com", 0,
                                         /*ocsp_response=*/std::string(),
                                         /*sct_list=*/std::string()),
        static_cast<uint32_t>(net_log.source().type), net_log.source().id,
        net_log.source().start_time,
        dummy_cv_service_req_receiver.BindNewPipeAndPassRemote());

    request_completed_run_loop.Run();
    ASSERT_EQ(dummy_cv_service_req.net_error, net::OK);
  }
}

TEST(CertVerifierServiceFactoryTest, OldRootStoreUpdateIgnored) {
  // Create leaf and root certs.
  base::test::TaskEnvironment task_environment;
  auto [leaf, root] = net::CertBuilder::CreateSimpleChain2();

  base::Time now = base::Time::Now();
  leaf->SetValidity(now - base::Days(1), now + base::Days(1));

  // Create updated Chrome Root Store with just the root cert from above, but
  // set the version # so that the version is ignored.
  chrome_root_store::RootStore root_store_proto;
  root_store_proto.set_version_major(net::CompiledChromeRootStoreVersion());
  chrome_root_store::TrustAnchor* anchor = root_store_proto.add_trust_anchors();
  anchor->set_der(root->GetDER());
  std::string proto_serialized;
  root_store_proto.SerializeToString(&proto_serialized);
  cert_verifier::mojom::ChromeRootStorePtr root_store_ptr =
      cert_verifier::mojom::ChromeRootStore::New(
          base::as_bytes(base::make_span(proto_serialized)));

  // Configure with Chrome Root Store enabled.
  mojom::CertVerifierServiceParamsPtr service_params =
      mojom::CertVerifierServiceParams::New();
  service_params->use_chrome_root_store = true;

  mojo::Remote<mojom::CertVerifierServiceFactory> cv_service_factory_remote;
  CertVerifierServiceFactoryImpl cv_service_factory_impl(
      std::move(service_params),
      cv_service_factory_remote.BindNewPipeAndPassReceiver());

  // Feed factory the new Chrome Root Store.
  cv_service_factory_impl.UpdateChromeRootStore(std::move(root_store_ptr));

  mojo::Remote<mojom::CertVerifierService> cv_service_remote;
  mojom::CertVerifierCreationParamsPtr cv_creation_params =
      mojom::CertVerifierCreationParams::New();

  cv_service_factory_remote->GetNewCertVerifier(
      cv_service_remote.BindNewPipeAndPassReceiver(),
      std::move(cv_creation_params));

  base::RunLoop request_completed_run_loop;
  DummyCVServiceRequest dummy_cv_service_req(
      request_completed_run_loop.QuitClosure());
  mojo::Receiver<mojom::CertVerifierRequest> dummy_cv_service_req_receiver(
      &dummy_cv_service_req);

  auto net_log(net::NetLogWithSource::Make(
      net::NetLog::Get(), net::NetLogSourceType::CERT_VERIFIER_JOB));
  cv_service_remote->Verify(
      net::CertVerifier::RequestParams(leaf->GetX509Certificate(),
                                       "www.example.com", 0,
                                       /*ocsp_response=*/std::string(),
                                       /*sct_list=*/std::string()),
      static_cast<uint32_t>(net_log.source().type), net_log.source().id,
      net_log.source().start_time,
      dummy_cv_service_req_receiver.BindNewPipeAndPassRemote());

  request_completed_run_loop.Run();
  // Request should result in error because root store update was ignored.
  ASSERT_EQ(dummy_cv_service_req.net_error, net::ERR_CERT_AUTHORITY_INVALID);
  ASSERT_TRUE(dummy_cv_service_req.result.cert_status &
              net::CERT_STATUS_AUTHORITY_INVALID);
}

TEST(CertVerifierServiceFactoryTest, BadRootStoreUpdateIgnored) {
  // Create leaf and root certs.
  base::test::TaskEnvironment task_environment;
  auto [leaf, root] = net::CertBuilder::CreateSimpleChain2();

  base::Time now = base::Time::Now();
  leaf->SetValidity(now - base::Days(1), now + base::Days(1));

  // Create updated Chrome Root Store with just the root cert from above.
  chrome_root_store::RootStore root_store_proto;
  root_store_proto.set_version_major(net::CompiledChromeRootStoreVersion() + 1);
  chrome_root_store::TrustAnchor* anchor = root_store_proto.add_trust_anchors();
  anchor->set_der(root->GetDER());
  std::string proto_serialized;
  root_store_proto.SerializeToString(&proto_serialized);
  cert_verifier::mojom::ChromeRootStorePtr root_store_ptr =
      cert_verifier::mojom::ChromeRootStore::New(
          base::as_bytes(base::make_span(proto_serialized)));

  // Configure with Chrome Root Store enabled.
  mojom::CertVerifierServiceParamsPtr service_params =
      mojom::CertVerifierServiceParams::New();
  service_params->use_chrome_root_store = true;

  mojo::Remote<mojom::CertVerifierServiceFactory> cv_service_factory_remote;
  CertVerifierServiceFactoryImpl cv_service_factory_impl(
      std::move(service_params),
      cv_service_factory_remote.BindNewPipeAndPassReceiver());

  // Feed factory the new Chrome Root Store.
  cv_service_factory_impl.UpdateChromeRootStore(std::move(root_store_ptr));

  mojo::Remote<mojom::CertVerifierService> cv_service_remote;
  mojom::CertVerifierCreationParamsPtr cv_creation_params =
      mojom::CertVerifierCreationParams::New();

  cv_service_factory_remote->GetNewCertVerifier(
      cv_service_remote.BindNewPipeAndPassReceiver(),
      std::move(cv_creation_params));

  // Initial request should succeed.
  {
    base::RunLoop request_completed_run_loop;
    DummyCVServiceRequest dummy_cv_service_req(
        request_completed_run_loop.QuitClosure());
    mojo::Receiver<mojom::CertVerifierRequest> dummy_cv_service_req_receiver(
        &dummy_cv_service_req);

    auto net_log(net::NetLogWithSource::Make(
        net::NetLog::Get(), net::NetLogSourceType::CERT_VERIFIER_JOB));
    cv_service_remote->Verify(
        net::CertVerifier::RequestParams(leaf->GetX509Certificate(),
                                         "www.example.com", 0,
                                         /*ocsp_response=*/std::string(),
                                         /*sct_list=*/std::string()),
        static_cast<uint32_t>(net_log.source().type), net_log.source().id,
        net_log.source().start_time,
        dummy_cv_service_req_receiver.BindNewPipeAndPassRemote());

    request_completed_run_loop.Run();
    // Request should be OK.
    ASSERT_EQ(dummy_cv_service_req.net_error, net::OK);
  }

  // Create updated Chrome Root Store with an invalid cert; update should be
  // ignored.
  chrome_root_store::RootStore invalid_root_store_proto;
  invalid_root_store_proto.set_version_major(
      net::CompiledChromeRootStoreVersion() + 2);
  chrome_root_store::TrustAnchor* invalid_anchor =
      invalid_root_store_proto.add_trust_anchors();
  invalid_anchor->set_der("gibberishcert");
  invalid_root_store_proto.SerializeToString(&proto_serialized);
  cert_verifier::mojom::ChromeRootStorePtr invalid_root_store_ptr =
      cert_verifier::mojom::ChromeRootStore::New(
          base::as_bytes(base::make_span(proto_serialized)));

  // Feed factory the new Chrome Root Store.
  cv_service_factory_impl.UpdateChromeRootStore(
      std::move(invalid_root_store_ptr));

  {
    base::RunLoop request_completed_run_loop;
    DummyCVServiceRequest dummy_cv_service_req(
        request_completed_run_loop.QuitClosure());
    mojo::Receiver<mojom::CertVerifierRequest> dummy_cv_service_req_receiver(
        &dummy_cv_service_req);

    auto net_log(net::NetLogWithSource::Make(
        net::NetLog::Get(), net::NetLogSourceType::CERT_VERIFIER_JOB));
    cv_service_remote->Verify(
        net::CertVerifier::RequestParams(leaf->GetX509Certificate(),
                                         "www.example.com", 0,
                                         /*ocsp_response=*/std::string(),
                                         /*sct_list=*/std::string()),
        static_cast<uint32_t>(net_log.source().type), net_log.source().id,
        net_log.source().start_time,
        dummy_cv_service_req_receiver.BindNewPipeAndPassRemote());

    request_completed_run_loop.Run();
    // Request should be OK because root store update was ignored.
    ASSERT_EQ(dummy_cv_service_req.net_error, net::OK);
  }

  // Clear all certs from the proto
  root_store_proto.clear_trust_anchors();
  root_store_proto.SerializeToString(&proto_serialized);
  cert_verifier::mojom::ChromeRootStorePtr empty_root_store_ptr =
      cert_verifier::mojom::ChromeRootStore::New(
          base::as_bytes(base::make_span(proto_serialized)));

  // Feed factory the new Chrome Root Store.
  cv_service_factory_impl.UpdateChromeRootStore(
      std::move(empty_root_store_ptr));

  {
    base::RunLoop request_completed_run_loop;
    DummyCVServiceRequest dummy_cv_service_req(
        request_completed_run_loop.QuitClosure());
    mojo::Receiver<mojom::CertVerifierRequest> dummy_cv_service_req_receiver(
        &dummy_cv_service_req);

    auto net_log(net::NetLogWithSource::Make(
        net::NetLog::Get(), net::NetLogSourceType::CERT_VERIFIER_JOB));
    cv_service_remote->Verify(
        net::CertVerifier::RequestParams(leaf->GetX509Certificate(),
                                         "www.example.com", 0,
                                         /*ocsp_response=*/std::string(),
                                         /*sct_list=*/std::string()),
        static_cast<uint32_t>(net_log.source().type), net_log.source().id,
        net_log.source().start_time,
        dummy_cv_service_req_receiver.BindNewPipeAndPassRemote());

    request_completed_run_loop.Run();
    // Request should be OK because root store update was ignored.
    ASSERT_EQ(dummy_cv_service_req.net_error, net::OK);
  }
}

void GetRootStoreInfo(cert_verifier::mojom::ChromeRootStoreInfoPtr* return_ptr,
                      base::RepeatingClosure quit_closure,
                      cert_verifier::mojom::ChromeRootStoreInfoPtr info) {
  *return_ptr = std::move(info);
  quit_closure.Run();
}

TEST(CertVerifierServiceFactoryTest, RootStoreInfoWithUpdatedRootStore) {
  // Create leaf and root certs.
  base::test::TaskEnvironment task_environment;
  auto [leaf, root] = net::CertBuilder::CreateSimpleChain2();

  base::Time now = base::Time::Now();
  leaf->SetValidity(now - base::Days(1), now + base::Days(1));

  // Create updated Chrome Root Store with just the root cert from above.
  chrome_root_store::RootStore root_store_proto;
  root_store_proto.set_version_major(net::CompiledChromeRootStoreVersion() + 1);
  chrome_root_store::TrustAnchor* anchor = root_store_proto.add_trust_anchors();
  anchor->set_der(root->GetDER());
  std::string proto_serialized;
  root_store_proto.SerializeToString(&proto_serialized);
  cert_verifier::mojom::ChromeRootStorePtr root_store_ptr =
      cert_verifier::mojom::ChromeRootStore::New(
          base::as_bytes(base::make_span(proto_serialized)));

  mojo::Remote<mojom::CertVerifierServiceFactory> cv_service_factory_remote;
  CertVerifierServiceFactoryImpl cv_service_factory_impl(
      /*params=*/nullptr,
      cv_service_factory_remote.BindNewPipeAndPassReceiver());

  // Feed factory the new Chrome Root Store.
  cv_service_factory_impl.UpdateChromeRootStore(std::move(root_store_ptr));

  cert_verifier::mojom::ChromeRootStoreInfoPtr info_ptr;
  base::RunLoop request_completed_run_loop;
  cv_service_factory_remote->GetChromeRootStoreInfo(base::BindOnce(
      &GetRootStoreInfo, &info_ptr, request_completed_run_loop.QuitClosure()));
  request_completed_run_loop.Run();
  ASSERT_TRUE(info_ptr);
  EXPECT_EQ(info_ptr->version, root_store_proto.version_major());
  ASSERT_EQ(info_ptr->root_cert_info.size(), static_cast<std::size_t>(1));

  net::der::Input subject_tlv(&root->GetSubject());
  net::RDNSequence subject_rdn;
  ASSERT_TRUE(net::ParseName(subject_tlv, &subject_rdn));
  std::string subject_string;
  ASSERT_TRUE(net::ConvertToRFC2253(subject_rdn, &subject_string));
  EXPECT_EQ(info_ptr->root_cert_info[0]->name, subject_string);

  net::SHA256HashValue root_hash =
      net::X509Certificate::CalculateFingerprint256(root->GetCertBuffer());
  EXPECT_EQ(info_ptr->root_cert_info[0]->sha256hash_hex,
            base::HexEncode(root_hash.data, std::size(root_hash.data)));
}

TEST(CertVerifierServiceFactoryTest, RootStoreInfoWithCompiledRootStore) {
  base::test::TaskEnvironment task_environment;
  net::ParsedCertificateList anchors = net::CompiledChromeRootStoreAnchors();

  mojo::Remote<mojom::CertVerifierServiceFactory> cv_service_factory_remote;
  CertVerifierServiceFactoryImpl cv_service_factory_impl(
      /*params=*/nullptr,
      cv_service_factory_remote.BindNewPipeAndPassReceiver());
  cert_verifier::mojom::ChromeRootStoreInfoPtr info_ptr;
  base::RunLoop request_completed_run_loop;
  cv_service_factory_remote->GetChromeRootStoreInfo(base::BindOnce(
      &GetRootStoreInfo, &info_ptr, request_completed_run_loop.QuitClosure()));
  request_completed_run_loop.Run();

  ASSERT_TRUE(info_ptr);
  EXPECT_EQ(info_ptr->version, net::CompiledChromeRootStoreVersion());
  EXPECT_EQ(info_ptr->root_cert_info.size(), anchors.size());
}

#endif

}  // namespace cert_verifier
