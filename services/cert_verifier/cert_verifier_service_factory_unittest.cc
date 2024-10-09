// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/cert_verifier/cert_verifier_service_factory.h"

#include <cstddef>
#include <memory>
#include <string_view>
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
#include "crypto/ec_private_key.h"
#include "crypto/sha2.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
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

#if BUILDFLAG(IS_CT_SUPPORTED)
#include "components/certificate_transparency/chrome_ct_policy_enforcer.h"
#include "services/network/public/mojom/ct_log_info.mojom.h"
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
#include "base/version_info/version_info.h"  // nogncheck
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
    std::string_view crlset_file_name) {
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
  chrome_root_store::RootStore root_store;
  root_store.set_version_major(net::CompiledChromeRootStoreVersion() + 1);
  chrome_root_store::TrustAnchor* anchor = root_store.add_trust_anchors();
  anchor->set_der(root->GetDER());

  mojo::Remote<mojom::CertVerifierServiceFactory> cv_service_factory_remote;
  CertVerifierServiceFactoryImpl cv_service_factory_impl(
      cv_service_factory_remote.BindNewPipeAndPassReceiver());

  EnableChromeRootStoreIfOptional(&cv_service_factory_impl);

  // Feed factory the new Chrome Root Store.
  {
    base::RunLoop update_run_loop;
    cv_service_factory_impl.UpdateChromeRootStore(
        mojo_base::ProtoWrapper(root_store), update_run_loop.QuitClosure());
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
  chrome_root_store::RootStore root_store;
  root_store.set_version_major(net::CompiledChromeRootStoreVersion() + 1);
  chrome_root_store::TrustAnchor* anchor = root_store.add_trust_anchors();
  anchor->set_der(root->GetDER());

  // Feed factory the new Chrome Root Store.
  {
    base::RunLoop update_run_loop;
    cv_service_factory_impl.UpdateChromeRootStore(
        mojo_base::ProtoWrapper(root_store), update_run_loop.QuitClosure());
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
  chrome_root_store::RootStore root_store;
  root_store.set_version_major(net::CompiledChromeRootStoreVersion());
  chrome_root_store::TrustAnchor* anchor = root_store.add_trust_anchors();
  anchor->set_der(root->GetDER());

  mojo::Remote<mojom::CertVerifierServiceFactory> cv_service_factory_remote;
  CertVerifierServiceFactoryImpl cv_service_factory_impl(
      cv_service_factory_remote.BindNewPipeAndPassReceiver());

  EnableChromeRootStoreIfOptional(&cv_service_factory_impl);

  // Feed factory the new Chrome Root Store.
  {
    base::RunLoop update_run_loop;
    cv_service_factory_impl.UpdateChromeRootStore(
        mojo_base::ProtoWrapper(root_store), update_run_loop.QuitClosure());
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
  chrome_root_store::RootStore root_store;
  root_store.set_version_major(net::CompiledChromeRootStoreVersion() + 1);
  chrome_root_store::TrustAnchor* anchor = root_store.add_trust_anchors();
  anchor->set_der(root->GetDER());

  mojo::Remote<mojom::CertVerifierServiceFactory> cv_service_factory_remote;
  CertVerifierServiceFactoryImpl cv_service_factory_impl(
      cv_service_factory_remote.BindNewPipeAndPassReceiver());

  EnableChromeRootStoreIfOptional(&cv_service_factory_impl);

  // Feed factory the new Chrome Root Store.
  {
    base::RunLoop update_run_loop;
    cv_service_factory_impl.UpdateChromeRootStore(
        mojo_base::ProtoWrapper(root_store), update_run_loop.QuitClosure());
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
  chrome_root_store::RootStore invalid_root_store;
  invalid_root_store.set_version_major(net::CompiledChromeRootStoreVersion() +
                                       2);
  chrome_root_store::TrustAnchor* invalid_anchor =
      invalid_root_store.add_trust_anchors();
  invalid_anchor->set_der("gibberishcert");

  // Feed factory the new Chrome Root Store.
  {
    base::RunLoop update_run_loop;
    cv_service_factory_impl.UpdateChromeRootStore(
        mojo_base::ProtoWrapper(invalid_root_store),
        update_run_loop.QuitClosure());
    update_run_loop.Run();
  }

  {
    auto [net_error, result] = Verify(
        cv_service_remote, leaf->GetX509Certificate(), "www.example.com");
    // Request should be OK because root store update was ignored.
    ASSERT_EQ(net_error, net::OK);
  }

  // Clear all certs from the proto
  root_store.clear_trust_anchors();

  // Feed factory the new empty Chrome Root Store.
  {
    base::RunLoop update_run_loop;
    cv_service_factory_impl.UpdateChromeRootStore(
        mojo_base::ProtoWrapper(root_store), update_run_loop.QuitClosure());
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
  chrome_root_store::RootStore root_store;
  root_store.set_version_major(net::CompiledChromeRootStoreVersion() + 1);
  chrome_root_store::TrustAnchor* anchor = root_store.add_trust_anchors();
  anchor->set_der(root->GetDER());

  mojo::Remote<mojom::CertVerifierServiceFactory> cv_service_factory_remote;
  CertVerifierServiceFactoryImpl cv_service_factory_impl(
      cv_service_factory_remote.BindNewPipeAndPassReceiver());

  // Feed factory the new Chrome Root Store.
  {
    base::RunLoop update_run_loop;
    cv_service_factory_impl.UpdateChromeRootStore(
        mojo_base::ProtoWrapper(root_store), update_run_loop.QuitClosure());
    update_run_loop.Run();
  }

  cert_verifier::mojom::ChromeRootStoreInfoPtr info_ptr;
  base::RunLoop request_completed_run_loop;
  cv_service_factory_remote->GetChromeRootStoreInfo(base::BindOnce(
      &GetRootStoreInfo, &info_ptr, request_completed_run_loop.QuitClosure()));
  request_completed_run_loop.Run();
  ASSERT_TRUE(info_ptr);
  EXPECT_EQ(info_ptr->version, root_store.version_major());
  ASSERT_EQ(info_ptr->root_cert_info.size(), static_cast<std::size_t>(1));

  net::SHA256HashValue root_hash =
      net::X509Certificate::CalculateFingerprint256(root->GetCertBuffer());
  EXPECT_EQ(info_ptr->root_cert_info[0]->sha256hash_hex,
            base::HexEncode(root_hash.data));
  EXPECT_TRUE(net::x509_util::CryptoBufferEqual(
      net::x509_util::CreateCryptoBuffer(info_ptr->root_cert_info[0]->cert)
          .get(),
      root->GetCertBuffer()));
}

std::string CurVersionString() {
  return version_info::GetVersion().GetString();
}
std::string NextVersionString() {
  const std::vector<uint32_t>& components =
      version_info::GetVersion().components();
  return base::Version(
             {components[0], components[1], components[2], components[3] + 1})
      .GetString();
}
std::string PrevVersionString() {
  const std::vector<uint32_t>& components =
      version_info::GetVersion().components();
  if (components[3] > 0) {
    return base::Version(
               {components[0], components[1], components[2], components[3] - 1})
        .GetString();
  } else {
    return base::Version(
               {components[0], components[1], components[2] - 1, UINT32_MAX})
        .GetString();
  }
}

TEST(CertVerifierServiceFactoryTest, RootStoreInfoWithVersionConstraintUnmet) {
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
  chrome_root_store::ConstraintSet* constraint = anchor->add_constraints();

  // root should not be trusted
  constraint->set_max_version_exclusive(PrevVersionString());

  mojo::Remote<mojom::CertVerifierServiceFactory> cv_service_factory_remote;
  CertVerifierServiceFactoryImpl cv_service_factory_impl(
      cv_service_factory_remote.BindNewPipeAndPassReceiver());

  // Feed factory the new Chrome Root Store.
  {
    base::RunLoop update_run_loop;
    cv_service_factory_impl.UpdateChromeRootStore(
        mojo_base::ProtoWrapper(root_store_proto),
        update_run_loop.QuitClosure());
    update_run_loop.Run();
  }

  cert_verifier::mojom::ChromeRootStoreInfoPtr info_ptr;
  base::RunLoop request_completed_run_loop;
  cv_service_factory_remote->GetChromeRootStoreInfo(base::BindOnce(
      &GetRootStoreInfo, &info_ptr, request_completed_run_loop.QuitClosure()));
  request_completed_run_loop.Run();
  ASSERT_TRUE(info_ptr);
  EXPECT_EQ(info_ptr->version, root_store_proto.version_major());
  ASSERT_EQ(info_ptr->root_cert_info.size(), static_cast<std::size_t>(0));
}

TEST(CertVerifierServiceFactoryTest, RootStoreInfoWithVersionConstraintMet) {
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
  // Root should not be trusted because of this constraint ...
  chrome_root_store::ConstraintSet* constraint = anchor->add_constraints();
  constraint->set_max_version_exclusive(PrevVersionString());
  // .. but should be trusted because of this.
  constraint = anchor->add_constraints();
  constraint->set_min_version(PrevVersionString());
  constraint->set_max_version_exclusive(NextVersionString());

  mojo::Remote<mojom::CertVerifierServiceFactory> cv_service_factory_remote;
  CertVerifierServiceFactoryImpl cv_service_factory_impl(
      cv_service_factory_remote.BindNewPipeAndPassReceiver());

  // Feed factory the new Chrome Root Store.
  {
    base::RunLoop update_run_loop;
    cv_service_factory_impl.UpdateChromeRootStore(
        mojo_base::ProtoWrapper(root_store_proto),
        update_run_loop.QuitClosure());
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
}

TEST(CertVerifierServiceFactoryTest, RootStoreInfoWithCompiledRootStore) {
  base::test::TaskEnvironment task_environment;
  std::vector<net::ChromeRootStoreData::Anchor> anchors =
      net::CompiledChromeRootStoreAnchors();

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
  // In cases where the compiled Chrome Root Store has roots with version
  // constraints, there might be less trusted roots depending on what version #
  // the test is running at.
  EXPECT_LE(info_ptr->root_cert_info.size(), anchors.size());
  EXPECT_GT(info_ptr->root_cert_info.size(), static_cast<std::size_t>(0));
}

#endif  // BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)

#if BUILDFLAG(IS_CT_SUPPORTED)
TEST(CertVerifierServiceFactoryTest, UpdateCtLogList) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::CertVerifierServiceFactory> cv_service_factory_remote;
  CertVerifierServiceFactoryImpl cv_service_factory_impl(
      cv_service_factory_remote.BindNewPipeAndPassReceiver());

  // Should start with empty log list.
  EXPECT_EQ(cv_service_factory_impl.get_impl_params().ct_logs.size(), 0u);
  EXPECT_FALSE(cv_service_factory_impl.get_impl_params().ct_policy_enforcer);

  auto log1_private_key = crypto::ECPrivateKey::Create();
  std::vector<uint8_t> log1_spki;
  ASSERT_TRUE(log1_private_key->ExportPublicKey(&log1_spki));
  const std::string log1_id =
      crypto::SHA256HashString(std::string(log1_spki.begin(), log1_spki.end()));
  auto log2_private_key = crypto::ECPrivateKey::Create();
  std::vector<uint8_t> log2_spki;
  ASSERT_TRUE(log2_private_key->ExportPublicKey(&log2_spki));
  const std::string log2_id =
      crypto::SHA256HashString(std::string(log2_spki.begin(), log2_spki.end()));
  const std::string kLog1Operator = "log operator";
  const std::string kLog2Operator = "log2 operator";
  const std::string kLog3Operator = "log3 operator";

  // Test the 1st log list update.
  {
    std::vector<network::mojom::CTLogInfoPtr> log_list_mojo;
    {
      network::mojom::CTLogInfoPtr log_info = network::mojom::CTLogInfo::New();
      log_info->public_key = std::string(log1_spki.begin(), log1_spki.end());
      log_info->id = log1_id;
      log_info->name = "log name";
      log_info->current_operator = kLog1Operator;
      log_list_mojo.push_back(std::move(log_info));
    }
    {
      network::mojom::CTLogInfoPtr log_info = network::mojom::CTLogInfo::New();
      log_info->public_key = std::string(log2_spki.begin(), log2_spki.end());
      log_info->id = log2_id;
      log_info->name = "log2 name";
      log_info->current_operator = kLog2Operator;
      log_list_mojo.push_back(std::move(log_info));
    }

    {
      base::RunLoop run_loop;
      cv_service_factory_remote->UpdateCtLogList(
          std::move(log_list_mojo), base::Time::Now(), run_loop.QuitClosure());
      run_loop.Run();
    }

    ASSERT_EQ(cv_service_factory_impl.get_impl_params().ct_logs.size(), 2u);
    EXPECT_EQ(cv_service_factory_impl.get_impl_params().ct_logs[0]->key_id(),
              crypto::SHA256HashString(
                  std::string(log1_spki.begin(), log1_spki.end())));
    EXPECT_EQ(cv_service_factory_impl.get_impl_params().ct_logs[1]->key_id(),
              crypto::SHA256HashString(
                  std::string(log2_spki.begin(), log2_spki.end())));

    net::CTPolicyEnforcer* request_enforcer =
        cv_service_factory_impl.get_impl_params().ct_policy_enforcer.get();
    ASSERT_TRUE(request_enforcer);
    certificate_transparency::ChromeCTPolicyEnforcer* policy_enforcer =
        reinterpret_cast<certificate_transparency::ChromeCTPolicyEnforcer*>(
            request_enforcer);

    std::map<std::string, certificate_transparency::LogInfo> log_info =
        policy_enforcer->log_info_for_testing();
    EXPECT_EQ(log_info[log1_id].operator_history.current_operator_,
              kLog1Operator);
    EXPECT_EQ(log_info[log2_id].operator_history.current_operator_,
              kLog2Operator);
  }

  // Test a 2nd log list update.
  {
    std::vector<network::mojom::CTLogInfoPtr> log_list_mojo;
    {
      network::mojom::CTLogInfoPtr log_info = network::mojom::CTLogInfo::New();
      log_info->public_key = std::string(log1_spki.begin(), log1_spki.end());
      log_info->id = log1_id;
      log_info->name = "log name";
      log_info->current_operator = kLog1Operator;
      log_list_mojo.push_back(std::move(log_info));
    }
    const std::string log3_public_key = "bad public key";
    const std::string log3_id = crypto::SHA256HashString(log3_public_key);
    {
      network::mojom::CTLogInfoPtr log_info = network::mojom::CTLogInfo::New();
      log_info->public_key = log3_public_key;
      log_info->id = log3_id;
      log_info->name = "log3 name";
      log_info->current_operator = kLog3Operator;
      log_list_mojo.push_back(std::move(log_info));
    }

    {
      base::RunLoop run_loop;
      cv_service_factory_remote->UpdateCtLogList(
          std::move(log_list_mojo), base::Time::Now(), run_loop.QuitClosure());
      run_loop.Run();
    }

    // The log with the bad key should have been ignored.
    ASSERT_EQ(cv_service_factory_impl.get_impl_params().ct_logs.size(), 1u);
    EXPECT_EQ(cv_service_factory_impl.get_impl_params().ct_logs[0]->key_id(),
              crypto::SHA256HashString(
                  std::string(log1_spki.begin(), log1_spki.end())));

    net::CTPolicyEnforcer* request_enforcer =
        cv_service_factory_impl.get_impl_params().ct_policy_enforcer.get();
    ASSERT_TRUE(request_enforcer);
    certificate_transparency::ChromeCTPolicyEnforcer* policy_enforcer =
        reinterpret_cast<certificate_transparency::ChromeCTPolicyEnforcer*>(
            request_enforcer);

    // CTPolicyEnforcer doesn't parse the key, so it accepts both logs.
    std::map<std::string, certificate_transparency::LogInfo> log_info =
        policy_enforcer->log_info_for_testing();
    EXPECT_EQ(log_info[log1_id].operator_history.current_operator_,
              kLog1Operator);
    EXPECT_EQ(log_info[log3_id].operator_history.current_operator_,
              kLog3Operator);
  }
}

TEST(CertVerifierServiceFactoryTest, CTPolicyEnforcerConfig) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::CertVerifierServiceFactory> cv_service_factory_remote;
  CertVerifierServiceFactoryImpl cv_service_factory_impl(
      cv_service_factory_remote.BindNewPipeAndPassReceiver());

  std::vector<network::mojom::CTLogInfoPtr> log_list_mojo;

  // The log public keys do not matter for the test, so invalid keys are used.
  // However, because the log IDs are derived from the SHA-256 hash of the log
  // key, the log keys are generated such that qualified logs are in the form
  // of four digits (e.g. "0000", "1111"), while disqualified logs are in the
  // form of four letters (e.g. "AAAA", "BBBB").

  for (int i = 0; i < 6; ++i) {
    network::mojom::CTLogInfoPtr log_info = network::mojom::CTLogInfo::New();
    // Shift to ASCII '0' (0x30)
    log_info->public_key = std::string(4, 0x30 + static_cast<char>(i));
    log_info->name = std::string(4, 0x30 + static_cast<char>(i));
    if (i % 2) {
      log_info->current_operator = "Google";
    } else {
      log_info->current_operator = "Not Google";
    }
    log_list_mojo.push_back(std::move(log_info));
  }
  for (int i = 0; i < 3; ++i) {
    network::mojom::CTLogInfoPtr log_info = network::mojom::CTLogInfo::New();
    // Shift to ASCII 'A' (0x41)
    log_info->public_key = std::string(4, 0x41 + static_cast<char>(i));
    log_info->name = std::string(4, 0x41 + static_cast<char>(i));
    log_info->disqualified_at = base::Time::FromTimeT(i);
    log_info->current_operator = "Not Google Either";

    log_list_mojo.push_back(std::move(log_info));
  }

  base::RunLoop run_loop;
  cv_service_factory_remote->UpdateCtLogList(
      std::move(log_list_mojo), base::Time::Now(), run_loop.QuitClosure());
  run_loop.Run();

  net::CTPolicyEnforcer* request_enforcer =
      cv_service_factory_impl.get_impl_params().ct_policy_enforcer.get();
  ASSERT_TRUE(request_enforcer);

  certificate_transparency::ChromeCTPolicyEnforcer* policy_enforcer =
      reinterpret_cast<certificate_transparency::ChromeCTPolicyEnforcer*>(
          request_enforcer);

  EXPECT_TRUE(
      std::is_sorted(policy_enforcer->disqualified_logs_for_testing().begin(),
                     policy_enforcer->disqualified_logs_for_testing().end()));

  EXPECT_THAT(policy_enforcer->disqualified_logs_for_testing(),
              ::testing::UnorderedElementsAre(
                  ::testing::Pair(crypto::SHA256HashString("AAAA"),
                                  base::Time::FromTimeT(0)),
                  ::testing::Pair(crypto::SHA256HashString("BBBB"),
                                  base::Time::FromTimeT(1)),
                  ::testing::Pair(crypto::SHA256HashString("CCCC"),
                                  base::Time::FromTimeT(2))));

  std::map<std::string, certificate_transparency::LogInfo> log_info =
      policy_enforcer->log_info_for_testing();

  for (auto log : policy_enforcer->disqualified_logs_for_testing()) {
    EXPECT_EQ(log_info[log.first].operator_history.current_operator_,
              "Not Google Either");
    EXPECT_TRUE(
        log_info[log.first].operator_history.previous_operators_.empty());
  }
}

TEST(CertVerifierServiceFactoryTest,
     CTPolicyEnforcerConfigWithOperatorSwitches) {
  base::test::TaskEnvironment task_environment;
  mojo::Remote<mojom::CertVerifierServiceFactory> cv_service_factory_remote;
  CertVerifierServiceFactoryImpl cv_service_factory_impl(
      cv_service_factory_remote.BindNewPipeAndPassReceiver());

  std::vector<network::mojom::CTLogInfoPtr> log_list_mojo;

  // The log public keys do not matter for the test, so invalid keys are used.
  // However, because the log IDs are derived from the SHA-256 hash of the log
  // key, the log keys are generated such that the log that never switched
  // operator is "0000", while the one that did is "AAAA".
  network::mojom::CTLogInfoPtr log_info = network::mojom::CTLogInfo::New();
  // Shift to ASCII '0' (0x30)
  log_info->public_key = std::string(4, 0x30);
  log_info->name = std::string(4, 0x30);
  log_info->current_operator = "Forever Operator";
  log_list_mojo.push_back(std::move(log_info));

  log_info = network::mojom::CTLogInfo::New();
  // Shift to ASCII 'A' (0x41)
  log_info->public_key = std::string(4, 0x41);
  log_info->name = std::string(4, 0x41);
  log_info->current_operator = "Changed Operator";
  for (int i = 0; i < 3; i++) {
    network::mojom::PreviousOperatorEntryPtr previous_operator =
        network::mojom::PreviousOperatorEntry::New();
    previous_operator->name = "Operator " + base::NumberToString(i);
    previous_operator->end_time = base::Time::FromTimeT(i);
    log_info->previous_operators.push_back(std::move(previous_operator));
  }
  log_list_mojo.push_back(std::move(log_info));

  base::RunLoop run_loop;
  cv_service_factory_remote->UpdateCtLogList(
      std::move(log_list_mojo), base::Time::Now(), run_loop.QuitClosure());
  run_loop.Run();

  net::CTPolicyEnforcer* request_enforcer =
      cv_service_factory_impl.get_impl_params().ct_policy_enforcer.get();
  ASSERT_TRUE(request_enforcer);

  certificate_transparency::ChromeCTPolicyEnforcer* policy_enforcer =
      reinterpret_cast<certificate_transparency::ChromeCTPolicyEnforcer*>(
          request_enforcer);

  std::map<std::string, certificate_transparency::LogInfo> log_info_map =
      policy_enforcer->log_info_for_testing();

  EXPECT_EQ(log_info_map[crypto::SHA256HashString("0000")]
                .operator_history.current_operator_,
            "Forever Operator");
  EXPECT_TRUE(log_info_map[crypto::SHA256HashString("0000")]
                  .operator_history.previous_operators_.empty());

  EXPECT_EQ(log_info_map[crypto::SHA256HashString("AAAA")]
                .operator_history.current_operator_,
            "Changed Operator");
  EXPECT_THAT(log_info_map[crypto::SHA256HashString("AAAA")]
                  .operator_history.previous_operators_,
              ::testing::ElementsAre(
                  ::testing::Pair("Operator 0", base::Time::FromTimeT(0)),
                  ::testing::Pair("Operator 1", base::Time::FromTimeT(1)),
                  ::testing::Pair("Operator 2", base::Time::FromTimeT(2))));
}
#endif

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
#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
    // On CHROME_ROOT_STORE_OPTIONAL platforms, the tests set
    // use_chrome_root_store=true, so the tests will also work on those
    // platforms.
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
  base::span<const uint8_t> root1_bytes = net::x509_util::CryptoBufferAsSpan(
      root1->GetX509Certificate()->cert_buffer());
  cv_creation_params->initial_additional_certificates->trust_anchors.push_back(
      std::vector(root1_bytes.begin(), root1_bytes.end()));

  base::span<const uint8_t> intermediate1_bytes =
      net::x509_util::CryptoBufferAsSpan(
          intermediate1->GetX509Certificate()->cert_buffer());
  cv_creation_params->initial_additional_certificates->all_certificates
      .push_back(
          std::vector(intermediate1_bytes.begin(), intermediate1_bytes.end()));

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
  base::span<const uint8_t> root2_bytes = net::x509_util::CryptoBufferAsSpan(
      root2->GetX509Certificate()->cert_buffer());
  new_additional_certificates->trust_anchors.push_back(
      std::vector<uint8_t>(root2_bytes.begin(), root2_bytes.end()));

  base::span<const uint8_t> intermediate2_bytes =
      net::x509_util::CryptoBufferAsSpan(
          intermediate2->GetX509Certificate()->cert_buffer());
  new_additional_certificates->all_certificates.push_back(std::vector<uint8_t>(
      intermediate2_bytes.begin(), intermediate2_bytes.end()));
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

// Tests that UpdateNetworkTime being called causes new cert verifiers to use
// the time tracker time for verification.
TEST_F(CertVerifierServiceFactoryBuiltinVerifierTest,
       UpdateNetworkTimeNewVerifier) {
  auto [leaf, root] = net::CertBuilder::CreateSimpleChain2();
  base::Time now = base::Time::Now();
  base::TimeTicks ticks_now = base::TimeTicks::Now();
  //  Configure the leaf certificate so it is no longer valid according to the
  //  system time.
  leaf->SetValidity(now - base::Days(3), now - base::Days(1));
  leaf->SetSubjectAltName("host.test");
  net::ScopedTestRoot scoped_test_root(root->GetX509Certificate());

  mojo::Remote<mojom::CertVerifierServiceFactory> cv_service_factory_remote;
  CertVerifierServiceFactoryImpl cv_service_factory_impl(
      cv_service_factory_remote.BindNewPipeAndPassReceiver());
  EnableChromeRootStoreIfOptional(&cv_service_factory_impl);

  // Update the time tracker so the current time is within the certificate
  // validity range.
  cv_service_factory_impl.UpdateNetworkTime(now, ticks_now,
                                            now - base::Days(2));

  mojo::Remote<mojom::CertVerifierService> cv_service_remote;
  DummyCVServiceClient cv_service_client;
  mojom::CertVerifierCreationParamsPtr cv_creation_params =
      mojom::CertVerifierCreationParams::New();

  // Create the cert verifier. It should start with the time tracker set to the
  // time passed to UpdateNetworkTime.
  cv_service_factory_remote->GetNewCertVerifier(
      cv_service_remote.BindNewPipeAndPassReceiver(),
      /*updater=*/mojo::NullReceiver(),
      cv_service_client.client_.BindNewPipeAndPassRemote(),
      std::move(cv_creation_params));

  auto [net_error, result] =
      Verify(cv_service_remote, leaf->GetX509Certificate(), "host.test");
  EXPECT_THAT(net_error, IsError(net::OK));
  EXPECT_FALSE(net::IsCertStatusError(result.cert_status));

  // Update happened before the CertVerifier was created, no change observers
  // should have been notified.
  EXPECT_EQ(cv_service_client.changed_count_, 0u);
}

// Tests that UpdateNetworkTime being called causes existing cert verifiers to
// use the time tracker time for verification.
TEST_F(CertVerifierServiceFactoryBuiltinVerifierTest,
       UpdateNetworkTimeExistingVerifier) {
  auto [leaf, root] = net::CertBuilder::CreateSimpleChain2();
  base::Time now = base::Time::Now();
  base::TimeTicks ticks_now = base::TimeTicks::Now();
  //  Configure the leaf certificate so it is no longer valid according to the
  //  system time.
  leaf->SetValidity(now - base::Days(3), now - base::Days(1));
  leaf->SetSubjectAltName("host.test");
  net::ScopedTestRoot scoped_test_root(root->GetX509Certificate());

  mojo::Remote<mojom::CertVerifierServiceFactory> cv_service_factory_remote;
  CertVerifierServiceFactoryImpl cv_service_factory_impl(
      cv_service_factory_remote.BindNewPipeAndPassReceiver());
  EnableChromeRootStoreIfOptional(&cv_service_factory_impl);

  mojo::Remote<mojom::CertVerifierService> cv_service_remote;
  DummyCVServiceClient cv_service_client;
  mojom::CertVerifierCreationParamsPtr cv_creation_params =
      mojom::CertVerifierCreationParams::New();

  // Create the cert verifier. Request should fail since time hasn't been
  // updated yet.
  cv_service_factory_remote->GetNewCertVerifier(
      cv_service_remote.BindNewPipeAndPassReceiver(),
      /*updater=*/mojo::NullReceiver(),
      cv_service_client.client_.BindNewPipeAndPassRemote(),
      std::move(cv_creation_params));
  {
    auto [net_error, result] =
        Verify(cv_service_remote, leaf->GetX509Certificate(), "host.test");
    EXPECT_THAT(net_error, IsError(net::ERR_CERT_DATE_INVALID));
    EXPECT_TRUE(net::IsCertStatusError(result.cert_status));
  }

  // No updates should have happened yet.
  EXPECT_EQ(cv_service_client.changed_count_, 0u);

  // Update the time tracker so the current time is within the certificate
  // validity range.
  cv_service_factory_impl.UpdateNetworkTime(now, ticks_now,
                                            now - base::Days(2));

  // Update should have been notified.
  EXPECT_NO_FATAL_FAILURE(cv_service_client.WaitForCertVerifierChange(1u));

  // Try request again and it should succeed.
  {
    auto [net_error, result] =
        Verify(cv_service_remote, leaf->GetX509Certificate(), "host.test");
    EXPECT_THAT(net_error, IsError(net::OK));
    EXPECT_FALSE(net::IsCertStatusError(result.cert_status));
  }
}

}  // namespace cert_verifier
