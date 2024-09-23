// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_proc_builtin.h"

#include <optional>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/network_time/time_tracker/time_tracker.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/crl_set.h"
#include "net/cert/do_nothing_ct_verifier.h"
#include "net/cert/ev_root_ca_metadata.h"
#include "net/cert/internal/system_trust_store.h"
#include "net/cert/sct_status_flags.h"
#include "net/cert/time_conversions.h"
#include "net/cert/x509_util.h"
#include "net/cert_net/cert_net_fetcher_url_request.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/test/cert_builder.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/test/gtest_util.h"
#include "net/test/revocation_builder.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/pki/trust_store.h"
#include "third_party/boringssl/src/pki/trust_store_collection.h"
#include "third_party/boringssl/src/pki/trust_store_in_memory.h"

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
#include "base/version_info/version_info.h"  // nogncheck
#endif

using net::test::IsError;
using net::test::IsOk;

using testing::_;

namespace net {

namespace {

std::unique_ptr<test_server::HttpResponse> HangRequestAndCallback(
    base::OnceClosure callback,
    const test_server::HttpRequest& request) {
  std::move(callback).Run();
  return std::make_unique<test_server::HungResponse>();
}

void FailTest(const std::string& message) {
  ADD_FAILURE() << message;
}

std::unique_ptr<test_server::HttpResponse> FailRequestAndFailTest(
    const std::string& message,
    scoped_refptr<base::TaskRunner> main_task_runner,
    const test_server::HttpRequest& request) {
  main_task_runner->PostTask(FROM_HERE, base::BindOnce(FailTest, message));
  auto response = std::make_unique<test_server::BasicHttpResponse>();
  response->set_code(HTTP_NOT_ACCEPTABLE);
  return response;
}

std::unique_ptr<test_server::HttpResponse> ServeResponse(
    HttpStatusCode status_code,
    const std::string& content_type,
    const std::string& content,
    const test_server::HttpRequest& request) {
  auto http_response = std::make_unique<test_server::BasicHttpResponse>();

  http_response->set_code(status_code);
  http_response->set_content_type(content_type);
  http_response->set_content(content);
  return http_response;
}

std::string MakeRandomHexString(size_t num_bytes) {
  std::vector<uint8_t> rand_bytes(num_bytes);
  base::RandBytes(rand_bytes);
  return base::HexEncode(rand_bytes);
}

static std::string MakeRandomPath(std::string_view suffix) {
  return "/" + MakeRandomHexString(12) + std::string(suffix);
}

int VerifyOnWorkerThread(const scoped_refptr<CertVerifyProc>& verify_proc,
                         scoped_refptr<X509Certificate> cert,
                         const std::string& hostname,
                         const std::string& ocsp_response,
                         const std::string& sct_list,
                         int flags,
                         CertVerifyResult* verify_result,
                         NetLogSource* out_source) {
  base::ScopedAllowBaseSyncPrimitivesForTesting scoped_allow_blocking;
  NetLogWithSource net_log(NetLogWithSource::Make(
      net::NetLog::Get(), net::NetLogSourceType::CERT_VERIFIER_TASK));
  int error = verify_proc->Verify(cert.get(), hostname, ocsp_response, sct_list,
                                  flags, verify_result, net_log);
  *out_source = net_log.source();
  return error;
}

class MockSystemTrustStore : public SystemTrustStore {
 public:
  bssl::TrustStore* GetTrustStore() override { return &trust_store_; }

  bool IsKnownRoot(const bssl::ParsedCertificate* trust_anchor) const override {
    return mock_is_known_root_;
  }

  void AddTrustStore(bssl::TrustStore* store) {
    trust_store_.AddTrustStore(store);
  }

  void SetMockIsKnownRoot(bool is_known_root) {
    mock_is_known_root_ = is_known_root;
  }

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  net::PlatformTrustStore* GetPlatformTrustStore() override { return nullptr; }

  void SetMockIsLocallyTrustedRoot(bool is_locally_trusted_root) {
    mock_is_locally_trusted_root_ = is_locally_trusted_root;
  }

  bool IsLocallyTrustedRoot(
      const bssl::ParsedCertificate* trust_anchor) override {
    return mock_is_locally_trusted_root_;
  }

  int64_t chrome_root_store_version() const override { return 0; }

  base::span<const ChromeRootCertConstraints> GetChromeRootConstraints(
      const bssl::ParsedCertificate* cert) const override {
    return mock_chrome_root_constraints_;
  }

  void SetMockChromeRootConstraints(
      std::vector<StaticChromeRootCertConstraints> chrome_root_constraints) {
    mock_chrome_root_constraints_.clear();
    for (const auto& constraint : chrome_root_constraints) {
      mock_chrome_root_constraints_.emplace_back(constraint);
    }
  }
#endif

 private:
  bssl::TrustStoreCollection trust_store_;
  bool mock_is_known_root_ = false;
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  bool mock_is_locally_trusted_root_ = false;
  std::vector<ChromeRootCertConstraints> mock_chrome_root_constraints_;
#endif
};

class BlockingTrustStore : public bssl::TrustStore {
 public:
  bssl::CertificateTrust GetTrust(
      const bssl::ParsedCertificate* cert) override {
    return backing_trust_store_.GetTrust(cert);
  }

  void SyncGetIssuersOf(const bssl::ParsedCertificate* cert,
                        bssl::ParsedCertificateList* issuers) override {
    sync_get_issuer_started_event_.Signal();
    sync_get_issuer_ok_to_finish_event_.Wait();

    backing_trust_store_.SyncGetIssuersOf(cert, issuers);
  }

  base::WaitableEvent sync_get_issuer_started_event_;
  base::WaitableEvent sync_get_issuer_ok_to_finish_event_;
  bssl::TrustStoreInMemory backing_trust_store_;
};

class MockCTVerifier : public CTVerifier {
 public:
  MOCK_CONST_METHOD6(Verify,
                     void(X509Certificate*,
                          std::string_view,
                          std::string_view,
                          base::Time current_time,
                          SignedCertificateTimestampAndStatusList*,
                          const NetLogWithSource&));
};

class MockCTPolicyEnforcer : public CTPolicyEnforcer {
 public:
  MOCK_CONST_METHOD4(CheckCompliance,
                     ct::CTPolicyCompliance(X509Certificate* cert,
                                            const ct::SCTList&,
                                            base::Time,
                                            const NetLogWithSource&));
  MOCK_CONST_METHOD1(GetLogDisqualificationTime,
                     std::optional<base::Time>(std::string_view log_id));
  MOCK_CONST_METHOD0(IsCtEnabled, bool());

 protected:
  ~MockCTPolicyEnforcer() override = default;
};

}  // namespace

class CertVerifyProcBuiltinTest : public ::testing::Test {
 public:
  void SetUp() override {
    cert_net_fetcher_ = base::MakeRefCounted<CertNetFetcherURLRequest>();

    InitializeVerifyProc(CreateParams({}));

    context_ = CreateTestURLRequestContextBuilder()->Build();

    cert_net_fetcher_->SetURLRequestContext(context_.get());
  }

  void TearDown() override { cert_net_fetcher_->Shutdown(); }

  CertVerifyProc::InstanceParams CreateParams(
      const CertificateList& additional_trust_anchors,
      const CertificateList&
          additional_trust_anchors_with_enforced_constraints = {},
      const CertificateList& additional_distrusted_certificates = {}) {
    CertVerifyProc::InstanceParams instance_params;
    instance_params.additional_trust_anchors =
        net::x509_util::ParseAllValidCerts(additional_trust_anchors);
    instance_params.additional_trust_anchors_with_enforced_constraints =
        net::x509_util::ParseAllValidCerts(
            additional_trust_anchors_with_enforced_constraints);
    std::vector<std::vector<uint8_t>> distrusted_spkis;
    for (const auto& x509_cert : additional_distrusted_certificates) {
      std::shared_ptr<const bssl::ParsedCertificate> cert =
          bssl::ParsedCertificate::Create(
              bssl::UpRef(x509_cert->cert_buffer()),
              net::x509_util::DefaultParseCertificateOptions(),
              /*errors=*/nullptr);
      EXPECT_TRUE(cert);
      std::string spki_string = cert->tbs().spki_tlv.AsString();
      distrusted_spkis.push_back(
          std::vector<uint8_t>(spki_string.begin(), spki_string.end()));
    }
    instance_params.additional_distrusted_spkis = distrusted_spkis;
    return instance_params;
  }

  void InitializeVerifyProc(
      const CertVerifyProc::InstanceParams& instance_params,
      std::optional<base::Time> current_time = std::nullopt) {
    auto mock_system_trust_store = std::make_unique<MockSystemTrustStore>();
    mock_system_trust_store_ = mock_system_trust_store.get();
    auto mock_ct_verifier = std::make_unique<MockCTVerifier>();
    mock_ct_verifier_ = mock_ct_verifier.get();
    mock_ct_policy_enforcer_ = base::MakeRefCounted<MockCTPolicyEnforcer>();
    std::optional<network_time::TimeTracker> time_tracker;
    if (current_time.has_value()) {
      time_tracker =
          network_time::TimeTracker(base::Time::Now(), base::TimeTicks::Now(),
                                    current_time.value(), base::TimeDelta());
    }
    verify_proc_ = CreateCertVerifyProcBuiltin(
        cert_net_fetcher_, CRLSet::EmptyCRLSetForTesting(),
        std::move(mock_ct_verifier), mock_ct_policy_enforcer_,
        std::move(mock_system_trust_store), instance_params, time_tracker);
  }

  void Verify(scoped_refptr<X509Certificate> cert,
              const std::string& hostname,
              int flags,
              CertVerifyResult* verify_result,
              NetLogSource* out_source,
              CompletionOnceCallback callback) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(
            &VerifyOnWorkerThread, verify_proc_, std::move(cert), hostname,
            /*ocsp_response=*/std::string(),
            /*sct_list=*/std::string(), flags, verify_result, out_source),
        std::move(callback));
  }

  void Verify(scoped_refptr<X509Certificate> cert,
              const std::string& hostname,
              const std::string& ocsp_response,
              const std::string& sct_list,
              int flags,
              CertVerifyResult* verify_result,
              NetLogSource* out_source,
              CompletionOnceCallback callback) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&VerifyOnWorkerThread, verify_proc_, std::move(cert),
                       hostname, ocsp_response, sct_list, flags, verify_result,
                       out_source),
        std::move(callback));
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  // Creates a CRL issued and signed by |crl_issuer|, marking |revoked_serials|
  // as revoked, and registers it to be served by the test server.
  // Returns the full URL to retrieve the CRL from the test server.
  GURL CreateAndServeCrl(EmbeddedTestServer* test_server,
                         CertBuilder* crl_issuer,
                         const std::vector<uint64_t>& revoked_serials,
                         std::optional<bssl::SignatureAlgorithm>
                             signature_algorithm = std::nullopt) {
    std::string crl = BuildCrl(crl_issuer->GetSubject(), crl_issuer->GetKey(),
                               revoked_serials, signature_algorithm);
    std::string crl_path = MakeRandomPath(".crl");
    test_server->RegisterRequestHandler(
        base::BindRepeating(&test_server::HandlePrefixedRequest, crl_path,
                            base::BindRepeating(ServeResponse, HTTP_OK,
                                                "application/pkix-crl", crl)));
    return test_server->GetURL(crl_path);
  }

  void AddTrustStore(bssl::TrustStore* store) {
    mock_system_trust_store_->AddTrustStore(store);
  }

  void SetMockIsKnownRoot(bool is_known_root) {
    mock_system_trust_store_->SetMockIsKnownRoot(is_known_root);
  }

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  void SetMockIsLocallyTrustedRoot(bool is_locally_trusted_root) {
    mock_system_trust_store_->SetMockIsLocallyTrustedRoot(
        is_locally_trusted_root);
  }

  void SetMockChromeRootConstraints(
      std::vector<StaticChromeRootCertConstraints> chrome_root_constraints) {
    mock_system_trust_store_->SetMockChromeRootConstraints(
        std::move(chrome_root_constraints));
  }
#endif

  net::URLRequestContext* context() { return context_.get(); }

  MockCTVerifier* mock_ct_verifier() { return mock_ct_verifier_; }
  MockCTPolicyEnforcer* mock_ct_policy_enforcer() {
    return mock_ct_policy_enforcer_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::MainThreadType::IO,
  };

  CertVerifier::Config config_;
  std::unique_ptr<net::URLRequestContext> context_;

  // Must outlive `mock_ct_verifier_` and `mock_system_trust_store_`.
  scoped_refptr<CertVerifyProc> verify_proc_;

  raw_ptr<MockCTVerifier> mock_ct_verifier_ = nullptr;
  scoped_refptr<MockCTPolicyEnforcer> mock_ct_policy_enforcer_;
  raw_ptr<MockSystemTrustStore> mock_system_trust_store_ = nullptr;
  scoped_refptr<CertNetFetcherURLRequest> cert_net_fetcher_;
};

TEST_F(CertVerifyProcBuiltinTest, ShouldBypassHSTS) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  InitializeVerifyProc(CreateParams(
      /*additional_trust_anchors=*/{root->GetX509Certificate()}));

  EmbeddedTestServer test_server(EmbeddedTestServer::TYPE_HTTP);
  ASSERT_TRUE(test_server.InitializeAndListen());

  // CRL that marks leaf as revoked.
  leaf->SetCrlDistributionPointUrl(
      CreateAndServeCrl(&test_server, root.get(), {leaf->GetSerialNumber()}));

  test_server.StartAcceptingConnections();

  {
    scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
    ASSERT_TRUE(chain.get());

    NetLogSource verify_net_log_source;
    CertVerifyResult verify_result;
    TestCompletionCallback verify_callback;
    // Ensure HSTS upgrades for the domain which hosts the CRLs.
    context()->transport_security_state()->AddHSTS(
        test_server.base_url().host(), base::Time::Now() + base::Seconds(30),
        /*include_subdomains=*/true);
    ASSERT_TRUE(context()->transport_security_state()->ShouldUpgradeToSSL(
        test_server.base_url().host()));
    Verify(chain.get(), "www.example.com",
           CertVerifyProc::VERIFY_REV_CHECKING_ENABLED,
           &verify_result, &verify_net_log_source, verify_callback.callback());

    int error = verify_callback.WaitForResult();
    EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));
    EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
  }
}

TEST_F(CertVerifyProcBuiltinTest, SimpleSuccess) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  InitializeVerifyProc(CreateParams(
      /*additional_trust_anchors=*/{root->GetX509Certificate()}));

  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", /*flags=*/0, &verify_result,
         &verify_net_log_source, callback.callback());

  int error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());
}

TEST_F(CertVerifyProcBuiltinTest, CallsCtVerifierAndReturnsSctStatus) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  InitializeVerifyProc(CreateParams(
      /*additional_trust_anchors=*/{root->GetX509Certificate()}));

  const std::string kOcspResponse = "OCSP response";
  const std::string kSctList = "SCT list";
  const std::string kLogId = "CT log id";
  const ct::SCTVerifyStatus kSctVerifyStatus = ct::SCT_STATUS_LOG_UNKNOWN;

  SignedCertificateTimestampAndStatus sct_and_status;
  sct_and_status.sct = base::MakeRefCounted<ct::SignedCertificateTimestamp>();
  sct_and_status.sct->log_id = kLogId;
  sct_and_status.status = kSctVerifyStatus;
  SignedCertificateTimestampAndStatusList sct_and_status_list;
  sct_and_status_list.push_back(sct_and_status);
  EXPECT_CALL(*mock_ct_verifier(), Verify(_, kOcspResponse, kSctList, _, _, _))
      .WillOnce(testing::SetArgPointee<4>(sct_and_status_list));
  EXPECT_CALL(*mock_ct_policy_enforcer(), CheckCompliance(_, _, _, _))
      .WillRepeatedly(
          testing::Return(ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS));

  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", kOcspResponse, kSctList, /*flags=*/0,
         &verify_result, &verify_net_log_source, callback.callback());

  int error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());
  ASSERT_EQ(verify_result.scts.size(), 1u);
  EXPECT_EQ(verify_result.scts.front().status, kSctVerifyStatus);
  EXPECT_EQ(verify_result.scts.front().sct->log_id, kLogId);
  EXPECT_EQ(verify_result.policy_compliance,
            ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS);
}

#if defined(PLATFORM_USES_CHROMIUM_EV_METADATA)
TEST_F(CertVerifyProcBuiltinTest, EVCertStatusMaintainedForCompliantCert) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();

  static const char kEVTestCertPolicy[] = "1.2.3.4";
  leaf->SetCertificatePolicies({kEVTestCertPolicy});
  ScopedTestEVPolicy scoped_test_ev_policy(
      EVRootCAMetadata::GetInstance(),
      X509Certificate::CalculateFingerprint256(root->GetCertBuffer()),
      kEVTestCertPolicy);
  InitializeVerifyProc(CreateParams(
      /*additional_trust_anchors=*/{root->GetX509Certificate()}));

  EXPECT_CALL(*mock_ct_verifier(), Verify(_, _, _, _, _, _));
  EXPECT_CALL(*mock_ct_policy_enforcer(), CheckCompliance(_, _, _, _))
      .WillRepeatedly(
          testing::Return(ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS));

  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", /*flags=*/0, &verify_result,
         &verify_net_log_source, callback.callback());

  int error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());
  EXPECT_EQ(verify_result.policy_compliance,
            ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS);
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_IS_EV);
}
#endif

TEST_F(CertVerifyProcBuiltinTest, DistrustedIntermediate) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  InitializeVerifyProc(CreateParams(
      /*additional_trust_anchors=*/{root->GetX509Certificate()},
      /*additional_trust_anchors_with_enforced_constraints=*/{},
      /*additional_distrusted_certificates=*/
      {intermediate->GetX509Certificate()}));

  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", /*flags=*/0, &verify_result,
         &verify_net_log_source, callback.callback());

  int error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  EXPECT_EQ(1u, verify_result.verified_cert->intermediate_buffers().size());
}

TEST_F(CertVerifyProcBuiltinTest, AddedRootWithConstraints) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  root->SetNameConstraintsDnsNames(/*permitted_dns_names=*/{"example.org"},
                                   /*excluded_dns_names=*/{});
  InitializeVerifyProc(CreateParams(
      /*additional_trust_anchors=*/{},
      /*additional_trust_anchors_with_enforced_constraints=*/
      {root->GetX509Certificate()},
      /*additional_distrusted_certificates=*/{}));

  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", /*flags=*/0, &verify_result,
         &verify_net_log_source, callback.callback());

  int error = callback.WaitForResult();
  // Doesn't chain back to any valid root.
  EXPECT_THAT(error, IsError(ERR_CERT_INVALID));
}

TEST_F(CertVerifyProcBuiltinTest, AddedRootWithConstraintsNotEnforced) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  root->SetNameConstraintsDnsNames(/*permitted_dns_names=*/{"example.org"},
                                   /*excluded_dns_names=*/{});
  InitializeVerifyProc(CreateParams(
      /*additional_trust_anchors=*/{root->GetX509Certificate()},
      /*additional_trust_anchors_with_enforced_constraints=*/{},
      /*additional_distrusted_certificates=*/{}));

  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", /*flags=*/0, &verify_result,
         &verify_net_log_source, callback.callback());

  int error = callback.WaitForResult();
  // Constraint isn't enforced.
  EXPECT_THAT(error, IsOk());
}

TEST_F(CertVerifyProcBuiltinTest, AddedRootWithOutsideDNSConstraints) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  CertVerifyProc::InstanceParams instance_params;

  std::shared_ptr<const bssl::ParsedCertificate> root_cert =
      bssl::ParsedCertificate::Create(
          bssl::UpRef(root->GetX509Certificate()->cert_buffer()),
          net::x509_util::DefaultParseCertificateOptions(), nullptr);
  ASSERT_TRUE(root_cert);
  CertVerifyProc::CertificateWithConstraints cert_with_constraints;
  cert_with_constraints.certificate = std::move(root_cert);
  cert_with_constraints.permitted_dns_names.push_back("example.com");

  instance_params.additional_trust_anchors_with_constraints.push_back(
      cert_with_constraints);

  InitializeVerifyProc(instance_params);

  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", /*flags=*/0, &verify_result,
         &verify_net_log_source, callback.callback());

  int error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());
}

TEST_F(CertVerifyProcBuiltinTest,
       AddedRootWithOutsideDNSConstraintsNotMatched) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  CertVerifyProc::InstanceParams instance_params;

  std::shared_ptr<const bssl::ParsedCertificate> root_cert =
      bssl::ParsedCertificate::Create(
          bssl::UpRef(root->GetX509Certificate()->cert_buffer()),
          net::x509_util::DefaultParseCertificateOptions(), nullptr);
  ASSERT_TRUE(root_cert);
  CertVerifyProc::CertificateWithConstraints cert_with_constraints;
  cert_with_constraints.certificate = std::move(root_cert);
  cert_with_constraints.permitted_dns_names.push_back("foobar.com");

  instance_params.additional_trust_anchors_with_constraints.push_back(
      cert_with_constraints);

  InitializeVerifyProc(instance_params);

  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());
  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", /*flags=*/0, &verify_result,
         &verify_net_log_source, callback.callback());

  int error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(ERR_CERT_INVALID));
}

TEST_F(CertVerifyProcBuiltinTest, AddedRootWithOutsideCIDRConstraints) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  CertVerifyProc::InstanceParams instance_params;

  std::shared_ptr<const bssl::ParsedCertificate> root_cert =
      bssl::ParsedCertificate::Create(
          bssl::UpRef(root->GetX509Certificate()->cert_buffer()),
          net::x509_util::DefaultParseCertificateOptions(), nullptr);
  ASSERT_TRUE(root_cert);
  CertVerifyProc::CertificateWithConstraints cert_with_constraints;
  cert_with_constraints.certificate = std::move(root_cert);
  cert_with_constraints.permitted_cidrs.push_back(
      {net::IPAddress(192, 168, 1, 104), net::IPAddress(255, 255, 255, 0)});

  instance_params.additional_trust_anchors_with_constraints.push_back(
      cert_with_constraints);

  InitializeVerifyProc(instance_params);

  leaf->SetSubjectAltNames(/*dns_names=*/{"www.example.com"},
                           /*ip_addresses=*/{net::IPAddress(192, 168, 1, 254)});
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", /*flags=*/0, &verify_result,
         &verify_net_log_source, callback.callback());

  int error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());
}

TEST_F(CertVerifyProcBuiltinTest,
       AddedRootWithOutsideCIDRConstraintsNotMatched) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  CertVerifyProc::InstanceParams instance_params = CreateParams({});

  std::shared_ptr<const bssl::ParsedCertificate> root_cert =
      bssl::ParsedCertificate::Create(
          bssl::UpRef(root->GetX509Certificate()->cert_buffer()),
          net::x509_util::DefaultParseCertificateOptions(), nullptr);
  ASSERT_TRUE(root_cert);
  CertVerifyProc::CertificateWithConstraints cert_with_constraints;
  cert_with_constraints.certificate = std::move(root_cert);
  cert_with_constraints.permitted_cidrs.push_back(
      {net::IPAddress(192, 168, 1, 1), net::IPAddress(255, 255, 255, 0)});

  instance_params.additional_trust_anchors_with_constraints.push_back(
      cert_with_constraints);

  InitializeVerifyProc(instance_params);

  leaf->SetSubjectAltNames(/*dns_names=*/{"www.example.com"},
                           /*ip_addresses=*/{net::IPAddress(10, 2, 2, 2)});
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", /*flags=*/0, &verify_result,
         &verify_net_log_source, callback.callback());

  int error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(ERR_CERT_INVALID));
}

TEST_F(CertVerifyProcBuiltinTest, AddedRootWithBadTime) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  root->SetValidity(/*not_before=*/base::Time::Now() - base::Days(10),
                    /*not_after=*/base::Time::Now() - base::Days(5));
  InitializeVerifyProc(CreateParams(
      /*additional_trust_anchors=*/{},
      /*additional_trust_anchors_with_enforced_constraints=*/
      {root->GetX509Certificate()},
      /*additional_distrusted_certificates=*/{}));

  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", /*flags=*/0, &verify_result,
         &verify_net_log_source, callback.callback());

  int error = callback.WaitForResult();
  // Root is valid but expired and we check it.
  EXPECT_THAT(error, IsError(ERR_CERT_DATE_INVALID));
}

TEST_F(CertVerifyProcBuiltinTest, AddedRootWithBadTimeButNotEnforced) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  root->SetValidity(/*not_before=*/base::Time::Now() - base::Days(10),
                    /*not_after=*/base::Time::Now() - base::Days(5));
  InitializeVerifyProc(CreateParams(
      /*additional_trust_anchors=*/{root->GetX509Certificate()},
      /*additional_trust_anchors_with_enforced_constraints=*/{},
      /*additional_distrusted_certificates=*/{}));

  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", /*flags=*/0, &verify_result,
         &verify_net_log_source, callback.callback());

  int error = callback.WaitForResult();
  // Root is valid but expired, but we don't check it.
  EXPECT_THAT(error, IsOk());
}

TEST_F(CertVerifyProcBuiltinTest, TimeTracker) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  root->SetValidity(/*not_before=*/base::Time::Now() - base::Days(10),
                    /*not_after=*/base::Time::Now() - base::Days(5));
  InitializeVerifyProc(
      CreateParams(
          /*additional_trust_anchors=*/{},
          /*additional_trust_anchors_with_enforced_constraints=*/
          {root->GetX509Certificate()},
          /*additional_distrusted_certificates=*/{}),
      base::Time::Now() - base::Days(7));

  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", /*flags=*/0, &verify_result,
         &verify_net_log_source, callback.callback());

  int error = callback.WaitForResult();
  // Root is expired when compared to base::Time::Now, but is valid in the
  // time provided by the time tracker.
  EXPECT_THAT(error, IsOk());
}

TEST_F(CertVerifyProcBuiltinTest, TimeTrackerFailureIsRetriedWithSystemTime) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  root->SetValidity(/*not_before=*/base::Time::Now() - base::Days(10),
                    /*not_after=*/base::Time::Now() + base::Days(10));
  InitializeVerifyProc(
      CreateParams(
          /*additional_trust_anchors=*/{},
          /*additional_trust_anchors_with_enforced_constraints=*/
          {root->GetX509Certificate()},
          /*additional_distrusted_certificates=*/{}),
      base::Time::Now() + base::Days(20));

  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", /*flags=*/0, &verify_result,
         &verify_net_log_source, callback.callback());

  int error = callback.WaitForResult();
  // Root is expired when compared to the time tracker time, but valid when
  // compared to base::Time::Now.
  EXPECT_THAT(error, IsOk());
}

TEST_F(CertVerifyProcBuiltinTest,
       TimeTrackerRevocationFailureIsRetriedWithSystemTime) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  root->SetValidity(/*not_before=*/base::Time::Now() - base::Days(3),
                    /*not_after=*/base::Time::Now() + base::Days(2));
  // The CRL DP sets its this_update time to base::Time::Now() - 1 day. Use two
  // days before now as the current time to cause checks to fail with
  // UNABLE_TO_CHECK_REVOCATION, which then should be retried with the system
  // time and succeed.
  InitializeVerifyProc(
      CreateParams(
          /*additional_trust_anchors=*/{},
          /*additional_trust_anchors_with_enforced_constraints=*/
          {root->GetX509Certificate()},
          /*additional_distrusted_certificates=*/{}),
      base::Time::Now() - base::Days(2));

  EmbeddedTestServer test_server(EmbeddedTestServer::TYPE_HTTP);
  ASSERT_TRUE(test_server.InitializeAndListen());
  // Valid CRL that does not mark the leaf as revoked.
  leaf->SetCrlDistributionPointUrl(
      CreateAndServeCrl(&test_server, root.get(), {1234}));
  test_server.StartAcceptingConnections();

  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com",
         CertVerifyProc::VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS,
         &verify_result, &verify_net_log_source, callback.callback());

  int error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());
}

TEST_F(CertVerifyProcBuiltinTest, CRLNotCheckedForKnownRoots) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  InitializeVerifyProc(CreateParams(
      /*additional_trust_anchors=*/{root->GetX509Certificate()}));

  EmbeddedTestServer test_server(EmbeddedTestServer::TYPE_HTTP);
  ASSERT_TRUE(test_server.InitializeAndListen());

  // CRL that marks leaf as revoked.
  leaf->SetCrlDistributionPointUrl(
      CreateAndServeCrl(&test_server, root.get(), {leaf->GetSerialNumber()}));

  test_server.StartAcceptingConnections();

  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  NetLogSource verify_net_log_source;

  {
    CertVerifyResult verify_result;
    TestCompletionCallback verify_callback;
    Verify(chain.get(), "www.example.com",
           CertVerifyProc::VERIFY_REV_CHECKING_ENABLED,
           &verify_result, &verify_net_log_source, verify_callback.callback());

    int error = verify_callback.WaitForResult();
    EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));
    EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
  }

  {
    // Pretend the root is a known root.
    SetMockIsKnownRoot(true);
    CertVerifyResult verify_result;
    TestCompletionCallback verify_callback;
    Verify(chain.get(), "www.example.com",
           CertVerifyProc::VERIFY_REV_CHECKING_ENABLED,
           &verify_result, &verify_net_log_source, verify_callback.callback());

    int error = verify_callback.WaitForResult();
    // CRLs are not checked for chains issued by known roots, so verification
    // should be successful.
    EXPECT_THAT(error, IsOk());
    EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
  }
}

// Tests that if the verification deadline is exceeded during revocation
// checking, additional CRL fetches will not be attempted.
TEST_F(CertVerifyProcBuiltinTest, RevocationCheckDeadlineCRL) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  InitializeVerifyProc(CreateParams(
      /*additional_trust_anchors=*/{root->GetX509Certificate()}));

  const base::TimeDelta timeout_increment =
      CertNetFetcherURLRequest::GetDefaultTimeoutForTesting() +
      base::Milliseconds(1);
  const int expected_request_count =
      base::ClampFloor(GetCertVerifyProcBuiltinTimeLimitForTesting() /
                       timeout_increment) +
      1;

  EmbeddedTestServer test_server(EmbeddedTestServer::TYPE_HTTP);
  ASSERT_TRUE(test_server.InitializeAndListen());

  // Set up the test cert to have enough crlDistributionPoint urls that if the
  // first N-1 requests hang the deadline will be exceeded before the Nth
  // request is made.
  std::vector<GURL> crl_urls;
  std::vector<base::RunLoop> runloops(expected_request_count);
  for (int i = 0; i < expected_request_count; ++i) {
    std::string path = base::StringPrintf("/hung/%i", i);
    crl_urls.emplace_back(test_server.GetURL(path));
    test_server.RegisterRequestHandler(
        base::BindRepeating(&test_server::HandlePrefixedRequest, path,
                            base::BindRepeating(&HangRequestAndCallback,
                                                runloops[i].QuitClosure())));
  }
  // Add CRL URLs and handlers that will add test failures if requested.
  for (int i = expected_request_count; i < expected_request_count + 1; ++i) {
    std::string path = base::StringPrintf("/failtest/%i", i);
    crl_urls.emplace_back(test_server.GetURL(path));
    test_server.RegisterRequestHandler(base::BindRepeating(
        &test_server::HandlePrefixedRequest, path,
        base::BindRepeating(FailRequestAndFailTest,
                            "additional request made after deadline exceeded",
                            base::SequencedTaskRunner::GetCurrentDefault())));
  }
  leaf->SetCrlDistributionPointUrls(crl_urls);

  test_server.StartAcceptingConnections();

  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback verify_callback;
  Verify(chain.get(), "www.example.com",
         CertVerifyProc::VERIFY_REV_CHECKING_ENABLED,
         &verify_result, &verify_net_log_source, verify_callback.callback());

  for (int i = 0; i < expected_request_count; i++) {
    // Wait for request #|i| to be made.
    runloops[i].Run();
    // Advance virtual time to cause the timeout task to become runnable.
    task_environment().AdvanceClock(timeout_increment);
  }

  // Once |expected_request_count| requests have been made and timed out, the
  // overall deadline should be reached, and no more requests should have been
  // made. (If they were, the test will fail due to the ADD_FAILURE callback in
  // the request handlers.)
  int error = verify_callback.WaitForResult();
  // Soft-fail revocation checking was used, therefore verification result
  // should be OK even though none of the CRLs could be retrieved.
  EXPECT_THAT(error, IsOk());
}

// Tests that if the verification deadline is exceeded during revocation
// checking, additional OCSP fetches will not be attempted.
TEST_F(CertVerifyProcBuiltinTest, RevocationCheckDeadlineOCSP) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  InitializeVerifyProc(CreateParams(
      /*additional_trust_anchors=*/{root->GetX509Certificate()}));

  const base::TimeDelta timeout_increment =
      CertNetFetcherURLRequest::GetDefaultTimeoutForTesting() +
      base::Milliseconds(1);
  const int expected_request_count =
      base::ClampFloor(GetCertVerifyProcBuiltinTimeLimitForTesting() /
                       timeout_increment) +
      1;

  EmbeddedTestServer test_server(EmbeddedTestServer::TYPE_HTTP);
  ASSERT_TRUE(test_server.InitializeAndListen());

  // Set up the test cert to have enough OCSP urls that if the
  // first N-1 requests hang the deadline will be exceeded before the Nth
  // request is made.
  std::vector<GURL> ocsp_urls;
  std::vector<base::RunLoop> runloops(expected_request_count);
  for (int i = 0; i < expected_request_count; ++i) {
    std::string path = base::StringPrintf("/hung/%i", i);
    ocsp_urls.emplace_back(test_server.GetURL(path));
    test_server.RegisterRequestHandler(
        base::BindRepeating(&test_server::HandlePrefixedRequest, path,
                            base::BindRepeating(&HangRequestAndCallback,
                                                runloops[i].QuitClosure())));
  }
  // Add OCSP URLs and handlers that will add test failures if requested.
  for (int i = expected_request_count; i < expected_request_count + 1; ++i) {
    std::string path = base::StringPrintf("/failtest/%i", i);
    ocsp_urls.emplace_back(test_server.GetURL(path));
    test_server.RegisterRequestHandler(base::BindRepeating(
        &test_server::HandlePrefixedRequest, path,
        base::BindRepeating(FailRequestAndFailTest,
                            "additional request made after deadline exceeded",
                            base::SequencedTaskRunner::GetCurrentDefault())));
  }
  leaf->SetCaIssuersAndOCSPUrls({}, ocsp_urls);

  test_server.StartAcceptingConnections();

  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback verify_callback;
  Verify(chain.get(), "www.example.com",
         CertVerifyProc::VERIFY_REV_CHECKING_ENABLED,
         &verify_result, &verify_net_log_source, verify_callback.callback());

  for (int i = 0; i < expected_request_count; i++) {
    // Wait for request #|i| to be made.
    runloops[i].Run();
    // Advance virtual time to cause the timeout task to become runnable.
    task_environment().AdvanceClock(timeout_increment);
  }

  // Once |expected_request_count| requests have been made and timed out, the
  // overall deadline should be reached, and no more requests should have been
  // made. (If they were, the test will fail due to the ADD_FAILURE callback in
  // the request handlers.)
  int error = verify_callback.WaitForResult();
  // Soft-fail revocation checking was used, therefore verification result
  // should be OK even though none of the OCSP responses could be retrieved.
  EXPECT_THAT(error, IsOk());
}

#if defined(PLATFORM_USES_CHROMIUM_EV_METADATA)
// Tests that if we're doing EV verification, that no OCSP revocation checking
// is done.
TEST_F(CertVerifyProcBuiltinTest, EVNoOCSPRevocationChecks) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  InitializeVerifyProc(CreateParams(
      /*additional_trust_anchors=*/{root->GetX509Certificate()}));

  // Add test EV policy to leaf and intermediate.
  static const char kEVTestCertPolicy[] = "1.2.3.4";
  leaf->SetCertificatePolicies({kEVTestCertPolicy});
  intermediate->SetCertificatePolicies({kEVTestCertPolicy});

  EmbeddedTestServer test_server(EmbeddedTestServer::TYPE_HTTP);
  ASSERT_TRUE(test_server.InitializeAndListen());

  // Set up the test intermediate to have an OCSP url that fails the test if
  // called.
  std::vector<GURL> ocsp_urls;
  std::string path = "/failtest";
  ocsp_urls.emplace_back(test_server.GetURL(path));
  test_server.RegisterRequestHandler(base::BindRepeating(
      &test_server::HandlePrefixedRequest, path,
      base::BindRepeating(FailRequestAndFailTest,
                          "no OCSP requests should be sent",
                          base::SequencedTaskRunner::GetCurrentDefault())));
  intermediate->SetCaIssuersAndOCSPUrls({}, ocsp_urls);
  test_server.StartAcceptingConnections();

  // Consider the root of the test chain a valid EV root for the test policy.
  ScopedTestEVPolicy scoped_test_ev_policy(
      EVRootCAMetadata::GetInstance(),
      X509Certificate::CalculateFingerprint256(root->GetCertBuffer()),
      kEVTestCertPolicy);

  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  RecordingNetLogObserver net_log_observer(NetLogCaptureMode::kDefault);
  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback verify_callback;
  Verify(chain.get(), "www.example.com",
         /*flags=*/0,
         &verify_result, &verify_net_log_source, verify_callback.callback());

  // EV doesn't do revocation checking, therefore verification result
  // should be OK and EV.
  int error = verify_callback.WaitForResult();
  EXPECT_THAT(error, IsOk());
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_IS_EV);
  EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);

  auto events = net_log_observer.GetEntriesForSource(verify_net_log_source);

  auto event = base::ranges::find(
      events, NetLogEventType::CERT_VERIFY_PROC_PATH_BUILD_ATTEMPT,
      &NetLogEntry::type);
  ASSERT_NE(event, events.end());
  EXPECT_EQ(net::NetLogEventPhase::BEGIN, event->phase);
  EXPECT_EQ(true, event->params.FindBool("is_ev_attempt"));

  event = base::ranges::find(++event, events.end(),
                             NetLogEventType::CERT_VERIFY_PROC_PATH_BUILT,
                             &NetLogEntry::type);
  ASSERT_NE(event, events.end());
  EXPECT_EQ(net::NetLogEventPhase::BEGIN, event->phase);

  event = base::ranges::find(++event, events.end(),
                             NetLogEventType::CERT_VERIFY_PROC_PATH_BUILT,
                             &NetLogEntry::type);
  ASSERT_NE(event, events.end());
  EXPECT_EQ(net::NetLogEventPhase::END, event->phase);
  EXPECT_FALSE(event->params.FindString("errors"));

  event = base::ranges::find(
      ++event, events.end(),
      NetLogEventType::CERT_VERIFY_PROC_PATH_BUILD_ATTEMPT, &NetLogEntry::type);
  ASSERT_NE(event, events.end());
  EXPECT_EQ(net::NetLogEventPhase::END, event->phase);
  EXPECT_EQ(true, event->params.FindBool("has_valid_path"));
}
#endif  // defined(PLATFORM_USES_CHROMIUM_EV_METADATA)

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)

scoped_refptr<ct::SignedCertificateTimestamp> MakeSct(base::Time t,
                                                      std::string_view log_id) {
  auto sct = base::MakeRefCounted<ct::SignedCertificateTimestamp>();
  sct->timestamp = t;
  sct->log_id = log_id;
  return sct;
}

// Test SCT constraints fail-open if CT is disabled.
TEST_F(CertVerifyProcBuiltinTest,
       ChromeRootStoreConstraintSctConstraintsWithCtDisabled) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  ScopedTestRoot scoped_root(root->GetX509Certificate());

  EXPECT_CALL(*mock_ct_policy_enforcer(), IsCtEnabled())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_ct_verifier(), Verify(_, _, _, _, _, _)).Times(2);

  scoped_refptr<X509Certificate> chain = leaf->GetX509Certificate();
  ASSERT_TRUE(chain.get());

  SetMockChromeRootConstraints(
      {{.sct_not_after = base::Time::Now() - base::Days(365)}});

  {
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(chain.get(), "www.example.com", /*ocsp_response=*/std::string(),
           /*sct_list=*/std::string(), /*flags=*/0, &verify_result,
           &verify_net_log_source, callback.callback());

    int error = callback.WaitForResult();
    EXPECT_THAT(error, IsOk());
    ASSERT_EQ(verify_result.scts.size(), 0u);
  }

  SetMockChromeRootConstraints(
      {{.sct_all_after = base::Time::Now() + base::Days(365)}});

  {
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(chain.get(), "www.example.com", /*ocsp_response=*/std::string(),
           /*sct_list=*/std::string(), /*flags=*/0, &verify_result,
           &verify_net_log_source, callback.callback());

    int error = callback.WaitForResult();
    EXPECT_THAT(error, IsOk());
    ASSERT_EQ(verify_result.scts.size(), 0u);
  }
}

// Test SctNotAfter constraint only requires 1 valid SCT that satisfies the
// constraint.
// Set a SctNotAfter constraint at time t1.
// Mock that there are two SCTs, one of which is at t1 and thus satisfies the
// constraint. The second is at t2 and does not satisfy the constraint, but
// this is ok as only one valid SCT that meets the constraint is needed.
TEST_F(CertVerifyProcBuiltinTest, ChromeRootStoreConstraintSctNotAfter) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  ScopedTestRoot scoped_root(root->GetX509Certificate());

  const std::string kSctList = "SCT list";
  const std::string kLog1 = "log1";
  const std::string kLog2 = "log2";
  base::Time now = base::Time::Now();
  base::Time t1 = now - base::Days(2);
  base::Time t2 = now - base::Days(1);
  SignedCertificateTimestampAndStatusList sct_and_status_list;
  sct_and_status_list.emplace_back(MakeSct(t1, kLog1), ct::SCT_STATUS_OK);
  sct_and_status_list.emplace_back(MakeSct(t2, kLog2), ct::SCT_STATUS_OK);

  EXPECT_CALL(*mock_ct_verifier(), Verify(_, _, kSctList, _, _, _))
      .WillRepeatedly(testing::SetArgPointee<4>(sct_and_status_list));

  SetMockChromeRootConstraints({{.sct_not_after = t1}});

  EXPECT_CALL(*mock_ct_policy_enforcer(), IsCtEnabled())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_ct_policy_enforcer(), GetLogDisqualificationTime(kLog1))
      .WillRepeatedly(testing::Return(std::nullopt));
  EXPECT_CALL(*mock_ct_policy_enforcer(), GetLogDisqualificationTime(kLog2))
      .WillRepeatedly(testing::Return(std::nullopt));
  EXPECT_CALL(*mock_ct_policy_enforcer(), CheckCompliance(_, _, _, _))
      .WillRepeatedly(
          testing::Return(ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS));

  scoped_refptr<X509Certificate> chain = leaf->GetX509Certificate();
  ASSERT_TRUE(chain.get());

  {
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(chain.get(), "www.example.com", /*ocsp_response=*/std::string(),
           kSctList, /*flags=*/0, &verify_result, &verify_net_log_source,
           callback.callback());

    int error = callback.WaitForResult();
    EXPECT_THAT(error, IsOk());
    ASSERT_EQ(verify_result.scts.size(), 2u);
  }

  // Try again with the SctNotAfter set to before both SCTs. Verification should
  // fail.
  SetMockChromeRootConstraints({{.sct_not_after = t1 - base::Seconds(1)}});
  {
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(chain.get(), "www.example.com", /*ocsp_response=*/std::string(),
           kSctList, /*flags=*/0, &verify_result, &verify_net_log_source,
           callback.callback());

    int error = callback.WaitForResult();
    EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
    ASSERT_EQ(verify_result.scts.size(), 2u);
  }
}

// Test SctNotAfter constraint is only satisfied by successfully verified SCTs.
// Set a SctNotAfter constraint at time t1.
// Mock that there are two SCTs. One SCT for time t1 but from an unknown log,
// thus should not be usable for the SctNotAfter constraint. The second CT is
// from a known log but is at time t2 which is after t1, so does not satisfy
// the constraint. Therefore the certificate should fail verification.
TEST_F(CertVerifyProcBuiltinTest,
       ChromeRootStoreConstraintSctNotAfterLogUnknown) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  ScopedTestRoot scoped_root(root->GetX509Certificate());

  const std::string kSctList = "SCT list";
  const std::string kLog1 = "log1";
  const std::string kLog2 = "log2";
  base::Time now = base::Time::Now();
  base::Time t1 = now - base::Days(2);
  base::Time t2 = now - base::Days(1);
  SignedCertificateTimestampAndStatusList sct_and_status_list;
  sct_and_status_list.emplace_back(MakeSct(t1, kLog1),
                                   ct::SCT_STATUS_LOG_UNKNOWN);
  sct_and_status_list.emplace_back(MakeSct(t2, kLog2), ct::SCT_STATUS_OK);

  EXPECT_CALL(*mock_ct_policy_enforcer(), IsCtEnabled())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_ct_verifier(), Verify(_, _, kSctList, _, _, _))
      .WillOnce(testing::SetArgPointee<4>(sct_and_status_list));

  SetMockChromeRootConstraints({{.sct_not_after = t1}});

  scoped_refptr<X509Certificate> chain = leaf->GetX509Certificate();
  ASSERT_TRUE(chain.get());

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", /*ocsp_response=*/std::string(),
         kSctList, /*flags=*/0, &verify_result, &verify_net_log_source,
         callback.callback());

  int error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  ASSERT_EQ(verify_result.scts.size(), 2u);
}

// Test SctNotAfter constraint is not satisfied by a SCT from a disqualified
// log even if the SCT timestamp is before the log was disqualified. Once a log
// is disqualified we assume it can not be trusted and could sign SCTs for any
// timestamp.
// SCT #1 is from a disqualified log and the timestamp is before the log was
// disqualified.
// SCT #2 is from a valid log but is after the SctNotAfter constraint, so does
// not satisfy the constraint.
TEST_F(
    CertVerifyProcBuiltinTest,
    ChromeRootStoreConstraintSctNotAfterFromDisqualifiedLogBeforeDisqualification) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  ScopedTestRoot scoped_root(root->GetX509Certificate());

  const std::string kSctList = "SCT list";
  const std::string kLog1 = "log1";
  const std::string kLog2 = "log2";
  base::Time now = base::Time::Now();
  base::Time t1 = now - base::Days(2);
  base::Time t2 = now - base::Days(1);
  SignedCertificateTimestampAndStatusList sct_and_status_list;
  sct_and_status_list.emplace_back(MakeSct(t1, kLog1), ct::SCT_STATUS_OK);
  sct_and_status_list.emplace_back(MakeSct(t2, kLog2), ct::SCT_STATUS_OK);

  EXPECT_CALL(*mock_ct_verifier(), Verify(_, _, kSctList, _, _, _))
      .WillOnce(testing::SetArgPointee<4>(sct_and_status_list));

  SetMockChromeRootConstraints({{.sct_not_after = t1}});

  EXPECT_CALL(*mock_ct_policy_enforcer(), IsCtEnabled())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_ct_policy_enforcer(), GetLogDisqualificationTime(kLog1))
      .WillRepeatedly(testing::Return(t2));
  EXPECT_CALL(*mock_ct_policy_enforcer(), GetLogDisqualificationTime(kLog2))
      .WillRepeatedly(testing::Return(std::nullopt));

  EXPECT_CALL(*mock_ct_policy_enforcer(), CheckCompliance(_, _, _, _))
      .WillRepeatedly(
          testing::Return(ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS));

  scoped_refptr<X509Certificate> chain = leaf->GetX509Certificate();
  ASSERT_TRUE(chain.get());

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", /*ocsp_response=*/std::string(),
         kSctList, /*flags=*/0, &verify_result, &verify_net_log_source,
         callback.callback());

  int error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
}

// Test SctNotAfter constraint is not satisfied by a SCT from a disqualified
// log if the SCT timestamp is after the log was disqualified.
// SCT #1 is from a disqualified log and the timestamp is after the log was
// disqualified.
// SCT #2 is from a valid log but is after the SctNotAfter constraint, so does
// not satisfy the constraint.
TEST_F(
    CertVerifyProcBuiltinTest,
    ChromeRootStoreConstraintSctNotAfterFromDisqualifiedLogAfterDisqualification) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  ScopedTestRoot scoped_root(root->GetX509Certificate());

  const std::string kSctList = "SCT list";
  const std::string kLog1 = "log1";
  const std::string kLog2 = "log2";
  base::Time now = base::Time::Now();
  base::Time t1 = now - base::Days(2);
  base::Time t2 = now - base::Days(1);
  SignedCertificateTimestampAndStatusList sct_and_status_list;
  sct_and_status_list.emplace_back(MakeSct(t1, kLog1), ct::SCT_STATUS_OK);
  sct_and_status_list.emplace_back(MakeSct(t2, kLog2), ct::SCT_STATUS_OK);

  EXPECT_CALL(*mock_ct_verifier(), Verify(_, _, kSctList, _, _, _))
      .WillOnce(testing::SetArgPointee<4>(sct_and_status_list));

  SetMockChromeRootConstraints({{.sct_not_after = t1}});

  EXPECT_CALL(*mock_ct_policy_enforcer(), IsCtEnabled())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_ct_policy_enforcer(), GetLogDisqualificationTime(kLog1))
      .WillRepeatedly(testing::Return(t1));
  EXPECT_CALL(*mock_ct_policy_enforcer(), GetLogDisqualificationTime(kLog2))
      .WillRepeatedly(testing::Return(std::nullopt));

  EXPECT_CALL(*mock_ct_policy_enforcer(), CheckCompliance(_, _, _, _))
      .WillRepeatedly(
          testing::Return(ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS));

  scoped_refptr<X509Certificate> chain = leaf->GetX509Certificate();
  ASSERT_TRUE(chain.get());

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", /*ocsp_response=*/std::string(),
         kSctList, /*flags=*/0, &verify_result, &verify_net_log_source,
         callback.callback());

  int error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
}

// Test SctNotAfter constraint is satisfied by a SCT from a disqualified
// log if the log disqualification time is in the future.
TEST_F(CertVerifyProcBuiltinTest,
       ChromeRootStoreConstraintSctNotAfterFromFutureDisqualifiedLog) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  ScopedTestRoot scoped_root(root->GetX509Certificate());

  const std::string kSctList = "SCT list";
  const std::string kLog1 = "log1";
  const std::string kLog2 = "log2";
  base::Time now = base::Time::Now();
  base::Time t1 = now - base::Days(2);
  base::Time future_t = now + base::Days(1);
  SignedCertificateTimestampAndStatusList sct_and_status_list;
  sct_and_status_list.emplace_back(MakeSct(t1, kLog1), ct::SCT_STATUS_OK);

  EXPECT_CALL(*mock_ct_verifier(), Verify(_, _, kSctList, _, _, _))
      .WillOnce(testing::SetArgPointee<4>(sct_and_status_list));

  SetMockChromeRootConstraints({{.sct_not_after = t1}});

  EXPECT_CALL(*mock_ct_policy_enforcer(), IsCtEnabled())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_ct_policy_enforcer(), GetLogDisqualificationTime(kLog1))
      .WillRepeatedly(testing::Return(future_t));

  EXPECT_CALL(*mock_ct_policy_enforcer(), CheckCompliance(_, _, _, _))
      .WillRepeatedly(
          testing::Return(ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS));

  scoped_refptr<X509Certificate> chain = leaf->GetX509Certificate();
  ASSERT_TRUE(chain.get());

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", /*ocsp_response=*/std::string(),
         kSctList, /*flags=*/0, &verify_result, &verify_net_log_source,
         callback.callback());

  int error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());
}

// Test SctAllAfter constraint requires all valid SCTs to satisfy the
// constraint.
TEST_F(CertVerifyProcBuiltinTest, ChromeRootStoreConstraintSctAllAfter) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  ScopedTestRoot scoped_root(root->GetX509Certificate());

  const std::string kSctList = "SCT list";
  const std::string kLog1 = "log1";
  const std::string kLog2 = "log2";
  base::Time now = base::Time::Now();
  base::Time t0 = now - base::Days(3);
  base::Time t1 = now - base::Days(2);
  base::Time t2 = now - base::Days(1);
  SignedCertificateTimestampAndStatusList sct_and_status_list;
  sct_and_status_list.emplace_back(MakeSct(t1, kLog1), ct::SCT_STATUS_OK);
  sct_and_status_list.emplace_back(MakeSct(t2, kLog2), ct::SCT_STATUS_OK);

  EXPECT_CALL(*mock_ct_verifier(), Verify(_, _, kSctList, _, _, _))
      .WillRepeatedly(testing::SetArgPointee<4>(sct_and_status_list));

  // Set a SctAllAfter constraint before the timestamp of either SCT.
  SetMockChromeRootConstraints({{.sct_all_after = t0}});

  EXPECT_CALL(*mock_ct_policy_enforcer(), IsCtEnabled())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_ct_policy_enforcer(), GetLogDisqualificationTime(kLog1))
      .WillRepeatedly(testing::Return(std::nullopt));
  EXPECT_CALL(*mock_ct_policy_enforcer(), GetLogDisqualificationTime(kLog2))
      .WillRepeatedly(testing::Return(std::nullopt));
  EXPECT_CALL(*mock_ct_policy_enforcer(), CheckCompliance(_, _, _, _))
      .WillRepeatedly(
          testing::Return(ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS));

  scoped_refptr<X509Certificate> chain = leaf->GetX509Certificate();
  ASSERT_TRUE(chain.get());

  {
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(chain.get(), "www.example.com", /*ocsp_response=*/std::string(),
           kSctList, /*flags=*/0, &verify_result, &verify_net_log_source,
           callback.callback());

    int error = callback.WaitForResult();
    EXPECT_THAT(error, IsOk());
    ASSERT_EQ(verify_result.scts.size(), 2u);
  }

  // Try again with the SctAllAfter set to the same time as one of the SCTs.
  // Verification should now fail.
  SetMockChromeRootConstraints({{.sct_all_after = t1}});
  {
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(chain.get(), "www.example.com", /*ocsp_response=*/std::string(),
           kSctList, /*flags=*/0, &verify_result, &verify_net_log_source,
           callback.callback());

    int error = callback.WaitForResult();
    EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
    ASSERT_EQ(verify_result.scts.size(), 2u);
  }
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

TEST_F(CertVerifyProcBuiltinTest, ChromeRootStoreConstraintMinVersion) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509Certificate();
  ASSERT_TRUE(chain.get());

  SetMockChromeRootConstraints({{.min_version = NextVersionString()}});
  {
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(chain.get(), "www.example.com",
           /*flags=*/0, &verify_result, &verify_net_log_source,
           callback.callback());

    int error = callback.WaitForResult();
    EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  }

  SetMockChromeRootConstraints({{.min_version = CurVersionString()}});
  {
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(chain.get(), "www.example.com",
           /*flags=*/0, &verify_result, &verify_net_log_source,
           callback.callback());

    int error = callback.WaitForResult();
    EXPECT_THAT(error, IsOk());
  }
}

TEST_F(CertVerifyProcBuiltinTest, ChromeRootStoreConstraintMaxVersion) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509Certificate();
  ASSERT_TRUE(chain.get());

  SetMockChromeRootConstraints({{.max_version_exclusive = CurVersionString()}});
  {
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(chain.get(), "www.example.com",
           /*flags=*/0, &verify_result, &verify_net_log_source,
           callback.callback());

    int error = callback.WaitForResult();
    EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  }

  SetMockChromeRootConstraints(
      {{.max_version_exclusive = NextVersionString()}});
  {
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(chain.get(), "www.example.com",
           /*flags=*/0, &verify_result, &verify_net_log_source,
           callback.callback());

    int error = callback.WaitForResult();
    EXPECT_THAT(error, IsOk());
  }
}

TEST_F(CertVerifyProcBuiltinTest, ChromeRootStoreConstraintMinAndMaxVersion) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509Certificate();
  ASSERT_TRUE(chain.get());

  // min_version satisfied, max_version_exclusive not satisfied = not trusted.
  SetMockChromeRootConstraints({{.min_version = PrevVersionString(),
                                 .max_version_exclusive = CurVersionString()}});
  {
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(chain.get(), "www.example.com",
           /*flags=*/0, &verify_result, &verify_net_log_source,
           callback.callback());

    int error = callback.WaitForResult();
    EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  }

  // min_version not satisfied, max_version_exclusive satisfied = not trusted.
  SetMockChromeRootConstraints(
      {{.min_version = NextVersionString(),
        .max_version_exclusive = NextVersionString()}});
  {
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(chain.get(), "www.example.com",
           /*flags=*/0, &verify_result, &verify_net_log_source,
           callback.callback());

    int error = callback.WaitForResult();
    EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  }

  // min_version satisfied, max_version_exclusive satisfied = trusted.
  SetMockChromeRootConstraints(
      {{.min_version = CurVersionString(),
        .max_version_exclusive = NextVersionString()}});
  {
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(chain.get(), "www.example.com",
           /*flags=*/0, &verify_result, &verify_net_log_source,
           callback.callback());

    int error = callback.WaitForResult();
    EXPECT_THAT(error, IsOk());
  }
}

// Tests multiple constraint objects in the constraints vector. The CRS
// constraints are satisfied if at least one of the constraint objects is
// satisfied.
//
// The first constraint has a SctNotAfter that is before the SCT and thus is
// not satisfied.
// The second constraint has a SctAllAfter set to the same time, which is
// before the certificate SCT, and thus the certificate verification succeeds.
//
// TODO(https://crbug.com/40941039): This test isn't very interesting right
// now. Once more constraint types are added change the test to be more
// realistic of how multiple constraint sets is expected to be used.
TEST_F(CertVerifyProcBuiltinTest,
       ChromeRootStoreConstraintMultipleConstraints) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  ScopedTestRoot scoped_root(root->GetX509Certificate());

  const std::string kSctList = "SCT list";
  const std::string kLog1 = "log1";
  base::Time now = base::Time::Now();
  base::Time t1 = now - base::Days(2);
  base::Time t2 = now - base::Days(1);
  SignedCertificateTimestampAndStatusList sct_and_status_list;
  sct_and_status_list.emplace_back(MakeSct(t2, kLog1), ct::SCT_STATUS_OK);

  EXPECT_CALL(*mock_ct_policy_enforcer(), IsCtEnabled())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_ct_verifier(), Verify(_, _, kSctList, _, _, _))
      .WillOnce(testing::SetArgPointee<4>(sct_and_status_list));
  EXPECT_CALL(*mock_ct_policy_enforcer(), GetLogDisqualificationTime(kLog1))
      .WillRepeatedly(testing::Return(std::nullopt));
  EXPECT_CALL(*mock_ct_policy_enforcer(), CheckCompliance(_, _, _, _))
      .WillRepeatedly(
          testing::Return(ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS));

  SetMockChromeRootConstraints({{.sct_not_after = t1}, {.sct_all_after = t1}});

  scoped_refptr<X509Certificate> chain = leaf->GetX509Certificate();
  ASSERT_TRUE(chain.get());

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", /*ocsp_response=*/std::string(),
         kSctList, /*flags=*/0, &verify_result, &verify_net_log_source,
         callback.callback());

  int error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());
}

TEST_F(CertVerifyProcBuiltinTest,
       ChromeRootStoreConstraintNotEnforcedIfAnchorLocallyTrusted) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509Certificate();
  ASSERT_TRUE(chain.get());

  SetMockChromeRootConstraints({{.min_version = NextVersionString()}});
  {
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(chain.get(), "www.example.com",
           /*flags=*/0, &verify_result, &verify_net_log_source,
           callback.callback());

    int error = callback.WaitForResult();
    EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  }

  // If the anchor is trusted locally, the Chrome Root Store constraints should
  // not be enforced.
  SetMockIsLocallyTrustedRoot(true);
  {
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(chain.get(), "www.example.com",
           /*flags=*/0, &verify_result, &verify_net_log_source,
           callback.callback());

    int error = callback.WaitForResult();
    EXPECT_THAT(error, IsOk());
  }
}

TEST_F(CertVerifyProcBuiltinTest,
       ChromeRootStoreConstraintNotEnforcedIfAnchorAdditionallyTrusted) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  // The anchor is trusted through additional_trust_anchors, so the Chrome Root
  // Store constraints should not be enforced.
  InitializeVerifyProc(CreateParams(
      /*additional_trust_anchors=*/{root->GetX509Certificate()}));
  scoped_refptr<X509Certificate> chain = leaf->GetX509Certificate();
  ASSERT_TRUE(chain.get());

  SetMockChromeRootConstraints({{.min_version = NextVersionString()}});

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com",
         /*flags=*/0, &verify_result, &verify_net_log_source,
         callback.callback());

  int error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());
}
#endif  // BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)

TEST_F(CertVerifyProcBuiltinTest, DeadlineExceededDuringSyncGetIssuers) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  InitializeVerifyProc(CreateParams(
      /*additional_trust_anchors=*/{root->GetX509Certificate()}));

  BlockingTrustStore trust_store;
  AddTrustStore(&trust_store);

  auto intermediate_parsed_cert = bssl::ParsedCertificate::Create(
      intermediate->DupCertBuffer(), {}, nullptr);
  ASSERT_TRUE(intermediate_parsed_cert);
  trust_store.backing_trust_store_.AddCertificateWithUnspecifiedTrust(
      intermediate_parsed_cert);

  scoped_refptr<X509Certificate> chain = leaf->GetX509Certificate();
  ASSERT_TRUE(chain.get());

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback verify_callback;
  Verify(chain.get(), "www.example.com",
         /*flags=*/0,
         &verify_result, &verify_net_log_source, verify_callback.callback());

  // Wait for trust_store.SyncGetIssuersOf to be called.
  trust_store.sync_get_issuer_started_event_.Wait();

  // Advance the clock past the verifier deadline.
  const base::TimeDelta timeout_increment =
      GetCertVerifyProcBuiltinTimeLimitForTesting() + base::Milliseconds(1);
  task_environment().AdvanceClock(timeout_increment);

  // Signal trust_store.SyncGetIssuersOf to finish.
  trust_store.sync_get_issuer_ok_to_finish_event_.Signal();

  int error = verify_callback.WaitForResult();
  // Because the deadline was reached while retrieving the intermediate, path
  // building should have stopped there and not found the root. The partial
  // path built up to that point should be returned, and the error should be
  // CERT_AUTHORITY_INVALID.
  EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  ASSERT_EQ(1u, verify_result.verified_cert->intermediate_buffers().size());
  EXPECT_EQ(intermediate->GetCertBuffer(),
            verify_result.verified_cert->intermediate_buffers()[0].get());
}

namespace {

// Returns a TLV to use as an unknown signature algorithm when building a cert.
// The specific contents are as follows (the OID is from
// https://davidben.net/oid):
//
// SEQUENCE {
//   OBJECT_IDENTIFIER { 1.2.840.113554.4.1.72585.0 }
//   NULL {}
// }
std::string UnknownSignatureAlgorithmTLV() {
  const uint8_t kInvalidSignatureAlgorithmTLV[] = {
      0x30, 0x10, 0x06, 0x0c, 0x2a, 0x86, 0x48, 0x86, 0xf7,
      0x12, 0x04, 0x01, 0x84, 0xb7, 0x09, 0x00, 0x05, 0x00};
  return std::string(std::begin(kInvalidSignatureAlgorithmTLV),
                     std::end(kInvalidSignatureAlgorithmTLV));
}

// Returns a TLV to use as an invalid signature algorithm when building a cert.
// This is a SEQUENCE so that it will pass the bssl::ParseCertificate code
// and fail inside bssl::ParseSignatureAlgorithm.
// SEQUENCE {
//   INTEGER { 42 }
// }
std::string InvalidSignatureAlgorithmTLV() {
  const uint8_t kInvalidSignatureAlgorithmTLV[] = {0x30, 0x03, 0x02, 0x01,
                                                   0x2a};
  return std::string(std::begin(kInvalidSignatureAlgorithmTLV),
                     std::end(kInvalidSignatureAlgorithmTLV));
}

}  // namespace

TEST_F(CertVerifyProcBuiltinTest, UnknownSignatureAlgorithmTarget) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  leaf->SetSignatureAlgorithmTLV(UnknownSignatureAlgorithmTLV());

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  int flags = 0;
  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", flags, &verify_result,
         &verify_net_log_source, callback.callback());
  int error = callback.WaitForResult();
  // Unknown signature algorithm in the leaf cert should result in the cert
  // being invalid.
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_INVALID);
  EXPECT_THAT(error, IsError(ERR_CERT_INVALID));
}

TEST_F(CertVerifyProcBuiltinTest,
       UnparsableMismatchedTBSSignatureAlgorithmTarget) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  // Set only the tbsCertificate signature to an invalid value.
  leaf->SetTBSSignatureAlgorithmTLV(InvalidSignatureAlgorithmTLV());

  // Trust the root and build a chain to verify.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  int flags = 0;
  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", flags, &verify_result,
         &verify_net_log_source, callback.callback());
  int error = callback.WaitForResult();
  // Invalid signature algorithm in the leaf cert should result in the
  // cert being invalid.
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_INVALID);
  EXPECT_THAT(error, IsError(ERR_CERT_INVALID));
}

TEST_F(CertVerifyProcBuiltinTest, UnknownSignatureAlgorithmIntermediate) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  intermediate->SetSignatureAlgorithmTLV(UnknownSignatureAlgorithmTLV());

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  int flags = 0;
  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", flags, &verify_result,
         &verify_net_log_source, callback.callback());
  int error = callback.WaitForResult();
  // Unknown signature algorithm in the intermediate cert should result in the
  // cert being invalid.
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_INVALID);
  EXPECT_THAT(error, IsError(ERR_CERT_INVALID));
}

TEST_F(CertVerifyProcBuiltinTest,
       UnparsableMismatchedTBSSignatureAlgorithmIntermediate) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  // Set only the tbsCertificate signature to an invalid value.
  intermediate->SetTBSSignatureAlgorithmTLV(InvalidSignatureAlgorithmTLV());

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());
  ASSERT_EQ(chain->intermediate_buffers().size(), 1U);

  int flags = 0;
  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", flags, &verify_result,
         &verify_net_log_source, callback.callback());
  int error = callback.WaitForResult();
  // Invalid signature algorithm in the intermediate cert should result in the
  // cert being invalid.
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_INVALID);
  EXPECT_THAT(error, IsError(ERR_CERT_INVALID));
}

TEST_F(CertVerifyProcBuiltinTest, UnknownSignatureAlgorithmRoot) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  root->SetSignatureAlgorithmTLV(UnknownSignatureAlgorithmTLV());

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  int flags = 0;
  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", flags, &verify_result,
         &verify_net_log_source, callback.callback());
  int error = callback.WaitForResult();
  // Unknown signature algorithm in the root cert should have no effect on
  // verification.
  EXPECT_THAT(error, IsOk());
}

// This test is disabled on Android as adding the invalid root through
// ScopedTestRoot causes it to be parsed by the Java X509 code which barfs. We
// could re-enable if Chrome on Android has fully switched to the
// builtin-verifier and ScopedTestRoot no longer has Android-specific code.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_UnparsableMismatchedTBSSignatureAlgorithmRoot \
  DISABLED_UnparsableMismatchedTBSSignatureAlgorithmRoot
#else
#define MAYBE_UnparsableMismatchedTBSSignatureAlgorithmRoot \
  UnparsableMismatchedTBSSignatureAlgorithmRoot
#endif
TEST_F(CertVerifyProcBuiltinTest,
       MAYBE_UnparsableMismatchedTBSSignatureAlgorithmRoot) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  // Set only the tbsCertificate signature to an invalid value.
  root->SetTBSSignatureAlgorithmTLV(InvalidSignatureAlgorithmTLV());

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  int flags = 0;
  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", flags, &verify_result,
         &verify_net_log_source, callback.callback());
  int error = callback.WaitForResult();
  // Invalid signature algorithm in the root cert should have no effect on
  // verification.
  EXPECT_THAT(error, IsOk());
}

TEST_F(CertVerifyProcBuiltinTest, IterationLimit) {
  // Create a chain which will require many iterations in the path builder.
  std::vector<std::unique_ptr<CertBuilder>> builders =
      CertBuilder::CreateSimpleChain(6);

  base::Time not_before = base::Time::Now() - base::Days(1);
  base::Time not_after = base::Time::Now() + base::Days(1);
  for (auto& builder : builders) {
    builder->SetValidity(not_before, not_after);
  }

  // Generate certificates, making two versions of each intermediate.
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  for (size_t i = 1; i < builders.size(); i++) {
    intermediates.push_back(builders[i]->DupCertBuffer());
    builders[i]->SetValidity(not_before, not_after + base::Seconds(1));
    intermediates.push_back(builders[i]->DupCertBuffer());
  }

  // The above alone is enough to make the path builder explore many paths, but
  // it will always return the best path it has found, so the error will be the
  // same. Instead, arrange for all those paths to be invalid (untrusted root),
  // and add a separate chain that is valid.
  CertBuilder root_ok(/*orig_cert=*/builders[2]->GetCertBuffer(),
                      /*issuer=*/nullptr);
  CertBuilder intermediate_ok(/*orig_cert=*/builders[1]->GetCertBuffer(),
                              /*issuer=*/&root_ok);
  // Using the old intermediate as a template does not preserve the subject,
  // SKID, or key.
  intermediate_ok.SetSubjectTLV(base::as_byte_span(builders[1]->GetSubject()));
  intermediate_ok.SetKey(bssl::UpRef(builders[1]->GetKey()));
  intermediate_ok.SetSubjectKeyIdentifier(
      builders[1]->GetSubjectKeyIdentifier());
  // Make the valid intermediate older than the invalid ones, so that it is
  // explored last.
  intermediate_ok.SetValidity(not_before - base::Seconds(10),
                              not_after - base::Seconds(10));
  intermediates.push_back(intermediate_ok.DupCertBuffer());

  // Verify the chain.
  ScopedTestRoot scoped_root(root_ok.GetX509Certificate().get());
  scoped_refptr<X509Certificate> chain = X509Certificate::CreateFromBuffer(
      builders[0]->DupCertBuffer(), std::move(intermediates));
  ASSERT_TRUE(chain.get());

  RecordingNetLogObserver net_log_observer(NetLogCaptureMode::kDefault);
  int flags = 0;
  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", flags, &verify_result,
         &verify_net_log_source, callback.callback());
  int error = callback.WaitForResult();

  auto events = net_log_observer.GetEntriesForSource(verify_net_log_source);
  auto event = base::ranges::find_if(events, [](const NetLogEntry& e) {
    return e.type == NetLogEventType::CERT_VERIFY_PROC_PATH_BUILD_ATTEMPT &&
           e.phase == NetLogEventPhase::END;
  });
  ASSERT_NE(event, events.end());

  // The path builder gives up before it finishes all the invalid paths.
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_AUTHORITY_INVALID);
  EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  EXPECT_EQ(true, event->params.FindBool("exceeded_iteration_limit"));
}

}  // namespace net
