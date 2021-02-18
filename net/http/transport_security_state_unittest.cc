// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/transport_security_state.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/rand_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "crypto/openssl_util.h"
#include "crypto/sha2.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_cert_types.h"
#include "net/cert/x509_certificate.h"
#include "net/extras/preload_data/decoder.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/net_buildflags.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/tools/huffman_trie/bit_writer.h"
#include "net/tools/huffman_trie/trie/trie_bit_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace net {

namespace {

namespace test_default {
#include "net/http/transport_security_state_static_unittest_default.h"
}
namespace test1 {
#include "net/http/transport_security_state_static_unittest1.h"
}
namespace test2 {
#include "net/http/transport_security_state_static_unittest2.h"
}
namespace test3 {
#include "net/http/transport_security_state_static_unittest3.h"
}

const char kHost[] = "example.test";
const uint16_t kPort = 443;
const char kReportUri[] = "http://report-example.test/test";
const char kExpectCTStaticHostname[] = "expect-ct.preloaded.test";
const char kExpectCTStaticReportURI[] =
    "http://report-uri.preloaded.test/expect-ct";

const char* const kGoodPath[] = {
    "sha256/AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
    "sha256/fzP+pVAbH0hRoUphJKenIP8+2tD/d2QH9J+kQNieM6Q=",
    "sha256/9vRUVdjloCa4wXUKfDWotV5eUXYD7vu0v0z9SRzQdzg=",
    "sha256/Nn8jk5By4Vkq6BeOVZ7R7AC6XUUBZsWmUbJR1f1Y5FY=",
    nullptr,
};

const char* const kBadPath[] = {
    "sha256/1111111111111111111111111111111111111111111=",
    "sha256/2222222222222222222222222222222222222222222=",
    "sha256/3333333333333333333333333333333333333333333=",
    nullptr,
};

// Constructs a SignedCertificateTimestampAndStatus with the given information
// and appends it to |sct_list|.
void MakeTestSCTAndStatus(ct::SignedCertificateTimestamp::Origin origin,
                          const std::string& log_id,
                          const std::string& extensions,
                          const std::string& signature_data,
                          const base::Time& timestamp,
                          ct::SCTVerifyStatus status,
                          SignedCertificateTimestampAndStatusList* sct_list) {
  scoped_refptr<net::ct::SignedCertificateTimestamp> sct(
      new net::ct::SignedCertificateTimestamp());
  sct->version = net::ct::SignedCertificateTimestamp::V1;
  sct->log_id = log_id;
  sct->extensions = extensions;
  sct->timestamp = timestamp;
  sct->signature.signature_data = signature_data;
  sct->origin = origin;
  sct_list->push_back(net::SignedCertificateTimestampAndStatus(sct, status));
}

// A mock ReportSenderInterface that just remembers the latest report
// URI and report to be sent.
class MockCertificateReportSender
    : public TransportSecurityState::ReportSenderInterface {
 public:
  MockCertificateReportSender() = default;
  ~MockCertificateReportSender() override = default;

  void Send(
      const GURL& report_uri,
      base::StringPiece content_type,
      base::StringPiece report,
      const NetworkIsolationKey& network_isolation_key,
      base::OnceCallback<void()> success_callback,
      base::OnceCallback<void(const GURL&, int, int)> error_callback) override {
    latest_report_uri_ = report_uri;
    latest_report_.assign(report.data(), report.size());
    latest_content_type_.assign(content_type.data(), content_type.size());
    latest_network_isolation_key_ = network_isolation_key;
  }

  void Clear() {
    latest_report_uri_ = GURL();
    latest_report_ = std::string();
    latest_content_type_ = std::string();
    latest_network_isolation_key_ = NetworkIsolationKey();
  }

  const GURL& latest_report_uri() { return latest_report_uri_; }
  const std::string& latest_report() { return latest_report_; }
  const std::string& latest_content_type() { return latest_content_type_; }
  const NetworkIsolationKey& latest_network_isolation_key() {
    return latest_network_isolation_key_;
  }

 private:
  GURL latest_report_uri_;
  std::string latest_report_;
  std::string latest_content_type_;
  NetworkIsolationKey latest_network_isolation_key_;
};

// A mock ReportSenderInterface that simulates a net error on every report sent.
class MockFailingCertificateReportSender
    : public TransportSecurityState::ReportSenderInterface {
 public:
  MockFailingCertificateReportSender() : net_error_(ERR_CONNECTION_FAILED) {}
  ~MockFailingCertificateReportSender() override = default;

  int net_error() { return net_error_; }

  // TransportSecurityState::ReportSenderInterface:
  void Send(
      const GURL& report_uri,
      base::StringPiece content_type,
      base::StringPiece report,
      const NetworkIsolationKey& network_isolation_key,
      base::OnceCallback<void()> success_callback,
      base::OnceCallback<void(const GURL&, int, int)> error_callback) override {
    ASSERT_FALSE(error_callback.is_null());
    std::move(error_callback).Run(report_uri, net_error_, 0);
  }

 private:
  const int net_error_;
};

// A mock ExpectCTReporter that remembers the latest violation that was
// reported and the number of violations reported.
class MockExpectCTReporter : public TransportSecurityState::ExpectCTReporter {
 public:
  MockExpectCTReporter() : num_failures_(0) {}
  ~MockExpectCTReporter() override = default;

  void OnExpectCTFailed(
      const HostPortPair& host_port_pair,
      const GURL& report_uri,
      base::Time expiration,
      const X509Certificate* validated_certificate_chain,
      const X509Certificate* served_certificate_chain,
      const SignedCertificateTimestampAndStatusList&
          signed_certificate_timestamps,
      const NetworkIsolationKey& network_isolation_key) override {
    num_failures_++;
    host_port_pair_ = host_port_pair;
    report_uri_ = report_uri;
    expiration_ = expiration;
    served_certificate_chain_ = served_certificate_chain;
    validated_certificate_chain_ = validated_certificate_chain;
    signed_certificate_timestamps_ = signed_certificate_timestamps;
    network_isolation_key_ = network_isolation_key;
  }

  const HostPortPair& host_port_pair() const { return host_port_pair_; }
  const GURL& report_uri() const { return report_uri_; }
  const base::Time& expiration() const { return expiration_; }
  uint32_t num_failures() const { return num_failures_; }
  const X509Certificate* served_certificate_chain() const {
    return served_certificate_chain_;
  }
  const X509Certificate* validated_certificate_chain() const {
    return validated_certificate_chain_;
  }
  const SignedCertificateTimestampAndStatusList& signed_certificate_timestamps()
      const {
    return signed_certificate_timestamps_;
  }
  const NetworkIsolationKey& network_isolation_key() const {
    return network_isolation_key_;
  }

 private:
  HostPortPair host_port_pair_;
  GURL report_uri_;
  base::Time expiration_;
  uint32_t num_failures_;
  const X509Certificate* served_certificate_chain_;
  const X509Certificate* validated_certificate_chain_;
  SignedCertificateTimestampAndStatusList signed_certificate_timestamps_;
  NetworkIsolationKey network_isolation_key_;
};

class MockRequireCTDelegate : public TransportSecurityState::RequireCTDelegate {
 public:
  MOCK_METHOD3(IsCTRequiredForHost,
               CTRequirementLevel(const std::string& hostname,
                                  const X509Certificate* chain,
                                  const HashValueVector& hashes));
};

void CompareCertificateChainWithList(
    const scoped_refptr<X509Certificate>& cert_chain,
    const base::Value* cert_list) {
  ASSERT_TRUE(cert_chain);
  ASSERT_TRUE(cert_list->is_list());
  std::vector<std::string> pem_encoded_chain;
  cert_chain->GetPEMEncodedChain(&pem_encoded_chain);
  ASSERT_EQ(pem_encoded_chain.size(), cert_list->GetList().size());

  for (size_t i = 0; i < pem_encoded_chain.size(); i++) {
    const std::string& list_cert = cert_list->GetList()[i].GetString();
    EXPECT_EQ(pem_encoded_chain[i], list_cert);
  }
}

void CheckHPKPReport(
    const std::string& report,
    const HostPortPair& host_port_pair,
    bool include_subdomains,
    const std::string& noted_hostname,
    const scoped_refptr<X509Certificate>& served_certificate_chain,
    const scoped_refptr<X509Certificate>& validated_certificate_chain,
    const HashValueVector& known_pins) {
  base::Optional<base::Value> value = base::JSONReader::Read(report);
  ASSERT_TRUE(value.has_value());
  const base::Value& report_dict = value.value();
  ASSERT_TRUE(report_dict.is_dict());

  const std::string* report_hostname = report_dict.FindStringKey("hostname");
  ASSERT_TRUE(report_hostname);
  EXPECT_EQ(host_port_pair.host(), *report_hostname);

  base::Optional<int> report_port = report_dict.FindIntKey("port");
  ASSERT_TRUE(report_port.has_value());
  EXPECT_EQ(host_port_pair.port(), report_port.value());

  base::Optional<bool> report_include_subdomains =
      report_dict.FindBoolKey("include-subdomains");
  ASSERT_TRUE(report_include_subdomains.has_value());
  EXPECT_EQ(include_subdomains, report_include_subdomains.value());

  const std::string* report_noted_hostname =
      report_dict.FindStringKey("noted-hostname");
  ASSERT_TRUE(report_noted_hostname);
  EXPECT_EQ(noted_hostname, *report_noted_hostname);

  // TODO(estark): check times in RFC3339 format.

  const std::string* report_expiration =
      report_dict.FindStringKey("effective-expiration-date");
  ASSERT_TRUE(report_expiration);
  EXPECT_FALSE(report_expiration->empty());

  const std::string* report_date = report_dict.FindStringKey("date-time");
  ASSERT_TRUE(report_date);
  EXPECT_FALSE(report_date->empty());

  const base::Value* report_served_certificate_chain =
      report_dict.FindKey("served-certificate-chain");
  ASSERT_TRUE(report_served_certificate_chain);
  ASSERT_NO_FATAL_FAILURE(CompareCertificateChainWithList(
      served_certificate_chain, report_served_certificate_chain));

  const base::Value* report_validated_certificate_chain =
      report_dict.FindKey("validated-certificate-chain");
  ASSERT_TRUE(report_validated_certificate_chain);
  ASSERT_NO_FATAL_FAILURE(CompareCertificateChainWithList(
      validated_certificate_chain, report_validated_certificate_chain));
}

bool operator==(const TransportSecurityState::STSState& lhs,
                const TransportSecurityState::STSState& rhs) {
  return lhs.last_observed == rhs.last_observed && lhs.expiry == rhs.expiry &&
         lhs.upgrade_mode == rhs.upgrade_mode &&
         lhs.include_subdomains == rhs.include_subdomains &&
         lhs.domain == rhs.domain;
}

bool operator==(const TransportSecurityState::PKPState& lhs,
                const TransportSecurityState::PKPState& rhs) {
  return lhs.last_observed == rhs.last_observed && lhs.expiry == rhs.expiry &&
         lhs.spki_hashes == rhs.spki_hashes &&
         lhs.bad_spki_hashes == rhs.bad_spki_hashes &&
         lhs.include_subdomains == rhs.include_subdomains &&
         lhs.domain == rhs.domain && lhs.report_uri == rhs.report_uri;
}

// Creates a unique new host name every time it's called. Tests should not
// depend on the exact domain names, as they may vary depending on what other
// tests have been run by the same process. Intended for Expect-CT pruning
// tests, which add a lot of domains.
std::string CreateUniqueHostName() {
  static int count = 0;
  return base::StringPrintf("%i.test", ++count);
}

// As with CreateUniqueHostName(), returns a unique NetworkIsolationKey for use
// with Expect-CT pruning tests.
NetworkIsolationKey CreateUniqueNetworkIsolationKey(bool is_transient) {
  if (is_transient)
    return NetworkIsolationKey::CreateTransient();
  SchemefulSite site = SchemefulSite(url::Origin::CreateFromNormalizedTuple(
      "https", CreateUniqueHostName(), 443));
  return NetworkIsolationKey(site /* top_frame_site */, site /* frame_site */);
}

}  // namespace

class TransportSecurityStateTest : public ::testing::Test,
                                   public WithTaskEnvironment {
 public:
  TransportSecurityStateTest()
      : WithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    SetTransportSecurityStateSourceForTesting(&test_default::kHSTSSource);
    // Need mocked out time for pruning tests. Don't start with a
    // time of 0, as code doesn't generally expect it.
    FastForwardBy(base::TimeDelta::FromDays(1));
  }

  ~TransportSecurityStateTest() override {
    SetTransportSecurityStateSourceForTesting(nullptr);
  }

  void SetUp() override { crypto::EnsureOpenSSLInit(); }

  static void DisableStaticPins(TransportSecurityState* state) {
    state->enable_static_pins_ = false;
  }

  static void EnableStaticPins(TransportSecurityState* state) {
    state->enable_static_pins_ = true;
  }

  static void EnableStaticExpectCT(TransportSecurityState* state) {
    state->enable_static_expect_ct_ = true;
  }

  static HashValueVector GetSampleSPKIHashes() {
    HashValueVector spki_hashes;
    HashValue hash(HASH_VALUE_SHA256);
    memset(hash.data(), 0, hash.size());
    spki_hashes.push_back(hash);
    return spki_hashes;
  }

  static HashValue GetSampleSPKIHash(uint8_t value) {
    HashValue hash(HASH_VALUE_SHA256);
    memset(hash.data(), value, hash.size());
    return hash;
  }

 protected:
  bool GetStaticDomainState(TransportSecurityState* state,
                            const std::string& host,
                            TransportSecurityState::STSState* sts_result,
                            TransportSecurityState::PKPState* pkp_result) {
    return state->GetStaticDomainState(host, sts_result, pkp_result);
  }

  bool GetExpectCTState(TransportSecurityState* state,
                        const std::string& host,
                        TransportSecurityState::ExpectCTState* result) {
    return state->GetStaticExpectCTState(host, result);
  }
};

TEST_F(TransportSecurityStateTest, DomainNameOddities) {
  TransportSecurityState state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  // DNS suffix search tests. Some DNS resolvers allow a terminal "." to
  // indicate not perform DNS suffix searching. Ensure that regardless
  // of how this is treated at the resolver layer, or at the URL/origin
  // layer (that is, whether they are treated as equivalent or distinct),
  // ensure that for policy matching, something lacking a terminal "."
  // is equivalent to something with a terminal "."
  EXPECT_FALSE(state.ShouldUpgradeToSSL("example.com"));

  state.AddHSTS("example.com", expiry, true /* include_subdomains */);
  EXPECT_TRUE(state.ShouldUpgradeToSSL("example.com"));
  // Trailing '.' should be equivalent; it's just a resolver hint
  EXPECT_TRUE(state.ShouldUpgradeToSSL("example.com."));
  // Leading '.' should be invalid
  EXPECT_FALSE(state.ShouldUpgradeToSSL(".example.com"));
  // Subdomains should work regardless
  EXPECT_TRUE(state.ShouldUpgradeToSSL("sub.example.com"));
  EXPECT_TRUE(state.ShouldUpgradeToSSL("sub.example.com."));
  // But invalid subdomains should be rejected
  EXPECT_FALSE(state.ShouldUpgradeToSSL("sub..example.com"));
  EXPECT_FALSE(state.ShouldUpgradeToSSL("sub..example.com."));

  // Now try the inverse form
  TransportSecurityState state2;
  state2.AddHSTS("example.net.", expiry, true /* include_subdomains */);
  EXPECT_TRUE(state2.ShouldUpgradeToSSL("example.net."));
  EXPECT_TRUE(state2.ShouldUpgradeToSSL("example.net"));
  EXPECT_TRUE(state2.ShouldUpgradeToSSL("sub.example.net."));
  EXPECT_TRUE(state2.ShouldUpgradeToSSL("sub.example.net"));

  // Finally, test weird things
  TransportSecurityState state3;
  state3.AddHSTS("", expiry, true /* include_subdomains */);
  EXPECT_FALSE(state3.ShouldUpgradeToSSL(""));
  EXPECT_FALSE(state3.ShouldUpgradeToSSL("."));
  EXPECT_FALSE(state3.ShouldUpgradeToSSL("..."));
  // Make sure it didn't somehow apply HSTS to the world
  EXPECT_FALSE(state3.ShouldUpgradeToSSL("example.org"));

  TransportSecurityState state4;
  state4.AddHSTS(".", expiry, true /* include_subdomains */);
  EXPECT_FALSE(state4.ShouldUpgradeToSSL(""));
  EXPECT_FALSE(state4.ShouldUpgradeToSSL("."));
  EXPECT_FALSE(state4.ShouldUpgradeToSSL("..."));
  EXPECT_FALSE(state4.ShouldUpgradeToSSL("example.org"));

  // Now do the same for preloaded entries
  TransportSecurityState state5;
  EXPECT_TRUE(state5.ShouldUpgradeToSSL("hsts-preloaded.test"));
  EXPECT_TRUE(state5.ShouldUpgradeToSSL("hsts-preloaded.test."));
  EXPECT_FALSE(state5.ShouldUpgradeToSSL("hsts-preloaded..test"));
  EXPECT_FALSE(state5.ShouldUpgradeToSSL("hsts-preloaded..test."));
}

TEST_F(TransportSecurityStateTest, SimpleMatches) {
  TransportSecurityState state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state.ShouldUpgradeToSSL("example.com"));
  bool include_subdomains = false;
  state.AddHSTS("example.com", expiry, include_subdomains);
  EXPECT_TRUE(state.ShouldUpgradeToSSL("example.com"));
  EXPECT_TRUE(state.ShouldSSLErrorsBeFatal("example.com"));
  EXPECT_FALSE(state.ShouldUpgradeToSSL("foo.example.com"));
  EXPECT_FALSE(state.ShouldSSLErrorsBeFatal("foo.example.com"));
}

TEST_F(TransportSecurityStateTest, MatchesCase1) {
  TransportSecurityState state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state.ShouldUpgradeToSSL("example.com"));
  bool include_subdomains = false;
  state.AddHSTS("EXample.coM", expiry, include_subdomains);
  EXPECT_TRUE(state.ShouldUpgradeToSSL("example.com"));
}

TEST_F(TransportSecurityStateTest, MatchesCase2) {
  TransportSecurityState state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  // Check dynamic entries
  EXPECT_FALSE(state.ShouldUpgradeToSSL("EXample.coM"));
  bool include_subdomains = false;
  state.AddHSTS("example.com", expiry, include_subdomains);
  EXPECT_TRUE(state.ShouldUpgradeToSSL("EXample.coM"));

  // Check static entries
  EXPECT_TRUE(state.ShouldUpgradeToSSL("hStS-prelOAded.tEsT"));
  EXPECT_TRUE(
      state.ShouldUpgradeToSSL("inClude-subDOmaIns-hsts-prEloaDed.TesT"));
}

TEST_F(TransportSecurityStateTest, SubdomainMatches) {
  TransportSecurityState state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state.ShouldUpgradeToSSL("example.test"));
  bool include_subdomains = true;
  state.AddHSTS("example.test", expiry, include_subdomains);
  EXPECT_TRUE(state.ShouldUpgradeToSSL("example.test"));
  EXPECT_TRUE(state.ShouldUpgradeToSSL("foo.example.test"));
  EXPECT_TRUE(state.ShouldUpgradeToSSL("foo.bar.example.test"));
  EXPECT_TRUE(state.ShouldUpgradeToSSL("foo.bar.baz.example.test"));
  EXPECT_FALSE(state.ShouldUpgradeToSSL("test"));
  EXPECT_FALSE(state.ShouldUpgradeToSSL("notexample.test"));
}

// Tests that a more-specific HSTS rule without the includeSubDomains bit does
// not override a less-specific rule with includeSubDomains. Applicability is
// checked before specificity. See https://crbug.com/821811.
TEST_F(TransportSecurityStateTest, STSSubdomainNoOverride) {
  const GURL report_uri(kReportUri);
  TransportSecurityState state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  const base::Time older = current_time - base::TimeDelta::FromSeconds(1000);

  state.AddHSTS("example.test", expiry, true);
  state.AddHSTS("foo.example.test", expiry, false);

  // The example.test rule applies to the entire domain, including subdomains of
  // foo.example.test.
  EXPECT_TRUE(state.ShouldUpgradeToSSL("example.test"));
  EXPECT_TRUE(state.ShouldUpgradeToSSL("foo.example.test"));
  EXPECT_TRUE(state.ShouldUpgradeToSSL("bar.foo.example.test"));
  EXPECT_TRUE(state.ShouldSSLErrorsBeFatal("bar.foo.example.test"));

  // Expire the foo.example.test rule.
  state.AddHSTS("foo.example.test", older, false);

  // The example.test rule still applies.
  EXPECT_TRUE(state.ShouldUpgradeToSSL("example.test"));
  EXPECT_TRUE(state.ShouldUpgradeToSSL("foo.example.test"));
  EXPECT_TRUE(state.ShouldUpgradeToSSL("bar.foo.example.test"));
  EXPECT_TRUE(state.ShouldSSLErrorsBeFatal("bar.foo.example.test"));
}

// Tests that a more-specific HPKP rule overrides a less-specific rule
// with it, regardless of the includeSubDomains bit. Note this behavior does not
// match HSTS. See https://crbug.com/821811.
TEST_F(TransportSecurityStateTest, PKPSubdomainCarveout) {
  const GURL report_uri(kReportUri);
  TransportSecurityState state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  const base::Time older = current_time - base::TimeDelta::FromSeconds(1000);

  state.AddHPKP("example.test", expiry, true, GetSampleSPKIHashes(),
                report_uri);
  state.AddHPKP("foo.example.test", expiry, false, GetSampleSPKIHashes(),
                report_uri);
  EXPECT_TRUE(state.HasPublicKeyPins("example.test"));
  EXPECT_TRUE(state.HasPublicKeyPins("foo.example.test"));

  // The foo.example.test rule overrides the example1.test rule, so
  // bar.foo.example.test has no HPKP state.
  EXPECT_FALSE(state.HasPublicKeyPins("bar.foo.example.test"));
  EXPECT_FALSE(state.ShouldSSLErrorsBeFatal("bar.foo.example.test"));

  // Expire the foo.example.test rule.
  state.AddHPKP("foo.example.test", older, false, GetSampleSPKIHashes(),
                report_uri);

  // Now the base example.test rule applies to bar.foo.example.test.
  EXPECT_TRUE(state.HasPublicKeyPins("bar.foo.example.test"));
  EXPECT_TRUE(state.ShouldSSLErrorsBeFatal("bar.foo.example.test"));
}

TEST_F(TransportSecurityStateTest, FatalSSLErrors) {
  const GURL report_uri(kReportUri);
  TransportSecurityState state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  state.AddHSTS("example1.test", expiry, false);
  state.AddHPKP("example2.test", expiry, false, GetSampleSPKIHashes(),
                report_uri);

  // The presense of either HSTS or HPKP is enough to make SSL errors fatal.
  EXPECT_TRUE(state.ShouldSSLErrorsBeFatal("example1.test"));
  EXPECT_TRUE(state.ShouldSSLErrorsBeFatal("example2.test"));
}

// Tests that HPKP and HSTS state both expire. Also tests that expired entries
// are pruned.
TEST_F(TransportSecurityStateTest, Expiration) {
  const GURL report_uri(kReportUri);
  TransportSecurityState state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  const base::Time older = current_time - base::TimeDelta::FromSeconds(1000);

  // Note: this test assumes that inserting an entry with an expiration time in
  // the past works and is pruned on query.
  state.AddHSTS("example1.test", older, false);
  EXPECT_TRUE(TransportSecurityState::STSStateIterator(state).HasNext());
  EXPECT_FALSE(state.ShouldUpgradeToSSL("example1.test"));
  // Querying |state| for a domain should flush out expired entries.
  EXPECT_FALSE(TransportSecurityState::STSStateIterator(state).HasNext());

  state.AddHPKP("example1.test", older, false, GetSampleSPKIHashes(),
                report_uri);
  EXPECT_TRUE(state.has_dynamic_pkp_state());
  EXPECT_FALSE(state.HasPublicKeyPins("example1.test"));
  // Querying |state| for a domain should flush out expired entries.
  EXPECT_FALSE(state.has_dynamic_pkp_state());

  state.AddHSTS("example1.test", older, false);
  state.AddHPKP("example1.test", older, false, GetSampleSPKIHashes(),
                report_uri);
  EXPECT_TRUE(TransportSecurityState::STSStateIterator(state).HasNext());
  EXPECT_TRUE(state.has_dynamic_pkp_state());
  EXPECT_FALSE(state.ShouldSSLErrorsBeFatal("example1.test"));
  // Querying |state| for a domain should flush out expired entries.
  EXPECT_FALSE(TransportSecurityState::STSStateIterator(state).HasNext());
  EXPECT_FALSE(state.has_dynamic_pkp_state());

  // Test that HSTS can outlive HPKP.
  state.AddHSTS("example1.test", expiry, false);
  state.AddHPKP("example1.test", older, false, GetSampleSPKIHashes(),
                report_uri);
  EXPECT_TRUE(state.ShouldUpgradeToSSL("example1.test"));
  EXPECT_FALSE(state.HasPublicKeyPins("example1.test"));

  // Test that HPKP can outlive HSTS.
  state.AddHSTS("example2.test", older, false);
  state.AddHPKP("example2.test", expiry, false, GetSampleSPKIHashes(),
                report_uri);
  EXPECT_FALSE(state.ShouldUpgradeToSSL("example2.test"));
  EXPECT_TRUE(state.HasPublicKeyPins("example2.test"));
}

// Tests that HPKP and HSTS state are queried independently for subdomain
// matches.
TEST_F(TransportSecurityStateTest, IndependentSubdomain) {
  const GURL report_uri(kReportUri);
  TransportSecurityState state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  state.AddHSTS("example1.test", expiry, true);
  state.AddHPKP("example1.test", expiry, false, GetSampleSPKIHashes(),
                report_uri);

  state.AddHSTS("example2.test", expiry, false);
  state.AddHPKP("example2.test", expiry, true, GetSampleSPKIHashes(),
                report_uri);

  EXPECT_TRUE(state.ShouldUpgradeToSSL("foo.example1.test"));
  EXPECT_FALSE(state.HasPublicKeyPins("foo.example1.test"));
  EXPECT_FALSE(state.ShouldUpgradeToSSL("foo.example2.test"));
  EXPECT_TRUE(state.HasPublicKeyPins("foo.example2.test"));
}

// Tests that HPKP and HSTS state are inserted and overridden independently.
TEST_F(TransportSecurityStateTest, IndependentInsertion) {
  const GURL report_uri(kReportUri);
  TransportSecurityState state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  // Place an includeSubdomains HSTS entry below a normal HPKP entry.
  state.AddHSTS("example1.test", expiry, true);
  state.AddHPKP("foo.example1.test", expiry, false, GetSampleSPKIHashes(),
                report_uri);

  EXPECT_TRUE(state.ShouldUpgradeToSSL("foo.example1.test"));
  EXPECT_TRUE(state.HasPublicKeyPins("foo.example1.test"));
  EXPECT_TRUE(state.ShouldUpgradeToSSL("example1.test"));
  EXPECT_FALSE(state.HasPublicKeyPins("example1.test"));

  // Drop the includeSubdomains from the HSTS entry.
  state.AddHSTS("example1.test", expiry, false);

  EXPECT_FALSE(state.ShouldUpgradeToSSL("foo.example1.test"));
  EXPECT_TRUE(state.HasPublicKeyPins("foo.example1.test"));

  // Place an includeSubdomains HPKP entry below a normal HSTS entry.
  state.AddHSTS("foo.example2.test", expiry, false);
  state.AddHPKP("example2.test", expiry, true, GetSampleSPKIHashes(),
                report_uri);

  EXPECT_TRUE(state.ShouldUpgradeToSSL("foo.example2.test"));
  EXPECT_TRUE(state.HasPublicKeyPins("foo.example2.test"));

  // Drop the includeSubdomains from the HSTS entry.
  state.AddHPKP("example2.test", expiry, false, GetSampleSPKIHashes(),
                report_uri);

  EXPECT_TRUE(state.ShouldUpgradeToSSL("foo.example2.test"));
  EXPECT_FALSE(state.HasPublicKeyPins("foo.example2.test"));
}

// Tests that GetDynamic[PKP|STS]State returns the correct data and that the
// states are not mixed together.
TEST_F(TransportSecurityStateTest, DynamicDomainState) {
  const GURL report_uri(kReportUri);
  TransportSecurityState state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry1 = current_time + base::TimeDelta::FromSeconds(1000);
  const base::Time expiry2 = current_time + base::TimeDelta::FromSeconds(2000);

  state.AddHSTS("example.com", expiry1, true);
  state.AddHPKP("foo.example.com", expiry2, false, GetSampleSPKIHashes(),
                report_uri);

  TransportSecurityState::STSState sts_state;
  TransportSecurityState::PKPState pkp_state;
  ASSERT_TRUE(state.GetDynamicSTSState("foo.example.com", &sts_state));
  ASSERT_TRUE(state.GetDynamicPKPState("foo.example.com", &pkp_state));
  EXPECT_TRUE(sts_state.ShouldUpgradeToSSL());
  EXPECT_TRUE(pkp_state.HasPublicKeyPins());
  EXPECT_TRUE(sts_state.include_subdomains);
  EXPECT_FALSE(pkp_state.include_subdomains);
  EXPECT_EQ(expiry1, sts_state.expiry);
  EXPECT_EQ(expiry2, pkp_state.expiry);
  EXPECT_EQ("example.com", sts_state.domain);
  EXPECT_EQ("foo.example.com", pkp_state.domain);
}

// Tests that new pins always override previous pins. This should be true for
// both pins at the same domain or includeSubdomains pins at a parent domain.
TEST_F(TransportSecurityStateTest, NewPinsOverride) {
  const GURL report_uri(kReportUri);
  TransportSecurityState state;
  TransportSecurityState::PKPState pkp_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  HashValue hash1(HASH_VALUE_SHA256);
  memset(hash1.data(), 0x01, hash1.size());
  HashValue hash2(HASH_VALUE_SHA256);
  memset(hash2.data(), 0x02, hash1.size());
  HashValue hash3(HASH_VALUE_SHA256);
  memset(hash3.data(), 0x03, hash1.size());

  state.AddHPKP("example.com", expiry, true, HashValueVector(1, hash1),
                report_uri);

  ASSERT_TRUE(state.GetDynamicPKPState("foo.example.com", &pkp_state));
  ASSERT_EQ(1u, pkp_state.spki_hashes.size());
  EXPECT_EQ(pkp_state.spki_hashes[0], hash1);

  state.AddHPKP("foo.example.com", expiry, false, HashValueVector(1, hash2),
                report_uri);

  ASSERT_TRUE(state.GetDynamicPKPState("foo.example.com", &pkp_state));
  ASSERT_EQ(1u, pkp_state.spki_hashes.size());
  EXPECT_EQ(pkp_state.spki_hashes[0], hash2);

  state.AddHPKP("foo.example.com", expiry, false, HashValueVector(1, hash3),
                report_uri);

  ASSERT_TRUE(state.GetDynamicPKPState("foo.example.com", &pkp_state));
  ASSERT_EQ(1u, pkp_state.spki_hashes.size());
  EXPECT_EQ(pkp_state.spki_hashes[0], hash3);
}

TEST_F(TransportSecurityStateTest, DeleteAllDynamicDataBetween) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      TransportSecurityState::kDynamicExpectCTFeature);
  TransportSecurityState::ExpectCTState expect_ct_state;

  TransportSecurityState state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  const base::Time older = current_time - base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state.ShouldUpgradeToSSL("example.com"));
  EXPECT_FALSE(state.HasPublicKeyPins("example.com"));
  EXPECT_FALSE(state.GetDynamicExpectCTState(
      "example.com", NetworkIsolationKey(), &expect_ct_state));
  bool include_subdomains = false;
  state.AddHSTS("example.com", expiry, include_subdomains);
  state.AddHPKP("example.com", expiry, include_subdomains,
                GetSampleSPKIHashes(), GURL());
  state.AddExpectCT("example.com", expiry, true, GURL(), NetworkIsolationKey());

  state.DeleteAllDynamicDataBetween(expiry, base::Time::Max(),
                                    base::DoNothing());
  EXPECT_TRUE(state.ShouldUpgradeToSSL("example.com"));
  EXPECT_TRUE(state.HasPublicKeyPins("example.com"));
  EXPECT_TRUE(state.GetDynamicExpectCTState(
      "example.com", NetworkIsolationKey(), &expect_ct_state));
  state.DeleteAllDynamicDataBetween(older, current_time, base::DoNothing());
  EXPECT_TRUE(state.ShouldUpgradeToSSL("example.com"));
  EXPECT_TRUE(state.HasPublicKeyPins("example.com"));
  EXPECT_TRUE(state.GetDynamicExpectCTState(
      "example.com", NetworkIsolationKey(), &expect_ct_state));
  state.DeleteAllDynamicDataBetween(base::Time(), current_time,
                                    base::DoNothing());
  EXPECT_TRUE(state.ShouldUpgradeToSSL("example.com"));
  EXPECT_TRUE(state.HasPublicKeyPins("example.com"));
  EXPECT_TRUE(state.GetDynamicExpectCTState(
      "example.com", NetworkIsolationKey(), &expect_ct_state));
  state.DeleteAllDynamicDataBetween(older, base::Time::Max(),
                                    base::DoNothing());
  EXPECT_FALSE(state.ShouldUpgradeToSSL("example.com"));
  EXPECT_FALSE(state.HasPublicKeyPins("example.com"));
  EXPECT_FALSE(state.GetDynamicExpectCTState(
      "example.com", NetworkIsolationKey(), &expect_ct_state));

  // Dynamic data in |state| should be empty now.
  EXPECT_FALSE(TransportSecurityState::STSStateIterator(state).HasNext());
  EXPECT_FALSE(state.has_dynamic_pkp_state());
  EXPECT_FALSE(TransportSecurityState::ExpectCTStateIterator(state).HasNext());
}

TEST_F(TransportSecurityStateTest, DeleteDynamicDataForHost) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /* enabled_features */
      {TransportSecurityState::kDynamicExpectCTFeature,
       features::kPartitionExpectCTStateByNetworkIsolationKey},
      /* disabled_features */
      {});
  TransportSecurityState state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  bool include_subdomains = false;

  NetworkIsolationKey network_isolation_key =
      NetworkIsolationKey::CreateTransient();
  state.AddHSTS("example1.test", expiry, include_subdomains);
  state.AddHPKP("example1.test", expiry, include_subdomains,
                GetSampleSPKIHashes(), GURL());
  state.AddExpectCT("example1.test", expiry, true, GURL(),
                    NetworkIsolationKey());

  EXPECT_TRUE(state.ShouldUpgradeToSSL("example1.test"));
  EXPECT_FALSE(state.ShouldUpgradeToSSL("example2.test"));
  EXPECT_TRUE(state.HasPublicKeyPins("example1.test"));
  EXPECT_FALSE(state.HasPublicKeyPins("example2.test"));
  TransportSecurityState::ExpectCTState expect_ct_state;
  EXPECT_TRUE(state.GetDynamicExpectCTState(
      "example1.test", NetworkIsolationKey(), &expect_ct_state));
  EXPECT_FALSE(state.GetDynamicExpectCTState(
      "example2.test", NetworkIsolationKey(), &expect_ct_state));
  EXPECT_FALSE(state.GetDynamicExpectCTState(
      "example1.test", network_isolation_key, &expect_ct_state));
  state.AddExpectCT("example1.test", expiry, true, GURL(),
                    network_isolation_key);
  EXPECT_TRUE(state.GetDynamicExpectCTState(
      "example1.test", network_isolation_key, &expect_ct_state));

  EXPECT_TRUE(state.DeleteDynamicDataForHost("example1.test"));
  EXPECT_FALSE(state.ShouldUpgradeToSSL("example1.test"));
  EXPECT_FALSE(state.HasPublicKeyPins("example1.test"));
  EXPECT_FALSE(state.GetDynamicExpectCTState(
      "example1.test", NetworkIsolationKey(), &expect_ct_state));
  EXPECT_FALSE(state.GetDynamicExpectCTState(
      "example1.test", network_isolation_key, &expect_ct_state));
}

TEST_F(TransportSecurityStateTest, LongNames) {
  TransportSecurityState state;
  const char kLongName[] =
      "lookupByWaveIdHashAndWaveIdIdAndWaveIdDomainAndWaveletIdIdAnd"
      "WaveletIdDomainAndBlipBlipid";
  TransportSecurityState::STSState sts_state;
  TransportSecurityState::PKPState pkp_state;
  // Just checks that we don't hit a NOTREACHED.
  EXPECT_FALSE(state.GetStaticDomainState(kLongName, &sts_state, &pkp_state));
  EXPECT_FALSE(state.GetDynamicSTSState(kLongName, &sts_state));
  EXPECT_FALSE(state.GetDynamicPKPState(kLongName, &pkp_state));
}

static bool AddHash(const std::string& type_and_base64, HashValueVector* out) {
  HashValue hash;
  if (!hash.FromString(type_and_base64))
    return false;

  out->push_back(hash);
  return true;
}

TEST_F(TransportSecurityStateTest, PinValidationWithoutRejectedCerts) {
  HashValueVector good_hashes, bad_hashes;

  for (size_t i = 0; kGoodPath[i]; i++) {
    EXPECT_TRUE(AddHash(kGoodPath[i], &good_hashes));
  }
  for (size_t i = 0; kBadPath[i]; i++) {
    EXPECT_TRUE(AddHash(kBadPath[i], &bad_hashes));
  }

  TransportSecurityState state;
  EnableStaticPins(&state);

  TransportSecurityState::STSState sts_state;
  TransportSecurityState::PKPState pkp_state;
  EXPECT_TRUE(state.GetStaticDomainState("no-rejected-pins-pkp.preloaded.test",
                                         &sts_state, &pkp_state));
  EXPECT_TRUE(pkp_state.HasPublicKeyPins());

  std::string failure_log;
  EXPECT_TRUE(pkp_state.CheckPublicKeyPins(good_hashes, &failure_log));
  EXPECT_FALSE(pkp_state.CheckPublicKeyPins(bad_hashes, &failure_log));
}

// Tests that pinning violations on preloaded pins trigger reports when
// the preloaded pin contains a report URI.
TEST_F(TransportSecurityStateTest, PreloadedPKPReportUri) {
  const char kPreloadedPinDomain[] = "with-report-uri-pkp.preloaded.test";
  const uint16_t kPort = 443;
  HostPortPair host_port_pair(kPreloadedPinDomain, kPort);
  net::NetworkIsolationKey network_isolation_key =
      NetworkIsolationKey::CreateTransient();

  TransportSecurityState state;
  MockCertificateReportSender mock_report_sender;
  state.SetReportSender(&mock_report_sender);

  EnableStaticPins(&state);

  TransportSecurityState::PKPState pkp_state;
  TransportSecurityState::STSState unused_sts_state;
  ASSERT_TRUE(state.GetStaticDomainState(kPreloadedPinDomain, &unused_sts_state,
                                         &pkp_state));
  ASSERT_TRUE(pkp_state.HasPublicKeyPins());

  GURL report_uri = pkp_state.report_uri;
  ASSERT_TRUE(report_uri.is_valid());
  ASSERT_FALSE(report_uri.is_empty());

  // Two dummy certs to use as the server-sent and validated chains. The
  // contents don't matter, as long as they are not the real google.com
  // certs in the pins.
  scoped_refptr<X509Certificate> cert1 =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  ASSERT_TRUE(cert1);
  scoped_refptr<X509Certificate> cert2 =
      ImportCertFromFile(GetTestCertsDirectory(), "expired_cert.pem");
  ASSERT_TRUE(cert2);

  HashValueVector bad_hashes;
  for (size_t i = 0; kBadPath[i]; i++)
    EXPECT_TRUE(AddHash(kBadPath[i], &bad_hashes));

  // Trigger a violation and check that it sends a report.
  std::string failure_log;
  EXPECT_EQ(TransportSecurityState::PKPStatus::VIOLATED,
            state.CheckPublicKeyPins(host_port_pair, true, bad_hashes,
                                     cert1.get(), cert2.get(),
                                     TransportSecurityState::ENABLE_PIN_REPORTS,
                                     network_isolation_key, &failure_log));

  EXPECT_EQ(report_uri, mock_report_sender.latest_report_uri());

  std::string report = mock_report_sender.latest_report();
  ASSERT_FALSE(report.empty());
  EXPECT_EQ("application/json; charset=utf-8",
            mock_report_sender.latest_content_type());
  ASSERT_NO_FATAL_FAILURE(CheckHPKPReport(
      report, host_port_pair, pkp_state.include_subdomains, pkp_state.domain,
      cert1.get(), cert2.get(), pkp_state.spki_hashes));
  EXPECT_EQ(network_isolation_key,
            mock_report_sender.latest_network_isolation_key());
}

// Tests that report URIs are thrown out if they point to the same host,
// over HTTPS, for which a pin was violated.
TEST_F(TransportSecurityStateTest, HPKPReportUriToSameHost) {
  HostPortPair host_port_pair(kHost, kPort);
  GURL https_report_uri("https://example.test/report");
  GURL http_report_uri("http://example.test/report");
  NetworkIsolationKey network_isolation_key =
      NetworkIsolationKey::CreateTransient();
  TransportSecurityState state;
  MockCertificateReportSender mock_report_sender;
  state.SetReportSender(&mock_report_sender);

  const base::Time current_time = base::Time::Now();
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  HashValueVector good_hashes;
  for (size_t i = 0; kGoodPath[i]; i++)
    EXPECT_TRUE(AddHash(kGoodPath[i], &good_hashes));

  // Two dummy certs to use as the server-sent and validated chains. The
  // contents don't matter, as long as they don't match the certs in the pins.
  scoped_refptr<X509Certificate> cert1 =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  ASSERT_TRUE(cert1);
  scoped_refptr<X509Certificate> cert2 =
      ImportCertFromFile(GetTestCertsDirectory(), "expired_cert.pem");
  ASSERT_TRUE(cert2);

  HashValueVector bad_hashes;
  for (size_t i = 0; kBadPath[i]; i++)
    EXPECT_TRUE(AddHash(kBadPath[i], &bad_hashes));

  state.AddHPKP(kHost, expiry, true, good_hashes, https_report_uri);

  // Trigger a violation and check that it does not send a report
  // because the report-uri is HTTPS and same-host as the pins.
  std::string failure_log;
  EXPECT_EQ(TransportSecurityState::PKPStatus::VIOLATED,
            state.CheckPublicKeyPins(host_port_pair, true, bad_hashes,
                                     cert1.get(), cert2.get(),
                                     TransportSecurityState::ENABLE_PIN_REPORTS,
                                     network_isolation_key, &failure_log));

  EXPECT_TRUE(mock_report_sender.latest_report_uri().is_empty());

  // An HTTP report uri to the same host should be okay.
  state.AddHPKP("example.test", expiry, true, good_hashes, http_report_uri);
  EXPECT_EQ(TransportSecurityState::PKPStatus::VIOLATED,
            state.CheckPublicKeyPins(host_port_pair, true, bad_hashes,
                                     cert1.get(), cert2.get(),
                                     TransportSecurityState::ENABLE_PIN_REPORTS,
                                     network_isolation_key, &failure_log));

  EXPECT_EQ(http_report_uri, mock_report_sender.latest_report_uri());
  EXPECT_EQ(network_isolation_key,
            mock_report_sender.latest_network_isolation_key());
}

// Tests that static (preloaded) expect CT state is read correctly.
TEST_F(TransportSecurityStateTest, PreloadedExpectCT) {
  TransportSecurityState state;
  TransportSecurityStateTest::EnableStaticExpectCT(&state);
  TransportSecurityState::ExpectCTState expect_ct_state;
  EXPECT_TRUE(
      GetExpectCTState(&state, kExpectCTStaticHostname, &expect_ct_state));
  EXPECT_EQ(GURL(kExpectCTStaticReportURI), expect_ct_state.report_uri);
  EXPECT_FALSE(
      GetExpectCTState(&state, "hsts-preloaded.test", &expect_ct_state));
}

// Tests that the Expect CT reporter is not notified for invalid or absent
// header values.
TEST_F(TransportSecurityStateTest, InvalidExpectCTHeader) {
  HostPortPair host_port(kExpectCTStaticHostname, 443);
  SSLInfo ssl_info;
  ssl_info.ct_policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS;
  ssl_info.is_issued_by_known_root = true;
  scoped_refptr<X509Certificate> cert1 =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  ASSERT_TRUE(cert1);
  scoped_refptr<X509Certificate> cert2 =
      ImportCertFromFile(GetTestCertsDirectory(), "expired_cert.pem");
  ASSERT_TRUE(cert2);
  ssl_info.unverified_cert = cert1;
  ssl_info.cert = cert2;

  TransportSecurityState state;
  TransportSecurityStateTest::EnableStaticExpectCT(&state);
  MockExpectCTReporter reporter;
  state.SetExpectCTReporter(&reporter);
  state.ProcessExpectCTHeader("", host_port, ssl_info, NetworkIsolationKey());
  EXPECT_EQ(0u, reporter.num_failures());

  state.ProcessExpectCTHeader("blah blah", host_port, ssl_info,
                              NetworkIsolationKey());
  EXPECT_EQ(0u, reporter.num_failures());

  state.ProcessExpectCTHeader("preload", host_port, ssl_info,
                              NetworkIsolationKey());
  EXPECT_EQ(1u, reporter.num_failures());
}

// Tests that the Expect CT reporter is only notified about certificates
// chaining to public roots.
TEST_F(TransportSecurityStateTest, ExpectCTNonPublicRoot) {
  HostPortPair host_port(kExpectCTStaticHostname, 443);
  SSLInfo ssl_info;
  ssl_info.ct_policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS;
  ssl_info.is_issued_by_known_root = false;
  scoped_refptr<X509Certificate> cert1 =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  ASSERT_TRUE(cert1);
  scoped_refptr<X509Certificate> cert2 =
      ImportCertFromFile(GetTestCertsDirectory(), "expired_cert.pem");
  ASSERT_TRUE(cert2);
  ssl_info.unverified_cert = cert1;
  ssl_info.cert = cert2;

  TransportSecurityState state;
  TransportSecurityStateTest::EnableStaticExpectCT(&state);
  MockExpectCTReporter reporter;
  state.SetExpectCTReporter(&reporter);
  state.ProcessExpectCTHeader("preload", host_port, ssl_info,
                              NetworkIsolationKey());
  EXPECT_EQ(0u, reporter.num_failures());

  ssl_info.is_issued_by_known_root = true;
  state.ProcessExpectCTHeader("preload", host_port, ssl_info,
                              NetworkIsolationKey());
  EXPECT_EQ(1u, reporter.num_failures());
}

// Tests that the Expect CT reporter is not notified when compliance
// details aren't available.
TEST_F(TransportSecurityStateTest, ExpectCTComplianceNotAvailable) {
  HostPortPair host_port(kExpectCTStaticHostname, 443);
  SSLInfo ssl_info;
  ssl_info.ct_policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_COMPLIANCE_DETAILS_NOT_AVAILABLE;
  ssl_info.is_issued_by_known_root = true;
  scoped_refptr<X509Certificate> cert1 =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  ASSERT_TRUE(cert1);
  scoped_refptr<X509Certificate> cert2 =
      ImportCertFromFile(GetTestCertsDirectory(), "expired_cert.pem");
  ASSERT_TRUE(cert2);
  ssl_info.unverified_cert = cert1;
  ssl_info.cert = cert2;

  TransportSecurityState state;
  TransportSecurityStateTest::EnableStaticExpectCT(&state);
  MockExpectCTReporter reporter;
  state.SetExpectCTReporter(&reporter);
  state.ProcessExpectCTHeader("preload", host_port, ssl_info,
                              NetworkIsolationKey());
  EXPECT_EQ(0u, reporter.num_failures());

  ssl_info.ct_policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS;
  state.ProcessExpectCTHeader("preload", host_port, ssl_info,
                              NetworkIsolationKey());
  EXPECT_EQ(1u, reporter.num_failures());
}

// Tests that the Expect CT reporter is not notified about compliant
// connections.
TEST_F(TransportSecurityStateTest, ExpectCTCompliantCert) {
  HostPortPair host_port(kExpectCTStaticHostname, 443);
  SSLInfo ssl_info;
  ssl_info.ct_policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS;
  ssl_info.is_issued_by_known_root = true;
  scoped_refptr<X509Certificate> cert1 =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  ASSERT_TRUE(cert1);
  scoped_refptr<X509Certificate> cert2 =
      ImportCertFromFile(GetTestCertsDirectory(), "expired_cert.pem");
  ASSERT_TRUE(cert2);
  ssl_info.unverified_cert = cert1;
  ssl_info.cert = cert2;

  TransportSecurityState state;
  TransportSecurityStateTest::EnableStaticExpectCT(&state);
  MockExpectCTReporter reporter;
  state.SetExpectCTReporter(&reporter);
  state.ProcessExpectCTHeader("preload", host_port, ssl_info,
                              NetworkIsolationKey());
  EXPECT_EQ(0u, reporter.num_failures());

  ssl_info.ct_policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS;
  state.ProcessExpectCTHeader("preload", host_port, ssl_info,
                              NetworkIsolationKey());
  EXPECT_EQ(1u, reporter.num_failures());
}

// Tests that the Expect CT reporter is not notified for preloaded Expect-CT
// when the build is not timely.
TEST_F(TransportSecurityStateTest, PreloadedExpectCTBuildNotTimely) {
  HostPortPair host_port(kExpectCTStaticHostname, 443);
  SSLInfo ssl_info;
  ssl_info.ct_policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY;
  ssl_info.is_issued_by_known_root = true;
  scoped_refptr<X509Certificate> cert1 =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  ASSERT_TRUE(cert1);
  scoped_refptr<X509Certificate> cert2 =
      ImportCertFromFile(GetTestCertsDirectory(), "expired_cert.pem");
  ASSERT_TRUE(cert2);
  ssl_info.unverified_cert = cert1;
  ssl_info.cert = cert2;

  TransportSecurityState state;
  TransportSecurityStateTest::EnableStaticExpectCT(&state);
  MockExpectCTReporter reporter;
  state.SetExpectCTReporter(&reporter);
  state.ProcessExpectCTHeader("preload", host_port, ssl_info,
                              NetworkIsolationKey());
  EXPECT_EQ(0u, reporter.num_failures());

  // Sanity-check that the reporter is notified if the build is timely and the
  // connection is not compliant.
  ssl_info.ct_policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS;
  state.ProcessExpectCTHeader("preload", host_port, ssl_info,
                              NetworkIsolationKey());
  EXPECT_EQ(1u, reporter.num_failures());
}

// Tests that the Expect CT reporter is not notified for dynamic Expect-CT when
// the build is not timely.
TEST_F(TransportSecurityStateTest, DynamicExpectCTBuildNotTimely) {
  HostPortPair host_port("example.test", 443);
  SSLInfo ssl_info;
  ssl_info.ct_policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY;
  ssl_info.is_issued_by_known_root = true;
  scoped_refptr<X509Certificate> cert1 =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  ASSERT_TRUE(cert1);
  scoped_refptr<X509Certificate> cert2 =
      ImportCertFromFile(GetTestCertsDirectory(), "expired_cert.pem");
  ASSERT_TRUE(cert2);
  ssl_info.unverified_cert = cert1;
  ssl_info.cert = cert2;

  TransportSecurityState state;
  MockExpectCTReporter reporter;
  state.SetExpectCTReporter(&reporter);
  const char kHeader[] = "max-age=10, report-uri=http://report.test";
  state.ProcessExpectCTHeader(kHeader, host_port, ssl_info,
                              NetworkIsolationKey());

  // No report should have been sent and the state should not have been saved.
  EXPECT_EQ(0u, reporter.num_failures());
  TransportSecurityState::ExpectCTState expect_ct_state;
  EXPECT_FALSE(state.GetDynamicExpectCTState(
      "example.test", NetworkIsolationKey(), &expect_ct_state));

  // Sanity-check that the reporter is notified if the build is timely and the
  // connection is not compliant.
  ssl_info.ct_policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS;
  state.ProcessExpectCTHeader(kHeader, host_port, ssl_info,
                              NetworkIsolationKey());
  EXPECT_EQ(1u, reporter.num_failures());
}

// Tests that the Expect CT reporter is not notified for a site that
// isn't preloaded.
TEST_F(TransportSecurityStateTest, ExpectCTNotPreloaded) {
  HostPortPair host_port("not-expect-ct-preloaded.test", 443);
  SSLInfo ssl_info;
  ssl_info.ct_policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS;
  ssl_info.is_issued_by_known_root = true;
  scoped_refptr<X509Certificate> cert1 =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  ASSERT_TRUE(cert1);
  scoped_refptr<X509Certificate> cert2 =
      ImportCertFromFile(GetTestCertsDirectory(), "expired_cert.pem");
  ASSERT_TRUE(cert2);
  ssl_info.unverified_cert = cert1;
  ssl_info.cert = cert2;

  TransportSecurityState state;
  TransportSecurityStateTest::EnableStaticExpectCT(&state);
  MockExpectCTReporter reporter;
  state.SetExpectCTReporter(&reporter);
  state.ProcessExpectCTHeader("preload", host_port, ssl_info,
                              NetworkIsolationKey());
  EXPECT_EQ(0u, reporter.num_failures());

  host_port.set_host(kExpectCTStaticHostname);
  state.ProcessExpectCTHeader("preload", host_port, ssl_info,
                              NetworkIsolationKey());
  EXPECT_EQ(1u, reporter.num_failures());
}

// Tests that the Expect CT reporter is notified for noncompliant
// connections.
TEST_F(TransportSecurityStateTest, ExpectCTReporter) {
  HostPortPair host_port(kExpectCTStaticHostname, 443);
  SSLInfo ssl_info;
  ssl_info.ct_policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS;
  ssl_info.is_issued_by_known_root = true;
  scoped_refptr<X509Certificate> cert1 =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  scoped_refptr<X509Certificate> cert2 =
      ImportCertFromFile(GetTestCertsDirectory(), "expired_cert.pem");
  ASSERT_TRUE(cert1);
  ASSERT_TRUE(cert2);
  ssl_info.unverified_cert = cert1;
  ssl_info.cert = cert2;
  MakeTestSCTAndStatus(ct::SignedCertificateTimestamp::SCT_EMBEDDED, "test_log",
                       std::string(), std::string(), base::Time::Now(),
                       ct::SCT_STATUS_INVALID_SIGNATURE,
                       &ssl_info.signed_certificate_timestamps);
  NetworkIsolationKey network_isolation_key =
      NetworkIsolationKey::CreateTransient();

  TransportSecurityState state;
  TransportSecurityStateTest::EnableStaticExpectCT(&state);
  MockExpectCTReporter reporter;
  state.SetExpectCTReporter(&reporter);
  state.ProcessExpectCTHeader("preload", host_port, ssl_info,
                              network_isolation_key);
  EXPECT_EQ(1u, reporter.num_failures());
  EXPECT_EQ(host_port.host(), reporter.host_port_pair().host());
  EXPECT_EQ(host_port.port(), reporter.host_port_pair().port());
  EXPECT_TRUE(reporter.expiration().is_null());
  EXPECT_EQ(GURL(kExpectCTStaticReportURI), reporter.report_uri());
  EXPECT_EQ(cert1.get(), reporter.served_certificate_chain());
  EXPECT_EQ(cert2.get(), reporter.validated_certificate_chain());
  EXPECT_EQ(ssl_info.signed_certificate_timestamps.size(),
            reporter.signed_certificate_timestamps().size());
  EXPECT_EQ(ssl_info.signed_certificate_timestamps[0].status,
            reporter.signed_certificate_timestamps()[0].status);
  EXPECT_EQ(ssl_info.signed_certificate_timestamps[0].sct,
            reporter.signed_certificate_timestamps()[0].sct);
  EXPECT_EQ(network_isolation_key, reporter.network_isolation_key());
}

// Tests that the Expect CT reporter is not notified for repeated noncompliant
// connections to the same preloaded host.
TEST_F(TransportSecurityStateTest, RepeatedExpectCTReportsForStaticExpectCT) {
  HostPortPair host_port(kExpectCTStaticHostname, 443);
  SSLInfo ssl_info;
  ssl_info.ct_policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS;
  ssl_info.is_issued_by_known_root = true;
  scoped_refptr<X509Certificate> cert1 =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  ASSERT_TRUE(cert1);
  scoped_refptr<X509Certificate> cert2 =
      ImportCertFromFile(GetTestCertsDirectory(), "expired_cert.pem");
  ASSERT_TRUE(cert2);
  ssl_info.unverified_cert = cert1;
  ssl_info.cert = cert2;
  MakeTestSCTAndStatus(ct::SignedCertificateTimestamp::SCT_EMBEDDED, "test_log",
                       std::string(), std::string(), base::Time::Now(),
                       ct::SCT_STATUS_INVALID_SIGNATURE,
                       &ssl_info.signed_certificate_timestamps);

  TransportSecurityState state;
  TransportSecurityStateTest::EnableStaticExpectCT(&state);
  MockExpectCTReporter reporter;
  state.SetExpectCTReporter(&reporter);
  state.ProcessExpectCTHeader("preload", host_port, ssl_info,
                              NetworkIsolationKey());
  EXPECT_EQ(1u, reporter.num_failures());

  // After processing a second header, the report should not be sent again.
  state.ProcessExpectCTHeader("preload", host_port, ssl_info,
                              NetworkIsolationKey());
  EXPECT_EQ(1u, reporter.num_failures());
}

// Simple test for the HSTS preload process. The trie (generated from
// transport_security_state_static_unittest1.json) contains 1 entry. Test that
// the lookup methods can find the entry and correctly decode the different
// preloaded states (HSTS, HPKP, and Expect-CT).
TEST_F(TransportSecurityStateTest, DecodePreloadedSingle) {
  SetTransportSecurityStateSourceForTesting(&test1::kHSTSSource);

  TransportSecurityState state;
  TransportSecurityStateTest::EnableStaticPins(&state);
  TransportSecurityStateTest::EnableStaticExpectCT(&state);

  TransportSecurityState::STSState sts_state;
  TransportSecurityState::PKPState pkp_state;
  EXPECT_TRUE(
      GetStaticDomainState(&state, "hsts.example.com", &sts_state, &pkp_state));
  EXPECT_TRUE(sts_state.include_subdomains);
  EXPECT_EQ(TransportSecurityState::STSState::MODE_FORCE_HTTPS,
            sts_state.upgrade_mode);
  EXPECT_TRUE(pkp_state.include_subdomains);
  EXPECT_EQ(GURL(), pkp_state.report_uri);
  ASSERT_EQ(1u, pkp_state.spki_hashes.size());
  EXPECT_EQ(pkp_state.spki_hashes[0], GetSampleSPKIHash(0x1));
  ASSERT_EQ(1u, pkp_state.bad_spki_hashes.size());
  EXPECT_EQ(pkp_state.bad_spki_hashes[0], GetSampleSPKIHash(0x2));

  TransportSecurityState::ExpectCTState ct_state;
  EXPECT_FALSE(GetExpectCTState(&state, "hsts.example.com", &ct_state));
}

// More advanced test for the HSTS preload process where the trie (generated
// from transport_security_state_static_unittest2.json) contains multiple
// entries with a common prefix. Test that the lookup methods can find all
// entries and correctly decode the different preloaded states (HSTS, HPKP,
// and Expect-CT) for each entry.
TEST_F(TransportSecurityStateTest, DecodePreloadedMultiplePrefix) {
  SetTransportSecurityStateSourceForTesting(&test2::kHSTSSource);

  TransportSecurityState state;
  TransportSecurityStateTest::EnableStaticPins(&state);
  TransportSecurityStateTest::EnableStaticExpectCT(&state);

  TransportSecurityState::STSState sts_state;
  TransportSecurityState::PKPState pkp_state;
  TransportSecurityState::ExpectCTState ct_state;

  EXPECT_TRUE(
      GetStaticDomainState(&state, "hsts.example.com", &sts_state, &pkp_state));
  EXPECT_FALSE(sts_state.include_subdomains);
  EXPECT_EQ(TransportSecurityState::STSState::MODE_FORCE_HTTPS,
            sts_state.upgrade_mode);
  EXPECT_TRUE(pkp_state == TransportSecurityState::PKPState());
  EXPECT_FALSE(GetExpectCTState(&state, "hsts.example.com", &ct_state));

  sts_state = TransportSecurityState::STSState();
  pkp_state = TransportSecurityState::PKPState();
  ct_state = TransportSecurityState::ExpectCTState();
  EXPECT_TRUE(
      GetStaticDomainState(&state, "hpkp.example.com", &sts_state, &pkp_state));
  EXPECT_TRUE(sts_state == TransportSecurityState::STSState());
  EXPECT_TRUE(pkp_state.include_subdomains);
  EXPECT_EQ(GURL("https://report.example.com/hpkp-upload"),
            pkp_state.report_uri);
  EXPECT_EQ(1U, pkp_state.spki_hashes.size());
  EXPECT_EQ(pkp_state.spki_hashes[0], GetSampleSPKIHash(0x1));
  EXPECT_EQ(0U, pkp_state.bad_spki_hashes.size());
  EXPECT_FALSE(GetExpectCTState(&state, "hpkp.example.com", &ct_state));

  sts_state = TransportSecurityState::STSState();
  pkp_state = TransportSecurityState::PKPState();
  ct_state = TransportSecurityState::ExpectCTState();
  EXPECT_TRUE(GetStaticDomainState(&state, "expect-ct.example.com", &sts_state,
                                   &pkp_state));
  EXPECT_TRUE(sts_state == TransportSecurityState::STSState());
  EXPECT_TRUE(pkp_state == TransportSecurityState::PKPState());
  EXPECT_TRUE(GetExpectCTState(&state, "expect-ct.example.com", &ct_state));
  EXPECT_EQ(GURL("https://report.example.com/ct-upload"), ct_state.report_uri);

  sts_state = TransportSecurityState::STSState();
  pkp_state = TransportSecurityState::PKPState();
  ct_state = TransportSecurityState::ExpectCTState();
  EXPECT_TRUE(
      GetStaticDomainState(&state, "mix.example.com", &sts_state, &pkp_state));
  EXPECT_FALSE(sts_state.include_subdomains);
  EXPECT_EQ(TransportSecurityState::STSState::MODE_FORCE_HTTPS,
            sts_state.upgrade_mode);
  EXPECT_TRUE(pkp_state.include_subdomains);
  EXPECT_EQ(GURL(), pkp_state.report_uri);
  EXPECT_EQ(1U, pkp_state.spki_hashes.size());
  EXPECT_EQ(pkp_state.spki_hashes[0], GetSampleSPKIHash(0x2));
  EXPECT_EQ(1U, pkp_state.bad_spki_hashes.size());
  EXPECT_EQ(pkp_state.bad_spki_hashes[0], GetSampleSPKIHash(0x1));
  EXPECT_TRUE(GetExpectCTState(&state, "mix.example.com", &ct_state));
  EXPECT_EQ(GURL("https://report.example.com/ct-upload-alt"),
            ct_state.report_uri);
}

// More advanced test for the HSTS preload process where the trie (generated
// from transport_security_state_static_unittest3.json) contains a mix of
// entries. Some entries share a prefix with the prefix also having its own
// preloaded state while others share no prefix. This results in a trie with
// several different internal structures. Test that the lookup methods can find
// all entries and correctly decode the different preloaded states (HSTS, HPKP,
// and Expect-CT) for each entry.
TEST_F(TransportSecurityStateTest, DecodePreloadedMultipleMix) {
  SetTransportSecurityStateSourceForTesting(&test3::kHSTSSource);

  TransportSecurityState state;
  TransportSecurityStateTest::EnableStaticPins(&state);
  TransportSecurityStateTest::EnableStaticExpectCT(&state);

  TransportSecurityState::STSState sts_state;
  TransportSecurityState::PKPState pkp_state;
  TransportSecurityState::ExpectCTState ct_state;

  EXPECT_TRUE(
      GetStaticDomainState(&state, "example.com", &sts_state, &pkp_state));
  EXPECT_TRUE(sts_state.include_subdomains);
  EXPECT_EQ(TransportSecurityState::STSState::MODE_FORCE_HTTPS,
            sts_state.upgrade_mode);
  EXPECT_TRUE(pkp_state == TransportSecurityState::PKPState());
  EXPECT_FALSE(GetExpectCTState(&state, "example.com", &ct_state));
  EXPECT_EQ(GURL(), ct_state.report_uri);

  sts_state = TransportSecurityState::STSState();
  pkp_state = TransportSecurityState::PKPState();
  ct_state = TransportSecurityState::ExpectCTState();
  EXPECT_TRUE(
      GetStaticDomainState(&state, "hpkp.example.com", &sts_state, &pkp_state));
  EXPECT_TRUE(sts_state == TransportSecurityState::STSState());
  EXPECT_TRUE(pkp_state.include_subdomains);
  EXPECT_EQ(GURL("https://report.example.com/hpkp-upload"),
            pkp_state.report_uri);
  EXPECT_EQ(1U, pkp_state.spki_hashes.size());
  EXPECT_EQ(pkp_state.spki_hashes[0], GetSampleSPKIHash(0x1));
  EXPECT_EQ(0U, pkp_state.bad_spki_hashes.size());
  EXPECT_FALSE(GetExpectCTState(&state, "hpkp.example.com", &ct_state));
  EXPECT_EQ(GURL(), ct_state.report_uri);

  sts_state = TransportSecurityState::STSState();
  pkp_state = TransportSecurityState::PKPState();
  ct_state = TransportSecurityState::ExpectCTState();
  EXPECT_TRUE(
      GetStaticDomainState(&state, "example.org", &sts_state, &pkp_state));
  EXPECT_FALSE(sts_state.include_subdomains);
  EXPECT_EQ(TransportSecurityState::STSState::MODE_FORCE_HTTPS,
            sts_state.upgrade_mode);
  EXPECT_TRUE(pkp_state == TransportSecurityState::PKPState());
  EXPECT_TRUE(GetExpectCTState(&state, "example.org", &ct_state));
  EXPECT_EQ(GURL("https://report.example.org/ct-upload"), ct_state.report_uri);

  sts_state = TransportSecurityState::STSState();
  pkp_state = TransportSecurityState::PKPState();
  ct_state = TransportSecurityState::ExpectCTState();
  EXPECT_TRUE(
      GetStaticDomainState(&state, "badssl.com", &sts_state, &pkp_state));
  EXPECT_TRUE(sts_state == TransportSecurityState::STSState());
  EXPECT_TRUE(pkp_state.include_subdomains);
  EXPECT_EQ(GURL("https://report.example.com/hpkp-upload"),
            pkp_state.report_uri);
  EXPECT_EQ(1U, pkp_state.spki_hashes.size());
  EXPECT_EQ(pkp_state.spki_hashes[0], GetSampleSPKIHash(0x1));
  EXPECT_EQ(0U, pkp_state.bad_spki_hashes.size());
  EXPECT_FALSE(GetExpectCTState(&state, "badssl.com", &ct_state));
  EXPECT_EQ(GURL(), ct_state.report_uri);

  sts_state = TransportSecurityState::STSState();
  pkp_state = TransportSecurityState::PKPState();
  ct_state = TransportSecurityState::ExpectCTState();
  EXPECT_TRUE(
      GetStaticDomainState(&state, "mix.badssl.com", &sts_state, &pkp_state));
  EXPECT_FALSE(sts_state.include_subdomains);
  EXPECT_EQ(TransportSecurityState::STSState::MODE_FORCE_HTTPS,
            sts_state.upgrade_mode);
  EXPECT_TRUE(pkp_state.include_subdomains);
  EXPECT_EQ(GURL(), pkp_state.report_uri);
  EXPECT_EQ(1U, pkp_state.spki_hashes.size());
  EXPECT_EQ(pkp_state.spki_hashes[0], GetSampleSPKIHash(0x2));
  EXPECT_EQ(1U, pkp_state.bad_spki_hashes.size());
  EXPECT_EQ(pkp_state.bad_spki_hashes[0], GetSampleSPKIHash(0x1));
  EXPECT_TRUE(GetExpectCTState(&state, "mix.badssl.com", &ct_state));
  EXPECT_EQ(GURL("https://report.example.com/ct-upload"), ct_state.report_uri);

  sts_state = TransportSecurityState::STSState();
  pkp_state = TransportSecurityState::PKPState();
  ct_state = TransportSecurityState::ExpectCTState();

  // This should be a simple entry in the context of
  // TrieWriter::IsSimpleEntry().
  EXPECT_TRUE(GetStaticDomainState(&state, "simple-entry.example.com",
                                   &sts_state, &pkp_state));
  EXPECT_TRUE(sts_state.include_subdomains);
  EXPECT_EQ(TransportSecurityState::STSState::MODE_FORCE_HTTPS,
            sts_state.upgrade_mode);
  EXPECT_TRUE(pkp_state == TransportSecurityState::PKPState());
  EXPECT_FALSE(GetExpectCTState(&state, "simple-entry.example.com", &ct_state));
}

TEST_F(TransportSecurityStateTest, HstsHostBypassList) {
  SetTransportSecurityStateSourceForTesting(&test_default::kHSTSSource);

  TransportSecurityState::STSState sts_state;
  TransportSecurityState::PKPState pkp_state;

  std::string preloaded_tld = "example";
  std::string subdomain = "sub.example";

  {
    TransportSecurityState state;
    // Check that "example" is preloaded with subdomains.
    EXPECT_TRUE(state.ShouldUpgradeToSSL(preloaded_tld));
    EXPECT_TRUE(state.ShouldUpgradeToSSL(subdomain));
  }

  {
    // Add "example" to the bypass list.
    TransportSecurityState state({preloaded_tld});
    EXPECT_FALSE(state.ShouldUpgradeToSSL(preloaded_tld));
    // The preloaded entry should still apply to the subdomain.
    EXPECT_TRUE(state.ShouldUpgradeToSSL(subdomain));
  }
}

// Tests that TransportSecurityState always consults the RequireCTDelegate,
// if supplied.
TEST_F(TransportSecurityStateTest, RequireCTConsultsDelegate) {
  using ::testing::_;
  using ::testing::Return;
  using CTRequirementLevel =
      TransportSecurityState::RequireCTDelegate::CTRequirementLevel;

  // Dummy cert to use as the validation chain. The contents do not matter.
  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(GetTestCertsDirectory(), "expired_cert.pem");
  ASSERT_TRUE(cert);

  HashValueVector hashes;
  hashes.push_back(
      HashValue(X509Certificate::CalculateFingerprint256(cert->cert_buffer())));

  // If CT is required, then the requirements are not met if the CT policy
  // wasn't met, but are met if the policy was met or the build was out of
  // date.
  {
    TransportSecurityState state;
    const TransportSecurityState::CTRequirementsStatus original_status =
        state.CheckCTRequirements(
            HostPortPair("www.example.com", 443), true, hashes, cert.get(),
            cert.get(), SignedCertificateTimestampAndStatusList(),
            TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
            ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
            NetworkIsolationKey());

    MockRequireCTDelegate always_require_delegate;
    EXPECT_CALL(always_require_delegate, IsCTRequiredForHost(_, _, _))
        .WillRepeatedly(Return(CTRequirementLevel::REQUIRED));
    state.SetRequireCTDelegate(&always_require_delegate);
    EXPECT_EQ(
        TransportSecurityState::CT_REQUIREMENTS_NOT_MET,
        state.CheckCTRequirements(
            HostPortPair("www.example.com", 443), true, hashes, cert.get(),
            cert.get(), SignedCertificateTimestampAndStatusList(),
            TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
            ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
            NetworkIsolationKey()));
    EXPECT_EQ(
        TransportSecurityState::CT_REQUIREMENTS_NOT_MET,
        state.CheckCTRequirements(
            HostPortPair("www.example.com", 443), true, hashes, cert.get(),
            cert.get(), SignedCertificateTimestampAndStatusList(),
            TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
            ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS,
            NetworkIsolationKey()));
    EXPECT_EQ(
        TransportSecurityState::CT_REQUIREMENTS_MET,
        state.CheckCTRequirements(
            HostPortPair("www.example.com", 443), true, hashes, cert.get(),
            cert.get(), SignedCertificateTimestampAndStatusList(),
            TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
            ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
            NetworkIsolationKey()));
    EXPECT_EQ(
        TransportSecurityState::CT_REQUIREMENTS_MET,
        state.CheckCTRequirements(
            HostPortPair("www.example.com", 443), true, hashes, cert.get(),
            cert.get(), SignedCertificateTimestampAndStatusList(),
            TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
            ct::CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY,
            NetworkIsolationKey()));

    state.SetRequireCTDelegate(nullptr);
    EXPECT_EQ(
        original_status,
        state.CheckCTRequirements(
            HostPortPair("www.example.com", 443), true, hashes, cert.get(),
            cert.get(), SignedCertificateTimestampAndStatusList(),
            TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
            ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
            NetworkIsolationKey()));
  }

  // If CT is not required, then regardless of the CT state for the host,
  // it should indicate CT is not required.
  {
    TransportSecurityState state;
    const TransportSecurityState::CTRequirementsStatus original_status =
        state.CheckCTRequirements(
            HostPortPair("www.example.com", 443), true, hashes, cert.get(),
            cert.get(), SignedCertificateTimestampAndStatusList(),
            TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
            ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
            NetworkIsolationKey());

    MockRequireCTDelegate never_require_delegate;
    EXPECT_CALL(never_require_delegate, IsCTRequiredForHost(_, _, _))
        .WillRepeatedly(Return(CTRequirementLevel::NOT_REQUIRED));
    state.SetRequireCTDelegate(&never_require_delegate);
    EXPECT_EQ(
        TransportSecurityState::CT_NOT_REQUIRED,
        state.CheckCTRequirements(
            HostPortPair("www.example.com", 443), true, hashes, cert.get(),
            cert.get(), SignedCertificateTimestampAndStatusList(),
            TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
            ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
            NetworkIsolationKey()));
    EXPECT_EQ(
        TransportSecurityState::CT_NOT_REQUIRED,
        state.CheckCTRequirements(
            HostPortPair("www.example.com", 443), true, hashes, cert.get(),
            cert.get(), SignedCertificateTimestampAndStatusList(),
            TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
            ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS,
            NetworkIsolationKey()));

    state.SetRequireCTDelegate(nullptr);
    EXPECT_EQ(
        original_status,
        state.CheckCTRequirements(
            HostPortPair("www.example.com", 443), true, hashes, cert.get(),
            cert.get(), SignedCertificateTimestampAndStatusList(),
            TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
            ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
            NetworkIsolationKey()));
  }

  // If the Delegate is in the default state, then it should return the same
  // result as if there was no delegate in the first place.
  {
    TransportSecurityState state;
    const TransportSecurityState::CTRequirementsStatus original_status =
        state.CheckCTRequirements(
            HostPortPair("www.example.com", 443), true, hashes, cert.get(),
            cert.get(), SignedCertificateTimestampAndStatusList(),
            TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
            ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
            NetworkIsolationKey());

    MockRequireCTDelegate default_require_ct_delegate;
    EXPECT_CALL(default_require_ct_delegate, IsCTRequiredForHost(_, _, _))
        .WillRepeatedly(Return(CTRequirementLevel::DEFAULT));
    state.SetRequireCTDelegate(&default_require_ct_delegate);
    EXPECT_EQ(
        original_status,
        state.CheckCTRequirements(
            HostPortPair("www.example.com", 443), true, hashes, cert.get(),
            cert.get(), SignedCertificateTimestampAndStatusList(),
            TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
            ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
            NetworkIsolationKey()));

    state.SetRequireCTDelegate(nullptr);
    EXPECT_EQ(
        original_status,
        state.CheckCTRequirements(
            HostPortPair("www.example.com", 443), true, hashes, cert.get(),
            cert.get(), SignedCertificateTimestampAndStatusList(),
            TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
            ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
            NetworkIsolationKey()));
  }
}

// Tests that Certificate Transparency is required for Symantec-issued
// certificates, unless the certificate was issued prior to 1 June 2016
// or the issuing CA is permitted as independently operated.
TEST_F(TransportSecurityStateTest, RequireCTForSymantec) {
  // Test certificates before and after the 1 June 2016 deadline.
  scoped_refptr<X509Certificate> before_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "pre_june_2016.pem");
  ASSERT_TRUE(before_cert);
  scoped_refptr<X509Certificate> after_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "post_june_2016.pem");
  ASSERT_TRUE(after_cert);

  const SHA256HashValue symantec_hash_value = {
      {0xb2, 0xde, 0xf5, 0x36, 0x2a, 0xd3, 0xfa, 0xcd, 0x04, 0xbd, 0x29,
       0x04, 0x7a, 0x43, 0x84, 0x4f, 0x76, 0x70, 0x34, 0xea, 0x48, 0x92,
       0xf8, 0x0e, 0x56, 0xbe, 0xe6, 0x90, 0x24, 0x3e, 0x25, 0x02}};
  const SHA256HashValue google_hash_value = {
      {0xec, 0x72, 0x29, 0x69, 0xcb, 0x64, 0x20, 0x0a, 0xb6, 0x63, 0x8f,
       0x68, 0xac, 0x53, 0x8e, 0x40, 0xab, 0xab, 0x5b, 0x19, 0xa6, 0x48,
       0x56, 0x61, 0x04, 0x2a, 0x10, 0x61, 0xc4, 0x61, 0x27, 0x76}};

  TransportSecurityState state;

  HashValueVector hashes;
  hashes.push_back(HashValue(symantec_hash_value));

  // Certificates issued by Symantec prior to 1 June 2016 should not
  // be required to be disclosed via CT.
  EXPECT_EQ(
      TransportSecurityState::CT_NOT_REQUIRED,
      state.CheckCTRequirements(
          HostPortPair("www.example.com", 443), true, hashes, before_cert.get(),
          before_cert.get(), SignedCertificateTimestampAndStatusList(),
          TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
          ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
          NetworkIsolationKey()));

  // ... but certificates issued after 1 June 2016 are required to be...
  EXPECT_EQ(
      TransportSecurityState::CT_REQUIREMENTS_NOT_MET,
      state.CheckCTRequirements(
          HostPortPair("www.example.com", 443), true, hashes, after_cert.get(),
          after_cert.get(), SignedCertificateTimestampAndStatusList(),
          TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
          ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
          NetworkIsolationKey()));
  EXPECT_EQ(
      TransportSecurityState::CT_REQUIREMENTS_NOT_MET,
      state.CheckCTRequirements(
          HostPortPair("www.example.com", 443), true, hashes, after_cert.get(),
          after_cert.get(), SignedCertificateTimestampAndStatusList(),
          TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
          ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS,
          NetworkIsolationKey()));
  EXPECT_EQ(
      TransportSecurityState::CT_REQUIREMENTS_MET,
      state.CheckCTRequirements(
          HostPortPair("www.example.com", 443), true, hashes, after_cert.get(),
          after_cert.get(), SignedCertificateTimestampAndStatusList(),
          TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
          ct::CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY,
          NetworkIsolationKey()));
  EXPECT_EQ(
      TransportSecurityState::CT_REQUIREMENTS_MET,
      state.CheckCTRequirements(
          HostPortPair("www.example.com", 443), true, hashes, after_cert.get(),
          after_cert.get(), SignedCertificateTimestampAndStatusList(),
          TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
          ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
          NetworkIsolationKey()));

  // ... unless they were issued by an excluded intermediate.
  hashes.push_back(HashValue(google_hash_value));
  EXPECT_EQ(
      TransportSecurityState::CT_NOT_REQUIRED,
      state.CheckCTRequirements(
          HostPortPair("www.example.com", 443), true, hashes, before_cert.get(),
          before_cert.get(), SignedCertificateTimestampAndStatusList(),
          TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
          ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
          NetworkIsolationKey()));
  EXPECT_EQ(
      TransportSecurityState::CT_NOT_REQUIRED,
      state.CheckCTRequirements(
          HostPortPair("www.example.com", 443), true, hashes, after_cert.get(),
          after_cert.get(), SignedCertificateTimestampAndStatusList(),
          TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
          ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
          NetworkIsolationKey()));

  // And other certificates should remain unaffected.
  SHA256HashValue unrelated_hash_value = {{0x01, 0x02}};
  HashValueVector unrelated_hashes;
  unrelated_hashes.push_back(HashValue(unrelated_hash_value));

  EXPECT_EQ(TransportSecurityState::CT_NOT_REQUIRED,
            state.CheckCTRequirements(
                HostPortPair("www.example.com", 443), true, unrelated_hashes,
                before_cert.get(), before_cert.get(),
                SignedCertificateTimestampAndStatusList(),
                TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
                ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
                NetworkIsolationKey()));
  EXPECT_EQ(TransportSecurityState::CT_NOT_REQUIRED,
            state.CheckCTRequirements(
                HostPortPair("www.example.com", 443), true, unrelated_hashes,
                after_cert.get(), after_cert.get(),
                SignedCertificateTimestampAndStatusList(),
                TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
                ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
                NetworkIsolationKey()));
}

// Tests that CAs can enable CT for testing their issuance practices, prior
// to CT becoming mandatory.
TEST_F(TransportSecurityStateTest, RequireCTViaFieldTrial) {
  // Test certificates before and after the 1 June 2016 deadline.
  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(GetTestCertsDirectory(), "dec_2017.pem");
  ASSERT_TRUE(cert);

  // The hashes here do not matter, but add some dummy values to simulate
  // a 'real' chain.
  HashValueVector hashes;
  const SHA256HashValue hash_a = {{0xAA, 0xAA}};
  hashes.push_back(HashValue(hash_a));
  const SHA256HashValue hash_b = {{0xBB, 0xBB}};
  hashes.push_back(HashValue(hash_b));

  TransportSecurityState state;

  // CT should not be required for this pre-existing certificate.
  EXPECT_EQ(TransportSecurityState::CT_NOT_REQUIRED,
            state.CheckCTRequirements(
                HostPortPair("www.example.com", 443), true, hashes, cert.get(),
                cert.get(), SignedCertificateTimestampAndStatusList(),
                TransportSecurityState::DISABLE_EXPECT_CT_REPORTS,
                ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
                NetworkIsolationKey()));

  // However, simulating a Field Trial in which CT is required for certificates
  // after 2017-12-01 should cause CT to be required for this certificate, as
  // it was issued 2017-12-20.

  base::FieldTrialParams params;
  // Set the enforcement date to 2017-12-01 00:00:00;
  params["date"] = "1512086400";

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(kEnforceCTForNewCerts,
                                                         params);

  // It should fail if it doesn't comply with policy.
  EXPECT_EQ(TransportSecurityState::CT_REQUIREMENTS_NOT_MET,
            state.CheckCTRequirements(
                HostPortPair("www.example.com", 443), true, hashes, cert.get(),
                cert.get(), SignedCertificateTimestampAndStatusList(),
                TransportSecurityState::DISABLE_EXPECT_CT_REPORTS,
                ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
                NetworkIsolationKey()));

  // It should succeed if it does comply with policy.
  EXPECT_EQ(TransportSecurityState::CT_REQUIREMENTS_MET,
            state.CheckCTRequirements(
                HostPortPair("www.example.com", 443), true, hashes, cert.get(),
                cert.get(), SignedCertificateTimestampAndStatusList(),
                TransportSecurityState::DISABLE_EXPECT_CT_REPORTS,
                ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
                NetworkIsolationKey()));

  // It should succeed if the build is outdated.
  EXPECT_EQ(TransportSecurityState::CT_REQUIREMENTS_MET,
            state.CheckCTRequirements(
                HostPortPair("www.example.com", 443), true, hashes, cert.get(),
                cert.get(), SignedCertificateTimestampAndStatusList(),
                TransportSecurityState::DISABLE_EXPECT_CT_REPORTS,
                ct::CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY,
                NetworkIsolationKey()));

  // It should succeed if it was a locally-trusted CA.
  EXPECT_EQ(TransportSecurityState::CT_NOT_REQUIRED,
            state.CheckCTRequirements(
                HostPortPair("www.example.com", 443), false, hashes, cert.get(),
                cert.get(), SignedCertificateTimestampAndStatusList(),
                TransportSecurityState::DISABLE_EXPECT_CT_REPORTS,
                ct::CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY,
                NetworkIsolationKey()));
}

// Tests that Certificate Transparency is required for all of the Symantec
// Managed CAs, regardless of when the certificate was issued.
TEST_F(TransportSecurityStateTest, RequireCTForSymantecManagedCAs) {
  const SHA256HashValue symantec_hash_value = {
      {0xb2, 0xde, 0xf5, 0x36, 0x2a, 0xd3, 0xfa, 0xcd, 0x04, 0xbd, 0x29,
       0x04, 0x7a, 0x43, 0x84, 0x4f, 0x76, 0x70, 0x34, 0xea, 0x48, 0x92,
       0xf8, 0x0e, 0x56, 0xbe, 0xe6, 0x90, 0x24, 0x3e, 0x25, 0x02}};
  const SHA256HashValue managed_hash_value = {
      {0x7c, 0xac, 0x9a, 0x0f, 0xf3, 0x15, 0x38, 0x77, 0x50, 0xba, 0x8b,
       0xaf, 0xdb, 0x1c, 0x2b, 0xc2, 0x9b, 0x3f, 0x0b, 0xba, 0x16, 0x36,
       0x2c, 0xa9, 0x3a, 0x90, 0xf8, 0x4d, 0xa2, 0xdf, 0x5f, 0x3e}};

  TransportSecurityState state;

  HashValueVector hashes;
  hashes.push_back(HashValue(symantec_hash_value));
  hashes.push_back(HashValue(managed_hash_value));

  // All certificates, both before and after the pre-existing 1 June 2016
  // date, are expected to be compliant.
  scoped_refptr<X509Certificate> before_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "pre_june_2016.pem");
  ASSERT_TRUE(before_cert);

  EXPECT_EQ(
      TransportSecurityState::CT_REQUIREMENTS_NOT_MET,
      state.CheckCTRequirements(
          HostPortPair("www.example.com", 443), true, hashes, before_cert.get(),
          before_cert.get(), SignedCertificateTimestampAndStatusList(),
          TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
          ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
          NetworkIsolationKey()));
  EXPECT_EQ(
      TransportSecurityState::CT_REQUIREMENTS_NOT_MET,
      state.CheckCTRequirements(
          HostPortPair("www.example.com", 443), true, hashes, before_cert.get(),
          before_cert.get(), SignedCertificateTimestampAndStatusList(),
          TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
          ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS,
          NetworkIsolationKey()));
  EXPECT_EQ(
      TransportSecurityState::CT_REQUIREMENTS_MET,
      state.CheckCTRequirements(
          HostPortPair("www.example.com", 443), true, hashes, before_cert.get(),
          before_cert.get(), SignedCertificateTimestampAndStatusList(),
          TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
          ct::CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY,
          NetworkIsolationKey()));
  EXPECT_EQ(
      TransportSecurityState::CT_REQUIREMENTS_MET,
      state.CheckCTRequirements(
          HostPortPair("www.example.com", 443), true, hashes, before_cert.get(),
          before_cert.get(), SignedCertificateTimestampAndStatusList(),
          TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
          ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
          NetworkIsolationKey()));

  scoped_refptr<X509Certificate> after_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "post_june_2016.pem");
  ASSERT_TRUE(after_cert);

  EXPECT_EQ(
      TransportSecurityState::CT_REQUIREMENTS_NOT_MET,
      state.CheckCTRequirements(
          HostPortPair("www.example.com", 443), true, hashes, after_cert.get(),
          after_cert.get(), SignedCertificateTimestampAndStatusList(),
          TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
          ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
          NetworkIsolationKey()));
  EXPECT_EQ(
      TransportSecurityState::CT_REQUIREMENTS_NOT_MET,
      state.CheckCTRequirements(
          HostPortPair("www.example.com", 443), true, hashes, after_cert.get(),
          after_cert.get(), SignedCertificateTimestampAndStatusList(),
          TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
          ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS,
          NetworkIsolationKey()));
  EXPECT_EQ(
      TransportSecurityState::CT_REQUIREMENTS_MET,
      state.CheckCTRequirements(
          HostPortPair("www.example.com", 443), true, hashes, after_cert.get(),
          after_cert.get(), SignedCertificateTimestampAndStatusList(),
          TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
          ct::CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY,
          NetworkIsolationKey()));
  EXPECT_EQ(
      TransportSecurityState::CT_REQUIREMENTS_MET,
      state.CheckCTRequirements(
          HostPortPair("www.example.com", 443), true, hashes, after_cert.get(),
          after_cert.get(), SignedCertificateTimestampAndStatusList(),
          TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
          ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
          NetworkIsolationKey()));
}

// Tests that dynamic Expect-CT state is cleared from ClearDynamicData().
TEST_F(TransportSecurityStateTest, DynamicExpectCTStateCleared) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      TransportSecurityState::kDynamicExpectCTFeature);
  const std::string host("example.test");
  TransportSecurityState state;
  TransportSecurityState::ExpectCTState expect_ct_state;
  const base::Time current_time = base::Time::Now();
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  state.AddExpectCT(host, expiry, true, GURL(), NetworkIsolationKey());
  EXPECT_TRUE(state.GetDynamicExpectCTState(host, NetworkIsolationKey(),
                                            &expect_ct_state));
  EXPECT_TRUE(expect_ct_state.enforce);
  EXPECT_TRUE(expect_ct_state.report_uri.is_empty());
  EXPECT_EQ(expiry, expect_ct_state.expiry);

  state.ClearDynamicData();
  EXPECT_FALSE(state.GetDynamicExpectCTState(host, NetworkIsolationKey(),
                                             &expect_ct_state));
}

// Tests that dynamic Expect-CT state can be added and retrieved.
TEST_F(TransportSecurityStateTest, DynamicExpectCTState) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      TransportSecurityState::kDynamicExpectCTFeature);
  const std::string host("example.test");
  TransportSecurityState state;
  TransportSecurityState::ExpectCTState expect_ct_state;
  const base::Time current_time = base::Time::Now();
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  // Test that Expect-CT state can be added and retrieved.
  state.AddExpectCT(host, expiry, true, GURL(), NetworkIsolationKey());
  EXPECT_TRUE(state.GetDynamicExpectCTState(host, NetworkIsolationKey(),
                                            &expect_ct_state));
  EXPECT_TRUE(expect_ct_state.enforce);
  EXPECT_TRUE(expect_ct_state.report_uri.is_empty());
  EXPECT_EQ(expiry, expect_ct_state.expiry);

  // Test that Expect-CT can be updated (e.g. by changing |enforce| to false and
  // adding a report-uri).
  const GURL report_uri("https://example-report.test");
  state.AddExpectCT(host, expiry, false, report_uri, NetworkIsolationKey());
  EXPECT_TRUE(state.GetDynamicExpectCTState(host, NetworkIsolationKey(),
                                            &expect_ct_state));
  EXPECT_FALSE(expect_ct_state.enforce);
  EXPECT_EQ(report_uri, expect_ct_state.report_uri);
  EXPECT_EQ(expiry, expect_ct_state.expiry);

  // Test that Expect-CT state is discarded when expired.
  state.AddExpectCT(host, current_time - base::TimeDelta::FromSeconds(1000),
                    true, report_uri, NetworkIsolationKey());
  EXPECT_FALSE(state.GetDynamicExpectCTState(host, NetworkIsolationKey(),
                                             &expect_ct_state));
}

// Tests that the Expect-CT reporter is not notified for repeated dynamic
// Expect-CT violations for the same host/port.
TEST_F(TransportSecurityStateTest, DynamicExpectCTDeduping) {
  const char kHeader[] = "max-age=123,enforce,report-uri=\"http://foo.test\"";
  SSLInfo ssl;
  ssl.is_issued_by_known_root = true;
  ssl.ct_policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS;

  scoped_refptr<X509Certificate> cert1 =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  ASSERT_TRUE(cert1);
  scoped_refptr<X509Certificate> cert2 =
      ImportCertFromFile(GetTestCertsDirectory(), "expired_cert.pem");
  ASSERT_TRUE(cert2);

  SignedCertificateTimestampAndStatusList sct_list;

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      TransportSecurityState::kDynamicExpectCTFeature);
  base::Time now = base::Time::Now();
  TransportSecurityState state;
  MockExpectCTReporter reporter;
  state.SetExpectCTReporter(&reporter);
  state.ProcessExpectCTHeader(kHeader, HostPortPair("example.test", 443), ssl,
                              NetworkIsolationKey());
  TransportSecurityState::ExpectCTState expect_ct_state;
  EXPECT_TRUE(state.GetDynamicExpectCTState(
      "example.test", NetworkIsolationKey(), &expect_ct_state));
  EXPECT_EQ(GURL("http://foo.test"), expect_ct_state.report_uri);
  EXPECT_TRUE(expect_ct_state.enforce);
  EXPECT_LT(now, expect_ct_state.expiry);
  // No report should be sent when the header was processed over a connection
  // that complied with CT policy.
  EXPECT_EQ(0u, reporter.num_failures());

  // The first time the host fails to meet CT requirements, a report should be
  // sent.
  EXPECT_EQ(TransportSecurityState::CT_REQUIREMENTS_NOT_MET,
            state.CheckCTRequirements(
                HostPortPair("example.test", 443), true, HashValueVector(),
                cert1.get(), cert2.get(), sct_list,
                TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
                ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
                NetworkIsolationKey()));
  EXPECT_EQ(1u, reporter.num_failures());

  // The second time it fails to meet CT requirements, a report should not be
  // sent.
  EXPECT_EQ(TransportSecurityState::CT_REQUIREMENTS_NOT_MET,
            state.CheckCTRequirements(
                HostPortPair("example.test", 443), true, HashValueVector(),
                cert1.get(), cert2.get(), sct_list,
                TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
                ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
                NetworkIsolationKey()));
  EXPECT_EQ(1u, reporter.num_failures());
}

// Tests that the Expect-CT reporter is not notified for CT-compliant
// connections.
TEST_F(TransportSecurityStateTest, DynamicExpectCTCompliantConnection) {
  const char kHeader[] = "max-age=123,report-uri=\"http://foo.test\"";
  SSLInfo ssl;
  ssl.is_issued_by_known_root = true;
  ssl.ct_policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS;

  scoped_refptr<X509Certificate> cert1 =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  ASSERT_TRUE(cert1);
  scoped_refptr<X509Certificate> cert2 =
      ImportCertFromFile(GetTestCertsDirectory(), "expired_cert.pem");
  ASSERT_TRUE(cert2);

  SignedCertificateTimestampAndStatusList sct_list;

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      TransportSecurityState::kDynamicExpectCTFeature);

  TransportSecurityState state;
  MockExpectCTReporter reporter;
  state.SetExpectCTReporter(&reporter);
  state.ProcessExpectCTHeader(kHeader, HostPortPair("example.test", 443), ssl,
                              NetworkIsolationKey());

  // No report should be sent when the header was processed over a connection
  // that complied with CT policy.
  EXPECT_EQ(TransportSecurityState::CT_NOT_REQUIRED,
            state.CheckCTRequirements(
                HostPortPair("example.test", 443), true, HashValueVector(),
                cert1.get(), cert2.get(), sct_list,
                TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
                ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
                NetworkIsolationKey()));
  EXPECT_EQ(0u, reporter.num_failures());
}

// Tests that the Expect-CT reporter is not notified when the Expect-CT header
// is received repeatedly over non-compliant connections.
TEST_F(TransportSecurityStateTest, DynamicExpectCTHeaderProcessingDeduping) {
  const char kHeader[] = "max-age=123,enforce,report-uri=\"http://foo.test\"";
  SSLInfo ssl;
  ssl.is_issued_by_known_root = true;
  ssl.ct_policy_compliance = ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS;

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      TransportSecurityState::kDynamicExpectCTFeature);
  TransportSecurityState state;
  MockExpectCTReporter reporter;
  state.SetExpectCTReporter(&reporter);
  state.ProcessExpectCTHeader(kHeader, HostPortPair("example.test", 443), ssl,
                              NetworkIsolationKey());
  TransportSecurityState::ExpectCTState expect_ct_state;
  EXPECT_FALSE(state.GetDynamicExpectCTState(
      "example.test", NetworkIsolationKey(), &expect_ct_state));
  // The first time the header was received over a connection that failed to
  // meet CT requirements, a report should be sent.
  EXPECT_EQ(1u, reporter.num_failures());

  // The second time the header was received, no report should be sent.
  state.ProcessExpectCTHeader(kHeader, HostPortPair("example.test", 443), ssl,
                              NetworkIsolationKey());
  EXPECT_EQ(1u, reporter.num_failures());
}

// Tests that dynamic Expect-CT state cannot be added when the feature is not
// enabled.
TEST_F(TransportSecurityStateTest, DynamicExpectCTStateDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      TransportSecurityState::kDynamicExpectCTFeature);
  const std::string host("example.test");
  TransportSecurityState state;
  TransportSecurityState::ExpectCTState expect_ct_state;
  const base::Time current_time = base::Time::Now();
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  state.AddExpectCT(host, expiry, true, GURL(), NetworkIsolationKey());
  EXPECT_FALSE(state.GetDynamicExpectCTState(host, NetworkIsolationKey(),
                                             &expect_ct_state));
}

// Tests that dynamic Expect-CT opt-ins are processed correctly (when the
// feature is enabled).
TEST_F(TransportSecurityStateTest, DynamicExpectCT) {
  const char kHeader[] = "max-age=123,enforce,report-uri=\"http://foo.test\"";
  SSLInfo ssl;
  ssl.is_issued_by_known_root = true;
  ssl.ct_policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS;

  // First test that the header is not processed when the feature is disabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        TransportSecurityState::kDynamicExpectCTFeature);
    TransportSecurityState state;
    state.ProcessExpectCTHeader(kHeader, HostPortPair("example.test", 443), ssl,
                                NetworkIsolationKey());
    TransportSecurityState::ExpectCTState expect_ct_state;
    EXPECT_FALSE(state.GetDynamicExpectCTState(
        "example.test", NetworkIsolationKey(), &expect_ct_state));
  }

  // Now test that the header is processed when the feature is enabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        TransportSecurityState::kDynamicExpectCTFeature);
    base::Time now = base::Time::Now();
    TransportSecurityState state;
    MockExpectCTReporter reporter;
    state.SetExpectCTReporter(&reporter);
    state.ProcessExpectCTHeader(kHeader, HostPortPair("example.test", 443), ssl,
                                NetworkIsolationKey());
    TransportSecurityState::ExpectCTState expect_ct_state;
    EXPECT_TRUE(state.GetDynamicExpectCTState(
        "example.test", NetworkIsolationKey(), &expect_ct_state));
    EXPECT_EQ(GURL("http://foo.test"), expect_ct_state.report_uri);
    EXPECT_TRUE(expect_ct_state.enforce);
    EXPECT_LT(now, expect_ct_state.expiry);
    // No report should be sent when the header was processed over a connection
    // that complied with CT policy.
    EXPECT_EQ(0u, reporter.num_failures());
  }
}

// Tests that dynamic Expect-CT is not processed for private roots.
TEST_F(TransportSecurityStateTest, DynamicExpectCTPrivateRoot) {
  const char kHeader[] = "max-age=123,enforce,report-uri=\"http://foo.test\"";
  SSLInfo ssl;
  ssl.is_issued_by_known_root = false;
  ssl.ct_policy_compliance = ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS;

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      TransportSecurityState::kDynamicExpectCTFeature);
  TransportSecurityState state;
  MockExpectCTReporter reporter;
  state.SetExpectCTReporter(&reporter);
  state.ProcessExpectCTHeader(kHeader, HostPortPair("example.test", 443), ssl,
                              NetworkIsolationKey());
  TransportSecurityState::ExpectCTState expect_ct_state;
  EXPECT_FALSE(state.GetDynamicExpectCTState(
      "example.test", NetworkIsolationKey(), &expect_ct_state));
  EXPECT_EQ(0u, reporter.num_failures());
}

// Tests that dynamic Expect-CT is not processed when CT compliance status
// wasn't computed.
TEST_F(TransportSecurityStateTest, DynamicExpectCTNoComplianceDetails) {
  const char kHeader[] = "max-age=123,enforce,report-uri=\"http://foo.test\"";
  SSLInfo ssl;
  ssl.is_issued_by_known_root = true;
  ssl.ct_policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_COMPLIANCE_DETAILS_NOT_AVAILABLE;

  scoped_refptr<X509Certificate> cert1 =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  ASSERT_TRUE(cert1);
  scoped_refptr<X509Certificate> cert2 =
      ImportCertFromFile(GetTestCertsDirectory(), "expired_cert.pem");
  ASSERT_TRUE(cert2);
  ssl.unverified_cert = cert1;
  ssl.cert = cert2;

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      TransportSecurityState::kDynamicExpectCTFeature);
  TransportSecurityState state;
  MockExpectCTReporter reporter;
  state.SetExpectCTReporter(&reporter);
  state.ProcessExpectCTHeader(kHeader, HostPortPair("example.test", 443), ssl,
                              NetworkIsolationKey());
  TransportSecurityState::ExpectCTState expect_ct_state;
  EXPECT_FALSE(state.GetDynamicExpectCTState(
      "example.test", NetworkIsolationKey(), &expect_ct_state));
  EXPECT_EQ(0u, reporter.num_failures());
}

// Tests that Expect-CT reports are sent when an Expect-CT header is received
// over a non-compliant connection.
TEST_F(TransportSecurityStateTest,
       DynamicExpectCTHeaderProcessingNonCompliant) {
  const char kHeader[] = "max-age=123,enforce,report-uri=\"http://foo.test\"";
  SSLInfo ssl;
  ssl.is_issued_by_known_root = true;
  ssl.ct_policy_compliance = ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS;

  scoped_refptr<X509Certificate> cert1 =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  ASSERT_TRUE(cert1);
  scoped_refptr<X509Certificate> cert2 =
      ImportCertFromFile(GetTestCertsDirectory(), "expired_cert.pem");
  ASSERT_TRUE(cert2);
  ssl.unverified_cert = cert1;
  ssl.cert = cert2;

  MakeTestSCTAndStatus(ct::SignedCertificateTimestamp::SCT_EMBEDDED, "test_log",
                       std::string(), std::string(), base::Time::Now(),
                       ct::SCT_STATUS_INVALID_SIGNATURE,
                       &ssl.signed_certificate_timestamps);

  NetworkIsolationKey network_isolation_key =
      NetworkIsolationKey::CreateTransient();
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      TransportSecurityState::kDynamicExpectCTFeature);
  TransportSecurityState state;
  MockExpectCTReporter reporter;
  state.SetExpectCTReporter(&reporter);
  state.ProcessExpectCTHeader(kHeader, HostPortPair("example.test", 443), ssl,
                              network_isolation_key);
  TransportSecurityState::ExpectCTState expect_ct_state;
  EXPECT_FALSE(state.GetDynamicExpectCTState(
      "example.test", NetworkIsolationKey(), &expect_ct_state));
  EXPECT_EQ(1u, reporter.num_failures());
  EXPECT_EQ("example.test", reporter.host_port_pair().host());
  EXPECT_TRUE(reporter.expiration().is_null());
  EXPECT_EQ(cert1.get(), reporter.served_certificate_chain());
  EXPECT_EQ(cert2.get(), reporter.validated_certificate_chain());
  EXPECT_EQ(ssl.signed_certificate_timestamps.size(),
            reporter.signed_certificate_timestamps().size());
  EXPECT_EQ(ssl.signed_certificate_timestamps[0].status,
            reporter.signed_certificate_timestamps()[0].status);
  EXPECT_EQ(ssl.signed_certificate_timestamps[0].sct,
            reporter.signed_certificate_timestamps()[0].sct);
  EXPECT_EQ(network_isolation_key, reporter.network_isolation_key());
}

// Tests that CheckCTRequirements() returns the correct response if a connection
// to a host violates an Expect-CT header, and that it reports violations.
TEST_F(TransportSecurityStateTest, CheckCTRequirementsWithExpectCT) {
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  scoped_refptr<X509Certificate> cert1 =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  ASSERT_TRUE(cert1);
  scoped_refptr<X509Certificate> cert2 =
      ImportCertFromFile(GetTestCertsDirectory(), "expired_cert.pem");
  ASSERT_TRUE(cert2);
  SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(ct::SignedCertificateTimestamp::SCT_EMBEDDED, "test_log",
                       std::string(), std::string(), base::Time::Now(),
                       ct::SCT_STATUS_INVALID_SIGNATURE, &sct_list);

  NetworkIsolationKey network_isolation_key =
      NetworkIsolationKey::CreateTransient();
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      TransportSecurityState::kDynamicExpectCTFeature);

  TransportSecurityState state;
  MockExpectCTReporter reporter;
  state.SetExpectCTReporter(&reporter);
  state.AddExpectCT("example.test", expiry, true /* enforce */,
                    GURL("https://example-report.test"), network_isolation_key);
  state.AddExpectCT("example-report-only.test", expiry, false /* enforce */,
                    GURL("https://example-report.test"), network_isolation_key);
  state.AddExpectCT("example-enforce-only.test", expiry, true /* enforce */,
                    GURL(), network_isolation_key);

  // Test that a connection to an unrelated host is not affected.
  EXPECT_EQ(TransportSecurityState::CT_NOT_REQUIRED,
            state.CheckCTRequirements(
                HostPortPair("example2.test", 443), true, HashValueVector(),
                cert1.get(), cert2.get(), sct_list,
                TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
                ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
                network_isolation_key));
  EXPECT_EQ(TransportSecurityState::CT_NOT_REQUIRED,
            state.CheckCTRequirements(
                HostPortPair("example2.test", 443), true, HashValueVector(),
                cert1.get(), cert2.get(), sct_list,
                TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
                ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS,
                network_isolation_key));
  EXPECT_EQ(0u, reporter.num_failures());

  // A connection to an Expect-CT host should be closed and reported.
  EXPECT_EQ(TransportSecurityState::CT_REQUIREMENTS_NOT_MET,
            state.CheckCTRequirements(
                HostPortPair("example.test", 443), true, HashValueVector(),
                cert1.get(), cert2.get(), sct_list,
                TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
                ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
                network_isolation_key));
  EXPECT_EQ(1u, reporter.num_failures());
  EXPECT_EQ("example.test", reporter.host_port_pair().host());
  EXPECT_EQ(443, reporter.host_port_pair().port());
  EXPECT_EQ(expiry, reporter.expiration());
  EXPECT_EQ(cert1.get(), reporter.validated_certificate_chain());
  EXPECT_EQ(cert2.get(), reporter.served_certificate_chain());
  EXPECT_EQ(sct_list.size(), reporter.signed_certificate_timestamps().size());
  EXPECT_EQ(sct_list[0].status,
            reporter.signed_certificate_timestamps()[0].status);
  EXPECT_EQ(sct_list[0].sct, reporter.signed_certificate_timestamps()[0].sct);
  EXPECT_EQ(network_isolation_key, reporter.network_isolation_key());

  // A compliant connection to an Expect-CT host should not be closed or
  // reported.
  EXPECT_EQ(TransportSecurityState::CT_REQUIREMENTS_MET,
            state.CheckCTRequirements(
                HostPortPair("example.test", 443), true, HashValueVector(),
                cert1.get(), cert2.get(), sct_list,
                TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
                ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
                network_isolation_key));
  EXPECT_EQ(1u, reporter.num_failures());
  EXPECT_EQ(TransportSecurityState::CT_REQUIREMENTS_MET,
            state.CheckCTRequirements(
                HostPortPair("example.test", 443), true, HashValueVector(),
                cert1.get(), cert2.get(), sct_list,
                TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
                ct::CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY,
                network_isolation_key));
  EXPECT_EQ(1u, reporter.num_failures());

  // A connection to a report-only host should be reported only.
  EXPECT_EQ(TransportSecurityState::CT_NOT_REQUIRED,
            state.CheckCTRequirements(
                HostPortPair("example-report-only.test", 443), true,
                HashValueVector(), cert1.get(), cert2.get(), sct_list,
                TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
                ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS,
                network_isolation_key));
  EXPECT_EQ(2u, reporter.num_failures());
  EXPECT_EQ("example-report-only.test", reporter.host_port_pair().host());
  EXPECT_EQ(443, reporter.host_port_pair().port());
  EXPECT_EQ(cert1.get(), reporter.validated_certificate_chain());
  EXPECT_EQ(cert2.get(), reporter.served_certificate_chain());
  EXPECT_EQ(sct_list.size(), reporter.signed_certificate_timestamps().size());
  EXPECT_EQ(sct_list[0].status,
            reporter.signed_certificate_timestamps()[0].status);
  EXPECT_EQ(sct_list[0].sct, reporter.signed_certificate_timestamps()[0].sct);
  EXPECT_EQ(network_isolation_key, reporter.network_isolation_key());

  // A connection to an enforce-only host should be closed but not reported.
  EXPECT_EQ(TransportSecurityState::CT_REQUIREMENTS_NOT_MET,
            state.CheckCTRequirements(
                HostPortPair("example-enforce-only.test", 443), true,
                HashValueVector(), cert1.get(), cert2.get(), sct_list,
                TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
                ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS,
                network_isolation_key));
  EXPECT_EQ(2u, reporter.num_failures());

  // A connection with a private root should be neither enforced nor reported.
  EXPECT_EQ(TransportSecurityState::CT_NOT_REQUIRED,
            state.CheckCTRequirements(
                HostPortPair("example.test", 443), false, HashValueVector(),
                cert1.get(), cert2.get(), sct_list,
                TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
                ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
                network_isolation_key));
  EXPECT_EQ(2u, reporter.num_failures());

  // A connection with DISABLE_EXPECT_CT_REPORTS should not send a report.
  EXPECT_EQ(TransportSecurityState::CT_REQUIREMENTS_NOT_MET,
            state.CheckCTRequirements(
                HostPortPair("example.test", 443), true, HashValueVector(),
                cert1.get(), cert2.get(), sct_list,
                TransportSecurityState::DISABLE_EXPECT_CT_REPORTS,
                ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
                network_isolation_key));
  EXPECT_EQ(2u, reporter.num_failures());
}

// Tests that for a host that requires CT by delegate and is also
// Expect-CT-enabled, CheckCTRequirements() sends reports.
TEST_F(TransportSecurityStateTest, CheckCTRequirementsWithExpectCTAndDelegate) {
  using ::testing::_;
  using ::testing::Return;
  using CTRequirementLevel =
      TransportSecurityState::RequireCTDelegate::CTRequirementLevel;

  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  scoped_refptr<X509Certificate> cert1 =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  ASSERT_TRUE(cert1);
  scoped_refptr<X509Certificate> cert2 =
      ImportCertFromFile(GetTestCertsDirectory(), "expired_cert.pem");
  ASSERT_TRUE(cert2);
  SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(ct::SignedCertificateTimestamp::SCT_EMBEDDED, "test_log",
                       std::string(), std::string(), base::Time::Now(),
                       ct::SCT_STATUS_INVALID_SIGNATURE, &sct_list);
  NetworkIsolationKey network_isolation_key =
      NetworkIsolationKey::CreateTransient();

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      TransportSecurityState::kDynamicExpectCTFeature);

  TransportSecurityState state;
  MockExpectCTReporter reporter;
  state.SetExpectCTReporter(&reporter);
  state.AddExpectCT("example.test", expiry, false /* enforce */,
                    GURL("https://example-report.test"), network_isolation_key);

  // A connection to an Expect-CT host, which also requires CT by the delegate,
  // should be closed and reported.
  MockRequireCTDelegate always_require_delegate;
  EXPECT_CALL(always_require_delegate, IsCTRequiredForHost(_, _, _))
      .WillRepeatedly(Return(CTRequirementLevel::REQUIRED));
  state.SetRequireCTDelegate(&always_require_delegate);
  EXPECT_EQ(TransportSecurityState::CT_REQUIREMENTS_NOT_MET,
            state.CheckCTRequirements(
                HostPortPair("example.test", 443), true, HashValueVector(),
                cert1.get(), cert2.get(), sct_list,
                TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
                ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
                network_isolation_key));
  EXPECT_EQ(1u, reporter.num_failures());
  EXPECT_EQ("example.test", reporter.host_port_pair().host());
  EXPECT_EQ(443, reporter.host_port_pair().port());
  EXPECT_EQ(expiry, reporter.expiration());
  EXPECT_EQ(cert1.get(), reporter.validated_certificate_chain());
  EXPECT_EQ(cert2.get(), reporter.served_certificate_chain());
  EXPECT_EQ(sct_list.size(), reporter.signed_certificate_timestamps().size());
  EXPECT_EQ(sct_list[0].status,
            reporter.signed_certificate_timestamps()[0].status);
  EXPECT_EQ(sct_list[0].sct, reporter.signed_certificate_timestamps()[0].sct);
  EXPECT_EQ(network_isolation_key, reporter.network_isolation_key());
}

// Tests that for a host that explicitly disabled CT by delegate and is also
// Expect-CT-enabled, CheckCTRequirements() sends reports.
TEST_F(TransportSecurityStateTest,
       CheckCTRequirementsWithExpectCTAndDelegateDisables) {
  using ::testing::_;
  using ::testing::Return;
  using CTRequirementLevel =
      TransportSecurityState::RequireCTDelegate::CTRequirementLevel;

  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  scoped_refptr<X509Certificate> cert1 =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  ASSERT_TRUE(cert1);
  scoped_refptr<X509Certificate> cert2 =
      ImportCertFromFile(GetTestCertsDirectory(), "expired_cert.pem");
  ASSERT_TRUE(cert2);
  SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(ct::SignedCertificateTimestamp::SCT_EMBEDDED, "test_log",
                       std::string(), std::string(), base::Time::Now(),
                       ct::SCT_STATUS_INVALID_SIGNATURE, &sct_list);
  NetworkIsolationKey network_isolation_key =
      NetworkIsolationKey::CreateTransient();

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      TransportSecurityState::kDynamicExpectCTFeature);

  TransportSecurityState state;
  MockExpectCTReporter reporter;
  state.SetExpectCTReporter(&reporter);
  state.AddExpectCT("example.test", expiry, false /* enforce */,
                    GURL("https://example-report.test"), network_isolation_key);

  // A connection to an Expect-CT host, which is exempted from the CT
  // requirements by the delegate, should be reported but not closed.
  MockRequireCTDelegate never_require_delegate;
  EXPECT_CALL(never_require_delegate, IsCTRequiredForHost(_, _, _))
      .WillRepeatedly(Return(CTRequirementLevel::NOT_REQUIRED));
  state.SetRequireCTDelegate(&never_require_delegate);
  EXPECT_EQ(TransportSecurityState::CT_NOT_REQUIRED,
            state.CheckCTRequirements(
                HostPortPair("example.test", 443), true, HashValueVector(),
                cert1.get(), cert2.get(), sct_list,
                TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
                ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
                network_isolation_key));
  EXPECT_EQ(1u, reporter.num_failures());
  EXPECT_EQ("example.test", reporter.host_port_pair().host());
  EXPECT_EQ(443, reporter.host_port_pair().port());
  EXPECT_EQ(expiry, reporter.expiration());
  EXPECT_EQ(cert1.get(), reporter.validated_certificate_chain());
  EXPECT_EQ(cert2.get(), reporter.served_certificate_chain());
  EXPECT_EQ(sct_list.size(), reporter.signed_certificate_timestamps().size());
  EXPECT_EQ(sct_list[0].status,
            reporter.signed_certificate_timestamps()[0].status);
  EXPECT_EQ(sct_list[0].sct, reporter.signed_certificate_timestamps()[0].sct);
  EXPECT_EQ(network_isolation_key, reporter.network_isolation_key());
}

// Tests that the dynamic Expect-CT UMA histogram is recorded correctly.
TEST_F(TransportSecurityStateTest, DynamicExpectCTUMA) {
  const char kHistogramName[] = "Net.ExpectCTHeader.ParseSuccess";
  SSLInfo ssl;
  ssl.is_issued_by_known_root = true;
  ssl.ct_policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS;

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      TransportSecurityState::kDynamicExpectCTFeature);

  // Test that the histogram is recorded correctly when the header successfully
  // parses.
  {
    const char kHeader[] = "max-age=123,enforce,report-uri=\"http://foo.test\"";
    base::HistogramTester histograms;
    TransportSecurityState state;
    MockExpectCTReporter reporter;
    state.SetExpectCTReporter(&reporter);
    state.ProcessExpectCTHeader(kHeader, HostPortPair("example.test", 443), ssl,
                                NetworkIsolationKey());
    histograms.ExpectTotalCount(kHistogramName, 1);
    histograms.ExpectBucketCount(kHistogramName, true, 1);
  }

  // Test that the histogram is recorded correctly when the header fails to
  // parse (due to semi-colons instead of commas).
  {
    const char kHeader[] = "max-age=123;enforce;report-uri=\"http://foo.test\"";
    base::HistogramTester histograms;
    TransportSecurityState state;
    MockExpectCTReporter reporter;
    state.SetExpectCTReporter(&reporter);
    state.ProcessExpectCTHeader(kHeader, HostPortPair("example.test", 443), ssl,
                                NetworkIsolationKey());
    histograms.ExpectTotalCount(kHistogramName, 1);
    histograms.ExpectBucketCount(kHistogramName, false, 1);
  }
}

#if BUILDFLAG(INCLUDE_TRANSPORT_SECURITY_STATE_PRELOAD_LIST)
const char kSubdomain[] = "foo.example.test";

class TransportSecurityStateStaticTest : public TransportSecurityStateTest {
 public:
  TransportSecurityStateStaticTest() {
    SetTransportSecurityStateSourceForTesting(nullptr);
  }
};

static bool StaticShouldRedirect(const char* hostname) {
  TransportSecurityState state;
  TransportSecurityState::STSState sts_state;
  TransportSecurityState::PKPState pkp_state;
  return state.GetStaticDomainState(hostname, &sts_state, &pkp_state) &&
         sts_state.ShouldUpgradeToSSL();
}

static bool HasStaticState(const char* hostname) {
  TransportSecurityState state;
  TransportSecurityState::STSState sts_state;
  TransportSecurityState::PKPState pkp_state;
  return state.GetStaticDomainState(hostname, &sts_state, &pkp_state);
}

static bool HasStaticPublicKeyPins(const char* hostname) {
  TransportSecurityState state;
  TransportSecurityStateTest::EnableStaticPins(&state);
  TransportSecurityState::STSState sts_state;
  TransportSecurityState::PKPState pkp_state;
  if (!state.GetStaticDomainState(hostname, &sts_state, &pkp_state))
    return false;

  return pkp_state.HasPublicKeyPins();
}

static bool OnlyPinningInStaticState(const char* hostname) {
  TransportSecurityState state;
  TransportSecurityStateTest::EnableStaticPins(&state);
  TransportSecurityState::STSState sts_state;
  TransportSecurityState::PKPState pkp_state;
  if (!state.GetStaticDomainState(hostname, &sts_state, &pkp_state))
    return false;

  return (pkp_state.spki_hashes.size() > 0 ||
          pkp_state.bad_spki_hashes.size() > 0) &&
         !sts_state.ShouldUpgradeToSSL();
}

TEST_F(TransportSecurityStateStaticTest, EnableStaticPins) {
  TransportSecurityState state;
  TransportSecurityState::STSState sts_state;
  TransportSecurityState::PKPState pkp_state;

  EnableStaticPins(&state);

  EXPECT_TRUE(
      state.GetStaticDomainState("chrome.google.com", &sts_state, &pkp_state));
  EXPECT_FALSE(pkp_state.spki_hashes.empty());
}

TEST_F(TransportSecurityStateStaticTest, DisableStaticPins) {
  TransportSecurityState state;
  TransportSecurityState::STSState sts_state;
  TransportSecurityState::PKPState pkp_state;

  DisableStaticPins(&state);
  EXPECT_TRUE(
      state.GetStaticDomainState("chrome.google.com", &sts_state, &pkp_state));
  EXPECT_TRUE(pkp_state.spki_hashes.empty());
}

TEST_F(TransportSecurityStateStaticTest, IsPreloaded) {
  const std::string paypal = "paypal.com";
  const std::string www_paypal = "www.paypal.com";
  const std::string foo_paypal = "foo.paypal.com";
  const std::string a_www_paypal = "a.www.paypal.com";
  const std::string abc_paypal = "a.b.c.paypal.com";
  const std::string example = "example.com";
  const std::string aypal = "aypal.com";
  const std::string google = "google";
  const std::string www_google = "www.google";
  const std::string foo = "foo";
  const std::string bank = "example.bank";
  const std::string insurance = "sub.example.insurance";

  TransportSecurityState state;
  TransportSecurityState::STSState sts_state;
  TransportSecurityState::PKPState pkp_state;

  EXPECT_TRUE(GetStaticDomainState(&state, paypal, &sts_state, &pkp_state));
  EXPECT_TRUE(GetStaticDomainState(&state, www_paypal, &sts_state, &pkp_state));
  EXPECT_FALSE(sts_state.include_subdomains);
  EXPECT_TRUE(GetStaticDomainState(&state, google, &sts_state, &pkp_state));
  EXPECT_TRUE(GetStaticDomainState(&state, www_google, &sts_state, &pkp_state));
  EXPECT_TRUE(GetStaticDomainState(&state, foo, &sts_state, &pkp_state));
  EXPECT_TRUE(GetStaticDomainState(&state, bank, &sts_state, &pkp_state));
  EXPECT_TRUE(sts_state.include_subdomains);
  EXPECT_TRUE(GetStaticDomainState(&state, insurance, &sts_state, &pkp_state));
  EXPECT_TRUE(sts_state.include_subdomains);
  EXPECT_FALSE(
      GetStaticDomainState(&state, a_www_paypal, &sts_state, &pkp_state));
  EXPECT_FALSE(
      GetStaticDomainState(&state, abc_paypal, &sts_state, &pkp_state));
  EXPECT_FALSE(GetStaticDomainState(&state, example, &sts_state, &pkp_state));
  EXPECT_FALSE(GetStaticDomainState(&state, aypal, &sts_state, &pkp_state));
}

TEST_F(TransportSecurityStateStaticTest, PreloadedDomainSet) {
  TransportSecurityState state;
  EnableStaticPins(&state);
  TransportSecurityState::STSState sts_state;
  TransportSecurityState::PKPState pkp_state;

  // The domain wasn't being set, leading to a blank string in the
  // chrome://net-internals/#hsts UI. So test that.
  EXPECT_TRUE(
      state.GetStaticDomainState("market.android.com", &sts_state, &pkp_state));
  EXPECT_EQ(sts_state.domain, "market.android.com");
  EXPECT_EQ(pkp_state.domain, "market.android.com");
  EXPECT_TRUE(state.GetStaticDomainState("sub.market.android.com", &sts_state,
                                         &pkp_state));
  EXPECT_EQ(sts_state.domain, "market.android.com");
  EXPECT_EQ(pkp_state.domain, "market.android.com");
}

TEST_F(TransportSecurityStateStaticTest, Preloaded) {
  TransportSecurityState state;
  TransportSecurityState::STSState sts_state;
  TransportSecurityState::PKPState pkp_state;

  // We do more extensive checks for the first domain.
  EXPECT_TRUE(
      state.GetStaticDomainState("www.paypal.com", &sts_state, &pkp_state));
  EXPECT_EQ(sts_state.upgrade_mode,
            TransportSecurityState::STSState::MODE_FORCE_HTTPS);
  EXPECT_FALSE(sts_state.include_subdomains);
  EXPECT_FALSE(pkp_state.include_subdomains);

  EXPECT_TRUE(HasStaticState("paypal.com"));
  EXPECT_FALSE(HasStaticState("www2.paypal.com"));

  // Google hosts:

  EXPECT_TRUE(StaticShouldRedirect("chrome.google.com"));
  EXPECT_TRUE(StaticShouldRedirect("checkout.google.com"));
  EXPECT_TRUE(StaticShouldRedirect("wallet.google.com"));
  EXPECT_TRUE(StaticShouldRedirect("docs.google.com"));
  EXPECT_TRUE(StaticShouldRedirect("sites.google.com"));
  EXPECT_TRUE(StaticShouldRedirect("drive.google.com"));
  EXPECT_TRUE(StaticShouldRedirect("spreadsheets.google.com"));
  EXPECT_TRUE(StaticShouldRedirect("appengine.google.com"));
  EXPECT_TRUE(StaticShouldRedirect("market.android.com"));
  EXPECT_TRUE(StaticShouldRedirect("encrypted.google.com"));
  EXPECT_TRUE(StaticShouldRedirect("accounts.google.com"));
  EXPECT_TRUE(StaticShouldRedirect("profiles.google.com"));
  EXPECT_TRUE(StaticShouldRedirect("mail.google.com"));
  EXPECT_TRUE(StaticShouldRedirect("chatenabled.mail.google.com"));
  EXPECT_TRUE(StaticShouldRedirect("talkgadget.google.com"));
  EXPECT_TRUE(StaticShouldRedirect("hostedtalkgadget.google.com"));
  EXPECT_TRUE(StaticShouldRedirect("talk.google.com"));
  EXPECT_TRUE(StaticShouldRedirect("plus.google.com"));
  EXPECT_TRUE(StaticShouldRedirect("groups.google.com"));
  EXPECT_TRUE(StaticShouldRedirect("apis.google.com"));
  EXPECT_TRUE(StaticShouldRedirect("oauthaccountmanager.googleapis.com"));
  EXPECT_TRUE(StaticShouldRedirect("passwordsleakcheck-pa.googleapis.com"));
  EXPECT_TRUE(StaticShouldRedirect("ssl.google-analytics.com"));
  EXPECT_TRUE(StaticShouldRedirect("google"));
  EXPECT_TRUE(StaticShouldRedirect("foo.google"));
  EXPECT_TRUE(StaticShouldRedirect("foo"));
  EXPECT_TRUE(StaticShouldRedirect("domaintest.foo"));
  EXPECT_TRUE(StaticShouldRedirect("gmail.com"));
  EXPECT_TRUE(StaticShouldRedirect("www.gmail.com"));
  EXPECT_TRUE(StaticShouldRedirect("googlemail.com"));
  EXPECT_TRUE(StaticShouldRedirect("www.googlemail.com"));
  EXPECT_TRUE(StaticShouldRedirect("googleplex.com"));
  EXPECT_TRUE(StaticShouldRedirect("www.googleplex.com"));
  EXPECT_TRUE(StaticShouldRedirect("www.google-analytics.com"));
  EXPECT_TRUE(StaticShouldRedirect("www.youtube.com"));
  EXPECT_TRUE(StaticShouldRedirect("youtube.com"));

  // These domains used to be only HSTS when SNI was available.
  EXPECT_TRUE(state.GetStaticDomainState("gmail.com", &sts_state, &pkp_state));
  EXPECT_TRUE(
      state.GetStaticDomainState("www.gmail.com", &sts_state, &pkp_state));
  EXPECT_TRUE(
      state.GetStaticDomainState("googlemail.com", &sts_state, &pkp_state));
  EXPECT_TRUE(
      state.GetStaticDomainState("www.googlemail.com", &sts_state, &pkp_state));

  // fi.g.co should not force HTTPS because there are still HTTP-only services
  // on it.
  EXPECT_FALSE(StaticShouldRedirect("fi.g.co"));

  // Other hosts:

  EXPECT_TRUE(StaticShouldRedirect("aladdinschools.appspot.com"));

  EXPECT_TRUE(StaticShouldRedirect("ottospora.nl"));
  EXPECT_TRUE(StaticShouldRedirect("www.ottospora.nl"));

  EXPECT_TRUE(StaticShouldRedirect("www.paycheckrecords.com"));

  EXPECT_TRUE(StaticShouldRedirect("lastpass.com"));
  EXPECT_TRUE(StaticShouldRedirect("www.lastpass.com"));
  EXPECT_FALSE(HasStaticState("blog.lastpass.com"));

  EXPECT_TRUE(StaticShouldRedirect("keyerror.com"));
  EXPECT_TRUE(StaticShouldRedirect("www.keyerror.com"));

  EXPECT_TRUE(StaticShouldRedirect("entropia.de"));
  EXPECT_TRUE(StaticShouldRedirect("www.entropia.de"));
  EXPECT_FALSE(HasStaticState("foo.entropia.de"));

  EXPECT_TRUE(StaticShouldRedirect("www.elanex.biz"));
  EXPECT_FALSE(HasStaticState("elanex.biz"));
  EXPECT_FALSE(HasStaticState("foo.elanex.biz"));

  EXPECT_TRUE(StaticShouldRedirect("sunshinepress.org"));
  EXPECT_TRUE(StaticShouldRedirect("www.sunshinepress.org"));
  EXPECT_TRUE(StaticShouldRedirect("a.b.sunshinepress.org"));

  EXPECT_TRUE(StaticShouldRedirect("www.noisebridge.net"));
  EXPECT_FALSE(HasStaticState("noisebridge.net"));
  EXPECT_FALSE(HasStaticState("foo.noisebridge.net"));

  EXPECT_TRUE(StaticShouldRedirect("neg9.org"));
  EXPECT_FALSE(HasStaticState("www.neg9.org"));

  EXPECT_TRUE(StaticShouldRedirect("riseup.net"));
  EXPECT_TRUE(StaticShouldRedirect("foo.riseup.net"));

  EXPECT_TRUE(StaticShouldRedirect("factor.cc"));
  EXPECT_FALSE(HasStaticState("www.factor.cc"));

  EXPECT_TRUE(StaticShouldRedirect("members.mayfirst.org"));
  EXPECT_TRUE(StaticShouldRedirect("support.mayfirst.org"));
  EXPECT_TRUE(StaticShouldRedirect("id.mayfirst.org"));
  EXPECT_TRUE(StaticShouldRedirect("lists.mayfirst.org"));
  EXPECT_FALSE(HasStaticState("www.mayfirst.org"));

  EXPECT_TRUE(StaticShouldRedirect("romab.com"));
  EXPECT_TRUE(StaticShouldRedirect("www.romab.com"));
  EXPECT_TRUE(StaticShouldRedirect("foo.romab.com"));

  EXPECT_TRUE(StaticShouldRedirect("logentries.com"));
  EXPECT_TRUE(StaticShouldRedirect("www.logentries.com"));
  EXPECT_FALSE(HasStaticState("foo.logentries.com"));

  EXPECT_TRUE(StaticShouldRedirect("stripe.com"));
  EXPECT_TRUE(StaticShouldRedirect("foo.stripe.com"));

  EXPECT_TRUE(StaticShouldRedirect("cloudsecurityalliance.org"));
  EXPECT_TRUE(StaticShouldRedirect("foo.cloudsecurityalliance.org"));

  EXPECT_TRUE(StaticShouldRedirect("login.sapo.pt"));
  EXPECT_TRUE(StaticShouldRedirect("foo.login.sapo.pt"));

  EXPECT_TRUE(StaticShouldRedirect("mattmccutchen.net"));
  EXPECT_TRUE(StaticShouldRedirect("foo.mattmccutchen.net"));

  EXPECT_TRUE(StaticShouldRedirect("betnet.fr"));
  EXPECT_TRUE(StaticShouldRedirect("foo.betnet.fr"));

  EXPECT_TRUE(StaticShouldRedirect("uprotect.it"));
  EXPECT_TRUE(StaticShouldRedirect("foo.uprotect.it"));

  EXPECT_TRUE(StaticShouldRedirect("cert.se"));
  EXPECT_TRUE(StaticShouldRedirect("foo.cert.se"));

  EXPECT_TRUE(StaticShouldRedirect("crypto.is"));
  EXPECT_TRUE(StaticShouldRedirect("foo.crypto.is"));

  EXPECT_TRUE(StaticShouldRedirect("simon.butcher.name"));
  EXPECT_TRUE(StaticShouldRedirect("foo.simon.butcher.name"));

  EXPECT_TRUE(StaticShouldRedirect("linx.net"));
  EXPECT_TRUE(StaticShouldRedirect("foo.linx.net"));

  EXPECT_TRUE(StaticShouldRedirect("dropcam.com"));
  EXPECT_TRUE(StaticShouldRedirect("www.dropcam.com"));
  EXPECT_FALSE(HasStaticState("foo.dropcam.com"));

  EXPECT_TRUE(StaticShouldRedirect("ebanking.indovinabank.com.vn"));
  EXPECT_TRUE(StaticShouldRedirect("foo.ebanking.indovinabank.com.vn"));

  EXPECT_TRUE(StaticShouldRedirect("epoxate.com"));
  EXPECT_FALSE(HasStaticState("foo.epoxate.com"));

  EXPECT_FALSE(HasStaticState("foo.torproject.org"));

  EXPECT_TRUE(StaticShouldRedirect("www.moneybookers.com"));
  EXPECT_FALSE(HasStaticState("moneybookers.com"));

  EXPECT_TRUE(StaticShouldRedirect("ledgerscope.net"));
  EXPECT_TRUE(StaticShouldRedirect("www.ledgerscope.net"));
  EXPECT_FALSE(HasStaticState("status.ledgerscope.net"));

  EXPECT_TRUE(StaticShouldRedirect("foo.app.recurly.com"));
  EXPECT_TRUE(StaticShouldRedirect("foo.api.recurly.com"));

  EXPECT_TRUE(StaticShouldRedirect("greplin.com"));
  EXPECT_TRUE(StaticShouldRedirect("www.greplin.com"));
  EXPECT_FALSE(HasStaticState("foo.greplin.com"));

  EXPECT_TRUE(StaticShouldRedirect("luneta.nearbuysystems.com"));
  EXPECT_TRUE(StaticShouldRedirect("foo.luneta.nearbuysystems.com"));

  EXPECT_TRUE(StaticShouldRedirect("ubertt.org"));
  EXPECT_TRUE(StaticShouldRedirect("foo.ubertt.org"));

  EXPECT_TRUE(StaticShouldRedirect("pixi.me"));
  EXPECT_TRUE(StaticShouldRedirect("www.pixi.me"));

  EXPECT_TRUE(StaticShouldRedirect("grepular.com"));
  EXPECT_TRUE(StaticShouldRedirect("www.grepular.com"));

  EXPECT_TRUE(StaticShouldRedirect("mydigipass.com"));
  EXPECT_FALSE(StaticShouldRedirect("foo.mydigipass.com"));
  EXPECT_TRUE(StaticShouldRedirect("www.mydigipass.com"));
  EXPECT_FALSE(StaticShouldRedirect("foo.www.mydigipass.com"));
  EXPECT_TRUE(StaticShouldRedirect("developer.mydigipass.com"));
  EXPECT_FALSE(StaticShouldRedirect("foo.developer.mydigipass.com"));
  EXPECT_TRUE(StaticShouldRedirect("www.developer.mydigipass.com"));
  EXPECT_FALSE(StaticShouldRedirect("foo.www.developer.mydigipass.com"));
  EXPECT_TRUE(StaticShouldRedirect("sandbox.mydigipass.com"));
  EXPECT_FALSE(StaticShouldRedirect("foo.sandbox.mydigipass.com"));
  EXPECT_TRUE(StaticShouldRedirect("www.sandbox.mydigipass.com"));
  EXPECT_FALSE(StaticShouldRedirect("foo.www.sandbox.mydigipass.com"));

  EXPECT_TRUE(StaticShouldRedirect("bigshinylock.minazo.net"));
  EXPECT_TRUE(StaticShouldRedirect("foo.bigshinylock.minazo.net"));

  EXPECT_TRUE(StaticShouldRedirect("crate.io"));
  EXPECT_TRUE(StaticShouldRedirect("foo.crate.io"));

  EXPECT_TRUE(StaticShouldRedirect("sub.bank"));
  EXPECT_TRUE(StaticShouldRedirect("sub.insurance"));
}

TEST_F(TransportSecurityStateStaticTest, PreloadedPins) {
  TransportSecurityState state;
  EnableStaticPins(&state);
  TransportSecurityState::STSState sts_state;
  TransportSecurityState::PKPState pkp_state;

  // We do more extensive checks for the first domain.
  EXPECT_TRUE(
      state.GetStaticDomainState("www.paypal.com", &sts_state, &pkp_state));
  EXPECT_EQ(sts_state.upgrade_mode,
            TransportSecurityState::STSState::MODE_FORCE_HTTPS);
  EXPECT_FALSE(sts_state.include_subdomains);
  EXPECT_FALSE(pkp_state.include_subdomains);

  EXPECT_TRUE(OnlyPinningInStaticState("www.google.com"));
  EXPECT_TRUE(OnlyPinningInStaticState("foo.google.com"));
  EXPECT_TRUE(OnlyPinningInStaticState("google.com"));
  EXPECT_TRUE(OnlyPinningInStaticState("i.ytimg.com"));
  EXPECT_TRUE(OnlyPinningInStaticState("ytimg.com"));
  EXPECT_TRUE(OnlyPinningInStaticState("googleusercontent.com"));
  EXPECT_TRUE(OnlyPinningInStaticState("www.googleusercontent.com"));
  EXPECT_TRUE(OnlyPinningInStaticState("googleapis.com"));
  EXPECT_TRUE(OnlyPinningInStaticState("googleadservices.com"));
  EXPECT_TRUE(OnlyPinningInStaticState("googlecode.com"));
  EXPECT_TRUE(OnlyPinningInStaticState("appspot.com"));
  EXPECT_TRUE(OnlyPinningInStaticState("googlesyndication.com"));
  EXPECT_TRUE(OnlyPinningInStaticState("doubleclick.net"));
  EXPECT_TRUE(OnlyPinningInStaticState("googlegroups.com"));

  EXPECT_TRUE(HasStaticPublicKeyPins("torproject.org"));
  EXPECT_TRUE(HasStaticPublicKeyPins("www.torproject.org"));
  EXPECT_TRUE(HasStaticPublicKeyPins("check.torproject.org"));
  EXPECT_TRUE(HasStaticPublicKeyPins("blog.torproject.org"));
  EXPECT_FALSE(HasStaticState("foo.torproject.org"));

  EXPECT_TRUE(
      state.GetStaticDomainState("torproject.org", &sts_state, &pkp_state));
  EXPECT_FALSE(pkp_state.spki_hashes.empty());
  EXPECT_TRUE(
      state.GetStaticDomainState("www.torproject.org", &sts_state, &pkp_state));
  EXPECT_FALSE(pkp_state.spki_hashes.empty());
  EXPECT_TRUE(state.GetStaticDomainState("check.torproject.org", &sts_state,
                                         &pkp_state));
  EXPECT_FALSE(pkp_state.spki_hashes.empty());
  EXPECT_TRUE(state.GetStaticDomainState("blog.torproject.org", &sts_state,
                                         &pkp_state));
  EXPECT_FALSE(pkp_state.spki_hashes.empty());

  EXPECT_TRUE(HasStaticPublicKeyPins("www.twitter.com"));

  // Check that Facebook subdomains have pinning but not HSTS.
  EXPECT_TRUE(
      state.GetStaticDomainState("facebook.com", &sts_state, &pkp_state));
  EXPECT_FALSE(pkp_state.spki_hashes.empty());
  EXPECT_TRUE(StaticShouldRedirect("facebook.com"));

  EXPECT_TRUE(
      state.GetStaticDomainState("foo.facebook.com", &sts_state, &pkp_state));
  EXPECT_FALSE(pkp_state.spki_hashes.empty());
  EXPECT_FALSE(StaticShouldRedirect("foo.facebook.com"));

  EXPECT_TRUE(
      state.GetStaticDomainState("www.facebook.com", &sts_state, &pkp_state));
  EXPECT_FALSE(pkp_state.spki_hashes.empty());
  EXPECT_TRUE(StaticShouldRedirect("www.facebook.com"));

  EXPECT_TRUE(state.GetStaticDomainState("foo.www.facebook.com", &sts_state,
                                         &pkp_state));
  EXPECT_FALSE(pkp_state.spki_hashes.empty());
  EXPECT_TRUE(StaticShouldRedirect("foo.www.facebook.com"));
}

TEST_F(TransportSecurityStateStaticTest, BuiltinCertPins) {
  TransportSecurityState state;
  EnableStaticPins(&state);
  TransportSecurityState::STSState sts_state;
  TransportSecurityState::PKPState pkp_state;

  EXPECT_TRUE(
      state.GetStaticDomainState("chrome.google.com", &sts_state, &pkp_state));
  EXPECT_TRUE(HasStaticPublicKeyPins("chrome.google.com"));

  HashValueVector hashes;
  std::string failure_log;
  // Checks that a built-in list does exist.
  EXPECT_FALSE(pkp_state.CheckPublicKeyPins(hashes, &failure_log));
  EXPECT_FALSE(HasStaticPublicKeyPins("www.paypal.com"));

  EXPECT_TRUE(HasStaticPublicKeyPins("docs.google.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("1.docs.google.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("sites.google.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("drive.google.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("spreadsheets.google.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("wallet.google.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("checkout.google.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("appengine.google.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("market.android.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("encrypted.google.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("accounts.google.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("profiles.google.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("mail.google.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("chatenabled.mail.google.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("talkgadget.google.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("hostedtalkgadget.google.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("talk.google.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("plus.google.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("groups.google.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("apis.google.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("www.google-analytics.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("www.youtube.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("youtube.com"));

  EXPECT_TRUE(HasStaticPublicKeyPins("ssl.gstatic.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("gstatic.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("www.gstatic.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("ssl.google-analytics.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("www.googleplex.com"));

  EXPECT_TRUE(HasStaticPublicKeyPins("twitter.com"));
  EXPECT_FALSE(HasStaticPublicKeyPins("foo.twitter.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("www.twitter.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("api.twitter.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("oauth.twitter.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("mobile.twitter.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("dev.twitter.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("business.twitter.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("platform.twitter.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("si0.twimg.com"));
}

TEST_F(TransportSecurityStateStaticTest, OptionalHSTSCertPins) {
  TransportSecurityState state;
  EnableStaticPins(&state);

  EXPECT_TRUE(HasStaticPublicKeyPins("google.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("www.google.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("mail-attachment.googleusercontent.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("www.youtube.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("i.ytimg.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("googleapis.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("ajax.googleapis.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("googleadservices.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("pagead2.googleadservices.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("googlecode.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("kibbles.googlecode.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("appspot.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("googlesyndication.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("doubleclick.net"));
  EXPECT_TRUE(HasStaticPublicKeyPins("ad.doubleclick.net"));
  EXPECT_TRUE(HasStaticPublicKeyPins("redirector.gvt1.com"));
  EXPECT_TRUE(HasStaticPublicKeyPins("a.googlegroups.com"));
}

TEST_F(TransportSecurityStateStaticTest, OverrideBuiltins) {
  EXPECT_TRUE(HasStaticPublicKeyPins("google.com"));
  EXPECT_FALSE(StaticShouldRedirect("google.com"));
  EXPECT_FALSE(StaticShouldRedirect("www.google.com"));

  TransportSecurityState state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  state.AddHSTS("www.google.com", expiry, true);

  EXPECT_TRUE(state.ShouldUpgradeToSSL("www.google.com"));
}

// Tests that redundant reports are rate-limited.
TEST_F(TransportSecurityStateStaticTest, HPKPReportRateLimiting) {
  HostPortPair host_port_pair(kHost, kPort);
  HostPortPair subdomain_host_port_pair(kSubdomain, kPort);
  GURL report_uri(kReportUri);
  NetworkIsolationKey network_isolation_key =
      NetworkIsolationKey::CreateTransient();
  // Two dummy certs to use as the server-sent and validated chains. The
  // contents don't matter.
  scoped_refptr<X509Certificate> cert1 =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  ASSERT_TRUE(cert1);
  scoped_refptr<X509Certificate> cert2 =
      ImportCertFromFile(GetTestCertsDirectory(), "expired_cert.pem");
  ASSERT_TRUE(cert2);

  HashValueVector good_hashes, bad_hashes;

  for (size_t i = 0; kGoodPath[i]; i++)
    EXPECT_TRUE(AddHash(kGoodPath[i], &good_hashes));
  for (size_t i = 0; kBadPath[i]; i++)
    EXPECT_TRUE(AddHash(kBadPath[i], &bad_hashes));

  TransportSecurityState state;
  EnableStaticPins(&state);
  MockCertificateReportSender mock_report_sender;
  state.SetReportSender(&mock_report_sender);

  EXPECT_EQ(GURL(), mock_report_sender.latest_report_uri());
  EXPECT_EQ(std::string(), mock_report_sender.latest_report());

  std::string failure_log;
  EXPECT_EQ(TransportSecurityState::PKPStatus::VIOLATED,
            state.CheckPublicKeyPins(host_port_pair, true, bad_hashes,
                                     cert1.get(), cert2.get(),
                                     TransportSecurityState::ENABLE_PIN_REPORTS,
                                     network_isolation_key, &failure_log));

  // A report should have been sent. Check that it contains the
  // right information.
  EXPECT_EQ(report_uri, mock_report_sender.latest_report_uri());
  std::string report = mock_report_sender.latest_report();
  ASSERT_FALSE(report.empty());
  ASSERT_NO_FATAL_FAILURE(CheckHPKPReport(report, host_port_pair, true, kHost,
                                          cert1.get(), cert2.get(),
                                          good_hashes));
  EXPECT_EQ(network_isolation_key,
            mock_report_sender.latest_network_isolation_key());
  mock_report_sender.Clear();

  // Now trigger the same violation; a duplicative report should not be
  // sent.
  EXPECT_EQ(TransportSecurityState::PKPStatus::VIOLATED,
            state.CheckPublicKeyPins(host_port_pair, true, bad_hashes,
                                     cert1.get(), cert2.get(),
                                     TransportSecurityState::ENABLE_PIN_REPORTS,
                                     network_isolation_key, &failure_log));
  EXPECT_EQ(GURL(), mock_report_sender.latest_report_uri());
  EXPECT_EQ(std::string(), mock_report_sender.latest_report());
  EXPECT_EQ(NetworkIsolationKey(),
            mock_report_sender.latest_network_isolation_key());
}

TEST_F(TransportSecurityStateStaticTest, HPKPReporting) {
  HostPortPair host_port_pair(kHost, kPort);
  HostPortPair subdomain_host_port_pair(kSubdomain, kPort);
  GURL report_uri(kReportUri);
  NetworkIsolationKey network_isolation_key =
      NetworkIsolationKey::CreateTransient();
  // Two dummy certs to use as the server-sent and validated chains. The
  // contents don't matter.
  scoped_refptr<X509Certificate> cert1 =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  ASSERT_TRUE(cert1);
  scoped_refptr<X509Certificate> cert2 =
      ImportCertFromFile(GetTestCertsDirectory(), "expired_cert.pem");
  ASSERT_TRUE(cert2);

  HashValueVector good_hashes, bad_hashes;

  for (size_t i = 0; kGoodPath[i]; i++)
    EXPECT_TRUE(AddHash(kGoodPath[i], &good_hashes));
  for (size_t i = 0; kBadPath[i]; i++)
    EXPECT_TRUE(AddHash(kBadPath[i], &bad_hashes));

  TransportSecurityState state;
  EnableStaticPins(&state);
  MockCertificateReportSender mock_report_sender;
  state.SetReportSender(&mock_report_sender);

  EXPECT_EQ(GURL(), mock_report_sender.latest_report_uri());
  EXPECT_EQ(std::string(), mock_report_sender.latest_report());

  std::string failure_log;
  EXPECT_EQ(TransportSecurityState::PKPStatus::VIOLATED,
            state.CheckPublicKeyPins(
                host_port_pair, true, bad_hashes, cert1.get(), cert2.get(),
                TransportSecurityState::DISABLE_PIN_REPORTS,
                network_isolation_key, &failure_log));

  // No report should have been sent because of the DISABLE_PIN_REPORTS
  // argument.
  EXPECT_EQ(GURL(), mock_report_sender.latest_report_uri());
  EXPECT_EQ(std::string(), mock_report_sender.latest_report());

  EXPECT_EQ(TransportSecurityState::PKPStatus::OK,
            state.CheckPublicKeyPins(host_port_pair, true, good_hashes,
                                     cert1.get(), cert2.get(),
                                     TransportSecurityState::ENABLE_PIN_REPORTS,
                                     network_isolation_key, &failure_log));

  // No report should have been sent because there was no violation.
  EXPECT_EQ(GURL(), mock_report_sender.latest_report_uri());
  EXPECT_EQ(std::string(), mock_report_sender.latest_report());

  EXPECT_EQ(TransportSecurityState::PKPStatus::BYPASSED,
            state.CheckPublicKeyPins(host_port_pair, false, bad_hashes,
                                     cert1.get(), cert2.get(),
                                     TransportSecurityState::ENABLE_PIN_REPORTS,
                                     network_isolation_key, &failure_log));

  // No report should have been sent because the certificate chained to a
  // non-public root.
  EXPECT_EQ(GURL(), mock_report_sender.latest_report_uri());
  EXPECT_EQ(std::string(), mock_report_sender.latest_report());

  EXPECT_EQ(TransportSecurityState::PKPStatus::OK,
            state.CheckPublicKeyPins(host_port_pair, false, good_hashes,
                                     cert1.get(), cert2.get(),
                                     TransportSecurityState::ENABLE_PIN_REPORTS,
                                     network_isolation_key, &failure_log));

  // No report should have been sent because there was no violation, even though
  // the certificate chained to a local trust anchor.
  EXPECT_EQ(GURL(), mock_report_sender.latest_report_uri());
  EXPECT_EQ(std::string(), mock_report_sender.latest_report());

  EXPECT_EQ(TransportSecurityState::PKPStatus::VIOLATED,
            state.CheckPublicKeyPins(host_port_pair, true, bad_hashes,
                                     cert1.get(), cert2.get(),
                                     TransportSecurityState::ENABLE_PIN_REPORTS,
                                     network_isolation_key, &failure_log));

  // Now a report should have been sent. Check that it contains the
  // right information.
  EXPECT_EQ(report_uri, mock_report_sender.latest_report_uri());
  std::string report = mock_report_sender.latest_report();
  ASSERT_FALSE(report.empty());
  EXPECT_EQ("application/json; charset=utf-8",
            mock_report_sender.latest_content_type());
  ASSERT_NO_FATAL_FAILURE(CheckHPKPReport(report, host_port_pair, true, kHost,
                                          cert1.get(), cert2.get(),
                                          good_hashes));
  mock_report_sender.Clear();
  EXPECT_EQ(TransportSecurityState::PKPStatus::VIOLATED,
            state.CheckPublicKeyPins(subdomain_host_port_pair, true, bad_hashes,
                                     cert1.get(), cert2.get(),
                                     TransportSecurityState::ENABLE_PIN_REPORTS,
                                     network_isolation_key, &failure_log));

  // Now a report should have been sent for the subdomain. Check that it
  // contains the right information.
  EXPECT_EQ(report_uri, mock_report_sender.latest_report_uri());
  report = mock_report_sender.latest_report();
  ASSERT_FALSE(report.empty());
  EXPECT_EQ("application/json; charset=utf-8",
            mock_report_sender.latest_content_type());
  ASSERT_NO_FATAL_FAILURE(CheckHPKPReport(report, subdomain_host_port_pair,
                                          true, kHost, cert1.get(), cert2.get(),
                                          good_hashes));
  EXPECT_EQ(network_isolation_key,
            mock_report_sender.latest_network_isolation_key());
}

TEST_F(TransportSecurityStateTest, WriteSizeDecodeSize) {
  for (size_t i = 0; i < 300; ++i) {
    SCOPED_TRACE(i);
    huffman_trie::TrieBitBuffer buffer;
    buffer.WriteSize(i);
    huffman_trie::BitWriter writer;
    buffer.WriteToBitWriter(&writer);
    size_t position = writer.position();
    writer.Flush();
    ASSERT_NE(writer.bytes().data(), nullptr);
    extras::PreloadDecoder::BitReader reader(writer.bytes().data(), position);
    size_t decoded_size;
    EXPECT_TRUE(reader.DecodeSize(&decoded_size));
    EXPECT_EQ(i, decoded_size);
  }
}

TEST_F(TransportSecurityStateTest, DecodeSizeFour) {
  // Test that BitReader::DecodeSize properly handles the number 4, including
  // not over-reading input bytes. BitReader::Next only fails if there's not
  // another byte to read from; if it reads past the number of bits in the
  // buffer but is still in the last byte it will still succeed. For this
  // reason, this test puts the encoding of 4 at the end of the byte to check
  // that DecodeSize doesn't over-read.
  //
  // 4 is encoded as 0b010. Shifted right to fill one byte, it is 0x02, with 5
  // bits of padding.
  uint8_t encoded = 0x02;
  extras::PreloadDecoder::BitReader reader(&encoded, 8);
  for (size_t i = 0; i < 5; ++i) {
    bool unused;
    ASSERT_TRUE(reader.Next(&unused));
  }
  size_t decoded_size;
  EXPECT_TRUE(reader.DecodeSize(&decoded_size));
  EXPECT_EQ(4u, decoded_size);
}

#endif  // BUILDFLAG(INCLUDE_TRANSPORT_SECURITY_STATE_PRELOAD_LIST)

TEST_F(TransportSecurityStateTest,
       PartitionExpectCTStateByNetworkIsolationKey) {
  const char kDomain[] = "example.test";
  HostPortPair host_port_pair(kDomain, 443);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      TransportSecurityState::kDynamicExpectCTFeature);

  const base::Time expiry =
      base::Time::Now() + base::TimeDelta::FromSeconds(1000);

  // Dummy cert to use as the validation chain. The contents do not matter.
  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(GetTestCertsDirectory(), "expired_cert.pem");
  ASSERT_TRUE(cert);
  HashValueVector hashes;
  hashes.push_back(
      HashValue(X509Certificate::CalculateFingerprint256(cert->cert_buffer())));

  // An ExpectCT entry is set using network_isolation_key1, and then accessed
  // using both keys. It should only be accessible using the other key when
  // kPartitionExpectCTStateByNetworkIsolationKey is disabled.
  NetworkIsolationKey network_isolation_key1 =
      NetworkIsolationKey::CreateTransient();
  NetworkIsolationKey network_isolation_key2 =
      NetworkIsolationKey::CreateTransient();

  for (bool partition_expect_ct_state : {false, true}) {
    base::test::ScopedFeatureList feature_list2;
    if (partition_expect_ct_state) {
      feature_list2.InitAndEnableFeature(
          features::kPartitionExpectCTStateByNetworkIsolationKey);
    } else {
      feature_list2.InitAndDisableFeature(
          features::kPartitionExpectCTStateByNetworkIsolationKey);
    }

    // Add Expect-CT entry.
    TransportSecurityState state;
    state.AddExpectCT(kDomain, expiry, true, GURL(), network_isolation_key1);
    TransportSecurityState::ExpectCTState expect_ct_state;
    EXPECT_TRUE(state.GetDynamicExpectCTState(kDomain, network_isolation_key1,
                                              &expect_ct_state));

    // The Expect-CT entry should only be respected with
    // |network_isolation_key2| when
    // kPartitionExpectCTStateByNetworkIsolationKey is disabled.
    EXPECT_EQ(!partition_expect_ct_state,
              state.GetDynamicExpectCTState(kDomain, network_isolation_key2,
                                            &expect_ct_state));
    EXPECT_EQ(TransportSecurityState::CT_REQUIREMENTS_NOT_MET,
              state.CheckCTRequirements(
                  host_port_pair, true, hashes, cert.get(), cert.get(),
                  SignedCertificateTimestampAndStatusList(),
                  TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
                  ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
                  network_isolation_key1));
    EXPECT_EQ(!partition_expect_ct_state,
              TransportSecurityState::CT_REQUIREMENTS_NOT_MET ==
                  state.CheckCTRequirements(
                      host_port_pair, true, hashes, cert.get(), cert.get(),
                      SignedCertificateTimestampAndStatusList(),
                      TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
                      ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
                      network_isolation_key2));

    // An Expect-CT header with |network_isolation_key2| should only overwrite
    // the entry when |partition_expect_ct_state| is false.
    SSLInfo ssl_info;
    ssl_info.ct_policy_compliance =
        ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS;
    ssl_info.is_issued_by_known_root = true;
    MockExpectCTReporter reporter;
    state.SetExpectCTReporter(&reporter);
    const char kHeader[] = "max-age=0";
    state.ProcessExpectCTHeader(kHeader, host_port_pair, ssl_info,
                                network_isolation_key2);
    EXPECT_EQ(partition_expect_ct_state,
              state.GetDynamicExpectCTState(kDomain, network_isolation_key1,
                                            &expect_ct_state));

    // An Expect-CT header with |network_isolation_key1| should always overwrite
    // the added entry.
    state.ProcessExpectCTHeader(kHeader, host_port_pair, ssl_info,
                                network_isolation_key1);
    EXPECT_FALSE(state.GetDynamicExpectCTState(kDomain, network_isolation_key1,
                                               &expect_ct_state));
  }
}

// Tests the eviction logic and priority of pruning resources, before applying
// the per-NetworkIsolationKey limit.
TEST_F(TransportSecurityStateTest, PruneExpectCTPriority) {
  const GURL report_uri(kReportUri);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // enabled_features
      {TransportSecurityState::kDynamicExpectCTFeature,
       features::kPartitionExpectCTStateByNetworkIsolationKey},
      // disabled_features
      {});

  // Each iteration adds two groups of |kGroupSize| entries, with specified
  // parameters, and then enough entries are added for a third group to trigger
  // pruning. |kGroupSize| is chosen so that exactly all the entries in the
  // first group or the second will typically be pruned. Note that group 1 is
  // always added before group 2.
  const size_t kGroupSize =
      features::kExpectCTPruneMax.Get() - features::kExpectCTPruneMin.Get();
  // This test requires |2 * kGroupSize| to be less than |kExpectCTPruneMax|.
  ASSERT_LT(2 * kGroupSize,
            static_cast<size_t>(features::kExpectCTPruneMax.Get()));
  const size_t kThirdGroupSize =
      features::kExpectCTPruneMax.Get() - 2 * kGroupSize;

  // Specifies where the entries of no groups or of only the first group are old
  // enough to be pruned.
  enum class GroupsOldEnoughToBePruned {
    kNone,
    kFirstGroupOnly,
    kFirstAndSecondGroups,
  };

  const struct TestCase {
    bool first_group_has_transient_nik;
    bool second_group_has_transient_nik;
    bool first_group_has_enforce;
    bool second_group_has_enforce;
    bool first_group_is_expired;
    bool second_group_is_expired;
    GroupsOldEnoughToBePruned groups_old_enough_to_be_pruned;
    bool expect_first_group_retained;
    bool expect_second_group_retained;
  } kTestCases[] = {
      // No entries are prunable, so will exceed features::kExpectCTPruneMax.
      {
          false /* first_group_has_transient_nik */,
          false /* second_group_has_transient_nik */,
          true /* bool first_group_has_enforce */,
          true /* bool second_group_has_enforce */,
          false /* first_group_is_expired */,
          false /* second_group_is_expired */, GroupsOldEnoughToBePruned::kNone,
          true /* expect_first_group_retained */,
          true /* expect_second_group_retained */
      },

      // Only second group is prunable, so it should end up empty.
      {
          false /* first_group_has_transient_nik */,
          false /* second_group_has_transient_nik */,
          true /* bool first_group_has_enforce */,
          false /* bool second_group_has_enforce */,
          false /* first_group_is_expired */,
          false /* second_group_is_expired */, GroupsOldEnoughToBePruned::kNone,
          true /* expect_first_group_retained */,
          false /* expect_second_group_retained */
      },
      {
          false /* first_group_has_transient_nik */,
          true /* second_group_has_transient_nik */,
          true /* bool first_group_has_enforce */,
          true /* bool second_group_has_enforce */,
          false /* first_group_is_expired */,
          false /* second_group_is_expired */, GroupsOldEnoughToBePruned::kNone,
          true /* expect_first_group_retained */,
          false /* expect_second_group_retained */
      },

      // Only first group is prunable, so only it should be evicted.
      {
          false /* first_group_has_transient_nik */,
          false /* second_group_has_transient_nik */,
          false /* bool first_group_has_enforce */,
          true /* bool second_group_has_enforce */,
          false /* first_group_is_expired */,
          false /* second_group_is_expired */, GroupsOldEnoughToBePruned::kNone,
          false /* expect_first_group_retained */,
          true /* expect_second_group_retained */
      },
      {
          false /* first_group_has_transient_nik */,
          false /* second_group_has_transient_nik */,
          true /* bool first_group_has_enforce */,
          true /* bool second_group_has_enforce */,
          false /* first_group_is_expired */,
          false /* second_group_is_expired */,
          GroupsOldEnoughToBePruned::kFirstGroupOnly,
          false /* expect_first_group_retained */,
          true /* expect_second_group_retained */
      },

      // Both groups are prunable for the same reason, but group 1 is older
      // (since group 1 is added first).
      {
          true /* first_group_has_transient_nik */,
          true /* second_group_has_transient_nik */,
          true /* bool first_group_has_enforce */,
          true /* bool second_group_has_enforce */,
          false /* first_group_is_expired */,
          false /* second_group_is_expired */, GroupsOldEnoughToBePruned::kNone,
          false /* expect_first_group_retained */,
          true /* expect_second_group_retained */
      },
      {
          false /* first_group_has_transient_nik */,
          false /* second_group_has_transient_nik */,
          true /* bool first_group_has_enforce */,
          true /* bool second_group_has_enforce */,
          false /* first_group_is_expired */,
          false /* second_group_is_expired */,
          GroupsOldEnoughToBePruned::kFirstAndSecondGroups,
          false /* expect_first_group_retained */,
          true /* expect_second_group_retained */
      },

      // First group has enforce not set, second uses a transient NIK. First
      // should take priority.
      {
          false /* first_group_has_transient_nik */,
          true /* second_group_has_transient_nik */,
          false /* bool first_group_has_enforce */,
          true /* bool second_group_has_enforce */,
          false /* first_group_is_expired */,
          false /* second_group_is_expired */, GroupsOldEnoughToBePruned::kNone,
          true /* expect_first_group_retained */,
          false /* expect_second_group_retained */
      },

      // First group outside the non-prunable window, second has enforce set.
      // not set. First should take priority.
      {
          false /* first_group_has_transient_nik */,
          false /* second_group_has_transient_nik */,
          true /* bool first_group_has_enforce */,
          false /* bool second_group_has_enforce */,
          false /* first_group_is_expired */,
          false /* second_group_is_expired */,
          GroupsOldEnoughToBePruned::kFirstGroupOnly,
          true /* expect_first_group_retained */,
          false /* expect_second_group_retained */
      },

      // Second group is expired, so it is evicted, even though the first group
      // would otherwise be prunable and the second would not.
      {
          true /* first_group_has_transient_nik */,
          false /* second_group_has_transient_nik */,
          false /* bool first_group_has_enforce */,
          true /* bool second_group_has_enforce */,
          false /* first_group_is_expired */,
          true /* second_group_is_expired */,
          GroupsOldEnoughToBePruned::kFirstGroupOnly,
          true /* expect_first_group_retained */,
          false /* expect_second_group_retained */
      },
  };

  for (const auto& test_case : kTestCases) {
    // Each test case simulates up to |features::kExpectCTSafeFromPruneDays
    // + 1| days passing, so if an entry added for a test case should not expire
    // over the course of running the test, its expiry date must be farther into
    // the future than that.
    base::Time unexpired_expiry_time =
        base::Time::Now() +
        base::TimeDelta::FromDays(
            2 * features::kExpectCTSafeFromPruneDays.Get() + 1);

    // Always add entries unexpired.
    base::Time first_group_expiry =
        test_case.first_group_is_expired
            ? base::Time::Now() + base::TimeDelta::FromMilliseconds(1)
            : unexpired_expiry_time;

    TransportSecurityState state;
    base::Time first_group_observation_time = base::Time::Now();
    for (size_t i = 0; i < kGroupSize; ++i) {
      // All entries use a unique NetworkIsolationKey, so
      // NetworkIsolationKey-based pruning will do nothing.
      state.AddExpectCT(CreateUniqueHostName(), first_group_expiry,
                        test_case.first_group_has_enforce, report_uri,
                        CreateUniqueNetworkIsolationKey(
                            test_case.first_group_has_transient_nik));
    }

    // Skip forward in time slightly, so the first group is always older than
    // the first.
    FastForwardBy(base::TimeDelta::FromSeconds(1));

    // If only the first group should be old enough to be pruned, wait until
    // enough time for the group to be prunable has passed.
    if (test_case.groups_old_enough_to_be_pruned ==
        GroupsOldEnoughToBePruned::kFirstGroupOnly) {
      FastForwardBy(base::TimeDelta::FromDays(
          features::kExpectCTSafeFromPruneDays.Get() + 1));
    }

    // Always add entries unexpired.
    base::Time second_group_expiry =
        test_case.second_group_is_expired
            ? base::Time::Now() + base::TimeDelta::FromMilliseconds(1)
            : unexpired_expiry_time;

    base::Time second_group_observation_time = base::Time::Now();
    ASSERT_NE(first_group_observation_time, second_group_observation_time);
    for (size_t i = 0; i < kGroupSize; ++i) {
      state.AddExpectCT(CreateUniqueHostName(), second_group_expiry,
                        test_case.second_group_has_enforce, report_uri,
                        CreateUniqueNetworkIsolationKey(
                            test_case.second_group_has_transient_nik));
    }

    // Skip forward in time slightly, so the first group is always older than
    // the first. This needs to be long enough so that if
    // |second_group_is_expired| is true, the entry will expire.
    FastForwardBy(base::TimeDelta::FromSeconds(1));

    // If both the first and second groups should be old enough to be pruned,
    // wait until enough time has passed for both groups to prunable.
    if (test_case.groups_old_enough_to_be_pruned ==
        GroupsOldEnoughToBePruned::kFirstAndSecondGroups) {
      FastForwardBy(base::TimeDelta::FromDays(
          features::kExpectCTSafeFromPruneDays.Get() + 1));
    }

    for (size_t i = 0; i < kThirdGroupSize; ++i) {
      state.AddExpectCT(
          CreateUniqueHostName(),
          base::Time::Now() + base::TimeDelta::FromSeconds(1),
          true /* enforce */, report_uri,
          CreateUniqueNetworkIsolationKey(false /* is_transient */));
    }

    size_t first_group_size = 0;
    size_t second_group_size = 0;
    size_t third_group_size = 0;
    for (TransportSecurityState::ExpectCTStateIterator iterator(state);
         iterator.HasNext(); iterator.Advance()) {
      if (iterator.domain_state().last_observed ==
          first_group_observation_time) {
        ++first_group_size;
      } else if (iterator.domain_state().last_observed ==
                 second_group_observation_time) {
        ++second_group_size;
      } else {
        ++third_group_size;
      }
    }

    EXPECT_EQ(test_case.expect_first_group_retained ? kGroupSize : 0,
              first_group_size);
    EXPECT_EQ(test_case.expect_second_group_retained ? kGroupSize : 0,
              second_group_size);
    EXPECT_EQ(kThirdGroupSize, third_group_size);

    // Make sure that |unexpired_expiry_time| was set correctly - if this fails,
    // it will need to be increased to avoid unexpected entry expirations.
    ASSERT_LT(base::Time::Now(), unexpired_expiry_time);
  }
}

// Test the delay between pruning Expect-CT entries.
TEST_F(TransportSecurityStateTest, PruneExpectCTDelay) {
  const GURL report_uri(kReportUri);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      TransportSecurityState::kDynamicExpectCTFeature);

  TransportSecurityState state;
  base::Time expiry = base::Time::Now() + base::TimeDelta::FromDays(10);
  // Add prunable entries until pruning is triggered.
  for (int i = 0; i < features::kExpectCTPruneMax.Get(); ++i) {
    state.AddExpectCT(CreateUniqueHostName(), expiry, false /* enforce */,
                      report_uri,
                      CreateUniqueNetworkIsolationKey(true /* is_transient */));
  }
  // Should have removed enough entries to get down to kExpectCTPruneMin
  // entries.
  EXPECT_EQ(features::kExpectCTPruneMin.Get(),
            static_cast<int>(state.num_expect_ct_entries()));

  // Add more prunable entries, but pruning should not be triggered, due to the
  // delay between subsequent pruning tasks.
  for (int i = 0; i < features::kExpectCTPruneMax.Get(); ++i) {
    state.AddExpectCT(CreateUniqueHostName(), expiry, false /* enforce */,
                      report_uri,
                      CreateUniqueNetworkIsolationKey(true /* is_transient */));
  }
  EXPECT_EQ(
      features::kExpectCTPruneMax.Get() + features::kExpectCTPruneMin.Get(),
      static_cast<int>(state.num_expect_ct_entries()));

  // Time passes, which does not trigger pruning.
  FastForwardBy(
      base::TimeDelta::FromSeconds(features::kExpectCTPruneDelaySecs.Get()));
  EXPECT_EQ(
      features::kExpectCTPruneMax.Get() + features::kExpectCTPruneMin.Get(),
      static_cast<int>(state.num_expect_ct_entries()));

  // Another entry is added, which triggers pruning, now that enough time has
  // passed.
  state.AddExpectCT(CreateUniqueHostName(), expiry, false /* enforce */,
                    report_uri,
                    CreateUniqueNetworkIsolationKey(true /* is_transient */));
  EXPECT_EQ(features::kExpectCTPruneMin.Get(),
            static_cast<int>(state.num_expect_ct_entries()));

  // More time passes.
  FastForwardBy(base::TimeDelta::FromSeconds(
      10 * features::kExpectCTPruneDelaySecs.Get()));
  EXPECT_EQ(features::kExpectCTPruneMin.Get(),
            static_cast<int>(state.num_expect_ct_entries()));

  // When enough entries are added to trigger pruning, it runs immediately,
  // since enough time has passed.
  for (int i = 0; i < features::kExpectCTPruneMax.Get() -
                          features::kExpectCTPruneMin.Get();
       ++i) {
    state.AddExpectCT(CreateUniqueHostName(), expiry, false /* enforce */,
                      report_uri,
                      CreateUniqueNetworkIsolationKey(true /* is_transient */));
  }
  EXPECT_EQ(features::kExpectCTPruneMin.Get(),
            static_cast<int>(state.num_expect_ct_entries()));
}

// Test that Expect-CT pruning respects kExpectCTMaxEntriesPerNik, which is only
// applied if there are more than kExpectCTPruneMin entries after global
// pruning.
TEST_F(TransportSecurityStateTest, PruneExpectCTNetworkIsolationKeyLimit) {
  const GURL report_uri(kReportUri);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // enabled_features
      {TransportSecurityState::kDynamicExpectCTFeature,
       features::kPartitionExpectCTStateByNetworkIsolationKey},
      // disabled_features
      {});

  TransportSecurityState state;

  // Three different expiration times, which are used to distinguish entries
  // added by each loop. No entries actually expire in this test.
  base::Time expiry1 = base::Time::Now() + base::TimeDelta::FromDays(10);
  base::Time expiry2 = expiry1 + base::TimeDelta::FromDays(10);
  base::Time expiry3 = expiry2 + base::TimeDelta::FromDays(10);

  // Add non-prunable entries using different non-transient NIKs. They should
  // not be pruned because they are recently-observed enforce entries.
  for (int i = 0; i < features::kExpectCTPruneMax.Get(); ++i) {
    state.AddExpectCT(
        CreateUniqueHostName(), expiry1, true /* enforce */, report_uri,
        CreateUniqueNetworkIsolationKey(false /* is_transient */));
  }
  EXPECT_EQ(features::kExpectCTPruneMax.Get(),
            static_cast<int>(state.num_expect_ct_entries()));

  // Add kExpectCTMaxEntriesPerNik non-prunable entries with a single NIK,
  // allowing pruning to run each time. No entries should be deleted.
  NetworkIsolationKey network_isolation_key =
      CreateUniqueNetworkIsolationKey(false /* is_transient */);
  for (int i = 0; i < features::kExpectCTMaxEntriesPerNik.Get(); ++i) {
    FastForwardBy(
        base::TimeDelta::FromSeconds(features::kExpectCTPruneDelaySecs.Get()));
    state.AddExpectCT(CreateUniqueHostName(), expiry2, true /* enforce */,
                      report_uri, network_isolation_key);
    EXPECT_EQ(features::kExpectCTPruneMax.Get() + i + 1,
              static_cast<int>(state.num_expect_ct_entries()));
  }

  // Add kExpectCTMaxEntriesPerNik non-prunable entries with the same NIK as
  // before, allowing pruning to run each time. Each time, a single entry should
  // be removed, resulting in the same total number of entries as before.
  for (int i = 0; i < features::kExpectCTMaxEntriesPerNik.Get(); ++i) {
    FastForwardBy(
        base::TimeDelta::FromSeconds(features::kExpectCTPruneDelaySecs.Get()));
    state.AddExpectCT(CreateUniqueHostName(), expiry3, true /* enforce */,
                      report_uri, network_isolation_key);
    EXPECT_EQ(features::kExpectCTPruneMax.Get() +
                  features::kExpectCTMaxEntriesPerNik.Get(),
              static_cast<int>(state.num_expect_ct_entries()));

    // Count entries with |expiry2| and |expiry3|. For each loop iteration, an
    // entry with |expiry2| should be replaced by one with |expiry3|.
    int num_expiry2_entries = 0;
    int num_expiry3_entries = 0;
    for (TransportSecurityState::ExpectCTStateIterator iterator(state);
         iterator.HasNext(); iterator.Advance()) {
      if (iterator.domain_state().expiry == expiry2) {
        EXPECT_EQ(network_isolation_key, iterator.network_isolation_key());
        ++num_expiry2_entries;
      } else if (iterator.domain_state().expiry == expiry3) {
        EXPECT_EQ(network_isolation_key, iterator.network_isolation_key());
        ++num_expiry3_entries;
      }
    }
    EXPECT_EQ(features::kExpectCTMaxEntriesPerNik.Get() - i - 1,
              num_expiry2_entries);
    EXPECT_EQ(i + 1, num_expiry3_entries);
  }
}

}  // namespace net
