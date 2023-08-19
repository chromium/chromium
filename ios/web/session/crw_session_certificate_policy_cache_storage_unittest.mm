// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/session/crw_session_certificate_policy_cache_storage.h"

#import "ios/web/public/session/proto/session.pb.h"
#import "net/cert/x509_certificate.h"
#import "net/cert/x509_util.h"
#import "net/test/cert_test_util.h"
#import "net/test/test_data_directory.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

class CRWSessionCertificatePolicyCacheStorageTest : public PlatformTest {
 protected:
  CRWSessionCertificatePolicyCacheStorageTest()
      : cache_storage_([[CRWSessionCertificatePolicyCacheStorage alloc] init]) {
    // Set up `cache_storage_`.
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
  EXPECT_NSEQ(cache_storage_, decoded);
}

// Tests that converting CRWSessionCertificatePolicyCacheStorage to proto and
// back results in an equivalent storage.
TEST_F(CRWSessionCertificatePolicyCacheStorageTest, EncodeDecodeToProto) {
  web::proto::CertificatesCacheStorage storage;
  [cache_storage_ serializeToProto:storage];

  CRWSessionCertificatePolicyCacheStorage* decoded =
      [[CRWSessionCertificatePolicyCacheStorage alloc] initWithProto:storage];

  EXPECT_NSEQ(cache_storage_, decoded);
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

// Tests that unarchiving a CRWSessionCertificateStorage from proto returns nil
// if the certificate data does not correctly decode to a certificate.
TEST_F(CRWSessionCertificateStorageTest, InvalidCertDataFromProto) {
  web::proto::CertificateStorage storage;
  storage.set_certificate("not a  cert");
  storage.set_host("host");
  storage.set_status(net::CERT_STATUS_INVALID);

  CRWSessionCertificateStorage* decoded =
      [[CRWSessionCertificateStorage alloc] initWithProto:storage];
  EXPECT_FALSE(decoded);
}
