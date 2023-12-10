// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/cert_verifier/cert_verifier_service_factory.h"

#include <cstddef>
#include <memory>
#include <vector>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
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
#include "net/cert/cert_verify_result.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/log/net_log_with_source.h"
#include "net/net_buildflags.h"
#include "net/test/cert_builder.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
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
#include "net/cert/root_store_proto_lite/root_store.pb.h"
#include "third_party/boringssl/src/pki/input.h"
#include "third_party/boringssl/src/pki/parse_name.h"
#endif

using net::test::IsError;
using net::test::IsOk;

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

class DummyCVServiceClient : public mojom::CertVerifierServiceClient {
 public:
  DummyCVServiceClient() : client_(this) {}

  // mojom::CertVerifierServiceClient implementation:
  void OnCertVerifierChanged() override {
    changed_count_++;
    run_loop_->Quit();
  }

  void WaitForCertVerifierChange(unsigned expected) {
    if (changed_count_ < expected) {
      run_loop_->Run();
    }
    run_loop_ = std::make_unique<base::RunLoop>();
    ASSERT_EQ(changed_count_, expected);
  }

  unsigned changed_count_ = 0;
  std::unique_ptr<base::RunLoop> run_loop_ = std::make_unique<base::RunLoop>();
  mojo::Receiver<mojom::CertVerifierServiceClient> client_;
};

std::tuple<int, net::CertVerifyResult> Verify(
    const mojo::Remote<mojom::CertVerifierService>& cv_service_remote,
    scoped_refptr<net::X509Certificate> cert,
    const std::string& hostname) {
  base::RunLoop request_completed_run_loop;
  DummyCVServiceRequest dummy_cv_service_req(
      request_completed_run_loop.QuitClosure());
  mojo::Receiver<mojom::CertVerifierRequest> dummy_cv_service_req_receiver(
      &dummy_cv_service_req);
  auto net_log(net::NetLogWithSource::Make(
      net::NetLog::Get(), net::NetLogSourceType::CERT_VERIFIER_JOB));
  cv_service_remote->Verify(
      net::CertVerifier::RequestParams(std::move(cert), hostname,
                                       /*flags=*/0,
                                       /*ocsp_response=*/std::string(),
                                       /*sct_list=*/std::string()),
      net_log.source(),
      dummy_cv_service_req_receiver.BindNewPipeAndPassRemote());

  request_completed_run_loop.Run();
  return {dummy_cv_service_req.net_error, dummy_cv_service_req.result};
}

void UpdateCRLSetWithTestFile(
    CertVerifierServiceFactoryImpl* cv_service_factory_impl,
    base::StringPiece crlset_file_name) {
  std::string crl_set_bytes;
  EXPECT_TRUE(base::ReadFileToString(
      net::GetTestCertsDirectory().AppendASCII(crlset_file_name),
      &crl_set_bytes));

  base::RunLoop update_run_loop;
  cv_service_factory_impl->UpdateCRLSet(
      base::as_bytes(base::make_span(crl_set_bytes)),
      update_run_loop.QuitClosure());
  update_run_loop.Run();
}

void EnableChromeRootStoreIfOptional(CertVerifierServiceFactoryImpl* factory) {
#if BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)
  // Configure with Chrome Root Store enabled.
  {
    base::RunLoop run_loop;
    factory->SetUseChromeRootStore(true, run_loop.QuitClosure());
    run_loop.Run();
  }
#endif
}

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
  mojo::PendingReceiver<mojom::CertVerifierServiceClient> cv_service_client;
  mojom::CertVerifierCreationParamsPtr cv_creation_params =
      mojom::CertVerifierCreationParams::New();

  cv_service_factory_remote->GetNewCertVerifier(
      cv_service_remote.BindNewPipeAndPassReceiver(),
      /*updater=*/mojo::NullReceiver(),
      cv_service_client.InitWithNewPipeAndPassRemote(),
      std::move(cv_creation_params));

  auto [net_error, result] =
      Verify(cv_service_remote, test_cert, "www.example.com");
  ASSERT_EQ(net_error, net::ERR_CERT_AUTHORITY_INVALID);
  ASSERT_TRUE(result.cert_status & net::CERT_STATUS_AUTHORITY_INVALID);
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

  mojo::Remote<mojom::CertVerifierServiceFactory> cv_service_factory_remote;
  CertVerifierServiceFactoryImpl cv_service_factory_impl(
      cv_service_factory_remote.BindNewPipeAndPassReceiver());

  EnableChromeRootStoreIfOptional(&cv_service_factory_impl);

  // Feed factory the new Chrome Root Store.
  {
    base::RunLoop update_run_loop;
    cv_service_factory_impl.UpdateChromeRootStore(
        std::move(root_store_ptr), update_run_loop.QuitClosure());
    update_run_loop.Run();
  }

  mojo::Remote<mojom::CertVerifierService> cv_service_remote;
  DummyCVServiceClient cv_service_client;
  mojom::CertVerifierCreationParamsPtr cv_creation_params =
      mojom::CertVerifierCreationParams::New();

  cv_service_factory_remote->GetNewCertVerifier(
      cv_service_remote.BindNewPipeAndPassReceiver(),
      /*updater=*/mojo::NullReceiver(),
      cv_service_client.client_.BindNewPipeAndPassRemote(),
      std::move(cv_creation_params));

  auto [net_error, result] =
      Verify(cv_service_remote, leaf->GetX509Certificate(), "www.example.com");
  ASSERT_EQ(net_error, net::OK);
  // Update happened before the CertVerifier was created, no change observers
  // should have been notified.
  EXPECT_EQ(cv_service_client.changed_count_, 0u);
}

// Test that an existing CertVerifierService will use an updated Chrome Root
// Store if one is provided to the CertVerifierServiceFactory
TEST(CertVerifierServiceFactoryTest, UpdateExistingCertVerifierWithRootStore) {
  // Create leaf and root certs.
  base::test::TaskEnvironment task_environment;
  auto [leaf, root] = net::CertBuilder::CreateSimpleChain2();

  base::Time now = base::Time::Now();
  leaf->SetValidity(now - base::Days(1), now + base::Days(1));

  mojo::Remote<mojom::CertVerifierServiceFactory> cv_service_factory_remote;
  CertVerifierServiceFactoryImpl cv_service_factory_impl(
      cv_service_factory_remote.BindNewPipeAndPassReceiver());

  EnableChromeRootStoreIfOptional(&cv_service_factory_impl);

  mojo::Remote<mojom::CertVerifierService> cv_service_remote;
  DummyCVServiceClient cv_service_client;
  mojom::CertVerifierCreationParamsPtr cv_creation_params =
      mojom::CertVerifierCreationParams::New();

  cv_service_factory_remote->GetNewCertVerifier(
      cv_service_remote.BindNewPipeAndPassReceiver(),
      /*updater=*/mojo::NullReceiver(),
      cv_service_client.client_.BindNewPipeAndPassRemote(),
      std::move(cv_creation_params));

  // Try request, it should fail because we haven't updated the Root Store yet.
  {
    auto [net_error, result] = Verify(
        cv_service_remote, leaf->GetX509Certificate(), "www.example.com");
    ASSERT_EQ(net_error, net::ERR_CERT_AUTHORITY_INVALID);
    ASSERT_TRUE(result.cert_status & net::CERT_STATUS_AUTHORITY_INVALID);
  }
  // No updates should have happened yet.
  EXPECT_EQ(cv_service_client.changed_count_, 0u);

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
  {
    base::RunLoop update_run_loop;
    cv_service_factory_impl.UpdateChromeRootStore(
        std::move(root_store_ptr), update_run_loop.QuitClosure());
    update_run_loop.Run();
  }

  // Try request, it should succeed.
  {
    auto [net_error, result] = Verify(
        cv_service_remote, leaf->GetX509Certificate(), "www.example.com");
    ASSERT_EQ(net_error, net::OK);
  }

  // Update should have been notified.
  EXPECT_EQ(cv_service_client.changed_count_, 1u);
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

  mojo::Remote<mojom::CertVerifierServiceFactory> cv_service_factory_remote;
  CertVerifierServiceFactoryImpl cv_service_factory_impl(
      cv_service_factory_remote.BindNewPipeAndPassReceiver());

  EnableChromeRootStoreIfOptional(&cv_service_factory_impl);

  // Feed factory the new Chrome Root Store.
  {
    base::RunLoop update_run_loop;
    cv_service_factory_impl.UpdateChromeRootStore(
        std::move(root_store_ptr), update_run_loop.QuitClosure());
    update_run_loop.Run();
  }

  mojo::Remote<mojom::CertVerifierService> cv_service_remote;
  DummyCVServiceClient cv_service_client;
  mojom::CertVerifierCreationParamsPtr cv_creation_params =
      mojom::CertVerifierCreationParams::New();

  cv_service_factory_remote->GetNewCertVerifier(
      cv_service_remote.BindNewPipeAndPassReceiver(),
      /*updater=*/mojo::NullReceiver(),
      cv_service_client.client_.BindNewPipeAndPassRemote(),
      std::move(cv_creation_params));

  auto [net_error, result] =
      Verify(cv_service_remote, leaf->GetX509Certificate(), "www.example.com");
  // Request should result in error because root store update was ignored.
  ASSERT_EQ(net_error, net::ERR_CERT_AUTHORITY_INVALID);
  ASSERT_TRUE(result.cert_status & net::CERT_STATUS_AUTHORITY_INVALID);
  // Update was ignored, so no change observers should have been notified.
  EXPECT_EQ(cv_service_client.changed_count_, 0u);
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

  mojo::Remote<mojom::CertVerifierServiceFactory> cv_service_factory_remote;
  CertVerifierServiceFactoryImpl cv_service_factory_impl(
      cv_service_factory_remote.BindNewPipeAndPassReceiver());

  EnableChromeRootStoreIfOptional(&cv_service_factory_impl);

  // Feed factory the new Chrome Root Store.
  {
    base::RunLoop update_run_loop;
    cv_service_factory_impl.UpdateChromeRootStore(
        std::move(root_store_ptr), update_run_loop.QuitClosure());
    update_run_loop.Run();
  }

  mojo::Remote<mojom::CertVerifierService> cv_service_remote;
  DummyCVServiceClient cv_service_client;
  mojom::CertVerifierCreationParamsPtr cv_creation_params =
      mojom::CertVerifierCreationParams::New();

  cv_service_factory_remote->GetNewCertVerifier(
      cv_service_remote.BindNewPipeAndPassReceiver(),
      /*updater=*/mojo::NullReceiver(),
      cv_service_client.client_.BindNewPipeAndPassRemote(),
      std::move(cv_creation_params));

  // Initial request should succeed.
  {
    auto [net_error, result] = Verify(
        cv_service_remote, leaf->GetX509Certificate(), "www.example.com");
    // Request should be OK.
    ASSERT_EQ(net_error, net::OK);
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
  {
    base::RunLoop update_run_loop;
    cv_service_factory_impl.UpdateChromeRootStore(
        std::move(invalid_root_store_ptr), update_run_loop.QuitClosure());
    update_run_loop.Run();
  }

  {
    auto [net_error, result] = Verify(
        cv_service_remote, leaf->GetX509Certificate(), "www.example.com");
    // Request should be OK because root store update was ignored.
    ASSERT_EQ(net_error, net::OK);
  }

  // Clear all certs from the proto
  root_store_proto.clear_trust_anchors();
  root_store_proto.SerializeToString(&proto_serialized);
  cert_verifier::mojom::ChromeRootStorePtr empty_root_store_ptr =
      cert_verifier::mojom::ChromeRootStore::New(
          base::as_bytes(base::make_span(proto_serialized)));

  // Feed factory the new Chrome Root Store.
  {
    base::RunLoop update_run_loop;
    cv_service_factory_impl.UpdateChromeRootStore(
        std::move(empty_root_store_ptr), update_run_loop.QuitClosure());
    update_run_loop.Run();
  }

  {
    auto [net_error, result] = Verify(
        cv_service_remote, leaf->GetX509Certificate(), "www.example.com");
    // Request should be OK because root store update was ignored.
    ASSERT_EQ(net_error, net::OK);
  }
  // Update was ignored, so no change observers should have been notified.
  EXPECT_EQ(cv_service_client.changed_count_, 0u);
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
      cv_service_factory_remote.BindNewPipeAndPassReceiver());

  // Feed factory the new Chrome Root Store.
  {
    base::RunLoop update_run_loop;
    cv_service_factory_impl.UpdateChromeRootStore(
        std::move(root_store_ptr), update_run_loop.QuitClosure());
    update_run_loop.Run();
  }

  cert_verifier::mojom::ChromeRootStoreInfoPtr info_ptr;
  base::RunLoop request_completed_run_loop;
  cv_service_factory_remote->GetChromeRootStoreInfo(base::BindOnce(
      &GetRootStoreInfo, &info_ptr, request_completed_run_loop.QuitClosure()));
  request_completed_run_loop.Run();
  ASSERT_TRUE(info_ptr);
  EXPECT_EQ(info_ptr->version, root_store_proto.version_major());
  ASSERT_EQ(info_ptr->root_cert_info.size(), static_cast<std::size_t>(1));

  bssl::der::Input subject_tlv(root->GetSubject());
  bssl::RDNSequence subject_rdn;
  ASSERT_TRUE(bssl::ParseName(subject_tlv, &subject_rdn));
  std::string subject_string;
  ASSERT_TRUE(bssl::ConvertToRFC2253(subject_rdn, &subject_string));
  EXPECT_EQ(info_ptr->root_cert_info[0]->name, subject_string);

  net::SHA256HashValue root_hash =
      net::X509Certificate::CalculateFingerprint256(root->GetCertBuffer());
  EXPECT_EQ(info_ptr->root_cert_info[0]->sha256hash_hex,
            base::HexEncode(root_hash.data, std::size(root_hash.data)));
}

TEST(CertVerifierServiceFactoryTest, RootStoreInfoWithCompiledRootStore) {
  base::test::TaskEnvironment task_environment;
  bssl::ParsedCertificateList anchors = net::CompiledChromeRootStoreAnchors();

  mojo::Remote<mojom::CertVerifierServiceFactory> cv_service_factory_remote;
  CertVerifierServiceFactoryImpl cv_service_factory_impl(
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

#endif  // BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)

class CertVerifierServiceFactoryBuiltinVerifierTest : public ::testing::Test {
 public:
  void SetUp() override {
    if (!SystemUsesBuiltinVerifier()) {
      GTEST_SKIP()
          << "Skipping test because system doesn't use builtin verifier";
    }

    ::testing::Test::SetUp();
  }

 private:
  bool SystemUsesBuiltinVerifier() {
#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(CHROME_ROOT_STORE_ONLY)
    return true;
#elif BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)
    // On CHROME_ROOT_STORE_OPTIONAL platforms, the tests set
    // use_chrome_root_store=true, so the tests will also work on those
    // platforms even if the kChromeRootStoreUsed default is false.
    // (This doesn't result in missing coverage of the
    // use_chrome_root_store=false case since the only non-CRS implementations
    // remaining don't support CRLSets.)
    return true;
#else
    return false;
#endif
  }

  base::test::TaskEnvironment task_environment_;
};

// Test that a new Cert verifier will use an updated CRLSet if
// one was already passed into CertVerifierServiceFactory.
TEST_F(CertVerifierServiceFactoryBuiltinVerifierTest,
       GetNewCertVerifierWithUpdatedCRLSet) {
  scoped_refptr<net::X509Certificate> test_root(net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "root_ca_cert.pem"));
  ASSERT_TRUE(test_root);
  net::ScopedTestRoot scoped_test_root(test_root);
  scoped_refptr<net::X509Certificate> ok_cert(
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(ok_cert);

  mojo::Remote<mojom::CertVerifierServiceFactory> cv_service_factory_remote;
  CertVerifierServiceFactoryImpl cv_service_factory_impl(
      cv_service_factory_remote.BindNewPipeAndPassReceiver());

  EnableChromeRootStoreIfOptional(&cv_service_factory_impl);

  // Feed factory the CRLSet which blocks |ok_cert|.
  UpdateCRLSetWithTestFile(&cv_service_factory_impl, "crlset_by_leaf_spki.raw");

  mojo::Remote<mojom::CertVerifierService> cv_service_remote;
  DummyCVServiceClient cv_service_client;
  mojom::CertVerifierCreationParamsPtr cv_creation_params =
      mojom::CertVerifierCreationParams::New();

  // Create the cert verifier. It should start with the previously configured
  // CRLSet already active.
  cv_service_factory_remote->GetNewCertVerifier(
      cv_service_remote.BindNewPipeAndPassReceiver(),
      /*updater=*/mojo::NullReceiver(),
      cv_service_client.client_.BindNewPipeAndPassRemote(),
      std::move(cv_creation_params));

  auto [net_error, result] = Verify(cv_service_remote, ok_cert, "127.0.0.1");
  EXPECT_THAT(net_error, IsError(net::ERR_CERT_REVOKED));
  EXPECT_TRUE(result.cert_status & net::CERT_STATUS_REVOKED);

  // Update happened before the CertVerifier was created, no change observers
  // should have been notified.
  EXPECT_EQ(cv_service_client.changed_count_, 0u);
}

// Test that an existing CertVerifierService will use an updated CRLSet if one
// is provided to the CertVerifierServiceFactory
TEST_F(CertVerifierServiceFactoryBuiltinVerifierTest,
       UpdateExistingCertVerifierWithCRLSet) {
  scoped_refptr<net::X509Certificate> test_root(net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "root_ca_cert.pem"));
  ASSERT_TRUE(test_root);
  net::ScopedTestRoot scoped_test_root(test_root);
  scoped_refptr<net::X509Certificate> ok_cert(
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(ok_cert);

  mojo::Remote<mojom::CertVerifierServiceFactory> cv_service_factory_remote;
  CertVerifierServiceFactoryImpl cv_service_factory_impl(
      cv_service_factory_remote.BindNewPipeAndPassReceiver());

  EnableChromeRootStoreIfOptional(&cv_service_factory_impl);

  mojo::Remote<mojom::CertVerifierService> cv_service_remote;
  DummyCVServiceClient cv_service_client;
  mojom::CertVerifierCreationParamsPtr cv_creation_params =
      mojom::CertVerifierCreationParams::New();

  cv_service_factory_remote->GetNewCertVerifier(
      cv_service_remote.BindNewPipeAndPassReceiver(),
      /*updater=*/mojo::NullReceiver(),
      cv_service_client.client_.BindNewPipeAndPassRemote(),
      std::move(cv_creation_params));

  // Try request, it should succeed since the leaf is not blocked yet.
  {
    auto [net_error, result] = Verify(cv_service_remote, ok_cert, "127.0.0.1");
    EXPECT_THAT(net_error, IsOk());
  }
  // No updates should have happened yet.
  EXPECT_EQ(cv_service_client.changed_count_, 0u);

  // Feed factory the CRLSet which blocks |ok_cert|.
  UpdateCRLSetWithTestFile(&cv_service_factory_impl, "crlset_by_leaf_spki.raw");

  // Update should have been notified.
  EXPECT_NO_FATAL_FAILURE(cv_service_client.WaitForCertVerifierChange(1u));

  // Try request again on existing verifier, it should be blocked now.
  {
    auto [net_error, result] = Verify(cv_service_remote, ok_cert, "127.0.0.1");
    EXPECT_THAT(net_error, IsError(net::ERR_CERT_REVOKED));
    EXPECT_TRUE(result.cert_status & net::CERT_STATUS_REVOKED);
  }
}

// Verifies newer CRLSets (by sequence number) are applied.
TEST_F(CertVerifierServiceFactoryBuiltinVerifierTest, CRLSetIsUpdatedIfNewer) {
  scoped_refptr<net::X509Certificate> test_root(net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "root_ca_cert.pem"));
  ASSERT_TRUE(test_root);
  net::ScopedTestRoot scoped_test_root(test_root);
  scoped_refptr<net::X509Certificate> ok_cert(
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(ok_cert);

  mojo::Remote<mojom::CertVerifierServiceFactory> cv_service_factory_remote;
  CertVerifierServiceFactoryImpl cv_service_factory_impl(
      cv_service_factory_remote.BindNewPipeAndPassReceiver());

  EnableChromeRootStoreIfOptional(&cv_service_factory_impl);

  mojo::Remote<mojom::CertVerifierService> cv_service_remote;
  DummyCVServiceClient cv_service_client;
  mojom::CertVerifierCreationParamsPtr cv_creation_params =
      mojom::CertVerifierCreationParams::New();

  cv_service_factory_remote->GetNewCertVerifier(
      cv_service_remote.BindNewPipeAndPassReceiver(),
      /*updater=*/mojo::NullReceiver(),
      cv_service_client.client_.BindNewPipeAndPassRemote(),
      std::move(cv_creation_params));

  // Wait for the CertVerifier to be created before sending the CRLSet update.
  // This ensures that the CertVerifierServiceClient will be registered and
  // thus receive the expected number of update notifications.
  cv_service_remote.FlushForTesting();

  // No updates should have happened yet.
  EXPECT_EQ(cv_service_client.changed_count_, 0u);

  // Send a CRLSet that only allows the root cert if it matches a known SPKI
  // hash (that matches the test server chain)
  UpdateCRLSetWithTestFile(&cv_service_factory_impl,
                           "crlset_by_root_subject.raw");

  // Client should have received notification of the update.
  EXPECT_NO_FATAL_FAILURE(cv_service_client.WaitForCertVerifierChange(1u));

  // Try request, it should succeed since the root SPKI hash is allowed.
  {
    auto [net_error, result] = Verify(cv_service_remote, ok_cert, "127.0.0.1");
    EXPECT_THAT(net_error, IsOk());
  }

  // Feed factory the CRLSet which blocks the root with no SPKI hash exception.
  UpdateCRLSetWithTestFile(&cv_service_factory_impl,
                           "crlset_by_root_subject_no_spki.raw");

  // Client should have received notification of the update.
  EXPECT_NO_FATAL_FAILURE(cv_service_client.WaitForCertVerifierChange(2u));

  // Try request again, it should be blocked now.
  {
    auto [net_error, result] = Verify(cv_service_remote, ok_cert, "127.0.0.1");
    EXPECT_THAT(net_error, IsError(net::ERR_CERT_REVOKED));
    EXPECT_TRUE(result.cert_status & net::CERT_STATUS_REVOKED);
  }
}

// Verifies that attempting to send an older CRLSet (by sequence number)
// does not apply to existing or new contexts.
TEST_F(CertVerifierServiceFactoryBuiltinVerifierTest, CRLSetDoesNotDowngrade) {
  scoped_refptr<net::X509Certificate> test_root(net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "root_ca_cert.pem"));
  ASSERT_TRUE(test_root);
  net::ScopedTestRoot scoped_test_root(test_root);
  scoped_refptr<net::X509Certificate> ok_cert(
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(ok_cert);

  mojo::Remote<mojom::CertVerifierServiceFactory> cv_service_factory_remote;
  CertVerifierServiceFactoryImpl cv_service_factory_impl(
      cv_service_factory_remote.BindNewPipeAndPassReceiver());

  EnableChromeRootStoreIfOptional(&cv_service_factory_impl);

  mojo::Remote<mojom::CertVerifierService> cv_service_remote;
  DummyCVServiceClient cv_service_client;
  mojom::CertVerifierCreationParamsPtr cv_creation_params =
      mojom::CertVerifierCreationParams::New();

  cv_service_factory_remote->GetNewCertVerifier(
      cv_service_remote.BindNewPipeAndPassReceiver(),
      /*updater=*/mojo::NullReceiver(),
      cv_service_client.client_.BindNewPipeAndPassRemote(),
      std::move(cv_creation_params));

  // Wait for the CertVerifier to be created before sending the CRLSet update.
  // This ensures that the CertVerifierServiceClient will be registered and
  // thus receive the expected number of update notifications.
  cv_service_remote.FlushForTesting();

  // No updates should have happened yet.
  EXPECT_EQ(cv_service_client.changed_count_, 0u);

  // Send a CRLSet which blocks the root with no SPKI hash exception.
  UpdateCRLSetWithTestFile(&cv_service_factory_impl,
                           "crlset_by_root_subject_no_spki.raw");

  // Make sure the connection fails, due to the certificate being revoked.
  {
    auto [net_error, result] = Verify(cv_service_remote, ok_cert, "127.0.0.1");
    EXPECT_THAT(net_error, IsError(net::ERR_CERT_REVOKED));
    EXPECT_TRUE(result.cert_status & net::CERT_STATUS_REVOKED);
  }

  // Attempt to configure an older CRLSet that allowed trust in the root.
  UpdateCRLSetWithTestFile(&cv_service_factory_impl,
                           "crlset_by_root_subject.raw");

  // Make sure the connection still fails, due to the newer CRLSet still
  // applying.
  {
    auto [net_error, result] = Verify(cv_service_remote, ok_cert, "127.0.0.1");
    EXPECT_THAT(net_error, IsError(net::ERR_CERT_REVOKED));
    EXPECT_TRUE(result.cert_status & net::CERT_STATUS_REVOKED);
  }

  // Change count should still be 1 since the CRLSet was ignored.
  EXPECT_EQ(cv_service_client.changed_count_, 1u);

  // Create a new CertVerifierService and ensure the latest CRLSet is still
  // applied.
  mojo::Remote<mojom::CertVerifierService> cv_service_remote2;
  mojo::Remote<mojom::CertVerifierServiceUpdater> cv_service_updater_remote2;
  DummyCVServiceClient cv_service_client2;
  mojom::CertVerifierCreationParamsPtr cv_creation_params2 =
      mojom::CertVerifierCreationParams::New();

  cv_service_factory_remote->GetNewCertVerifier(
      cv_service_remote2.BindNewPipeAndPassReceiver(),
      cv_service_updater_remote2.BindNewPipeAndPassReceiver(),
      cv_service_client2.client_.BindNewPipeAndPassRemote(),
      std::move(cv_creation_params2));

  // The newer CRLSet that blocks the connection should still apply, even to
  // new CertVerifierServices.
  {
    auto [net_error, result] = Verify(cv_service_remote2, ok_cert, "127.0.0.1");
    EXPECT_THAT(net_error, IsError(net::ERR_CERT_REVOKED));
    EXPECT_TRUE(result.cert_status & net::CERT_STATUS_REVOKED);
  }
}

// Verifies that attempting to send an invalid CRLSet does not affect existing
// or new contexts.
TEST_F(CertVerifierServiceFactoryBuiltinVerifierTest, BadCRLSetIgnored) {
  scoped_refptr<net::X509Certificate> test_root(net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "root_ca_cert.pem"));
  ASSERT_TRUE(test_root);
  net::ScopedTestRoot scoped_test_root(test_root);
  scoped_refptr<net::X509Certificate> ok_cert(
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(ok_cert);

  mojo::Remote<mojom::CertVerifierServiceFactory> cv_service_factory_remote;
  CertVerifierServiceFactoryImpl cv_service_factory_impl(
      cv_service_factory_remote.BindNewPipeAndPassReceiver());

  EnableChromeRootStoreIfOptional(&cv_service_factory_impl);

  mojo::Remote<mojom::CertVerifierService> cv_service_remote;
  DummyCVServiceClient cv_service_client;
  mojom::CertVerifierCreationParamsPtr cv_creation_params =
      mojom::CertVerifierCreationParams::New();

  cv_service_factory_remote->GetNewCertVerifier(
      cv_service_remote.BindNewPipeAndPassReceiver(),
      /*updater=*/mojo::NullReceiver(),
      cv_service_client.client_.BindNewPipeAndPassRemote(),
      std::move(cv_creation_params));

  // Try verifying, it should succeed with the builtin CRLSet.
  {
    auto [net_error, result] = Verify(cv_service_remote, ok_cert, "127.0.0.1");
    EXPECT_THAT(net_error, IsOk());
  }

  // No updates should have happened yet.
  EXPECT_EQ(cv_service_client.changed_count_, 0u);

  // Send a CRLSet which blocks the root with no SPKI hash exception.
  UpdateCRLSetWithTestFile(&cv_service_factory_impl,
                           "crlset_by_root_subject_no_spki.raw");

  // Make sure verifying fails, due to the certificate being revoked.
  {
    auto [net_error, result] = Verify(cv_service_remote, ok_cert, "127.0.0.1");
    EXPECT_THAT(net_error, IsError(net::ERR_CERT_REVOKED));
    EXPECT_TRUE(result.cert_status & net::CERT_STATUS_REVOKED);
  }

  // Send an invalid CRLSet.
  {
    std::string crl_set_bytes(1000, '\xff');

    base::RunLoop update_run_loop;
    cv_service_factory_impl.UpdateCRLSet(
        base::as_bytes(base::make_span(crl_set_bytes)),
        update_run_loop.QuitClosure());
    update_run_loop.Run();
  }

  // Verification should still fail, due to the invalid CRLSet being ignored.
  {
    auto [net_error, result] = Verify(cv_service_remote, ok_cert, "127.0.0.1");
    EXPECT_THAT(net_error, IsError(net::ERR_CERT_REVOKED));
    EXPECT_TRUE(result.cert_status & net::CERT_STATUS_REVOKED);
  }

  // Change count should still be 1 since the CRLSet was ignored.
  EXPECT_EQ(cv_service_client.changed_count_, 1u);

  // Create a new CertVerifierService and ensure the latest valid CRLSet is
  // still applied.
  mojo::Remote<mojom::CertVerifierService> cv_service_remote2;
  mojo::Remote<mojom::CertVerifierServiceUpdater> cv_service_updater_remote2;
  DummyCVServiceClient cv_service_client2;
  mojom::CertVerifierCreationParamsPtr cv_creation_params2 =
      mojom::CertVerifierCreationParams::New();

  cv_service_factory_remote->GetNewCertVerifier(
      cv_service_remote2.BindNewPipeAndPassReceiver(),
      cv_service_updater_remote2.BindNewPipeAndPassReceiver(),
      cv_service_client2.client_.BindNewPipeAndPassRemote(),
      std::move(cv_creation_params2));

  // The CRLSet that blocks the root should still apply, even to new
  // CertVerifierServices.
  {
    auto [net_error, result] = Verify(cv_service_remote2, ok_cert, "127.0.0.1");
    EXPECT_THAT(net_error, IsError(net::ERR_CERT_REVOKED));
    EXPECT_TRUE(result.cert_status & net::CERT_STATUS_REVOKED);
  }
}

TEST_F(CertVerifierServiceFactoryBuiltinVerifierTest,
       GetNewCertVerifierWithAdditionalCerts) {
  auto [leaf1, intermediate1, root1] = net::CertBuilder::CreateSimpleChain3();
  auto [leaf2, intermediate2, root2] = net::CertBuilder::CreateSimpleChain3();

  mojo::Remote<mojom::CertVerifierServiceFactory> cv_service_factory_remote;
  CertVerifierServiceFactoryImpl cv_service_factory_impl(
      cv_service_factory_remote.BindNewPipeAndPassReceiver());

  EnableChromeRootStoreIfOptional(&cv_service_factory_impl);

  mojo::Remote<mojom::CertVerifierService> cv_service_remote;
  mojo::Remote<mojom::CertVerifierServiceUpdater> cv_service_updater_remote;
  DummyCVServiceClient cv_service_client;
  mojom::CertVerifierCreationParamsPtr cv_creation_params =
      mojom::CertVerifierCreationParams::New();
  // Initial creation params supply `root1` as an additional trust anchor and
  // `intermediate1` as an untrusted cert.
  cv_creation_params->initial_additional_certificates =
      mojom::AdditionalCertificates::New();
  cv_creation_params->initial_additional_certificates->trust_anchors.push_back(
      root1->GetX509Certificate());
  cv_creation_params->initial_additional_certificates->all_certificates
      .push_back(intermediate1->GetX509Certificate());

  // Create the cert verifier. It should start with the additional trust
  // anchors from the creation params already trusted.
  cv_service_factory_remote->GetNewCertVerifier(
      cv_service_remote.BindNewPipeAndPassReceiver(),
      cv_service_updater_remote.BindNewPipeAndPassReceiver(),
      cv_service_client.client_.BindNewPipeAndPassRemote(),
      std::move(cv_creation_params));

  // `leaf1` should be trusted and `leaf2` should not be trusted.
  {
    auto [net_error, result] = Verify(
        cv_service_remote, leaf1->GetX509Certificate(), "www.example.com");
    EXPECT_THAT(net_error, IsError(net::OK));
  }
  {
    auto [net_error, result] = Verify(
        cv_service_remote, leaf2->GetX509Certificate(), "www.example.com");
    EXPECT_THAT(net_error, IsError(net::ERR_CERT_AUTHORITY_INVALID));
  }

  EXPECT_EQ(cv_service_client.changed_count_, 0u);

  // Supply a new set of additional certificates with `root2` trusted this time.
  auto new_additional_certificates = mojom::AdditionalCertificates::New();
  new_additional_certificates->trust_anchors.push_back(
      root2->GetX509Certificate());
  new_additional_certificates->all_certificates.push_back(
      intermediate2->GetX509Certificate());
  cv_service_updater_remote->UpdateAdditionalCertificates(
      std::move(new_additional_certificates));

  // Client should have received notification of the update.
  EXPECT_NO_FATAL_FAILURE(cv_service_client.WaitForCertVerifierChange(1u));

  // Now `leaf1` should not be trusted and `leaf2` should be trusted.
  {
    auto [net_error, result] = Verify(
        cv_service_remote, leaf1->GetX509Certificate(), "www.example.com");
    EXPECT_THAT(net_error, IsError(net::ERR_CERT_AUTHORITY_INVALID));
  }
  {
    auto [net_error, result] = Verify(
        cv_service_remote, leaf2->GetX509Certificate(), "www.example.com");
    EXPECT_THAT(net_error, IsError(net::OK));
  }
}

}  // namespace cert_verifier
