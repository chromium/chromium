// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/session/session_certificate_policy_cache_storage_builder.h"

#import "ios/web/public/session/crw_session_certificate_policy_cache_storage.h"
#include "ios/web/public/test/web_task_environment.h"
#import "ios/web/session/session_certificate_policy_cache_impl.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using SessionCertificatePolicyCacheStorageBuilderTest = PlatformTest;

// Tests that creating a CRWSessionCertificateCacheStorage using BuildStorage()
// populates the storage with the correct data.
TEST_F(SessionCertificatePolicyCacheStorageBuilderTest, BuildStorage) {
  // Create a cache and populate it with an allowed cert.
  web::WebTaskEnvironment task_environment;
  web::SessionCertificatePolicyCacheImpl cache;
  scoped_refptr<net::X509Certificate> cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  std::string host("test.com");
  net::CertStatus status = net::CERT_STATUS_REVOKED;
  cache.RegisterAllowedCertificate(cert, host, status);
  // Build the cert policy cache storage and verify that the data was copied.
  web::SessionCertificatePolicyCacheStorageBuilder builder;
  CRWSessionCertificatePolicyCacheStorage* cache_storage =
      builder.BuildStorage(&cache);
  EXPECT_EQ(1U, cache_storage.certificateStorages.count);
  CRWSessionCertificateStorage* cert_storage =
      [cache_storage.certificateStorages anyObject];
  EXPECT_EQ(cert.get(), cert_storage.certificate);
  EXPECT_EQ(host, cert_storage.host);
  EXPECT_EQ(status, cert_storage.status);
}

// Tests that creating a SessionCertificatePolicyCache using
// BuildSessionCertificatePolicyCache() creates the cache with the correct data.
TEST_F(SessionCertificatePolicyCacheStorageBuilderTest,
       BuildSessionCertificatePolicyCache) {
  // Create the cert cache storage.
  scoped_refptr<net::X509Certificate> cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  std::string host("test.com");
  net::CertStatus status = net::CERT_STATUS_REVOKED;
  CRWSessionCertificateStorage* cert_storage =
      [[CRWSessionCertificateStorage alloc] initWithCertificate:cert
                                                           host:host
                                                         status:status];
  CRWSessionCertificatePolicyCacheStorage* cache_storage =
      [[CRWSessionCertificatePolicyCacheStorage alloc] init];
  [cache_storage setCertificateStorages:[NSSet setWithObject:cert_storage]];
  // Build the cert policy cache and verify its contents.
  web::SessionCertificatePolicyCacheStorageBuilder builder;
  std::unique_ptr<web::SessionCertificatePolicyCacheImpl> cache =
      builder.BuildSessionCertificatePolicyCache(cache_storage);
  EXPECT_NSEQ([cache_storage certificateStorages], cache -> GetAllowedCerts());
}
