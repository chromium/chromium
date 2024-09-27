// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/http/transport_security_state.h"

#include <stdint.h>

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "crypto/sha2.h"
#include "net/base/features.h"
#include "net/base/hash_value.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/schemeful_site.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/extras/preload_data/decoder.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/http/transport_security_state_source.h"
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

class MockRequireCTDelegate : public TransportSecurityState::RequireCTDelegate {
 public:
  MOCK_METHOD3(IsCTRequiredForHost,
               CTRequirementLevel(std::string_view hostname,
                                  const X509Certificate* chain,
                                  const HashValueVector& hashes));
};

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
         lhs.domain == rhs.domain;
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
    FastForwardBy(base::Days(1));
  }

  ~TransportSecurityStateTest() override {
    SetTransportSecurityStateSourceForTesting(nullptr);
  }

  static void DisableStaticPins(TransportSecurityState* state) {
    state->enable_static_pins_ = false;
  }

  static void EnableStaticPins(TransportSecurityState* state) {
    state->enable_static_pins_ = true;
    state->SetPinningListAlwaysTimelyForTesting(true);
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
    bool ret = state->GetStaticSTSState(host, sts_result);
    if (state->GetStaticPKPState(host, pkp_result))
      ret = true;
    return ret;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(TransportSecurityStateTest, DomainNameOddities) {
  TransportSecurityState state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::Seconds(1000);

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
  const base::Time expiry = current_time + base::Seconds(1000);

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
  const base::Time expiry = current_time + base::Seconds(1000);

  EXPECT_FALSE(state.ShouldUpgradeToSSL("example.com"));
  bool include_subdomains = false;
  state.AddHSTS("EXample.coM", expiry, include_subdomains);
  EXPECT_TRUE(state.ShouldUpgradeToSSL("example.com"));
}

TEST_F(TransportSecurityStateTest, MatchesCase2) {
  TransportSecurityState state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::Seconds(1000);

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
  const base::Time expiry = current_time + base::Seconds(1000);

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
  TransportSecurityState state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::Seconds(1000);
  const base::Time older = current_time - base::Seconds(1000);

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
  TransportSecurityState state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::Seconds(1000);
  const base::Time older = current_time - base::Seconds(1000);

  state.AddHPKP("example.test", expiry, true, GetSampleSPKIHashes());
  state.AddHPKP("foo.example.test", expiry, false, GetSampleSPKIHashes());
  EXPECT_TRUE(state.HasPublicKeyPins("example.test"));
  EXPECT_TRUE(state.HasPublicKeyPins("foo.example.test"));

  // The foo.example.test rule overrides the example1.test rule, so
  // bar.foo.example.test has no HPKP state.
  EXPECT_FALSE(state.HasPublicKeyPins("bar.foo.example.test"));
  EXPECT_FALSE(state.ShouldSSLErrorsBeFatal("bar.foo.example.test"));

  // Expire the foo.example.test rule.
  state.AddHPKP("foo.example.test", older, false, GetSampleSPKIHashes());

  // Now the base example.test rule applies to bar.foo.example.test.
  EXPECT_TRUE(state.HasPublicKeyPins("bar.foo.example.test"));
  EXPECT_TRUE(state.ShouldSSLErrorsBeFatal("bar.foo.example.test"));
}

TEST_F(TransportSecurityStateTest, FatalSSLErrors) {
  TransportSecurityState state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::Seconds(1000);

  state.AddHSTS("example1.test", expiry, false);
  state.AddHPKP("example2.test", expiry, false, GetSampleSPKIHashes());

  // The presense of either HSTS or HPKP is enough to make SSL errors fatal.
  EXPECT_TRUE(state.ShouldSSLErrorsBeFatal("example1.test"));
  EXPECT_TRUE(state.ShouldSSLErrorsBeFatal("example2.test"));
}

// Tests that HPKP and HSTS state both expire. Also tests that expired entries
// are pruned.
TEST_F(TransportSecurityStateTest, Expiration) {
  TransportSecurityState state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::Seconds(1000);
  const base::Time older = current_time - base::Seconds(1000);

  // Note: this test assumes that inserting an entry with an expiration time in
  // the past works and is pruned on query.
  state.AddHSTS("example1.test", older, false);
  EXPECT_TRUE(TransportSecurityState::STSStateIterator(state).HasNext());
  EXPECT_FALSE(state.ShouldUpgradeToSSL("example1.test"));
  // Querying |state| for a domain should flush out expired entries.
  EXPECT_FALSE(TransportSecurityState::STSStateIterator(state).HasNext());

  state.AddHPKP("example1.test", older, false, GetSampleSPKIHashes());
  EXPECT_TRUE(state.has_dynamic_pkp_state());
  EXPECT_FALSE(state.HasPublicKeyPins("example1.test"));
  // Querying |state| for a domain should flush out expired entries.
  EXPECT_FALSE(state.has_dynamic_pkp_state());

  state.AddHSTS("example1.test", older, false);
  state.AddHPKP("example1.test", older, false, GetSampleSPKIHashes());
  EXPECT_TRUE(TransportSecurityState::STSStateIterator(state).HasNext());
  EXPECT_TRUE(state.has_dynamic_pkp_state());
  EXPECT_FALSE(state.ShouldSSLErrorsBeFatal("example1.test"));
  // Querying |state| for a domain should flush out expired entries.
  EXPECT_FALSE(TransportSecurityState::STSStateIterator(state).HasNext());
  EXPECT_FALSE(state.has_dynamic_pkp_state());

  // Test that HSTS can outlive HPKP.
  state.AddHSTS("example1.test", expiry, false);
  state.AddHPKP("example1.test", older, false, GetSampleSPKIHashes());
  EXPECT_TRUE(state.ShouldUpgradeToSSL("example1.test"));
  EXPECT_FALSE(state.HasPublicKeyPins("example1.test"));

  // Test that HPKP can outlive HSTS.
  state.AddHSTS("example2.test", older, false);
  state.AddHPKP("example2.test", expiry, false, GetSampleSPKIHashes());
  EXPECT_FALSE(state.ShouldUpgradeToSSL("example2.test"));
  EXPECT_TRUE(state.HasPublicKeyPins("example2.test"));
}

// Tests that HPKP and HSTS state are queried independently for subdomain
// matches.
TEST_F(TransportSecurityStateTest, IndependentSubdomain) {
  TransportSecurityState state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::Seconds(1000);

  state.AddHSTS("example1.test", expiry, true);
  state.AddHPKP("example1.test", expiry, false, GetSampleSPKIHashes());

  state.AddHSTS("example2.test", expiry, false);
  state.AddHPKP("example2.test", expiry, true, GetSampleSPKIHashes());

  EXPECT_TRUE(state.ShouldUpgradeToSSL("foo.example1.test"));
  EXPECT_FALSE(state.HasPublicKeyPins("foo.example1.test"));
  EXPECT_FALSE(state.ShouldUpgradeToSSL("foo.example2.test"));
  EXPECT_TRUE(state.HasPublicKeyPins("foo.example2.test"));
}

// Tests that HPKP and HSTS state are inserted and overridden independently.
TEST_F(TransportSecurityStateTest, IndependentInsertion) {
  TransportSecurityState state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::Seconds(1000);

  // Place an includeSubdomains HSTS entry below a normal HPKP entry.
  state.AddHSTS("example1.test", expiry, true);
  state.AddHPKP("foo.example1.test", expiry, false, GetSampleSPKIHashes());

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
  state.AddHPKP("example2.test", expiry, true, GetSampleSPKIHashes());

  EXPECT_TRUE(state.ShouldUpgradeToSSL("foo.example2.test"));
  EXPECT_TRUE(state.HasPublicKeyPins("foo.example2.test"));

  // Drop the includeSubdomains from the HSTS entry.
  state.AddHPKP("example2.test", expiry, false, GetSampleSPKIHashes());

  EXPECT_TRUE(state.ShouldUpgradeToSSL("foo.example2.test"));
  EXPECT_FALSE(state.HasPublicKeyPins("foo.example2.test"));
}

// Tests that GetDynamic[PKP|STS]State returns the correct data and that the
// states are not mixed together.
TEST_F(TransportSecurityStateTest, DynamicDomainState) {
  TransportSecurityState state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry1 = current_time + base::Seconds(1000);
  const base::Time expiry2 = current_time + base::Seconds(2000);

  state.AddHSTS("example.com", expiry1, true);
  state.AddHPKP("foo.example.com", expiry2, false, GetSampleSPKIHashes());

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

// Tests that GetSSLUpgradeDecision() matches the result of ShouldUpgradeToSSL()
// and correctly identifies the source of the decision.
TEST_F(TransportSecurityStateTest, StaticOrDynamicSource) {
  TransportSecurityState state;
  SetTransportSecurityStateSourceForTesting(&test1::kHSTSSource);

  // Check preconditions of preloaded states.
  TransportSecurityState::STSState sts_state;
  ASSERT_TRUE(state.GetStaticSTSState("hsts.example.com", &sts_state));
  ASSERT_EQ(sts_state.upgrade_mode,
            TransportSecurityState::STSState::MODE_FORCE_HTTPS);
  ASSERT_TRUE(sts_state.include_subdomains);
  ASSERT_FALSE(state.GetStaticSTSState("dynamic.example.com", &sts_state));

  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::Seconds(1000);

  EXPECT_EQ(state.GetSSLUpgradeDecision("dynamic.example.com"),
            SSLUpgradeDecision::kNoUpgrade);
  EXPECT_FALSE(state.ShouldUpgradeToSSL("dynamic.example.com"));

  EXPECT_EQ(state.GetSSLUpgradeDecision("hsts.example.com"),
            SSLUpgradeDecision::kStaticUpgrade);
  EXPECT_TRUE(state.ShouldUpgradeToSSL("hsts.example.com"));

  state.AddHSTS("dynamic.example.com", expiry, false);
  EXPECT_EQ(state.GetSSLUpgradeDecision("dynamic.example.com"),
            SSLUpgradeDecision::kDynamicUpgrade);
  EXPECT_TRUE(state.ShouldUpgradeToSSL("dynamic.example.com"));

  // Dynamic state for a host that already has static state doesn't change the
  // decision.
  state.AddHSTS("subdomain.hsts.example.com", expiry, false);
  EXPECT_EQ(state.GetSSLUpgradeDecision("subdomain.hsts.example.com"),
            SSLUpgradeDecision::kStaticUpgrade);
  EXPECT_TRUE(state.ShouldUpgradeToSSL("subdomain.hsts.example.com"));
}

// Tests that new pins always override previous pins. This should be true for
// both pins at the same domain or includeSubdomains pins at a parent domain.
TEST_F(TransportSecurityStateTest, NewPinsOverride) {
  TransportSecurityState state;
  TransportSecurityState::PKPState pkp_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::Seconds(1000);
  HashValue hash1(HASH_VALUE_SHA256);
  memset(hash1.data(), 0x01, hash1.size());
  HashValue hash2(HASH_VALUE_SHA256);
  memset(hash2.data(), 0x02, hash1.size());
  HashValue hash3(HASH_VALUE_SHA256);
  memset(hash3.data(), 0x03, hash1.size());

  state.AddHPKP("example.com", expiry, true, HashValueVector(1, hash1));

  ASSERT_TRUE(state.GetDynamicPKPState("foo.example.com", &pkp_state));
  ASSERT_EQ(1u, pkp_state.spki_hashes.size());
  EXPECT_EQ(pkp_state.spki_hashes[0], hash1);

  state.AddHPKP("foo.example.com", expiry, false, HashValueVector(1, hash2));

  ASSERT_TRUE(state.GetDynamicPKPState("foo.example.com", &pkp_state));
  ASSERT_EQ(1u, pkp_state.spki_hashes.size());
  EXPECT_EQ(pkp_state.spki_hashes[0], hash2);

  state.AddHPKP("foo.example.com", expiry, false, HashValueVector(1, hash3));

  ASSERT_TRUE(state.GetDynamicPKPState("foo.example.com", &pkp_state));
  ASSERT_EQ(1u, pkp_state.spki_hashes.size());
  EXPECT_EQ(pkp_state.spki_hashes[0], hash3);
}

TEST_F(TransportSecurityStateTest, DeleteAllDynamicDataBetween) {
  TransportSecurityState state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::Seconds(1000);
  const base::Time older = current_time - base::Seconds(1000);

  EXPECT_FALSE(state.ShouldUpgradeToSSL("example.com"));
  EXPECT_FALSE(state.HasPublicKeyPins("example.com"));
  bool include_subdomains = false;
  state.AddHSTS("example.com", expiry, include_subdomains);
  state.AddHPKP("example.com", expiry, include_subdomains,
                GetSampleSPKIHashes());

  state.DeleteAllDynamicDataBetween(expiry, base::Time::Max(),
                                    base::DoNothing());
  EXPECT_TRUE(state.ShouldUpgradeToSSL("example.com"));
  EXPECT_TRUE(state.HasPublicKeyPins("example.com"));

  state.DeleteAllDynamicDataBetween(older, current_time, base::DoNothing());
  EXPECT_TRUE(state.ShouldUpgradeToSSL("example.com"));
  EXPECT_TRUE(state.HasPublicKeyPins("example.com"));

  state.DeleteAllDynamicDataBetween(base::Time(), current_time,
                                    base::DoNothing());
  EXPECT_TRUE(state.ShouldUpgradeToSSL("example.com"));
  EXPECT_TRUE(state.HasPublicKeyPins("example.com"));

  state.DeleteAllDynamicDataBetween(older, base::Time::Max(),
                                    base::DoNothing());
  EXPECT_FALSE(state.ShouldUpgradeToSSL("example.com"));
  EXPECT_FALSE(state.HasPublicKeyPins("example.com"));

  // Dynamic data in |state| should be empty now.
  EXPECT_FALSE(TransportSecurityState::STSStateIterator(state).HasNext());
  EXPECT_FALSE(state.has_dynamic_pkp_state());
}

TEST_F(TransportSecurityStateTest, DeleteDynamicDataForHost) {
  TransportSecurityState state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::Seconds(1000);
  bool include_subdomains = false;

  state.AddHSTS("example1.test", expiry, include_subdomains);
  state.AddHPKP("example1.test", expiry, include_subdomains,
                GetSampleSPKIHashes());

  EXPECT_TRUE(state.ShouldUpgradeToSSL("example1.test"));
  EXPECT_FALSE(state.ShouldUpgradeToSSL("example2.test"));
  EXPECT_TRUE(state.HasPublicKeyPins("example1.test"));
  EXPECT_FALSE(state.HasPublicKeyPins("example2.test"));

  EXPECT_TRUE(state.DeleteDynamicDataForHost("example1.test"));
  EXPECT_FALSE(state.ShouldUpgradeToSSL("example1.test"));
  EXPECT_FALSE(state.HasPublicKeyPins("example1.test"));
}

TEST_F(TransportSecurityStateTest, LongNames) {
  TransportSecurityState state;
  state.SetPinningListAlwaysTimelyForTesting(true);
  const char kLongName[] =
      "lookupByWaveIdHashAndWaveIdIdAndWaveIdDomainAndWaveletIdIdAnd"
      "WaveletIdDomainAndBlipBlipid";
  TransportSecurityState::STSState sts_state;
  TransportSecurityState::PKPState pkp_state;
  // Just checks that we don't hit a NOTREACHED
  EXPECT_FALSE(state.GetStaticSTSState(kLongName, &sts_state));
  EXPECT_FALSE(state.GetStaticPKPState(kLongName, &pkp_state));
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
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      features::kStaticKeyPinningEnforcement);
  HashValueVector good_hashes, bad_hashes;

  for (size_t i = 0; kGoodPath[i]; i++) {
    EXPECT_TRUE(AddHash(kGoodPath[i], &good_hashes));
  }
  for (size_t i = 0; kBadPath[i]; i++) {
    EXPECT_TRUE(AddHash(kBadPath[i], &bad_hashes));
  }

  TransportSecurityState state;
  state.SetPinningListAlwaysTimelyForTesting(true);
  EnableStaticPins(&state);

  TransportSecurityState::PKPState pkp_state;
  EXPECT_TRUE(state.GetStaticPKPState("no-rejected-pins-pkp.preloaded.test",
                                      &pkp_state));
  EXPECT_TRUE(pkp_state.HasPublicKeyPins());

  EXPECT_TRUE(pkp_state.CheckPublicKeyPins(good_hashes));
  EXPECT_FALSE(pkp_state.CheckPublicKeyPins(bad_hashes));
}

// Simple test for the HSTS preload process. The trie (generated from
// transport_security_state_static_unittest1.json) contains 1 entry. Test that
// the lookup methods can find the entry and correctly decode the different
// preloaded states (HSTS and HPKP).
TEST_F(TransportSecurityStateTest, DecodePreloadedSingle) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      features::kStaticKeyPinningEnforcement);
  SetTransportSecurityStateSourceForTesting(&test1::kHSTSSource);

  TransportSecurityState state;
  TransportSecurityStateTest::EnableStaticPins(&state);

  TransportSecurityState::STSState sts_state;
  TransportSecurityState::PKPState pkp_state;
  EXPECT_TRUE(
      GetStaticDomainState(&state, "hsts.example.com", &sts_state, &pkp_state));
  EXPECT_TRUE(sts_state.include_subdomains);
  EXPECT_EQ(TransportSecurityState::STSState::MODE_FORCE_HTTPS,
            sts_state.upgrade_mode);
  EXPECT_TRUE(pkp_state.include_subdomains);
  ASSERT_EQ(1u, pkp_state.spki_hashes.size());
  EXPECT_EQ(pkp_state.spki_hashes[0], GetSampleSPKIHash(0x1));
  ASSERT_EQ(1u, pkp_state.bad_spki_hashes.size());
  EXPECT_EQ(pkp_state.bad_spki_hashes[0], GetSampleSPKIHash(0x2));
}

// More advanced test for the HSTS preload process where the trie (generated
// from transport_security_state_static_unittest2.json) contains multiple
// entries with a common prefix. Test that the lookup methods can find all
// entries and correctly decode the different preloaded states (HSTS and HPKP)
// for each entry.
TEST_F(TransportSecurityStateTest, DecodePreloadedMultiplePrefix) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      features::kStaticKeyPinningEnforcement);
  SetTransportSecurityStateSourceForTesting(&test2::kHSTSSource);

  TransportSecurityState state;
  TransportSecurityStateTest::EnableStaticPins(&state);

  TransportSecurityState::STSState sts_state;
  TransportSecurityState::PKPState pkp_state;

  EXPECT_TRUE(
      GetStaticDomainState(&state, "hsts.example.com", &sts_state, &pkp_state));
  EXPECT_FALSE(sts_state.include_subdomains);
  EXPECT_EQ(TransportSecurityState::STSState::MODE_FORCE_HTTPS,
            sts_state.upgrade_mode);
  EXPECT_TRUE(pkp_state == TransportSecurityState::PKPState());

  sts_state = TransportSecurityState::STSState();
  pkp_state = TransportSecurityState::PKPState();
  EXPECT_TRUE(
      GetStaticDomainState(&state, "hpkp.example.com", &sts_state, &pkp_state));
  EXPECT_TRUE(sts_state == TransportSecurityState::STSState());
  EXPECT_TRUE(pkp_state.include_subdomains);
  EXPECT_EQ(1U, pkp_state.spki_hashes.size());
  EXPECT_EQ(pkp_state.spki_hashes[0], GetSampleSPKIHash(0x1));
  EXPECT_EQ(0U, pkp_state.bad_spki_hashes.size());

  sts_state = TransportSecurityState::STSState();
  pkp_state = TransportSecurityState::PKPState();
  EXPECT_TRUE(
      GetStaticDomainState(&state, "mix.example.com", &sts_state, &pkp_state));
  EXPECT_FALSE(sts_state.include_subdomains);
  EXPECT_EQ(TransportSecurityState::STSState::MODE_FORCE_HTTPS,
            sts_state.upgrade_mode);
  EXPECT_TRUE(pkp_state.include_subdomains);
  EXPECT_EQ(1U, pkp_state.spki_hashes.size());
  EXPECT_EQ(pkp_state.spki_hashes[0], GetSampleSPKIHash(0x2));
  EXPECT_EQ(1U, pkp_state.bad_spki_hashes.size());
  EXPECT_EQ(pkp_state.bad_spki_hashes[0], GetSampleSPKIHash(0x1));
}

// More advanced test for the HSTS preload process where the trie (generated
// from transport_security_state_static_unittest3.json) contains a mix of
// entries. Some entries share a prefix with the prefix also having its own
// preloaded state while others share no prefix. This results in a trie with
// several different internal structures. Test that the lookup methods can find
// all entries and correctly decode the different preloaded states (HSTS and
// HPKP) for each entry.
TEST_F(TransportSecurityStateTest, DecodePreloadedMultipleMix) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      features::kStaticKeyPinningEnforcement);
  SetTransportSecurityStateSourceForTesting(&test3::kHSTSSource);

  TransportSecurityState state;
  TransportSecurityStateTest::EnableStaticPins(&state);

  TransportSecurityState::STSState sts_state;
  TransportSecurityState::PKPState pkp_state;

  EXPECT_TRUE(
      GetStaticDomainState(&state, "example.com", &sts_state, &pkp_state));
  EXPECT_TRUE(sts_state.include_subdomains);
  EXPECT_EQ(TransportSecurityState::STSState::MODE_FORCE_HTTPS,
            sts_state.upgrade_mode);
  EXPECT_TRUE(pkp_state == TransportSecurityState::PKPState());

  sts_state = TransportSecurityState::STSState();
  pkp_state = TransportSecurityState::PKPState();
  EXPECT_TRUE(
      GetStaticDomainState(&state, "hpkp.example.com", &sts_state, &pkp_state));
  EXPECT_TRUE(sts_state == TransportSecurityState::STSState());
  EXPECT_TRUE(pkp_state.include_subdomains);
  EXPECT_EQ(1U, pkp_state.spki_hashes.size());
  EXPECT_EQ(pkp_state.spki_hashes[0], GetSampleSPKIHash(0x1));
  EXPECT_EQ(0U, pkp_state.bad_spki_hashes.size());

  sts_state = TransportSecurityState::STSState();
  pkp_state = TransportSecurityState::PKPState();
  EXPECT_TRUE(
      GetStaticDomainState(&state, "example.org", &sts_state, &pkp_state));
  EXPECT_FALSE(sts_state.include_subdomains);
  EXPECT_EQ(TransportSecurityState::STSState::MODE_FORCE_HTTPS,
            sts_state.upgrade_mode);
  EXPECT_TRUE(pkp_state == TransportSecurityState::PKPState());

  sts_state = TransportSecurityState::STSState();
  pkp_state = TransportSecurityState::PKPState();
  EXPECT_TRUE(
      GetStaticDomainState(&state, "badssl.com", &sts_state, &pkp_state));
  EXPECT_TRUE(sts_state == TransportSecurityState::STSState());
  EXPECT_TRUE(pkp_state.include_subdomains);
  EXPECT_EQ(1U, pkp_state.spki_hashes.size());
  EXPECT_EQ(pkp_state.spki_hashes[0], GetSampleSPKIHash(0x1));
  EXPECT_EQ(0U, pkp_state.bad_spki_hashes.size());

  sts_state = TransportSecurityState::STSState();
  pkp_state = TransportSecurityState::PKPState();
  EXPECT_TRUE(
      GetStaticDomainState(&state, "mix.badssl.com", &sts_state, &pkp_state));
  EXPECT_FALSE(sts_state.include_subdomains);
  EXPECT_EQ(TransportSecurityState::STSState::MODE_FORCE_HTTPS,
            sts_state.upgrade_mode);
  EXPECT_TRUE(pkp_state.include_subdomains);
  EXPECT_EQ(1U, pkp_state.spki_hashes.size());
  EXPECT_EQ(pkp_state.spki_hashes[0], GetSampleSPKIHash(0x2));
  EXPECT_EQ(1U, pkp_state.bad_spki_hashes.size());
  EXPECT_EQ(pkp_state.bad_spki_hashes[0], GetSampleSPKIHash(0x1));

  sts_state = TransportSecurityState::STSState();
  pkp_state = TransportSecurityState::PKPState();

  // This should be a simple entry in the context of
  // TrieWriter::IsSimpleEntry().
  EXPECT_TRUE(GetStaticDomainState(&state, "simple-entry.example.com",
                                   &sts_state, &pkp_state));
  EXPECT_TRUE(sts_state.include_subdomains);
  EXPECT_EQ(TransportSecurityState::STSState::MODE_FORCE_HTTPS,
            sts_state.upgrade_mode);
  EXPECT_TRUE(pkp_state == TransportSecurityState::PKPState());
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
            ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS);

    MockRequireCTDelegate always_require_delegate;
    EXPECT_CALL(always_require_delegate, IsCTRequiredForHost(_, _, _))
        .WillRepeatedly(Return(CTRequirementLevel::REQUIRED));
    state.SetRequireCTDelegate(&always_require_delegate);
    EXPECT_EQ(
        TransportSecurityState::CT_REQUIREMENTS_NOT_MET,
        state.CheckCTRequirements(
            HostPortPair("www.example.com", 443), true, hashes, cert.get(),
            ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS));
    EXPECT_EQ(
        TransportSecurityState::CT_REQUIREMENTS_NOT_MET,
        state.CheckCTRequirements(
            HostPortPair("www.example.com", 443), true, hashes, cert.get(),
            ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS));
    EXPECT_EQ(
        TransportSecurityState::CT_REQUIREMENTS_MET,
        state.CheckCTRequirements(
            HostPortPair("www.example.com", 443), true, hashes, cert.get(),
            ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS));
    EXPECT_EQ(
        TransportSecurityState::CT_REQUIREMENTS_MET,
        state.CheckCTRequirements(
            HostPortPair("www.example.com", 443), true, hashes, cert.get(),
            ct::CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY));

    state.SetRequireCTDelegate(nullptr);
    EXPECT_EQ(
        original_status,
        state.CheckCTRequirements(
            HostPortPair("www.example.com", 443), true, hashes, cert.get(),
            ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS));
  }

  // If CT is not required, then regardless of the CT state for the host,
  // it should indicate CT is not required.
  {
    TransportSecurityState state;
    const TransportSecurityState::CTRequirementsStatus original_status =
        state.CheckCTRequirements(
            HostPortPair("www.example.com", 443), true, hashes, cert.get(),
            ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS);

    MockRequireCTDelegate never_require_delegate;
    EXPECT_CALL(never_require_delegate, IsCTRequiredForHost(_, _, _))
        .WillRepeatedly(Return(CTRequirementLevel::NOT_REQUIRED));
    state.SetRequireCTDelegate(&never_require_delegate);
    EXPECT_EQ(
        TransportSecurityState::CT_NOT_REQUIRED,
        state.CheckCTRequirements(
            HostPortPair("www.example.com", 443), true, hashes, cert.get(),
            ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS));
    EXPECT_EQ(
        TransportSecurityState::CT_NOT_REQUIRED,
        state.CheckCTRequirements(
            HostPortPair("www.example.com", 443), true, hashes, cert.get(),
            ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS));

    state.SetRequireCTDelegate(nullptr);
    EXPECT_EQ(
        original_status,
        state.CheckCTRequirements(
            HostPortPair("www.example.com", 443), true, hashes, cert.get(),
            ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS));
  }
}

// Tests that the emergency disable flags cause CT to stop being required
// regardless of host or delegate status.
TEST(CTEmergencyDisableTest, CTEmergencyDisable) {
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

  TransportSecurityState state;
  state.SetCTEmergencyDisabled(true);

  MockRequireCTDelegate always_require_delegate;
  EXPECT_CALL(always_require_delegate, IsCTRequiredForHost(_, _, _))
      .WillRepeatedly(Return(CTRequirementLevel::REQUIRED));
  state.SetRequireCTDelegate(&always_require_delegate);
  EXPECT_EQ(TransportSecurityState::CT_NOT_REQUIRED,
            state.CheckCTRequirements(
                HostPortPair("www.example.com", 443), true, hashes, cert.get(),
                ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS));
  EXPECT_EQ(TransportSecurityState::CT_NOT_REQUIRED,
            state.CheckCTRequirements(
                HostPortPair("www.example.com", 443), true, hashes, cert.get(),
                ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS));
  EXPECT_EQ(TransportSecurityState::CT_NOT_REQUIRED,
            state.CheckCTRequirements(
                HostPortPair("www.example.com", 443), true, hashes, cert.get(),
                ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS));
  EXPECT_EQ(TransportSecurityState::CT_NOT_REQUIRED,
            state.CheckCTRequirements(
                HostPortPair("www.example.com", 443), true, hashes, cert.get(),
                ct::CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY));

  state.SetRequireCTDelegate(nullptr);
  EXPECT_EQ(TransportSecurityState::CT_NOT_REQUIRED,
            state.CheckCTRequirements(
                HostPortPair("www.example.com", 443), true, hashes, cert.get(),
                ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS));
}

#if BUILDFLAG(INCLUDE_TRANSPORT_SECURITY_STATE_PRELOAD_LIST)

class TransportSecurityStateStaticTest : public TransportSecurityStateTest {
 public:
  TransportSecurityStateStaticTest() {
    SetTransportSecurityStateSourceForTesting(nullptr);
  }
};

static bool StaticShouldRedirect(const char* hostname) {
  TransportSecurityState state;
  TransportSecurityState::STSState sts_state;
  return state.GetStaticSTSState(hostname, &sts_state) &&
         sts_state.ShouldUpgradeToSSL();
}

static bool HasStaticState(const char* hostname) {
  TransportSecurityState state;
  state.SetPinningListAlwaysTimelyForTesting(true);
  TransportSecurityState::STSState sts_state;
  TransportSecurityState::PKPState pkp_state;
  return state.GetStaticSTSState(hostname, &sts_state) ||
         state.GetStaticPKPState(hostname, &pkp_state);
}

static bool HasStaticPublicKeyPins(const char* hostname) {
  TransportSecurityState state;
  state.SetPinningListAlwaysTimelyForTesting(true);
  TransportSecurityStateTest::EnableStaticPins(&state);
  TransportSecurityState::PKPState pkp_state;
  if (!state.GetStaticPKPState(hostname, &pkp_state))
    return false;

  return pkp_state.HasPublicKeyPins();
}

static bool OnlyPinningInStaticState(const char* hostname) {
  TransportSecurityState state;
  TransportSecurityStateTest::EnableStaticPins(&state);
  TransportSecurityState::STSState sts_state;
  TransportSecurityState::PKPState pkp_state;
  return HasStaticPublicKeyPins(hostname) && !StaticShouldRedirect(hostname);
}

TEST_F(TransportSecurityStateStaticTest, EnableStaticPins) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      features::kStaticKeyPinningEnforcement);
  TransportSecurityState state;
  state.SetPinningListAlwaysTimelyForTesting(true);
  TransportSecurityState::PKPState pkp_state;

  EnableStaticPins(&state);

  EXPECT_TRUE(state.GetStaticPKPState("chrome.google.com", &pkp_state));
  EXPECT_FALSE(pkp_state.spki_hashes.empty());
}

TEST_F(TransportSecurityStateStaticTest, DisableStaticPins) {
  TransportSecurityState state;
  state.SetPinningListAlwaysTimelyForTesting(true);
  TransportSecurityState::PKPState pkp_state;

  DisableStaticPins(&state);
  EXPECT_FALSE(state.GetStaticPKPState("chrome.google.com", &pkp_state));
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
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      features::kStaticKeyPinningEnforcement);
  TransportSecurityState state;
  EnableStaticPins(&state);
  TransportSecurityState::STSState sts_state;
  TransportSecurityState::PKPState pkp_state;

  // The domain wasn't being set, leading to a blank string in the
  // chrome://net-internals/#hsts UI. So test that.
  EXPECT_TRUE(state.GetStaticPKPState("market.android.com", &pkp_state));
  EXPECT_TRUE(state.GetStaticSTSState("market.android.com", &sts_state));
  EXPECT_EQ(sts_state.domain, "market.android.com");
  EXPECT_EQ(pkp_state.domain, "market.android.com");
  EXPECT_TRUE(state.GetStaticPKPState("sub.market.android.com", &pkp_state));
  EXPECT_TRUE(state.GetStaticSTSState("sub.market.android.com", &sts_state));
  EXPECT_EQ(sts_state.domain, "market.android.com");
  EXPECT_EQ(pkp_state.domain, "market.android.com");
}

TEST_F(TransportSecurityStateStaticTest, Preloaded) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      features::kStaticKeyPinningEnforcement);
  TransportSecurityState state;
  EnableStaticPins(&state);
  TransportSecurityState::STSState sts_state;
  TransportSecurityState::PKPState pkp_state;

  // We do more extensive checks for the first domain.
  EXPECT_TRUE(state.GetStaticSTSState("www.paypal.com", &sts_state));
  EXPECT_FALSE(state.GetStaticPKPState("www.paypal.com", &pkp_state));
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
  EXPECT_TRUE(state.GetStaticSTSState("gmail.com", &sts_state));
  EXPECT_TRUE(state.GetStaticPKPState("gmail.com", &pkp_state));
  EXPECT_TRUE(state.GetStaticSTSState("www.gmail.com", &sts_state));
  EXPECT_TRUE(state.GetStaticPKPState("www.gmail.com", &pkp_state));
  EXPECT_TRUE(state.GetStaticSTSState("googlemail.com", &sts_state));
  EXPECT_TRUE(state.GetStaticPKPState("googlemail.com", &pkp_state));
  EXPECT_TRUE(state.GetStaticSTSState("www.googlemail.com", &sts_state));
  EXPECT_TRUE(state.GetStaticPKPState("www.googlemail.com", &pkp_state));

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
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      features::kStaticKeyPinningEnforcement);
  TransportSecurityState state;
  EnableStaticPins(&state);
  TransportSecurityState::STSState sts_state;
  TransportSecurityState::PKPState pkp_state;

  // We do more extensive checks for the first domain.
  EXPECT_TRUE(state.GetStaticSTSState("www.paypal.com", &sts_state));
  EXPECT_FALSE(state.GetStaticPKPState("www.paypal.com", &pkp_state));
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

  // Facebook has pinning and hsts on facebook.com, but only pinning on
  // subdomains.
  EXPECT_TRUE(state.GetStaticPKPState("facebook.com", &pkp_state));
  EXPECT_FALSE(pkp_state.spki_hashes.empty());
  EXPECT_TRUE(StaticShouldRedirect("facebook.com"));

  EXPECT_TRUE(state.GetStaticPKPState("foo.facebook.com", &pkp_state));
  EXPECT_FALSE(pkp_state.spki_hashes.empty());
  EXPECT_FALSE(StaticShouldRedirect("foo.facebook.com"));

  // www.facebook.com and subdomains have both pinning and hsts.
  EXPECT_TRUE(state.GetStaticPKPState("www.facebook.com", &pkp_state));
  EXPECT_FALSE(pkp_state.spki_hashes.empty());
  EXPECT_TRUE(StaticShouldRedirect("www.facebook.com"));

  EXPECT_TRUE(state.GetStaticPKPState("foo.www.facebook.com", &pkp_state));
  EXPECT_FALSE(pkp_state.spki_hashes.empty());
  EXPECT_TRUE(StaticShouldRedirect("foo.www.facebook.com"));
}

TEST_F(TransportSecurityStateStaticTest, BuiltinCertPins) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      features::kStaticKeyPinningEnforcement);
  TransportSecurityState state;
  EnableStaticPins(&state);
  TransportSecurityState::PKPState pkp_state;

  EXPECT_TRUE(state.GetStaticPKPState("chrome.google.com", &pkp_state));
  EXPECT_TRUE(HasStaticPublicKeyPins("chrome.google.com"));

  HashValueVector hashes;
  // Checks that a built-in list does exist.
  EXPECT_FALSE(pkp_state.CheckPublicKeyPins(hashes));
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
}

TEST_F(TransportSecurityStateStaticTest, OptionalHSTSCertPins) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      features::kStaticKeyPinningEnforcement);
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
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      features::kStaticKeyPinningEnforcement);
  EXPECT_TRUE(HasStaticPublicKeyPins("google.com"));
  EXPECT_FALSE(StaticShouldRedirect("google.com"));
  EXPECT_FALSE(StaticShouldRedirect("www.google.com"));

  TransportSecurityState state;
  state.SetPinningListAlwaysTimelyForTesting(true);

  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::Seconds(1000);
  state.AddHSTS("www.google.com", expiry, true);

  EXPECT_TRUE(state.ShouldUpgradeToSSL("www.google.com"));
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

TEST_F(TransportSecurityStateTest, UpdateKeyPinsListValidPin) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      features::kStaticKeyPinningEnforcement);
  HostPortPair host_port_pair(kHost, kPort);

  HashValueVector bad_hashes;
  for (size_t i = 0; kBadPath[i]; i++)
    EXPECT_TRUE(AddHash(kBadPath[i], &bad_hashes));

  TransportSecurityState state;
  EnableStaticPins(&state);

  // Prior to updating the list, bad_hashes should be rejected.
  EXPECT_EQ(TransportSecurityState::PKPStatus::VIOLATED,
            state.CheckPublicKeyPins(host_port_pair, true, bad_hashes));

  // Update the pins list, adding bad_hashes to the accepted hashes for this
  // host.
  std::vector<std::vector<uint8_t>> accepted_hashes;
  for (size_t i = 0; kBadPath[i]; i++) {
    HashValue hash;
    ASSERT_TRUE(hash.FromString(kBadPath[i]));
    accepted_hashes.emplace_back(hash.data(), hash.data() + hash.size());
  }
  TransportSecurityState::PinSet test_pinset(
      /*name=*/"test",
      /*static_spki_hashes=*/accepted_hashes,
      /*bad_static_spki_hashes=*/{});
  TransportSecurityState::PinSetInfo test_pinsetinfo(
      /*hostname=*/kHost, /*pinset_name=*/"test",
      /*include_subdomains=*/false);
  state.UpdatePinList({test_pinset}, {test_pinsetinfo}, base::Time::Now());

  // Hashes should now be accepted.
  EXPECT_EQ(TransportSecurityState::PKPStatus::OK,
            state.CheckPublicKeyPins(host_port_pair, true, bad_hashes));
}

TEST_F(TransportSecurityStateTest, UpdateKeyPinsListNotValidPin) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      features::kStaticKeyPinningEnforcement);
  HostPortPair host_port_pair(kHost, kPort);

  HashValueVector good_hashes;
  for (size_t i = 0; kGoodPath[i]; i++)
    EXPECT_TRUE(AddHash(kGoodPath[i], &good_hashes));

  TransportSecurityState state;
  EnableStaticPins(&state);

  // Prior to updating the list, good_hashes should be accepted
  EXPECT_EQ(TransportSecurityState::PKPStatus::OK,
            state.CheckPublicKeyPins(host_port_pair, true, good_hashes));

  // Update the pins list, adding good_hashes to the rejected hashes for this
  // host.
  std::vector<std::vector<uint8_t>> rejected_hashes;
  for (size_t i = 0; kGoodPath[i]; i++) {
    HashValue hash;
    ASSERT_TRUE(hash.FromString(kGoodPath[i]));
    rejected_hashes.emplace_back(hash.data(), hash.data() + hash.size());
  }
  TransportSecurityState::PinSet test_pinset(
      /*name=*/"test",
      /*static_spki_hashes=*/{},
      /*bad_static_spki_hashes=*/rejected_hashes);
  TransportSecurityState::PinSetInfo test_pinsetinfo(
      /*hostname=*/kHost, /* pinset_name=*/"test",
      /*include_subdomains=*/false);
  state.UpdatePinList({test_pinset}, {test_pinsetinfo}, base::Time::Now());

  // Hashes should now be rejected.
  EXPECT_EQ(TransportSecurityState::PKPStatus::VIOLATED,
            state.CheckPublicKeyPins(host_port_pair, true, good_hashes));

  // Hashes should also be rejected if the hostname has a trailing dot.
  host_port_pair = HostPortPair("example.test.", kPort);
  EXPECT_EQ(TransportSecurityState::PKPStatus::VIOLATED,
            state.CheckPublicKeyPins(host_port_pair, true, good_hashes));

  // Hashes should also be rejected if the hostname has different
  // capitalization.
  host_port_pair = HostPortPair("ExAmpLe.tEsT", kPort);
  EXPECT_EQ(TransportSecurityState::PKPStatus::VIOLATED,
            state.CheckPublicKeyPins(host_port_pair, true, good_hashes));
}

TEST_F(TransportSecurityStateTest, UpdateKeyPinsEmptyList) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      features::kStaticKeyPinningEnforcement);
  HostPortPair host_port_pair(kHost, kPort);

  HashValueVector bad_hashes;
  for (size_t i = 0; kBadPath[i]; i++)
    EXPECT_TRUE(AddHash(kBadPath[i], &bad_hashes));

  TransportSecurityState state;
  EnableStaticPins(&state);

  // Prior to updating the list, bad_hashes should be rejected.
  EXPECT_EQ(TransportSecurityState::PKPStatus::VIOLATED,
            state.CheckPublicKeyPins(host_port_pair, true, bad_hashes));

  // Update the pins list with an empty list.
  state.UpdatePinList({}, {}, base::Time::Now());

  // Hashes should now be accepted.
  EXPECT_EQ(TransportSecurityState::PKPStatus::OK,
            state.CheckPublicKeyPins(host_port_pair, true, bad_hashes));
}

TEST_F(TransportSecurityStateTest, UpdateKeyPinsIncludeSubdomains) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      features::kStaticKeyPinningEnforcement);
  HostPortPair host_port_pair("example.sub.test", kPort);

  // unpinned_hashes is a set of hashes that (after the update) won't match the
  // expected hashes for the tld of this domain. kGoodPath is used here because
  // it's a path that is accepted prior to any updates, and this test will
  // validate it is rejected afterwards.
  HashValueVector unpinned_hashes;
  for (size_t i = 0; kGoodPath[i]; i++) {
    EXPECT_TRUE(AddHash(kGoodPath[i], &unpinned_hashes));
  }

  TransportSecurityState state;
  EnableStaticPins(&state);

  // Prior to updating the list, unpinned_hashes should be accepted
  EXPECT_EQ(TransportSecurityState::PKPStatus::OK,
            state.CheckPublicKeyPins(host_port_pair, true, unpinned_hashes));

  // Update the pins list, adding kBadPath to the accepted hashes for this
  // host, relying on include_subdomains for enforcement. The contents of the
  // hashes don't matter as long as they are different from unpinned_hashes,
  // kBadPath is used for convenience.
  std::vector<std::vector<uint8_t>> accepted_hashes;
  for (size_t i = 0; kBadPath[i]; i++) {
    HashValue hash;
    ASSERT_TRUE(hash.FromString(kBadPath[i]));
    accepted_hashes.emplace_back(hash.data(), hash.data() + hash.size());
  }
  TransportSecurityState::PinSet test_pinset(
      /*name=*/"test",
      /*static_spki_hashes=*/{accepted_hashes},
      /*bad_static_spki_hashes=*/{});
  // The host used in the test is "example.sub.test", so this pinset will only
  // match due to include subdomains.
  TransportSecurityState::PinSetInfo test_pinsetinfo(
      /*hostname=*/"sub.test", /* pinset_name=*/"test",
      /*include_subdomains=*/true);
  state.UpdatePinList({test_pinset}, {test_pinsetinfo}, base::Time::Now());

  // The path that was accepted before updating the pins should now be rejected.
  EXPECT_EQ(TransportSecurityState::PKPStatus::VIOLATED,
            state.CheckPublicKeyPins(host_port_pair, true, unpinned_hashes));
}

TEST_F(TransportSecurityStateTest, UpdateKeyPinsIncludeSubdomainsTLD) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      features::kStaticKeyPinningEnforcement);
  HostPortPair host_port_pair(kHost, kPort);

  // unpinned_hashes is a set of hashes that (after the update) won't match the
  // expected hashes for the tld of this domain. kGoodPath is used here because
  // it's a path that is accepted prior to any updates, and this test will
  // validate it is rejected afterwards.
  HashValueVector unpinned_hashes;
  for (size_t i = 0; kGoodPath[i]; i++) {
    EXPECT_TRUE(AddHash(kGoodPath[i], &unpinned_hashes));
  }

  TransportSecurityState state;
  EnableStaticPins(&state);

  // Prior to updating the list, unpinned_hashes should be accepted
  EXPECT_EQ(TransportSecurityState::PKPStatus::OK,
            state.CheckPublicKeyPins(host_port_pair, true, unpinned_hashes));

  // Update the pins list, adding kBadPath to the accepted hashes for this
  // host, relying on include_subdomains for enforcement. The contents of the
  // hashes don't matter as long as they are different from unpinned_hashes,
  // kBadPath is used for convenience.
  std::vector<std::vector<uint8_t>> accepted_hashes;
  for (size_t i = 0; kBadPath[i]; i++) {
    HashValue hash;
    ASSERT_TRUE(hash.FromString(kBadPath[i]));
    accepted_hashes.emplace_back(hash.data(), hash.data() + hash.size());
  }
  TransportSecurityState::PinSet test_pinset(
      /*name=*/"test",
      /*static_spki_hashes=*/{accepted_hashes},
      /*bad_static_spki_hashes=*/{});
  // The host used in the test is "example.test", so this pinset will only match
  // due to include subdomains.
  TransportSecurityState::PinSetInfo test_pinsetinfo(
      /*hostname=*/"test", /* pinset_name=*/"test",
      /*include_subdomains=*/true);
  state.UpdatePinList({test_pinset}, {test_pinsetinfo}, base::Time::Now());

  // The path that was accepted before updating the pins should now be rejected.
  EXPECT_EQ(TransportSecurityState::PKPStatus::VIOLATED,
            state.CheckPublicKeyPins(host_port_pair, true, unpinned_hashes));
}

TEST_F(TransportSecurityStateTest, UpdateKeyPinsDontIncludeSubdomains) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      features::kStaticKeyPinningEnforcement);
  HostPortPair host_port_pair(kHost, kPort);

  // unpinned_hashes is a set of hashes that (after the update) won't match the
  // expected hashes for the tld of this domain. kGoodPath is used here because
  // it's a path that is accepted prior to any updates, and this test will
  // validate it is accepted or rejected afterwards depending on whether the
  // domain is an exact match.
  HashValueVector unpinned_hashes;
  for (size_t i = 0; kGoodPath[i]; i++) {
    EXPECT_TRUE(AddHash(kGoodPath[i], &unpinned_hashes));
  }

  TransportSecurityState state;
  EnableStaticPins(&state);

  // Prior to updating the list, unpinned_hashes should be accepted
  EXPECT_EQ(TransportSecurityState::PKPStatus::OK,
            state.CheckPublicKeyPins(host_port_pair, true, unpinned_hashes));

  // Update the pins list, adding kBadPath to the accepted hashes for the
  // tld of this host, but without include_subdomains set. The contents of the
  // hashes don't matter as long as they are different from unpinned_hashes,
  // kBadPath is used for convenience.
  std::vector<std::vector<uint8_t>> accepted_hashes;
  for (size_t i = 0; kBadPath[i]; i++) {
    HashValue hash;
    ASSERT_TRUE(hash.FromString(kBadPath[i]));
    accepted_hashes.emplace_back(hash.data(), hash.data() + hash.size());
  }
  TransportSecurityState::PinSet test_pinset(
      /*name=*/"test",
      /*static_spki_hashes=*/{accepted_hashes},
      /*bad_static_spki_hashes=*/{});
  // The host used in the test is "example.test", so this pinset will not match
  // due to include subdomains not being set.
  TransportSecurityState::PinSetInfo test_pinsetinfo(
      /*hostname=*/"test", /* pinset_name=*/"test",
      /*include_subdomains=*/false);
  state.UpdatePinList({test_pinset}, {test_pinsetinfo}, base::Time::Now());

  // Hashes that were accepted before the update should still be accepted since
  // include subdomains is not set for the pinset, and this is not an exact
  // match.
  EXPECT_EQ(TransportSecurityState::PKPStatus::OK,
            state.CheckPublicKeyPins(host_port_pair, true, unpinned_hashes));

  // Hashes should be rejected for an exact match of the hostname.
  HostPortPair exact_match_host("test", kPort);
  EXPECT_EQ(TransportSecurityState::PKPStatus::VIOLATED,
            state.CheckPublicKeyPins(exact_match_host, true, unpinned_hashes));
}

TEST_F(TransportSecurityStateTest, UpdateKeyPinsListTimestamp) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      features::kStaticKeyPinningEnforcement);
  HostPortPair host_port_pair(kHost, kPort);

  HashValueVector bad_hashes;
  for (size_t i = 0; kBadPath[i]; i++)
    EXPECT_TRUE(AddHash(kBadPath[i], &bad_hashes));

  TransportSecurityState state;
  EnableStaticPins(&state);

  // Prior to updating the list, bad_hashes should be rejected.
  EXPECT_EQ(TransportSecurityState::PKPStatus::VIOLATED,
            state.CheckPublicKeyPins(host_port_pair, true, bad_hashes));

  // TransportSecurityStateTest sets a flag when EnableStaticPins is called that
  // results in TransportSecurityState considering the pins list as always
  // timely. We need to disable it so we can test that the timestamp has the
  // required effect.
  state.SetPinningListAlwaysTimelyForTesting(false);

  // Update the pins list, with bad hashes as rejected, but a timestamp >70 days
  // old.
  std::vector<std::vector<uint8_t>> rejected_hashes;
  for (size_t i = 0; kBadPath[i]; i++) {
    HashValue hash;
    ASSERT_TRUE(hash.FromString(kBadPath[i]));
    rejected_hashes.emplace_back(hash.data(), hash.data() + hash.size());
  }
  TransportSecurityState::PinSet test_pinset(
      /*name=*/"test",
      /*static_spki_hashes=*/{},
      /*bad_static_spki_hashes=*/rejected_hashes);
  TransportSecurityState::PinSetInfo test_pinsetinfo(
      /*hostname=*/kHost, /* pinset_name=*/"test",
      /*include_subdomains=*/false);
  state.UpdatePinList({test_pinset}, {test_pinsetinfo},
                      base::Time::Now() - base::Days(70));

  // Hashes should now be accepted.
  EXPECT_EQ(TransportSecurityState::PKPStatus::OK,
            state.CheckPublicKeyPins(host_port_pair, true, bad_hashes));

  // Update the pins list again, with a timestamp <70 days old.
  state.UpdatePinList({test_pinset}, {test_pinsetinfo},
                      base::Time::Now() - base::Days(69));

  // Hashes should now be rejected.
  EXPECT_EQ(TransportSecurityState::PKPStatus::VIOLATED,
            state.CheckPublicKeyPins(host_port_pair, true, bad_hashes));
}

class TransportSecurityStatePinningKillswitchTest
    : public TransportSecurityStateTest {
 public:
  TransportSecurityStatePinningKillswitchTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kStaticKeyPinningEnforcement);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(TransportSecurityStatePinningKillswitchTest, PinningKillswitchSet) {
  HostPortPair host_port_pair(kHost, kPort);

  HashValueVector bad_hashes;
  for (size_t i = 0; kBadPath[i]; i++)
    EXPECT_TRUE(AddHash(kBadPath[i], &bad_hashes));

  TransportSecurityState state;
  EnableStaticPins(&state);

  // Hashes should be accepted since pinning enforcement is disabled.
  EXPECT_EQ(TransportSecurityState::PKPStatus::OK,
            state.CheckPublicKeyPins(host_port_pair, true, bad_hashes));
}

}  // namespace net
