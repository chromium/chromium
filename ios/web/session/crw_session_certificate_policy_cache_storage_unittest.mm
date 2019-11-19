// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/session/crw_session_certificate_policy_cache_storage.h"

#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Checks for equality between |cert_storage1| and |cert_storage2|.
bool CertStoragesAreEqual(CRWSessionCertificateStorage* cert_storage1,
                          CRWSessionCertificateStorage* cert_storage2) {
  return net::x509_util::CryptoBufferEqual(
             cert_storage1.certificate->cert_buffer(),
             cert_storage2.certificate->cert_buffer()) &&
         cert_storage1.host == cert_storage2.host &&
         cert_storage1.status == cert_storage2.status;
}
// Checks for equality between |cache_storage1| and |cache_storage2|.
bool CacheStoragesAreEqual(
    CRWSessionCertificatePolicyCacheStorage* cache_storage1,
    CRWSessionCertificatePolicyCacheStorage* cache_storage2) {
  NSArray* certs1 = [cache_storage1.certificateStorages allObjects];
  NSArray* certs2 = [cache_storage2.certificateStorages allObjects];
  if (certs1.count != certs2.count)
    return false;
  for (NSUInteger i = 0; i < certs1.count; ++i) {
    if (!CertStoragesAreEqual(certs1[i], certs2[i]))
      return false;
  }
  return true;
}
}  // namespace

class CRWSessionCertificatePolicyCacheStorageTest : public PlatformTest {
 protected:
  CRWSessionCertificatePolicyCacheStorageTest()
      : cache_storage_([[CRWSessionCertificatePolicyCacheStorage alloc] init]) {
    // Set up |cache_storage_|.
    scoped_refptr<net::X509Certificate> cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    NSMutableSet* certs = [[NSMutableSet alloc] init];
    [certs addObject:[[CRWSessionCertificateStorage alloc]
                         initWithCertificate:cert
                                        host:"test1.com"
                                      status:net::CERT_STATUS_REVOKED]];
    [cache_storage_ setCertificateStorages:certs];
  }

 protected:
  CRWSessionCertificatePolicyCacheStorage* cache_storage_;
};

// Tests that unarchiving CRWSessionCertificatePolicyCacheStorage data results
// in an equivalent storage.
TEST_F(CRWSessionCertificatePolicyCacheStorageTest, EncodeDecode) {
  NSData* data = [NSKeyedArchiver archivedDataWithRootObject:cache_storage_
                                       requiringSecureCoding:NO
                                                       error:nil];
  NSKeyedUnarchiver* unarchiver =
      [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:nil];
  unarchiver.requiresSecureCoding = NO;
  id decoded = [unarchiver decodeObjectForKey:NSKeyedArchiveRootObjectKey];
  EXPECT_TRUE(CacheStoragesAreEqual(cache_storage_, decoded));
}

using CRWSessionCertificateStorageTest = PlatformTest;

// Tests that unarchiving a CRWSessionCertificateStorage returns nil if the
// certificate data does not correctly decode to a certificate.
TEST_F(CRWSessionCertificateStorageTest, InvalidCertData) {
  NSKeyedArchiver* archiver =
      [[NSKeyedArchiver alloc] initRequiringSecureCoding:NO];
  [archiver encodeObject:[@"not a  cert" dataUsingEncoding:NSUTF8StringEncoding]
                  forKey:web::kCertificateSerializationKey];
  [archiver encodeObject:@"host" forKey:web::kHostSerializationKey];
  [archiver encodeObject:@(net::CERT_STATUS_INVALID)
                  forKey:web::kStatusSerializationKey];
  [archiver finishEncoding];
  NSData* data = [archiver encodedData];

  NSKeyedUnarchiver* unarchiver =
      [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:nil];
  unarchiver.requiresSecureCoding = NO;
  id decoded = [unarchiver decodeObjectForKey:NSKeyedArchiveRootObjectKey];
  EXPECT_FALSE(decoded);
}
