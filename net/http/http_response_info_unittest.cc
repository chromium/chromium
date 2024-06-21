// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_response_info.h"

#include "base/pickle.h"
#include "net/base/proxy_chain.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/http/http_response_headers.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/test/cert_test_util.h"
#include "net/test/ct_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class HttpResponseInfoTest : public testing::Test {
 protected:
  void SetUp() override {
    response_info_.headers = base::MakeRefCounted<HttpResponseHeaders>("");
  }

  void PickleAndRestore(const HttpResponseInfo& response_info,
                        HttpResponseInfo* restored_response_info) const {
    base::Pickle pickle;
    response_info.Persist(&pickle, false, false);
    bool truncated = false;
    EXPECT_TRUE(restored_response_info->InitFromPickle(pickle, &truncated));
  }

  HttpResponseInfo response_info_;
};

TEST_F(HttpResponseInfoTest, UnusedSincePrefetchDefault) {
  EXPECT_FALSE(response_info_.unused_since_prefetch);
}

TEST_F(HttpResponseInfoTest, UnusedSincePrefetchCopy) {
  response_info_.unused_since_prefetch = true;
  HttpResponseInfo response_info_clone(response_info_);
  EXPECT_TRUE(response_info_clone.unused_since_prefetch);
}

TEST_F(HttpResponseInfoTest, UnusedSincePrefetchPersistFalse) {
  HttpResponseInfo restored_response_info;
  PickleAndRestore(response_info_, &restored_response_info);
  EXPECT_FALSE(restored_response_info.unused_since_prefetch);
}

TEST_F(HttpResponseInfoTest, UnusedSincePrefetchPersistTrue) {
  response_info_.unused_since_prefetch = true;
  HttpResponseInfo restored_response_info;
  PickleAndRestore(response_info_, &restored_response_info);
  EXPECT_TRUE(restored_response_info.unused_since_prefetch);
}

TEST_F(HttpResponseInfoTest, ProxyChainDefault) {
  EXPECT_FALSE(response_info_.proxy_chain.IsValid());
  EXPECT_FALSE(response_info_.WasFetchedViaProxy());
}

TEST_F(HttpResponseInfoTest, ProxyChainCopy) {
  response_info_.proxy_chain =
      ProxyChain::FromSchemeHostAndPort(ProxyServer::SCHEME_HTTP, "foo", 80);
  HttpResponseInfo response_info_clone(response_info_);
  EXPECT_TRUE(response_info_clone.proxy_chain.IsValid());
  EXPECT_TRUE(response_info_clone.WasFetchedViaProxy());
}

TEST_F(HttpResponseInfoTest, ProxyChainPersistDirect) {
  response_info_.proxy_chain = ProxyChain::Direct();
  HttpResponseInfo restored_response_info;
  PickleAndRestore(response_info_, &restored_response_info);
  EXPECT_TRUE(restored_response_info.proxy_chain.IsValid());
  EXPECT_FALSE(restored_response_info.WasFetchedViaProxy());
}

TEST_F(HttpResponseInfoTest, ProxyChainPersistProxy) {
  response_info_.proxy_chain =
      ProxyChain::FromSchemeHostAndPort(ProxyServer::SCHEME_HTTP, "foo", 80);
  HttpResponseInfo restored_response_info;
  PickleAndRestore(response_info_, &restored_response_info);
  EXPECT_TRUE(restored_response_info.proxy_chain.IsValid());
  EXPECT_TRUE(restored_response_info.WasFetchedViaProxy());
}

TEST_F(HttpResponseInfoTest, PKPBypassPersistTrue) {
  response_info_.ssl_info.pkp_bypassed = true;
  HttpResponseInfo restored_response_info;
  PickleAndRestore(response_info_, &restored_response_info);
  EXPECT_TRUE(restored_response_info.ssl_info.pkp_bypassed);
}

TEST_F(HttpResponseInfoTest, PKPBypassPersistFalse) {
  response_info_.ssl_info.pkp_bypassed = false;
  HttpResponseInfo restored_response_info;
  PickleAndRestore(response_info_, &restored_response_info);
  EXPECT_FALSE(restored_response_info.ssl_info.pkp_bypassed);
}

TEST_F(HttpResponseInfoTest, AsyncRevalidationRequestedDefault) {
  EXPECT_FALSE(response_info_.async_revalidation_requested);
}

TEST_F(HttpResponseInfoTest, AsyncRevalidationRequestedCopy) {
  response_info_.async_revalidation_requested = true;
  HttpResponseInfo response_info_clone(response_info_);
  EXPECT_TRUE(response_info_clone.async_revalidation_requested);
}

TEST_F(HttpResponseInfoTest, AsyncRevalidationRequestedAssign) {
  response_info_.async_revalidation_requested = true;
  HttpResponseInfo response_info_clone;
  response_info_clone = response_info_;
  EXPECT_TRUE(response_info_clone.async_revalidation_requested);
}

TEST_F(HttpResponseInfoTest, AsyncRevalidationRequestedNotPersisted) {
  response_info_.async_revalidation_requested = true;
  HttpResponseInfo restored_response_info;
  PickleAndRestore(response_info_, &restored_response_info);
  EXPECT_FALSE(restored_response_info.async_revalidation_requested);
}

TEST_F(HttpResponseInfoTest, StaleRevalidationTimeoutDefault) {
  EXPECT_TRUE(response_info_.stale_revalidate_timeout.is_null());
}

TEST_F(HttpResponseInfoTest, StaleRevalidationTimeoutCopy) {
  base::Time test_time = base::Time::FromSecondsSinceUnixEpoch(1000);
  response_info_.stale_revalidate_timeout = test_time;
  HttpResponseInfo response_info_clone(response_info_);
  EXPECT_EQ(test_time, response_info_clone.stale_revalidate_timeout);
}

TEST_F(HttpResponseInfoTest, StaleRevalidationTimeoutRestoreValue) {
  base::Time test_time = base::Time::FromSecondsSinceUnixEpoch(1000);
  response_info_.stale_revalidate_timeout = test_time;
  HttpResponseInfo restored_response_info;
  PickleAndRestore(response_info_, &restored_response_info);
  EXPECT_EQ(test_time, restored_response_info.stale_revalidate_timeout);
}

TEST_F(HttpResponseInfoTest, StaleRevalidationTimeoutRestoreNoValue) {
  EXPECT_TRUE(response_info_.stale_revalidate_timeout.is_null());
  HttpResponseInfo restored_response_info;
  PickleAndRestore(response_info_, &restored_response_info);
  EXPECT_TRUE(restored_response_info.stale_revalidate_timeout.is_null());
}

// Test that key_exchange_group is preserved for ECDHE ciphers.
TEST_F(HttpResponseInfoTest, KeyExchangeGroupECDHE) {
  response_info_.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  SSLConnectionStatusSetVersion(SSL_CONNECTION_VERSION_TLS1_2,
                                &response_info_.ssl_info.connection_status);
  SSLConnectionStatusSetCipherSuite(
      0xcca8 /* TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256 */,
      &response_info_.ssl_info.connection_status);
  response_info_.ssl_info.key_exchange_group = 23;  // X25519
  HttpResponseInfo restored_response_info;
  PickleAndRestore(response_info_, &restored_response_info);
  EXPECT_EQ(23, restored_response_info.ssl_info.key_exchange_group);
}

// Test that key_exchange_group is preserved for TLS 1.3.
TEST_F(HttpResponseInfoTest, KeyExchangeGroupTLS13) {
  response_info_.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  SSLConnectionStatusSetVersion(SSL_CONNECTION_VERSION_TLS1_3,
                                &response_info_.ssl_info.connection_status);
  SSLConnectionStatusSetCipherSuite(0x1303 /* TLS_CHACHA20_POLY1305_SHA256 */,
                                    &response_info_.ssl_info.connection_status);
  response_info_.ssl_info.key_exchange_group = 23;  // X25519
  HttpResponseInfo restored_response_info;
  PickleAndRestore(response_info_, &restored_response_info);
  EXPECT_EQ(23, restored_response_info.ssl_info.key_exchange_group);
}

// Test that key_exchange_group is discarded for non-ECDHE ciphers prior to TLS
// 1.3, to account for the historical key_exchange_info field. See
// https://crbug.com/639421.
TEST_F(HttpResponseInfoTest, LegacyKeyExchangeInfoDHE) {
  response_info_.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  SSLConnectionStatusSetVersion(SSL_CONNECTION_VERSION_TLS1_2,
                                &response_info_.ssl_info.connection_status);
  SSLConnectionStatusSetCipherSuite(
      0x0093 /* TLS_DHE_RSA_WITH_AES_128_GCM_SHA256 */,
      &response_info_.ssl_info.connection_status);
  response_info_.ssl_info.key_exchange_group = 1024;
  HttpResponseInfo restored_response_info;
  PickleAndRestore(response_info_, &restored_response_info);
  EXPECT_EQ(0, restored_response_info.ssl_info.key_exchange_group);
}

// Test that key_exchange_group is discarded for unknown ciphers prior to TLS
// 1.3, to account for the historical key_exchange_info field. See
// https://crbug.com/639421.
TEST_F(HttpResponseInfoTest, LegacyKeyExchangeInfoUnknown) {
  response_info_.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  SSLConnectionStatusSetVersion(SSL_CONNECTION_VERSION_TLS1_2,
                                &response_info_.ssl_info.connection_status);
  SSLConnectionStatusSetCipherSuite(0xffff,
                                    &response_info_.ssl_info.connection_status);
  response_info_.ssl_info.key_exchange_group = 1024;
  HttpResponseInfo restored_response_info;
  PickleAndRestore(response_info_, &restored_response_info);
  EXPECT_EQ(0, restored_response_info.ssl_info.key_exchange_group);
}

// Test that peer_signature_algorithm is preserved.
TEST_F(HttpResponseInfoTest, PeerSignatureAlgorithm) {
  response_info_.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  response_info_.ssl_info.peer_signature_algorithm =
      0x0804;  // rsa_pss_rsae_sha256
  HttpResponseInfo restored_response_info;
  PickleAndRestore(response_info_, &restored_response_info);
  EXPECT_EQ(0x0804, restored_response_info.ssl_info.peer_signature_algorithm);
}

// Test that encrypted_client_hello is preserved.
TEST_F(HttpResponseInfoTest, EncryptedClientHello) {
  response_info_.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  {
    HttpResponseInfo restored_response_info;
    PickleAndRestore(response_info_, &restored_response_info);
    EXPECT_FALSE(restored_response_info.ssl_info.encrypted_client_hello);
  }

  response_info_.ssl_info.encrypted_client_hello = true;
  {
    HttpResponseInfo restored_response_info;
    PickleAndRestore(response_info_, &restored_response_info);
    EXPECT_TRUE(restored_response_info.ssl_info.encrypted_client_hello);
  }
}

// Tests that cache entries loaded over SSLv3 (no longer supported) are dropped.
TEST_F(HttpResponseInfoTest, FailsInitFromPickleWithSSLV3) {
  // A valid certificate is needed for ssl_info.is_valid() to be true.
  response_info_.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");

  // Non-SSLv3 versions should succeed.
  SSLConnectionStatusSetVersion(SSL_CONNECTION_VERSION_TLS1_2,
                                &response_info_.ssl_info.connection_status);
  base::Pickle tls12_pickle;
  response_info_.Persist(&tls12_pickle, false, false);
  bool truncated = false;
  HttpResponseInfo restored_tls12_response_info;
  EXPECT_TRUE(
      restored_tls12_response_info.InitFromPickle(tls12_pickle, &truncated));
  EXPECT_EQ(SSL_CONNECTION_VERSION_TLS1_2,
            SSLConnectionStatusToVersion(
                restored_tls12_response_info.ssl_info.connection_status));
  EXPECT_FALSE(truncated);

  // SSLv3 should fail.
  SSLConnectionStatusSetVersion(SSL_CONNECTION_VERSION_SSL3,
                                &response_info_.ssl_info.connection_status);
  base::Pickle ssl3_pickle;
  response_info_.Persist(&ssl3_pickle, false, false);
  HttpResponseInfo restored_ssl3_response_info;
  EXPECT_FALSE(
      restored_ssl3_response_info.InitFromPickle(ssl3_pickle, &truncated));
}

// Test that `dns_aliases` is preserved.
TEST_F(HttpResponseInfoTest, DnsAliases) {
  response_info_.dns_aliases = {"alias1", "alias2", "alias3"};
  HttpResponseInfo restored_response_info;
  PickleAndRestore(response_info_, &restored_response_info);
  EXPECT_THAT(restored_response_info.dns_aliases,
              testing::ElementsAre("alias1", "alias2", "alias3"));
}

// Test that an empty `dns_aliases` is preserved and doesn't throw an error.
TEST_F(HttpResponseInfoTest, EmptyDnsAliases) {
  response_info_.dns_aliases = {};
  HttpResponseInfo restored_response_info;
  PickleAndRestore(response_info_, &restored_response_info);
  EXPECT_TRUE(restored_response_info.dns_aliases.empty());
}

// Test that `browser_run_id` is preserved.
TEST_F(HttpResponseInfoTest, BrowserRunId) {
  response_info_.browser_run_id = 1;
  HttpResponseInfo restored_response_info;
  PickleAndRestore(response_info_, &restored_response_info);
  EXPECT_EQ(1, restored_response_info.browser_run_id);
}

// Test that an empty `browser_run_id` is preserved and doesn't throw an error.
TEST_F(HttpResponseInfoTest, EmptyBrowserRunId) {
  response_info_.browser_run_id = std::nullopt;
  HttpResponseInfo restored_response_info;
  PickleAndRestore(response_info_, &restored_response_info);
  EXPECT_FALSE(restored_response_info.browser_run_id.has_value());
}

// Test that did_use_shared_dictionary is preserved .
TEST_F(HttpResponseInfoTest, DidUseSharedDictionary) {
  response_info_.did_use_shared_dictionary = true;
  HttpResponseInfo restored_response_info;
  PickleAndRestore(response_info_, &restored_response_info);
  EXPECT_TRUE(restored_response_info.did_use_shared_dictionary);
}

}  // namespace

}  // namespace net
