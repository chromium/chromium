// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_proc_builtin.h"

#include <algorithm>
#include <optional>
#include <string_view>

#include "base/containers/to_vector.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_view_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "components/network_time/time_tracker/time_tracker.h"
#include "net/base/features.h"
#include "net/base/hash_value.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/crl_set.h"
#include "net/cert/do_nothing_ct_verifier.h"
#include "net/cert/ev_root_ca_metadata.h"
#include "net/cert/internal/system_trust_store.h"
#include "net/cert/sct_status_flags.h"
#include "net/cert/time_conversions.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/cert_net/cert_net_fetcher_url_request.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log_event_type.h"
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
#include "net/test/two_qwac_cert_binding_builder.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/pki/parse_certificate.h"
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

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
// Parses a single PEM certificate from `*pem_value`, or adds a gtest failure
// and returns empty vector on error.
//
// Since the input from the test often comes from a base::Dict and thus may be
// null if the expected element isn't found, this takes a pointer as a
// convenience and will add a failure and an return empty vector if the input
// is null, so that each test expectation doesn't need to null-check the input
// before calling.
std::vector<uint8_t> ParsePemCertificate(const std::string* pem_value) {
  if (!pem_value) {
    ADD_FAILURE() << "pem_value is null";
    return {};
  }
  CertificateList certs = X509Certificate::CreateCertificateListFromBytes(
      base::as_byte_span(*pem_value),
      X509Certificate::Format::FORMAT_PEM_CERT_SEQUENCE);
  if (certs.empty()) {
    ADD_FAILURE() << "error decoding pem";
    return {};
  }
  if (certs.size() > 1) {
    ADD_FAILURE() << "multiple certs in pem";
    return {};
  }
  return base::ToVector(certs[0]->cert_span());
}

std::vector<std::string> ParseNetLogCertificatesList(
    const base::Value::List& list) {
  std::vector<std::string> result;
  for (const auto& pem_value : list) {
    if (!pem_value.is_string()) {
      result.push_back("Value is not a string");
      continue;
    }
    CertificateList certs = X509Certificate::CreateCertificateListFromBytes(
        base::as_byte_span(pem_value.GetString()),
        X509Certificate::Format::FORMAT_PEM_CERT_SEQUENCE);
    if (certs.empty()) {
      result.push_back("error decoding pem");
      continue;
    }
    if (certs.size() > 1) {
      result.push_back("multiple certs in pem");
      continue;
    }
    result.emplace_back(base::as_string_view(certs[0]->cert_span()));
  }
  return result;
}

std::vector<std::string> ParseNetLogCertificatesDict(
    const base::Value::Dict& dict) {
  auto* cert_list = dict.FindList("certificates");
  if (!cert_list) {
    ADD_FAILURE() << "no cerificates key in dict";
    return {};
  }
  return ParseNetLogCertificatesList(*cert_list);
}
#endif  // BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)

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

  bool IsKnownMtcAnchor(const bssl::MTCAnchor* anchor) const override {
    return mock_is_known_mtc_anchor_;
  }

  void AddTrustStore(bssl::TrustStore* store) {
    trust_store_.AddTrustStore(store);
  }

  void SetMockIsKnownRoot(bool is_known_root) {
    mock_is_known_root_ = is_known_root;
  }

  void SetMockIsKnownMtcAnchor(bool is_known_root) {
    mock_is_known_mtc_anchor_ = is_known_root;
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

  bssl::TrustStore* eutl_trust_store() override { return &eutl_trust_store_; }

  void SetMockChromeRootConstraints(
      std::vector<StaticChromeRootCertConstraints> chrome_root_constraints) {
    mock_chrome_root_constraints_.clear();
    for (const auto& constraint : chrome_root_constraints) {
      mock_chrome_root_constraints_.emplace_back(constraint);
    }
  }

  void AddMockEutlRoot(CRYPTO_BUFFER* der_cert) {
    auto parsed_cert =
        bssl::ParsedCertificate::Create(bssl::UpRef(der_cert), {}, nullptr);
    ASSERT_TRUE(parsed_cert);
    eutl_trust_store_.AddTrustAnchor(std::move(parsed_cert));
  }
#endif

 private:
  bssl::TrustStoreCollection trust_store_;
  bool mock_is_known_root_ = false;
  bool mock_is_known_mtc_anchor_ = false;
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  bool mock_is_locally_trusted_root_ = false;
  std::vector<ChromeRootCertConstraints> mock_chrome_root_constraints_;
  bssl::TrustStoreInMemory eutl_trust_store_;
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

class MockRequireCTDelegate : public RequireCTDelegate {
 public:
  MOCK_CONST_METHOD3(
      IsCTRequiredForHost,
      CTRequirementLevel(std::string_view host,
                         const X509Certificate* chain,
                         const std::vector<SHA256HashValue>& hashes));

 protected:
  ~MockRequireCTDelegate() override = default;
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
      distrusted_spkis.push_back(base::ToVector(cert->tbs().spki_tlv));
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

  scoped_refptr<X509Certificate> Verify2QwacBinding(
      std::string_view binding,
      const std::string& hostname,
      base::span<const uint8_t> tls_cert,
      NetLogSource* out_source) {
    // 2-QWAC verification does not do any blocking calls, so the unittest does
    // not need to run it on a worker thread.
    NetLogWithSource net_log(NetLogWithSource::Make(
        net::NetLog::Get(), net::NetLogSourceType::CERT_VERIFIER_TASK));
    *out_source = net_log.source();
    return verify_proc_->Verify2QwacBinding(binding, hostname, tls_cert,
                                            net_log);
  }

  int Verify2Qwac(scoped_refptr<X509Certificate> cert,
                  const std::string& hostname,
                  CertVerifyResult* verify_result,
                  NetLogSource* out_source) {
    // 2-QWAC verification does not do any blocking calls, so the unittest does
    // not need to run it on a worker thread.
    NetLogWithSource net_log(NetLogWithSource::Make(
        net::NetLog::Get(), net::NetLogSourceType::CERT_VERIFIER_TASK));
    *out_source = net_log.source();
    return verify_proc_->Verify2Qwac(cert.get(), hostname, verify_result,
                                     net_log);
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

  void SetMockIsKnownMtcAnchor(bool is_known_root) {
    mock_system_trust_store_->SetMockIsKnownMtcAnchor(is_known_root);
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

  void AddMockEutlRoot(CRYPTO_BUFFER* der_cert) {
    mock_system_trust_store_->AddMockEutlRoot(der_cert);
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
        test_server.base_url().GetHost(), base::Time::Now() + base::Seconds(30),
        /*include_subdomains=*/true);
    // Setting `is_top_level_nav` true prevents the upgrade from being blocked
    // by kHstsTopLevelNavigationsOnly.
    ASSERT_TRUE(context()->transport_security_state()->ShouldUpgradeToSSL(
        test_server.base_url().GetHost(), /*is_top_level_nav=*/true));
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

TEST_F(CertVerifyProcBuiltinTest, SimpleSignaturelessMtcSuccess) {
  constexpr uint8_t kMtcLogId[] = {0x09, 0x08, 0x07};
  net::MtcLogBuilder mtc_log(kMtcLogId);
  // TODO(crbug.com/469624806): improve interface for creating MTC cert
  // builders.
  std::unique_ptr<net::CertBuilder> mtc_leaf1 =
      std::move(net::CertBuilder::CreateSimpleChain(1u)[0]);
  uint64_t leaf_index = mtc_log.AddEntry(*mtc_leaf1);
  mtc_log.AdvanceLandmark();

  InitializeVerifyProc(CreateParams(/*additional_trust_anchors=*/{}));

  bssl::TrustStoreInMemory trust_store;
  auto mtc_anchor = std::make_shared<const bssl::MTCAnchor>(
      kMtcLogId, mtc_log.GetLandmarkSubtreeHashes());
  ASSERT_TRUE(trust_store.AddMTCTrustAnchor(mtc_anchor));
  AddTrustStore(&trust_store);

  auto leaf_der = mtc_log.CreateSignaturelessCertificate(leaf_index);
  ASSERT_TRUE(leaf_der);
  scoped_refptr<X509Certificate> chain =
      X509Certificate::CreateFromBytes(*leaf_der);
  ASSERT_TRUE(chain);

  // MTCs don't use IsKnownRoot, so this returning true shouldn't mark it as a
  // known root.
  SetMockIsKnownRoot(true);
  SetMockIsKnownMtcAnchor(false);

  {
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(chain.get(), "www.example.com", /*flags=*/0, &verify_result,
           &verify_net_log_source, callback.callback());

    int error = callback.WaitForResult();
    EXPECT_THAT(error, IsOk());

    EXPECT_EQ(2u, verify_result.verified_cert->cert_buffers().size());
    EXPECT_TRUE(chain->EqualsExcludingChain(verify_result.verified_cert.get()));
    EXPECT_TRUE(x509_util::CryptoBufferEqual(
        mtc_anchor->AsCert()->cert_buffer(),
        verify_result.verified_cert->cert_buffers()[1].get()));
    EXPECT_FALSE(verify_result.is_issued_by_known_root);
  }

  SetMockIsKnownRoot(false);
  SetMockIsKnownMtcAnchor(true);

  {
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(chain.get(), "www.example.com", /*flags=*/0, &verify_result,
           &verify_net_log_source, callback.callback());

    int error = callback.WaitForResult();
    EXPECT_THAT(error, IsOk());
    EXPECT_TRUE(verify_result.is_issued_by_known_root);
  }
}

TEST_F(CertVerifyProcBuiltinTest, SignaturelessMtcNonTrivialProof) {
  constexpr uint8_t kMtcLogId[] = {0x09, 0x08, 0x07};
  net::MtcLogBuilder mtc_log(kMtcLogId);
  // TODO(crbug.com/469624806): improve interface for creating MTC cert
  // builders.
  std::unique_ptr<net::CertBuilder> mtc_leaf1 =
      std::move(net::CertBuilder::CreateSimpleChain(1u)[0]);

  mtc_log.AddUnusedEntries(27);
  uint64_t leaf_index = mtc_log.AddEntry(*mtc_leaf1);
  mtc_log.AddUnusedEntries(13);
  mtc_log.AdvanceLandmark();

  InitializeVerifyProc(CreateParams(/*additional_trust_anchors=*/{}));

  bssl::TrustStoreInMemory trust_store;
  auto mtc_anchor = std::make_shared<const bssl::MTCAnchor>(
      kMtcLogId, mtc_log.GetLandmarkSubtreeHashes());
  ASSERT_TRUE(trust_store.AddMTCTrustAnchor(mtc_anchor));
  AddTrustStore(&trust_store);

  {
    scoped_refptr<X509Certificate> cert1 = X509Certificate::CreateFromBytes(
        *mtc_log.CreateSignaturelessCertificate(leaf_index));
    ASSERT_TRUE(cert1);

    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(cert1.get(), "www.example.com", /*flags=*/0, &verify_result,
           &verify_net_log_source, callback.callback());

    int error = callback.WaitForResult();
    EXPECT_THAT(error, IsOk());
  }
}

TEST_F(CertVerifyProcBuiltinTest, CallsCtVerifierAndReturnsSctStatus) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  CertVerifyProc::InstanceParams instance_params = CreateParams(
      /*additional_trust_anchors=*/{root->GetX509Certificate()});
  InitializeVerifyProc(instance_params);
  net::ScopedTestKnownRoot scoped_known_root(root->GetX509Certificate().get());

  constexpr char kHostname[] = "www.example.com";
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
      .WillRepeatedly(testing::SetArgPointee<4>(sct_and_status_list));
  EXPECT_CALL(*mock_ct_policy_enforcer(), IsCtEnabled())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_ct_policy_enforcer(), CheckCompliance(_, _, _, _))
      .WillRepeatedly(
          testing::Return(ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS));

  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // If a RequireCTDelegate is not supplied, SCT verification is done, but the
  // cert verification result is not affected.
  {
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(chain.get(), kHostname, kOcspResponse, kSctList, /*flags=*/0,
           &verify_result, &verify_net_log_source, callback.callback());

    int error = callback.WaitForResult();
    EXPECT_THAT(error, IsOk());
    ASSERT_EQ(verify_result.scts.size(), 1u);
    EXPECT_EQ(verify_result.scts.front().status, kSctVerifyStatus);
    EXPECT_EQ(verify_result.scts.front().sct->log_id, kLogId);
    EXPECT_EQ(verify_result.policy_compliance,
              ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS);
    EXPECT_EQ(verify_result.ct_requirement_status,
              ct::CTRequirementsStatus::CT_NOT_REQUIRED);
  }

  // If a RequireCTDelegate is supplied, it is consulted to check whether the
  // CT result should affect the cert verification result.
  auto mock_require_ct_delegate = base::MakeRefCounted<MockRequireCTDelegate>();
  instance_params.require_ct_delegate = mock_require_ct_delegate;
  EXPECT_CALL(*mock_require_ct_delegate, IsCTRequiredForHost(kHostname, _, _))
      .WillRepeatedly(
          testing::Return(RequireCTDelegate::CTRequirementLevel::REQUIRED));
  InitializeVerifyProc(instance_params);
  EXPECT_CALL(*mock_ct_verifier(), Verify(_, kOcspResponse, kSctList, _, _, _))
      .WillRepeatedly(testing::SetArgPointee<4>(sct_and_status_list));
  EXPECT_CALL(*mock_ct_policy_enforcer(), IsCtEnabled())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_ct_policy_enforcer(), CheckCompliance(_, _, _, _))
      .WillRepeatedly(
          testing::Return(ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS));
  {
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(chain.get(), kHostname, kOcspResponse, kSctList, /*flags=*/0,
           &verify_result, &verify_net_log_source, callback.callback());

    int error = callback.WaitForResult();
    EXPECT_THAT(error, IsError(ERR_CERTIFICATE_TRANSPARENCY_REQUIRED));
    ASSERT_EQ(verify_result.scts.size(), 1u);
    EXPECT_EQ(verify_result.scts.front().status, kSctVerifyStatus);
    EXPECT_EQ(verify_result.scts.front().sct->log_id, kLogId);
    EXPECT_EQ(verify_result.policy_compliance,
              ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS);
    EXPECT_EQ(verify_result.ct_requirement_status,
              ct::CTRequirementsStatus::CT_REQUIREMENTS_NOT_MET);
  }
}

TEST_F(CertVerifyProcBuiltinTest, CtIsRequiredAndCtVerificationComplies) {
  constexpr char kHostname[] = "www.example.com";
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  CertVerifyProc::InstanceParams instance_params = CreateParams(
      /*additional_trust_anchors=*/{root->GetX509Certificate()});
  auto mock_require_ct_delegate = base::MakeRefCounted<MockRequireCTDelegate>();
  instance_params.require_ct_delegate = mock_require_ct_delegate;
  EXPECT_CALL(*mock_require_ct_delegate, IsCTRequiredForHost(kHostname, _, _))
      .WillRepeatedly(
          testing::Return(RequireCTDelegate::CTRequirementLevel::REQUIRED));
  InitializeVerifyProc(instance_params);
  net::ScopedTestKnownRoot scoped_known_root(root->GetX509Certificate().get());

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

  InitializeVerifyProc(instance_params);
  EXPECT_CALL(*mock_ct_verifier(), Verify(_, kOcspResponse, kSctList, _, _, _))
      .WillRepeatedly(testing::SetArgPointee<4>(sct_and_status_list));
  EXPECT_CALL(*mock_ct_policy_enforcer(), IsCtEnabled())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_ct_policy_enforcer(), CheckCompliance(_, _, _, _))
      .WillRepeatedly(
          testing::Return(ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS));

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(leaf->GetX509CertificateChain().get(), kHostname, kOcspResponse,
         kSctList, /*flags=*/0, &verify_result, &verify_net_log_source,
         callback.callback());

  int error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());
  ASSERT_EQ(verify_result.scts.size(), 1u);
  EXPECT_EQ(verify_result.scts.front().status, kSctVerifyStatus);
  EXPECT_EQ(verify_result.scts.front().sct->log_id, kLogId);
  EXPECT_EQ(verify_result.policy_compliance,
            ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS);
  EXPECT_EQ(verify_result.ct_requirement_status,
            ct::CTRequirementsStatus::CT_REQUIREMENTS_MET);
}

TEST_F(CertVerifyProcBuiltinTest, DefaultCtComplianceIsNotAvailable) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  const std::string kOcspResponse = "OCSP response";
  const std::string kSctList = "SCT list";
  const std::string kLogId = "CT log id";
  const ct::SCTVerifyStatus kSctVerifyStatus = ct::SCT_STATUS_OK;

  SignedCertificateTimestampAndStatus sct_and_status;
  sct_and_status.sct = base::MakeRefCounted<ct::SignedCertificateTimestamp>();
  sct_and_status.sct->log_id = kLogId;
  sct_and_status.status = kSctVerifyStatus;
  SignedCertificateTimestampAndStatusList sct_and_status_list;
  sct_and_status_list.push_back(sct_and_status);
  EXPECT_CALL(*mock_ct_verifier(), Verify(_, kOcspResponse, kSctList, _, _, _))
      .WillOnce(testing::SetArgPointee<4>(sct_and_status_list));

  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", kOcspResponse, kSctList, /*flags=*/0,
         &verify_result, &verify_net_log_source, callback.callback());

  int error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  ASSERT_EQ(verify_result.scts.size(), 1u);
  EXPECT_EQ(verify_result.scts.front().status, kSctVerifyStatus);
  EXPECT_EQ(verify_result.scts.front().sct->log_id, kLogId);
  // Verification failed, so CT policy compliance isn't checked, and the default
  // value should be COMPLIANCE_DETAILS_NOT_AVAILABLE.
  EXPECT_EQ(verify_result.policy_compliance,
            ct::CTPolicyCompliance::CT_POLICY_COMPLIANCE_DETAILS_NOT_AVAILABLE);
}

TEST_F(CertVerifyProcBuiltinTest,
       DefaultCtComplianceIsNotAvailableWhenCtDisabled) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  CertVerifyProc::InstanceParams instance_params = CreateParams(
      /*additional_trust_anchors=*/{root->GetX509Certificate()});
  InitializeVerifyProc(instance_params);
  net::ScopedTestKnownRoot scoped_known_root(root->GetX509Certificate().get());

  const std::string kOcspResponse = "OCSP response";
  const std::string kSctList = "SCT list";
  const std::string kLogId = "CT log id";
  const ct::SCTVerifyStatus kSctVerifyStatus = ct::SCT_STATUS_OK;

  SignedCertificateTimestampAndStatus sct_and_status;
  sct_and_status.sct = base::MakeRefCounted<ct::SignedCertificateTimestamp>();
  sct_and_status.sct->log_id = kLogId;
  sct_and_status.status = kSctVerifyStatus;
  SignedCertificateTimestampAndStatusList sct_and_status_list;
  sct_and_status_list.push_back(sct_and_status);
  EXPECT_CALL(*mock_ct_verifier(), Verify(_, kOcspResponse, kSctList, _, _, _))
      .WillOnce(testing::SetArgPointee<4>(sct_and_status_list));
  EXPECT_CALL(*mock_ct_policy_enforcer(), IsCtEnabled())
      .WillRepeatedly(testing::Return(false));

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
  // Verification failed, so CT policy compliance isn't checked, and the default
  // value should be COMPLIANCE_DETAILS_NOT_AVAILABLE.
  EXPECT_EQ(verify_result.policy_compliance,
            ct::CTPolicyCompliance::CT_POLICY_COMPLIANCE_DETAILS_NOT_AVAILABLE);
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
  EXPECT_CALL(*mock_ct_policy_enforcer(), IsCtEnabled())
      .WillRepeatedly(testing::Return(true));
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

  auto event = std::ranges::find(
      events, NetLogEventType::CERT_VERIFY_PROC_PATH_BUILD_ATTEMPT,
      &NetLogEntry::type);
  ASSERT_NE(event, events.end());
  EXPECT_EQ(net::NetLogEventPhase::BEGIN, event->phase);
  EXPECT_EQ(true, event->params.FindBool("is_ev_attempt"));

  event = std::ranges::find(++event, events.end(),
                            NetLogEventType::CERT_VERIFY_PROC_PATH_BUILT,
                            &NetLogEntry::type);
  ASSERT_NE(event, events.end());
  EXPECT_EQ(net::NetLogEventPhase::BEGIN, event->phase);

  event = std::ranges::find(++event, events.end(),
                            NetLogEventType::CERT_VERIFY_PROC_PATH_BUILT,
                            &NetLogEntry::type);
  ASSERT_NE(event, events.end());
  EXPECT_EQ(net::NetLogEventPhase::END, event->phase);
  EXPECT_FALSE(event->params.FindString("errors"));

  event = std::ranges::find(
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

TEST_F(CertVerifyProcBuiltinTest, ChromeRootStoreConstraintNameConstraints) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  ScopedTestRoot scoped_root(root->GetX509Certificate());

  // If the the CRS root has dns name constraints and the cert's names don't
  // match the name constraints, verification should fail.
  {
    std::array<std::string_view, 2> permitted_dns_names = {
        std::string_view("example.org"),
        std::string_view("foo.example.com"),
    };
    SetMockChromeRootConstraints(
        {{.permitted_dns_names = permitted_dns_names}});
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(leaf->GetX509Certificate(), "www.example.com",
           /*flags=*/0, &verify_result, &verify_net_log_source,
           callback.callback());

    int error = callback.WaitForResult();
    EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  }

  // If cert's names match the CRS name constraints, verification should
  // succeed.
  {
    std::array<std::string_view, 2> permitted_dns_names = {
        std::string_view("example.org"),
        std::string_view("example.com"),
    };
    SetMockChromeRootConstraints(
        {{.permitted_dns_names = permitted_dns_names}});
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(leaf->GetX509Certificate(), "www.example.com",
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

class CertVerifyProcBuiltin1QwacTest
    : public CertVerifyProcBuiltinTest,
      public testing::WithParamInterface<bool> {
 public:
  CertVerifyProcBuiltin1QwacTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(features::kVerifyQWACs);
    } else {
      feature_list_.InitAndDisableFeature(features::kVerifyQWACs);
    }
  }

  void ExpectHistogramSample(const base::HistogramTester& histograms,
                             Verify1QwacResult result) {
    if (GetParam()) {
      histograms.ExpectUniqueSample("Net.CertVerifier.Qwac.1Qwac", result, 1u);
    } else {
      histograms.ExpectTotalCount("Net.CertVerifier.Qwac.1Qwac", 0u);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(CertVerifyProcBuiltin1QwacTest, NotQwac) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  InitializeVerifyProc(CreateParams(
      /*additional_trust_anchors=*/{}));
  {
    base::HistogramTester histograms;
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(leaf->GetX509CertificateChain(), "www.example.com",
           /*flags=*/0, &verify_result, &verify_net_log_source,
           callback.callback());

    int error = callback.WaitForResult();
    EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
    EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_IS_QWAC);

    // The histogram is not logged if regular verification failed.
    histograms.ExpectTotalCount("Net.CertVerifier.Qwac.1Qwac", 0u);
  }

  InitializeVerifyProc(CreateParams(
      /*additional_trust_anchors=*/{root->GetX509Certificate()}));
  {
    base::HistogramTester histograms;
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(leaf->GetX509CertificateChain(), "www.example.com",
           /*flags=*/0, &verify_result, &verify_net_log_source,
           callback.callback());

    int error = callback.WaitForResult();
    EXPECT_THAT(error, IsOk());
    EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_IS_QWAC);

    ExpectHistogramSample(histograms, Verify1QwacResult::kNotQwac);
  }
}

TEST_P(CertVerifyProcBuiltin1QwacTest,
       CanUseEutlCertsAsHintsInNormalPathbuilding) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  // CABF OV, ETSI QNCP-w
  leaf->SetCertificatePolicies({"2.23.140.1.2.2", "0.4.0.194112.1.5"});

  leaf->SetQwacQcStatements({bssl::der::Input(kEtsiQctWebOid)});

  InitializeVerifyProc(CreateParams(
      /*additional_trust_anchors=*/{root->GetX509Certificate()}));

  {
    base::HistogramTester histograms;
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(leaf->GetX509Certificate(), "www.example.com",
           /*flags=*/0, &verify_result, &verify_net_log_source,
           callback.callback());

    int error = callback.WaitForResult();
    // The intermediate was not supplied, so verification fails to find a path
    // to the root.
    EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
    EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_IS_QWAC);
    histograms.ExpectTotalCount("Net.CertVerifier.Qwac.1Qwac", 0u);
  }

  AddMockEutlRoot(intermediate->GetCertBuffer());

  {
    base::HistogramTester histograms;
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(leaf->GetX509Certificate(), "www.example.com",
           /*flags=*/0, &verify_result, &verify_net_log_source,
           callback.callback());

    int error = callback.WaitForResult();
    if (GetParam()) {
      // If the intermediate is on the EUTL, regular path building is able to
      // use it as a hint, so the chain now verifies successfully.
      EXPECT_THAT(error, IsOk());
      EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_IS_QWAC);
      ASSERT_EQ(2u, verify_result.verified_cert->intermediate_buffers().size());
      // The verified chain has the cert chain from the normal TLS verification,
      // not the QWAC verification.
      EXPECT_EQ(intermediate->GetCertBuffer(),
                verify_result.verified_cert->intermediate_buffers()[0].get());
      EXPECT_EQ(root->GetCertBuffer(),
                verify_result.verified_cert->intermediate_buffers()[1].get());
    } else {
      EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
      EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_IS_QWAC);
    }
    ExpectHistogramSample(histograms, Verify1QwacResult::kValid1Qwac);
  }

  InitializeVerifyProc(CreateParams(
      /*additional_trust_anchors=*/{}));
  AddMockEutlRoot(intermediate->GetCertBuffer());

  {
    base::HistogramTester histograms;
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(leaf->GetX509Certificate(), "www.example.com",
           /*flags=*/0, &verify_result, &verify_net_log_source,
           callback.callback());

    int error = callback.WaitForResult();
    // If the intermediate is an EUTL cert but the root is not trusted,
    // verification should fail. The EUTL certs are only used as hints in
    // the regular path building attempt, but are not trust anchors.
    EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
    EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_IS_QWAC);
    if (GetParam()) {
      // The path builder should have been able to build the partial path to the
      // hint certificate, but there is no root to build a path to from there.
      ASSERT_EQ(1u, verify_result.verified_cert->intermediate_buffers().size());
      EXPECT_EQ(intermediate->GetCertBuffer(),
                verify_result.verified_cert->intermediate_buffers()[0].get());
    } else {
      ASSERT_EQ(0u, verify_result.verified_cert->intermediate_buffers().size());
    }
    histograms.ExpectTotalCount("Net.CertVerifier.Qwac.1Qwac", 0u);
  }
}

TEST_P(CertVerifyProcBuiltin1QwacTest, OneQwacRequiresEutl) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  // intermediate->SetCertificatePolicies({"2.5.29.32.0"}); // anyPolicy

  // CABF OV, ETSI QNCP-w
  leaf->SetCertificatePolicies({"2.23.140.1.2.2", "0.4.0.194112.1.5"});

  leaf->SetQwacQcStatements({bssl::der::Input(kEtsiQctWebOid)});

  InitializeVerifyProc(CreateParams(
      /*additional_trust_anchors=*/{root->GetX509Certificate()}));

  {
    base::HistogramTester histograms;
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(leaf->GetX509CertificateChain(), "www.example.com",
           /*flags=*/0, &verify_result, &verify_net_log_source,
           callback.callback());

    int error = callback.WaitForResult();
    // If the intermediate is not on the EUTL, the certificate verifies
    // successfully but does not have QWAC status set.
    EXPECT_THAT(error, IsOk());
    EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_IS_QWAC);
    ExpectHistogramSample(histograms, Verify1QwacResult::kFailedVerification);
  }

  AddMockEutlRoot(intermediate->GetCertBuffer());

  {
    base::HistogramTester histograms;
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(leaf->GetX509CertificateChain(), "www.example.com",
           /*flags=*/0, &verify_result, &verify_net_log_source,
           callback.callback());

    int error = callback.WaitForResult();
    // If the intermediate is on the EUTL, the same certificate verifies
    // successfully with the QWAC status set.
    EXPECT_THAT(error, IsOk());
    EXPECT_EQ(GetParam(), !!(verify_result.cert_status & CERT_STATUS_IS_QWAC));
    ExpectHistogramSample(histograms, Verify1QwacResult::kValid1Qwac);
  }
}

TEST_P(CertVerifyProcBuiltin1QwacTest, OneQwacRequiresPolicies) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  // CABF OV
  leaf->SetCertificatePolicies({"2.23.140.1.2.2"});

  leaf->SetQwacQcStatements({bssl::der::Input(kEtsiQctWebOid)});

  InitializeVerifyProc(CreateParams(
      /*additional_trust_anchors=*/{root->GetX509Certificate()}));

  AddMockEutlRoot(intermediate->GetCertBuffer());

  {
    base::HistogramTester histograms;
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(leaf->GetX509CertificateChain(), "www.example.com",
           /*flags=*/0, &verify_result, &verify_net_log_source,
           callback.callback());

    int error = callback.WaitForResult();
    // If the leaf doesn't have the necessary policies, the certificate
    // verifies successfully but does not have QWAC status set.
    EXPECT_THAT(error, IsOk());
    EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_IS_QWAC);
    ExpectHistogramSample(histograms, Verify1QwacResult::kInconsistentBits);
  }

  // CABF OV, ETSI QNCP-w
  leaf->SetCertificatePolicies({"2.23.140.1.2.2", "0.4.0.194112.1.5"});

  {
    base::HistogramTester histograms;
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(leaf->GetX509CertificateChain(), "www.example.com",
           /*flags=*/0, &verify_result, &verify_net_log_source,
           callback.callback());

    int error = callback.WaitForResult();
    // If the leaf has the qwac policies, verifies successfully with the QWAC
    // status set.
    EXPECT_THAT(error, IsOk());
    EXPECT_EQ(GetParam(), !!(verify_result.cert_status & CERT_STATUS_IS_QWAC));
    ExpectHistogramSample(histograms, Verify1QwacResult::kValid1Qwac);
  }
}

TEST_P(CertVerifyProcBuiltin1QwacTest, OneQwacRequiresQcStatements) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  // CABF OV, ETSI QNCP-w
  leaf->SetCertificatePolicies({"2.23.140.1.2.2", "0.4.0.194112.1.5"});

  // Initially, set QcStatements with the wrong QcType.
  // id-etsi-qct-eseal OBJECT IDENTIFIER ::= { id-etsi-qcs-QcType 2 }
  constexpr uint8_t kEtsiQctEsealOid[] = {0x04, 0x00, 0x8e, 0x46,
                                          0x01, 0x06, 0x02};
  leaf->SetQwacQcStatements({bssl::der::Input(kEtsiQctEsealOid)});

  InitializeVerifyProc(CreateParams(
      /*additional_trust_anchors=*/{root->GetX509Certificate()}));

  AddMockEutlRoot(intermediate->GetCertBuffer());

  {
    base::HistogramTester histograms;
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(leaf->GetX509CertificateChain(), "www.example.com",
           /*flags=*/0, &verify_result, &verify_net_log_source,
           callback.callback());

    int error = callback.WaitForResult();
    // If the leaf doesn't have the necessary QcStatements, the certificate
    // verifies successfully but does not have QWAC status set.
    EXPECT_THAT(error, IsOk());
    EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_IS_QWAC);
    ExpectHistogramSample(histograms, Verify1QwacResult::kInconsistentBits);
  }

  // Try again with the correct QcType.
  leaf->SetQwacQcStatements({bssl::der::Input(kEtsiQctWebOid)});

  {
    base::HistogramTester histograms;
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    Verify(leaf->GetX509CertificateChain(), "www.example.com",
           /*flags=*/0, &verify_result, &verify_net_log_source,
           callback.callback());

    int error = callback.WaitForResult();
    // If the leaf has the qwac QcStatements, verifies successfully with the
    // QWAC status set.
    EXPECT_THAT(error, IsOk());
    EXPECT_EQ(GetParam(), !!(verify_result.cert_status & CERT_STATUS_IS_QWAC));
    ExpectHistogramSample(histograms, Verify1QwacResult::kValid1Qwac);
  }
}

TEST_P(CertVerifyProcBuiltin1QwacTest, OneQwacCanBuildAlternatePath) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  // CABF OV, ETSI QNCP-w
  leaf->SetCertificatePolicies({"2.23.140.1.2.2", "0.4.0.194112.1.5"});

  leaf->SetQwacQcStatements({bssl::der::Input(kEtsiQctWebOid)});

  InitializeVerifyProc(CreateParams(
      /*additional_trust_anchors=*/{root->GetX509Certificate()}));

  // Create separate intermediate which chains to a different root but has same
  // subject, private key, and SKI so that `leaf` can also be verified with
  // this chain.
  auto [unused, root2] = CertBuilder::CreateSimpleChain2();
  CertBuilder eutl_intermediate(/*orig_cert=*/intermediate->GetCertBuffer(),
                                /*issuer=*/root2.get());
  eutl_intermediate.SetSubjectTLV(
      base::as_byte_span(intermediate->GetSubject()));
  eutl_intermediate.SetKey(bssl::UpRef(intermediate->GetKey()));
  eutl_intermediate.SetSubjectKeyIdentifier(
      intermediate->GetSubjectKeyIdentifier());
  AddMockEutlRoot(eutl_intermediate.GetCertBuffer());

  RecordingNetLogObserver net_log_observer(NetLogCaptureMode::kDefault);
  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(leaf->GetX509CertificateChain(), "www.example.com",
         /*flags=*/0, &verify_result, &verify_net_log_source,
         callback.callback());

  int error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());
  EXPECT_EQ(GetParam(), !!(verify_result.cert_status & CERT_STATUS_IS_QWAC));

  ASSERT_EQ(2u, verify_result.verified_cert->intermediate_buffers().size());
  // The verified chain has the cert chain from the normal TLS verification,
  // not the QWAC verification.
  EXPECT_EQ(intermediate->GetCertBuffer(),
            verify_result.verified_cert->intermediate_buffers()[0].get());
  EXPECT_EQ(root->GetCertBuffer(),
            verify_result.verified_cert->intermediate_buffers()[1].get());

  auto events = net_log_observer.GetEntriesForSource(verify_net_log_source);

  auto event = std::ranges::find(
      events, NetLogEventType::CERT_VERIFY_PROC_PATH_BUILD_ATTEMPT,
      &NetLogEntry::type);
  ASSERT_NE(event, events.end());
  EXPECT_EQ(net::NetLogEventPhase::BEGIN, event->phase);
  EXPECT_EQ(std::nullopt, event->params.FindBool("is_qwac_attempt"));

  event = std::ranges::find(++event, events.end(),
                            NetLogEventType::CERT_VERIFY_PROC_PATH_BUILT,
                            &NetLogEntry::type);
  ASSERT_NE(event, events.end());
  EXPECT_EQ(net::NetLogEventPhase::BEGIN, event->phase);

  event = std::ranges::find(++event, events.end(),
                            NetLogEventType::CERT_VERIFY_PROC_PATH_BUILT,
                            &NetLogEntry::type);
  ASSERT_NE(event, events.end());
  EXPECT_EQ(net::NetLogEventPhase::END, event->phase);
  EXPECT_EQ(true, event->params.FindBool("is_valid"));
  base::Value::List* pem_certs = event->params.FindList("certificates");
  ASSERT_TRUE(pem_certs);
  // The CERT_VERIFY_PROC_PATH_BUILT netlog for the main verification should
  // contain the TLS cert chain.
  EXPECT_THAT(ParseNetLogCertificatesList(*pem_certs),
              testing::ElementsAre(leaf->GetDER(), intermediate->GetDER(),
                                   root->GetDER()));

  event = std::ranges::find(
      ++event, events.end(),
      NetLogEventType::CERT_VERIFY_PROC_PATH_BUILD_ATTEMPT, &NetLogEntry::type);
  ASSERT_NE(event, events.end());
  EXPECT_EQ(net::NetLogEventPhase::END, event->phase);
  EXPECT_EQ(true, event->params.FindBool("has_valid_path"));

  event = std::ranges::find(
      ++event, events.end(),
      NetLogEventType::CERT_VERIFY_PROC_PATH_BUILD_ATTEMPT, &NetLogEntry::type);
  if (!GetParam()) {
    // If the feature flag wasn't enabled, there should only be one
    // CERT_VERIFY_PROC_PATH_BUILD_ATTEMPT.
    ASSERT_EQ(event, events.end());
    return;
  }
  ASSERT_NE(event, events.end());
  EXPECT_EQ(net::NetLogEventPhase::BEGIN, event->phase);
  EXPECT_EQ(true, event->params.FindBool("is_qwac_attempt"));

  event = std::ranges::find(++event, events.end(),
                            NetLogEventType::CERT_VERIFY_PROC_PATH_BUILT,
                            &NetLogEntry::type);
  ASSERT_NE(event, events.end());
  EXPECT_EQ(net::NetLogEventPhase::BEGIN, event->phase);

  event = std::ranges::find(++event, events.end(),
                            NetLogEventType::CERT_VERIFY_PROC_PATH_BUILT,
                            &NetLogEntry::type);
  ASSERT_NE(event, events.end());
  EXPECT_EQ(net::NetLogEventPhase::END, event->phase);
  EXPECT_EQ(true, event->params.FindBool("is_valid"));
  pem_certs = event->params.FindList("certificates");
  ASSERT_TRUE(pem_certs);
  // The CERT_VERIFY_PROC_PATH_BUILT netlog for the 1-QWAC verification should
  // contain the QWAC cert chain.
  EXPECT_THAT(ParseNetLogCertificatesList(*pem_certs),
              testing::ElementsAre(leaf->GetDER(), eutl_intermediate.GetDER()));

  event = std::ranges::find(
      ++event, events.end(),
      NetLogEventType::CERT_VERIFY_PROC_PATH_BUILD_ATTEMPT, &NetLogEntry::type);
  ASSERT_NE(event, events.end());
  EXPECT_EQ(net::NetLogEventPhase::END, event->phase);
  EXPECT_EQ(true, event->params.FindBool("has_valid_path"));

  event = std::ranges::find(
      ++event, events.end(),
      NetLogEventType::CERT_VERIFY_PROC_PATH_BUILD_ATTEMPT, &NetLogEntry::type);
  ASSERT_EQ(event, events.end());
}

INSTANTIATE_TEST_SUITE_P(, CertVerifyProcBuiltin1QwacTest, testing::Bool());

class CertVerifyProcBuiltin2QwacTest : public CertVerifyProcBuiltinTest {
 public:
  void ExpectHistogramSample(const base::HistogramTester& histograms,
                             Verify2QwacBindingResult result) {
    histograms.ExpectUniqueSample("Net.CertVerifier.Qwac.2QwacBinding", result,
                                  1u);
  }
  void ExpectNoHistogramSample(const base::HistogramTester& histograms) {
    histograms.ExpectTotalCount("Net.CertVerifier.Qwac.2QwacBinding", 0);
  }
};

TEST_F(CertVerifyProcBuiltin2QwacTest, InvalidCertificate) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  intermediate->SetCertificatePolicies({"2.5.29.32.0"});  // anyPolicy

  leaf->SetCertificatePolicies({"0.4.0.194112.1.6"});  // QNCP-w-gen
  leaf->SetQwacQcStatements({bssl::der::Input(kEtsiQctWebOid)});
  leaf->SetExtendedKeyUsages({bssl::der::Input(kIdKpTlsBinding)});
  leaf->SetExtension(bssl::der::Input(bssl::kBasicConstraintsOid),
                     "invalid extension value", /*critical=*/true);

  InitializeVerifyProc(CreateParams(/*additional_trust_anchors=*/{}));
  AddMockEutlRoot(root->GetCertBuffer());

  {
    base::HistogramTester histograms;
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    int error = Verify2Qwac(leaf->GetX509CertificateChain(), "www.example.com",
                            &verify_result, &verify_net_log_source);

    EXPECT_THAT(error, IsError(ERR_CERT_INVALID));
    EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_INVALID);
    ExpectHistogramSample(histograms,
                          Verify2QwacBindingResult::kCertLeafParsingError);
  }
}

TEST_F(CertVerifyProcBuiltin2QwacTest, TwoQwacRequiresEutl) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  intermediate->SetCertificatePolicies({"2.5.29.32.0"});  // anyPolicy

  leaf->SetCertificatePolicies({"0.4.0.194112.1.6"});  // QNCP-w-gen
  leaf->SetQwacQcStatements({bssl::der::Input(kEtsiQctWebOid)});
  leaf->SetExtendedKeyUsages({bssl::der::Input(kIdKpTlsBinding)});

  InitializeVerifyProc(CreateParams(
      /*additional_trust_anchors=*/{root->GetX509Certificate()}));

  {
    base::HistogramTester histograms;
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    int error = Verify2Qwac(leaf->GetX509CertificateChain(), "www.example.com",
                            &verify_result, &verify_net_log_source);

    // If the root is not on the EUTL, a valid path cannot be found, even if
    // it's a normal root.
    EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
    EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_AUTHORITY_INVALID);
    ExpectHistogramSample(histograms,
                          Verify2QwacBindingResult::kCertAuthorityInvalid);

    // The path builder should have found the intermediate, but no root.
    EXPECT_EQ(leaf->GetCertBuffer(),
              verify_result.verified_cert->cert_buffer());
    ASSERT_EQ(1u, verify_result.verified_cert->intermediate_buffers().size());
    EXPECT_EQ(intermediate->GetCertBuffer(),
              verify_result.verified_cert->intermediate_buffers()[0].get());
  }

  InitializeVerifyProc(CreateParams(/*additional_trust_anchors=*/{}));
  AddMockEutlRoot(root->GetCertBuffer());

  {
    base::HistogramTester histograms;
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    int error = Verify2Qwac(leaf->GetX509CertificateChain(), "www.example.com",
                            &verify_result, &verify_net_log_source);

    // If the root is on the EUTL, the certificate verifies successfully with
    // the QWAC status set.
    EXPECT_THAT(error, IsOk());
    EXPECT_FALSE(IsCertStatusError(verify_result.cert_status));
    ExpectNoHistogramSample(histograms);

    // The verified chain has the full cert chain.
    EXPECT_EQ(leaf->GetCertBuffer(),
              verify_result.verified_cert->cert_buffer());
    ASSERT_EQ(2u, verify_result.verified_cert->intermediate_buffers().size());
    EXPECT_EQ(intermediate->GetCertBuffer(),
              verify_result.verified_cert->intermediate_buffers()[0].get());
    EXPECT_EQ(root->GetCertBuffer(),
              verify_result.verified_cert->intermediate_buffers()[1].get());
  }
}

TEST_F(CertVerifyProcBuiltin2QwacTest, TwoQwacRequiresPolicies) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  intermediate->SetCertificatePolicies({"2.5.29.32.0"});  // anyPolicy

  leaf->SetQwacQcStatements({bssl::der::Input(kEtsiQctWebOid)});
  leaf->SetExtendedKeyUsages({bssl::der::Input(kIdKpTlsBinding)});

  InitializeVerifyProc(CreateParams(/*additional_trust_anchors=*/{}));
  AddMockEutlRoot(root->GetCertBuffer());

  {
    base::HistogramTester histograms;
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    int error = Verify2Qwac(leaf->GetX509CertificateChain(), "www.example.com",
                            &verify_result, &verify_net_log_source);

    EXPECT_THAT(error, IsError(ERR_CERT_INVALID));
    EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_INVALID);
    ExpectHistogramSample(histograms,
                          Verify2QwacBindingResult::kCertInconsistentBits);
  }

  leaf->SetCertificatePolicies({"0.4.0.194112.1.6"});  // QNCP-w-gen

  {
    base::HistogramTester histograms;
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    int error = Verify2Qwac(leaf->GetX509CertificateChain(), "www.example.com",
                            &verify_result, &verify_net_log_source);

    EXPECT_THAT(error, IsOk());
    EXPECT_FALSE(IsCertStatusError(verify_result.cert_status));
    ExpectNoHistogramSample(histograms);
  }
}

TEST_F(CertVerifyProcBuiltin2QwacTest, TwoQwacRequiresQcStatements) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  intermediate->SetCertificatePolicies({"2.5.29.32.0"});  // anyPolicy

  leaf->SetCertificatePolicies({"0.4.0.194112.1.6"});  // QNCP-w-gen
  leaf->SetExtendedKeyUsages({bssl::der::Input(kIdKpTlsBinding)});

  InitializeVerifyProc(CreateParams(/*additional_trust_anchors=*/{}));
  AddMockEutlRoot(root->GetCertBuffer());

  {
    base::HistogramTester histograms;
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    int error = Verify2Qwac(leaf->GetX509CertificateChain(), "www.example.com",
                            &verify_result, &verify_net_log_source);

    EXPECT_THAT(error, IsError(ERR_CERT_INVALID));
    EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_INVALID);
    ExpectHistogramSample(histograms,
                          Verify2QwacBindingResult::kCertInconsistentBits);
  }

  leaf->SetQwacQcStatements({bssl::der::Input(kEtsiQctWebOid)});

  {
    base::HistogramTester histograms;
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    int error = Verify2Qwac(leaf->GetX509CertificateChain(), "www.example.com",
                            &verify_result, &verify_net_log_source);

    EXPECT_THAT(error, IsOk());
    EXPECT_FALSE(IsCertStatusError(verify_result.cert_status));
    ExpectNoHistogramSample(histograms);
  }
}

TEST_F(CertVerifyProcBuiltin2QwacTest, TwoQwacRequiresEku) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  intermediate->SetCertificatePolicies({"2.5.29.32.0"});  // anyPolicy

  leaf->SetCertificatePolicies({"0.4.0.194112.1.6"});  // QNCP-w-gen
  leaf->SetQwacQcStatements({bssl::der::Input(kEtsiQctWebOid)});

  InitializeVerifyProc(CreateParams(/*additional_trust_anchors=*/{}));
  AddMockEutlRoot(root->GetCertBuffer());

  {
    base::HistogramTester histograms;
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    int error = Verify2Qwac(leaf->GetX509CertificateChain(), "www.example.com",
                            &verify_result, &verify_net_log_source);

    EXPECT_THAT(error, IsError(ERR_CERT_INVALID));
    EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_INVALID);
    ExpectHistogramSample(histograms,
                          Verify2QwacBindingResult::kCertInconsistentBits);
  }

  leaf->SetExtendedKeyUsages({bssl::der::Input(kIdKpTlsBinding)});

  {
    base::HistogramTester histograms;
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    int error = Verify2Qwac(leaf->GetX509CertificateChain(), "www.example.com",
                            &verify_result, &verify_net_log_source);

    EXPECT_THAT(error, IsOk());
    EXPECT_FALSE(IsCertStatusError(verify_result.cert_status));
    ExpectNoHistogramSample(histograms);
  }
}

TEST_F(CertVerifyProcBuiltin2QwacTest, TwoQwacVerifiesName) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  intermediate->SetCertificatePolicies({"2.5.29.32.0"});  // anyPolicy

  leaf->SetCertificatePolicies({"0.4.0.194112.1.6"});  // QNCP-w-gen
  leaf->SetQwacQcStatements({bssl::der::Input(kEtsiQctWebOid)});
  leaf->SetExtendedKeyUsages({bssl::der::Input(kIdKpTlsBinding)});

  InitializeVerifyProc(CreateParams(/*additional_trust_anchors=*/{}));
  AddMockEutlRoot(root->GetCertBuffer());

  {
    base::HistogramTester histograms;
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    TestCompletionCallback callback;
    int error = Verify2Qwac(leaf->GetX509CertificateChain(), "www.wrong.com",
                            &verify_result, &verify_net_log_source);

    EXPECT_THAT(error, IsError(ERR_CERT_COMMON_NAME_INVALID));
    EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_COMMON_NAME_INVALID);
    ExpectHistogramSample(histograms,
                          Verify2QwacBindingResult::kCertNameInvalid);
  }

  {
    base::HistogramTester histograms;
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    int error = Verify2Qwac(leaf->GetX509CertificateChain(), "www.example.com",
                            &verify_result, &verify_net_log_source);

    EXPECT_THAT(error, IsOk());
    EXPECT_FALSE(IsCertStatusError(verify_result.cert_status));
    ExpectNoHistogramSample(histograms);
  }
}

TEST_F(CertVerifyProcBuiltin2QwacTest, TwoQwacVerifiesValidityDate) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  intermediate->SetCertificatePolicies({"2.5.29.32.0"});  // anyPolicy

  leaf->SetCertificatePolicies({"0.4.0.194112.1.6"});  // QNCP-w-gen
  leaf->SetQwacQcStatements({bssl::der::Input(kEtsiQctWebOid)});
  leaf->SetExtendedKeyUsages({bssl::der::Input(kIdKpTlsBinding)});
  leaf->SetValidity(base::Time::Now() - base::Days(2),
                    base::Time::Now() - base::Days(1));

  InitializeVerifyProc(CreateParams(/*additional_trust_anchors=*/{}));
  AddMockEutlRoot(root->GetCertBuffer());

  {
    base::HistogramTester histograms;
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    int error = Verify2Qwac(leaf->GetX509CertificateChain(), "www.example.com",
                            &verify_result, &verify_net_log_source);

    EXPECT_THAT(error, IsError(ERR_CERT_DATE_INVALID));
    EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_DATE_INVALID);
    ExpectHistogramSample(histograms,
                          Verify2QwacBindingResult::kCertDateInvalid);
  }

  // 2-QWACs are not bound by BR lifetime limits, so we don't enforce any
  // validity too long errors.
  leaf->SetValidity(base::Time::Now() - base::Days(2),
                    base::Time::Now() + base::Days(3650));

  {
    base::HistogramTester histograms;
    CertVerifyResult verify_result;
    NetLogSource verify_net_log_source;
    int error = Verify2Qwac(leaf->GetX509CertificateChain(), "www.example.com",
                            &verify_result, &verify_net_log_source);

    EXPECT_THAT(error, IsOk());
    EXPECT_FALSE(IsCertStatusError(verify_result.cert_status));
    ExpectNoHistogramSample(histograms);
  }
}

class CertVerifyProcBuiltin2QwacBindingTest : public CertVerifyProcBuiltinTest {
 public:
  void ExpectHistogramSample(const base::HistogramTester& histograms,
                             Verify2QwacBindingResult result) {
    histograms.ExpectUniqueSample("Net.CertVerifier.Qwac.2QwacBinding", result,
                                  1u);
  }
};

TEST_F(CertVerifyProcBuiltin2QwacBindingTest, TestValidBinding) {
  auto [tls_leaf, tls_root] = CertBuilder::CreateSimpleChain2();

  TwoQwacCertBindingBuilder binding_builder;
  binding_builder.SetBoundCerts({tls_leaf->GetDER()});
  std::string jws = binding_builder.GetJWS();

  InitializeVerifyProc(CreateParams(/*additional_trust_anchors=*/{}));
  AddMockEutlRoot(binding_builder.GetRootBuilder()->GetCertBuffer());

  base::HistogramTester histograms;
  RecordingNetLogObserver net_log_observer(NetLogCaptureMode::kDefault);
  NetLogSource verify_net_log_source;
  scoped_refptr<X509Certificate> verified_2qwac = Verify2QwacBinding(
      jws, "www.example.com", base::as_byte_span(tls_leaf->GetDER()),
      &verify_net_log_source);
  ASSERT_TRUE(verified_2qwac);
  EXPECT_TRUE(verified_2qwac->EqualsIncludingChain(
      binding_builder.GetLeafBuilder()->GetX509CertificateFullChain().get()));
  ExpectHistogramSample(histograms,
                        Verify2QwacBindingResult::kValid2QwacBinding);

  auto events = net_log_observer.GetEntriesForSource(verify_net_log_source);
  auto event =
      std::ranges::find(events, NetLogEventType::CERT_VERIFY_PROC_2QWAC_BINDING,
                        &NetLogEntry::type);
  ASSERT_NE(event, events.end());
  EXPECT_EQ(net::NetLogEventPhase::BEGIN, event->phase);
  EXPECT_EQ(jws, base::optional_ref(event->params.FindString("binding")));
  EXPECT_EQ("www.example.com",
            base::optional_ref(event->params.FindString("host")));
  EXPECT_EQ(base::as_byte_span(tls_leaf->GetDER()),
            ParsePemCertificate(event->params.FindString("tls_certificate")));

  event = std::ranges::find(++event, events.end(),
                            NetLogEventType::CERT_VERIFY_PROC_2QWAC,
                            &NetLogEntry::type);
  ASSERT_NE(event, events.end());
  EXPECT_EQ(net::NetLogEventPhase::BEGIN, event->phase);

  event = std::ranges::find(++event, events.end(),
                            NetLogEventType::CERT_VERIFY_PROC_2QWAC,
                            &NetLogEntry::type);
  ASSERT_NE(event, events.end());
  EXPECT_EQ(net::NetLogEventPhase::END, event->phase);

  EXPECT_FALSE(event->params.Find("net_error"));
  base::Value::Dict* pem_verified_certs =
      event->params.FindDict("verified_cert");
  ASSERT_TRUE(pem_verified_certs);
  EXPECT_THAT(ParseNetLogCertificatesDict(*pem_verified_certs),
              testing::ElementsAre(binding_builder.GetLeafBuilder()->GetDER(),
                                   binding_builder.GetRootBuilder()->GetDER()));

  event = std::ranges::find(++event, events.end(),
                            NetLogEventType::CERT_VERIFY_PROC_2QWAC_BINDING,
                            &NetLogEntry::type);
  ASSERT_NE(event, events.end());
  EXPECT_EQ(net::NetLogEventPhase::END, event->phase);
  EXPECT_FALSE(event->params.Find("net_error"));
  EXPECT_EQ(true, event->params.FindBool("is_valid_2qwac_binding"));
}

TEST_F(CertVerifyProcBuiltin2QwacBindingTest, TestBindingFailsParsing) {
  auto [tls_leaf, tls_root] = CertBuilder::CreateSimpleChain2();

  TwoQwacCertBindingBuilder binding_builder;
  binding_builder.SetBoundCerts({tls_leaf->GetDER()});
  std::string jws = "invalid:" + binding_builder.GetJWS();

  InitializeVerifyProc(CreateParams(/*additional_trust_anchors=*/{}));
  AddMockEutlRoot(binding_builder.GetRootBuilder()->GetCertBuffer());

  RecordingNetLogObserver net_log_observer(NetLogCaptureMode::kDefault);
  base::HistogramTester histograms;
  NetLogSource verify_net_log_source;
  EXPECT_FALSE(Verify2QwacBinding(jws, "www.example.com",
                                  base::as_byte_span(tls_leaf->GetDER()),
                                  &verify_net_log_source));
  ExpectHistogramSample(histograms,
                        Verify2QwacBindingResult::kBindingParsingError);

  auto end_events = net_log_observer.GetEntriesForSourceWithType(
      verify_net_log_source, NetLogEventType::CERT_VERIFY_PROC_2QWAC_BINDING,
      net::NetLogEventPhase::END);
  ASSERT_EQ(1U, end_events.size());
  auto& event = end_events[0];
  EXPECT_EQ(ERR_FAILED, event.params.FindInt("net_error"));
  EXPECT_EQ("binding parsing error: base64 decoding header error",
            base::optional_ref(event.params.FindString("error_description")));
}

TEST_F(CertVerifyProcBuiltin2QwacBindingTest, TestBindingInvalidSignature) {
  auto [tls_leaf, tls_root] = CertBuilder::CreateSimpleChain2();

  TwoQwacCertBindingBuilder binding_builder;
  binding_builder.SetBoundCerts({tls_leaf->GetDER()});
  std::string jws = binding_builder.GetJWSWithInvalidSignature();

  InitializeVerifyProc(CreateParams(/*additional_trust_anchors=*/{}));
  AddMockEutlRoot(binding_builder.GetRootBuilder()->GetCertBuffer());

  RecordingNetLogObserver net_log_observer(NetLogCaptureMode::kDefault);
  base::HistogramTester histograms;
  NetLogSource verify_net_log_source;
  EXPECT_FALSE(Verify2QwacBinding(jws, "www.example.com",
                                  base::as_byte_span(tls_leaf->GetDER()),
                                  &verify_net_log_source));
  ExpectHistogramSample(histograms,
                        Verify2QwacBindingResult::kBindingSignatureInvalid);

  auto end_events = net_log_observer.GetEntriesForSourceWithType(
      verify_net_log_source, NetLogEventType::CERT_VERIFY_PROC_2QWAC_BINDING,
      net::NetLogEventPhase::END);
  ASSERT_EQ(1U, end_events.size());
  auto& event = end_events[0];
  EXPECT_EQ(ERR_FAILED, event.params.FindInt("net_error"));
  EXPECT_EQ("binding signature invalid",
            base::optional_ref(event.params.FindString("error_description")));
}

TEST_F(CertVerifyProcBuiltin2QwacBindingTest,
       TestBinding2QwacFailsVerification) {
  auto [tls_leaf, tls_root] = CertBuilder::CreateSimpleChain2();

  TwoQwacCertBindingBuilder binding_builder;
  binding_builder.SetBoundCerts({tls_leaf->GetDER()});
  std::string jws = binding_builder.GetJWS();

  // The qwac root is not added to the EUTL, so cert verification of the 2-QWAC
  // certificate should fail.
  InitializeVerifyProc(CreateParams(/*additional_trust_anchors=*/{}));

  RecordingNetLogObserver net_log_observer(NetLogCaptureMode::kDefault);
  base::HistogramTester histograms;
  NetLogSource verify_net_log_source;
  EXPECT_FALSE(Verify2QwacBinding(jws, "www.example.com",
                                  base::as_byte_span(tls_leaf->GetDER()),
                                  &verify_net_log_source));
  ExpectHistogramSample(histograms,
                        Verify2QwacBindingResult::kCertAuthorityInvalid);

  auto end_events = net_log_observer.GetEntriesForSourceWithType(
      verify_net_log_source, NetLogEventType::CERT_VERIFY_PROC_2QWAC_BINDING,
      net::NetLogEventPhase::END);
  ASSERT_EQ(1U, end_events.size());
  auto& event = end_events[0];
  EXPECT_EQ(ERR_FAILED, event.params.FindInt("net_error"));
  EXPECT_EQ("2-QWAC cert verify failed",
            base::optional_ref(event.params.FindString("error_description")));
}

TEST_F(CertVerifyProcBuiltin2QwacBindingTest, TestTlsCertIsNotBound) {
  auto [bound_leaf, bound_root] = CertBuilder::CreateSimpleChain2();
  auto [tls_leaf, tls_root] = CertBuilder::CreateSimpleChain2();

  TwoQwacCertBindingBuilder binding_builder;
  binding_builder.SetBoundCerts({bound_leaf->GetDER()});
  std::string jws = binding_builder.GetJWS();

  InitializeVerifyProc(CreateParams(/*additional_trust_anchors=*/{}));
  AddMockEutlRoot(binding_builder.GetRootBuilder()->GetCertBuffer());

  RecordingNetLogObserver net_log_observer(NetLogCaptureMode::kDefault);
  base::HistogramTester histograms;
  NetLogSource verify_net_log_source;
  EXPECT_FALSE(Verify2QwacBinding(jws, "www.example.com",
                                  base::as_byte_span(tls_leaf->GetDER()),
                                  &verify_net_log_source));
  ExpectHistogramSample(histograms, Verify2QwacBindingResult::kTlsCertNotBound);

  auto end_events = net_log_observer.GetEntriesForSourceWithType(
      verify_net_log_source, NetLogEventType::CERT_VERIFY_PROC_2QWAC_BINDING,
      net::NetLogEventPhase::END);
  ASSERT_EQ(1U, end_events.size());
  auto& event = end_events[0];
  EXPECT_EQ(ERR_FAILED, event.params.FindInt("net_error"));
  EXPECT_EQ("TLS cert not bound",
            base::optional_ref(event.params.FindString("error_description")));
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
  auto event = std::ranges::find_if(events, [](const NetLogEntry& e) {
    return e.type == NetLogEventType::CERT_VERIFY_PROC_PATH_BUILD_ATTEMPT &&
           e.phase == NetLogEventPhase::END;
  });
  ASSERT_NE(event, events.end());

  // The path builder gives up before it finishes all the invalid paths.
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_AUTHORITY_INVALID);
  EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  EXPECT_EQ(true, event->params.FindBool("exceeded_iteration_limit"));
}

class CertVerifyProcBuiltinSelfSignedTest
    : public CertVerifyProcBuiltinTest,
      public testing::WithParamInterface<bool> {
 public:
  CertVerifyProcBuiltinSelfSignedTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(
          features::kSelfSignedLocalNetworkInterstitial);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kSelfSignedLocalNetworkInterstitial);
    }
  }

  scoped_refptr<X509Certificate> CreateSelfSigned(
      std::string_view subject_dns_name) {
    // Create a chain of size 1, which will result in a self-signed certificate
    std::vector<std::unique_ptr<CertBuilder>> builders =
        CertBuilder::CreateSimpleChain(1);
    base::Time not_before = base::Time::Now() - base::Days(1);
    base::Time not_after = base::Time::Now() + base::Days(1);
    builders[0]->SetValidity(not_before, not_after);
    builders[0]->SetSubjectAltName(subject_dns_name);
    return builders[0]->GetX509Certificate();
  }

  scoped_refptr<X509Certificate> CreateSelfSignedIPSubject(
      std::string_view ip_address) {
    // Create a chain of size 1, which will result in a self-signed certificate
    std::vector<std::unique_ptr<CertBuilder>> builders =
        CertBuilder::CreateSimpleChain(1);
    base::Time not_before = base::Time::Now() - base::Days(1);
    base::Time not_after = base::Time::Now() + base::Days(1);
    builders[0]->SetValidity(not_before, not_after);
    IPAddress ip;
    if (!ParseURLHostnameToAddress(ip_address, &ip)) {
      ADD_FAILURE() << "Failed to parse IP address";
    }

    builders[0]->SetSubjectAltNames({}, {ip});
    return builders[0]->GetX509Certificate();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(CertVerifyProcBuiltinSelfSignedTest,
       SelfSignedCertOnLocalNetworkHostname) {
  scoped_refptr<X509Certificate> cert = CreateSelfSigned("testurl.local");

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(cert, "testurl.local", 0, &verify_result, &verify_net_log_source,
         callback.callback());
  int error = callback.WaitForResult();

  if (GetParam()) {
    EXPECT_TRUE(verify_result.cert_status &
                CERT_STATUS_SELF_SIGNED_LOCAL_NETWORK);
    EXPECT_THAT(error, IsError(ERR_CERT_SELF_SIGNED_LOCAL_NETWORK));
  } else {
    EXPECT_FALSE(verify_result.cert_status &
                 CERT_STATUS_SELF_SIGNED_LOCAL_NETWORK);
    EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  }
}

TEST_P(CertVerifyProcBuiltinSelfSignedTest, SelfSignedCertOnLocalNetworkIP) {
  scoped_refptr<X509Certificate> cert =
      CreateSelfSignedIPSubject("192.168.0.1");

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(cert, "192.168.0.1", 0, &verify_result, &verify_net_log_source,
         callback.callback());
  int error = callback.WaitForResult();

  if (GetParam()) {
    EXPECT_TRUE(verify_result.cert_status &
                CERT_STATUS_SELF_SIGNED_LOCAL_NETWORK);
    EXPECT_THAT(error, IsError(ERR_CERT_SELF_SIGNED_LOCAL_NETWORK));
  } else {
    EXPECT_FALSE(verify_result.cert_status &
                 CERT_STATUS_SELF_SIGNED_LOCAL_NETWORK);
    EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  }
}

TEST_P(CertVerifyProcBuiltinSelfSignedTest, SelfSignedCertOnLocalNetworkIPv6) {
  scoped_refptr<X509Certificate> cert =
      CreateSelfSignedIPSubject("[fc00:0:0:0:0:0:0:0]");

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(cert, "fc00:0:0:0:0:0:0:0", 0, &verify_result, &verify_net_log_source,
         callback.callback());
  int error = callback.WaitForResult();

  if (GetParam()) {
    EXPECT_TRUE(verify_result.cert_status &
                CERT_STATUS_SELF_SIGNED_LOCAL_NETWORK);
    EXPECT_THAT(error, IsError(ERR_CERT_SELF_SIGNED_LOCAL_NETWORK));
  } else {
    EXPECT_FALSE(verify_result.cert_status &
                 CERT_STATUS_SELF_SIGNED_LOCAL_NETWORK);
    EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  }
}

TEST_P(CertVerifyProcBuiltinSelfSignedTest, NonSelfSignedCertOnLocalNetwork) {
  std::vector<std::unique_ptr<CertBuilder>> builders =
      CertBuilder::CreateSimpleChain(2);

  base::Time not_before = base::Time::Now() - base::Days(2);
  base::Time not_after = base::Time::Now() - base::Days(2);
  builders[0]->SetValidity(not_before, not_after);
  builders[0]->SetSubjectAltName("testurl.local");

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(builders[0]->GetX509CertificateChain(), "testurl.local", 0,
         &verify_result, &verify_net_log_source, callback.callback());
  int error = callback.WaitForResult();

  EXPECT_FALSE(verify_result.cert_status &
               CERT_STATUS_SELF_SIGNED_LOCAL_NETWORK);
  EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
}

TEST_P(CertVerifyProcBuiltinSelfSignedTest,
       SelfSignedCertNotLocalNetworkHostname) {
  scoped_refptr<X509Certificate> cert = CreateSelfSigned("www.example.com");

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(cert, "www.example.com", 0, &verify_result, &verify_net_log_source,
         callback.callback());
  int error = callback.WaitForResult();

  EXPECT_FALSE(verify_result.cert_status &
               CERT_STATUS_SELF_SIGNED_LOCAL_NETWORK);
  EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
}

TEST_P(CertVerifyProcBuiltinSelfSignedTest, SelfSignedCertNotLocalNetworkIP) {
  scoped_refptr<X509Certificate> cert = CreateSelfSignedIPSubject("8.8.8.8");

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(cert, "8.8.8.8", 0, &verify_result, &verify_net_log_source,
         callback.callback());
  int error = callback.WaitForResult();

  EXPECT_FALSE(verify_result.cert_status &
               CERT_STATUS_SELF_SIGNED_LOCAL_NETWORK);
  EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
}

TEST_P(CertVerifyProcBuiltinSelfSignedTest, SelfSignedCertNotLocalNetworkIPv6) {
  scoped_refptr<X509Certificate> cert =
      CreateSelfSignedIPSubject("[2001:4860:4860::8888]");

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(cert, "2001:4860:4860::8888", 0, &verify_result,
         &verify_net_log_source, callback.callback());
  int error = callback.WaitForResult();

  EXPECT_FALSE(verify_result.cert_status &
               CERT_STATUS_SELF_SIGNED_LOCAL_NETWORK);
  EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
}

TEST_P(CertVerifyProcBuiltinSelfSignedTest,
       SelfSignedCertOnLocalNetworkHostnameNameMismatchTakesPrecedence) {
  scoped_refptr<X509Certificate> cert = CreateSelfSigned("nottesturl.local");

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(cert, "testurl.local", 0, &verify_result, &verify_net_log_source,
         callback.callback());
  int error = callback.WaitForResult();

  if (GetParam()) {
    EXPECT_TRUE(verify_result.cert_status &
                CERT_STATUS_SELF_SIGNED_LOCAL_NETWORK);
    EXPECT_THAT(error, IsError(ERR_CERT_COMMON_NAME_INVALID));
  } else {
    EXPECT_FALSE(verify_result.cert_status &
                 CERT_STATUS_SELF_SIGNED_LOCAL_NETWORK);
    EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  }
}

INSTANTIATE_TEST_SUITE_P(SelfSignedInterstitial,
                         CertVerifyProcBuiltinSelfSignedTest,
                         testing::Bool());

}  // namespace net
