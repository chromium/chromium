// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_client_auth_cache.h"

#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_private_key.h"
#include "net/ssl/test_ssl_private_key.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace net {

namespace {
scoped_refptr<SSLPrivateKey> MakeMockKey() {
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());
  return WrapOpenSSLPrivateKey(std::move(pkey));
}
}  // namespace

TEST(SSLClientAuthCacheTest, LookupAddRemove) {
  SSLClientAuthCache cache;

  HostPortPair server1("foo1", 443);
  scoped_refptr<X509Certificate> cert1(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(cert1);

  HostPortPair server2("foo2", 443);
  scoped_refptr<X509Certificate> cert2(
      ImportCertFromFile(GetTestCertsDirectory(), "expired_cert.pem"));
  ASSERT_TRUE(cert2);

  HostPortPair server3("foo3", 443);
  scoped_refptr<X509Certificate> cert3(
      ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem"));
  ASSERT_TRUE(cert3);

  scoped_refptr<X509Certificate> cached_cert;
  scoped_refptr<SSLPrivateKey> cached_pkey;
  // Lookup non-existent client certificate.
  cached_cert = nullptr;
  EXPECT_FALSE(cache.Lookup(server1, &cached_cert, &cached_pkey));

  // Add client certificate for server1.
  cache.Add(server1, cert1.get(), MakeMockKey());
  cached_cert = nullptr;
  EXPECT_TRUE(cache.Lookup(server1, &cached_cert, &cached_pkey));
  EXPECT_EQ(cert1, cached_cert);

  // Add client certificate for server2.
  cache.Add(server2, cert2.get(), MakeMockKey());
  cached_cert = nullptr;
  EXPECT_TRUE(cache.Lookup(server1, &cached_cert, &cached_pkey));
  EXPECT_EQ(cert1.get(), cached_cert.get());
  cached_cert = nullptr;
  EXPECT_TRUE(cache.Lookup(server2, &cached_cert, &cached_pkey));
  EXPECT_EQ(cert2, cached_cert);

  // Overwrite the client certificate for server1.
  cache.Add(server1, cert3.get(), MakeMockKey());
  cached_cert = nullptr;
  EXPECT_TRUE(cache.Lookup(server1, &cached_cert, &cached_pkey));
  EXPECT_EQ(cert3, cached_cert);
  cached_cert = nullptr;
  EXPECT_TRUE(cache.Lookup(server2, &cached_cert, &cached_pkey));
  EXPECT_EQ(cert2, cached_cert);

  // Remove client certificate of server1.
  cache.Remove(server1);
  cached_cert = nullptr;
  EXPECT_FALSE(cache.Lookup(server1, &cached_cert, &cached_pkey));
  cached_cert = nullptr;
  EXPECT_TRUE(cache.Lookup(server2, &cached_cert, &cached_pkey));
  EXPECT_EQ(cert2, cached_cert);

  // Remove non-existent client certificate.
  cache.Remove(server1);
  cached_cert = nullptr;
  EXPECT_FALSE(cache.Lookup(server1, &cached_cert, &cached_pkey));
  cached_cert = nullptr;
  EXPECT_TRUE(cache.Lookup(server2, &cached_cert, &cached_pkey));
  EXPECT_EQ(cert2, cached_cert);
}

// Check that if the server differs only by port number, it is considered
// a separate server.
TEST(SSLClientAuthCacheTest, LookupWithPort) {
  SSLClientAuthCache cache;

  HostPortPair server1("foo", 443);
  scoped_refptr<X509Certificate> cert1(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(cert1);

  HostPortPair server2("foo", 8443);
  scoped_refptr<X509Certificate> cert2(
      ImportCertFromFile(GetTestCertsDirectory(), "expired_cert.pem"));
  ASSERT_TRUE(cert2);

  cache.Add(server1, cert1.get(), MakeMockKey());
  cache.Add(server2, cert2.get(), MakeMockKey());

  scoped_refptr<X509Certificate> cached_cert;
  scoped_refptr<SSLPrivateKey> cached_pkey;
  EXPECT_TRUE(cache.Lookup(server1, &cached_cert, &cached_pkey));
  EXPECT_EQ(cert1.get(), cached_cert.get());
  EXPECT_TRUE(cache.Lookup(server2, &cached_cert, &cached_pkey));
  EXPECT_EQ(cert2.get(), cached_cert.get());
}

// Check that the a nullptr certificate, indicating the user has declined to
// send a certificate, is properly cached.
TEST(SSLClientAuthCacheTest, LookupNullPreference) {
  SSLClientAuthCache cache;

  HostPortPair server1("foo", 443);
  scoped_refptr<X509Certificate> cert1(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(cert1);

  cache.Add(server1, nullptr, MakeMockKey());

  scoped_refptr<X509Certificate> cached_cert(cert1);
  scoped_refptr<SSLPrivateKey> cached_pkey;
  // Make sure that |cached_cert| is updated to nullptr, indicating the user
  // declined to send a certificate to |server1|.
  EXPECT_TRUE(cache.Lookup(server1, &cached_cert, &cached_pkey));
  EXPECT_EQ(nullptr, cached_cert.get());

  // Remove the existing cached certificate.
  cache.Remove(server1);
  cached_cert = nullptr;
  EXPECT_FALSE(cache.Lookup(server1, &cached_cert, &cached_pkey));

  // Add a new preference for a specific certificate.
  cache.Add(server1, cert1.get(), MakeMockKey());
  cached_cert = nullptr;
  EXPECT_TRUE(cache.Lookup(server1, &cached_cert, &cached_pkey));
  EXPECT_EQ(cert1, cached_cert);

  // Replace the specific preference with a nullptr certificate.
  cache.Add(server1, nullptr, MakeMockKey());
  cached_cert = nullptr;
  EXPECT_TRUE(cache.Lookup(server1, &cached_cert, &cached_pkey));
  EXPECT_EQ(nullptr, cached_cert.get());
}

// Check that the Clear() method removes all cache entries.
TEST(SSLClientAuthCacheTest, Clear) {
  SSLClientAuthCache cache;

  HostPortPair server1("foo", 443);
  scoped_refptr<X509Certificate> cert1(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(cert1);

  cache.Add(server1, cert1.get(), MakeMockKey());

  HostPortPair server2("foo2", 443);
  cache.Add(server2, nullptr, MakeMockKey());

  scoped_refptr<X509Certificate> cached_cert;
  scoped_refptr<SSLPrivateKey> cached_pkey;

  // Demonstrate the set up is correct.
  EXPECT_TRUE(cache.Lookup(server1, &cached_cert, &cached_pkey));
  EXPECT_EQ(cert1, cached_cert);

  EXPECT_TRUE(cache.Lookup(server2, &cached_cert, &cached_pkey));
  EXPECT_EQ(nullptr, cached_cert.get());

  cache.Clear();

  // Check that we no longer have entries for either server.
  EXPECT_FALSE(cache.Lookup(server1, &cached_cert, &cached_pkey));
  EXPECT_FALSE(cache.Lookup(server2, &cached_cert, &cached_pkey));
}

}  // namespace net
