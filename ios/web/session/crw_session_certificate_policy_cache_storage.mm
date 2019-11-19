// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/session/crw_session_certificate_policy_cache_storage.h"

#import "base/strings/sys_string_conversions.h"
#include "net/base/hash_value.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
// CRWSessionCertificateStorage serialization keys.
NSString* const kCertificateSerializationKey = @"CertificateSerializationKey";
NSString* const kHostSerializationKey = @"HostSerializationKey";
NSString* const kStatusSerializationKey = @"StatusSerializationKey";

// CRWSessionCertificatePolicyCacheStorage serialization keys.
NSString* const kCertificateStoragesKey = @"kCertificateStoragesKey";
NSString* const kCertificateStoragesDeprecatedKey = @"allowedCertificates";
}  // namespace web

namespace {

// The deprecated serialization technique serialized each certificate policy as
// an NSArray, where the necessary information is stored at the following
// indices.
typedef NS_ENUM(NSInteger, DeprecatedSerializationIndices) {
  CertificateDataIndex = 0,
  HostStringIndex,
  StatusIndex,
  DeprecatedSerializationIndexCount,
};

// Converts |certificate| to NSData for serialization.
NSData* CertificateToNSData(net::X509Certificate* certificate) {
  base::StringPiece cert_string =
      net::x509_util::CryptoBufferAsStringPiece(certificate->cert_buffer());
  return [NSData dataWithBytes:cert_string.data() length:cert_string.length()];
}

// Converts serialized NSData to a certificate.
scoped_refptr<net::X509Certificate> NSDataToCertificate(NSData* data) {
  return net::X509Certificate::CreateFromBytes(
      static_cast<const char*>(data.bytes), data.length);
}

}  // namespace

#pragma mark - CRWSessionCertificateStorage

@interface CRWSessionCertificateStorage () {
  // Backing objects for properties of the same name.
  scoped_refptr<net::X509Certificate> _certificate;
  std::string _host;
}

// Initializes the CRWSessionCertificateStorage using decoded values.  Can
// return nil if the parameters cannot be converted correctly to a cert storage.
- (instancetype)initWithCertData:(NSData*)certData
                        hostName:(NSString*)hostName
                      certStatus:(NSNumber*)certStatus;

// Initializes the CRWSessionCertificateStorage using the deprecated
// serialization technique.  See DeprecatedSerializationIndices above for more
// details.
- (instancetype)initWithDeprecatedSerialization:(NSArray*)serialization;

@end

@implementation CRWSessionCertificateStorage

@synthesize host = _host;
@synthesize status = _status;

- (instancetype)initWithCertificate:(scoped_refptr<net::X509Certificate>)cert
                               host:(const std::string&)host
                             status:(net::CertStatus)status {
  DCHECK(cert);
  DCHECK(host.length());
  if ((self = [super init])) {
    _certificate = cert;
    _host = host;
    _status = status;
  }
  return self;
}

#pragma mark Accessors

- (net::X509Certificate*)certificate {
  return _certificate.get();
}

#pragma mark NSCoding

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  NSData* certData =
      [aDecoder decodeObjectForKey:web::kCertificateSerializationKey];
  NSString* hostName = [aDecoder decodeObjectForKey:web::kHostSerializationKey];
  NSNumber* certStatus =
      [aDecoder decodeObjectForKey:web::kStatusSerializationKey];
  return [self initWithCertData:certData
                       hostName:hostName
                     certStatus:certStatus];
}

- (void)encodeWithCoder:(NSCoder*)aCoder {
  [aCoder encodeObject:CertificateToNSData(_certificate.get())
                forKey:web::kCertificateSerializationKey];
  [aCoder encodeObject:base::SysUTF8ToNSString(_host)
                forKey:web::kHostSerializationKey];
  [aCoder encodeObject:@(_status) forKey:web::kStatusSerializationKey];
}

#pragma mark Private

- (instancetype)initWithCertData:(NSData*)certData
                        hostName:(NSString*)hostName
                      certStatus:(NSNumber*)certStatus {
  scoped_refptr<net::X509Certificate> cert = NSDataToCertificate(certData);
  std::string host = base::SysNSStringToUTF8(hostName);
  if (!cert || !host.length() || !certStatus)
    return nil;
  net::CertStatus status = certStatus.unsignedIntegerValue;
  return [self initWithCertificate:cert host:host status:status];
}

- (instancetype)initWithDeprecatedSerialization:(NSArray*)serialization {
  if (serialization.count != DeprecatedSerializationIndexCount)
    return nil;
  return [self initWithCertData:serialization[CertificateDataIndex]
                       hostName:serialization[HostStringIndex]
                     certStatus:serialization[StatusIndex]];
}

@end

#pragma mark - CRWSessionCertificatePolicyCacheStorage

@implementation CRWSessionCertificatePolicyCacheStorage

@synthesize certificateStorages = _certificateStorages;

#pragma mark NSCoding

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  if ((self = [super init])) {
    _certificateStorages =
        [aDecoder decodeObjectForKey:web::kCertificateStoragesKey];
    if (!_certificateStorages.count) {
      // Attempt to use the deprecated serialization if none were decoded.
      NSMutableSet* deprecatedSerializations =
          [aDecoder decodeObjectForKey:web::kCertificateStoragesDeprecatedKey];
      NSMutableSet* certificateStorages = [[NSMutableSet alloc]
          initWithCapacity:deprecatedSerializations.count];
      for (NSArray* serialiazation in deprecatedSerializations) {
        CRWSessionCertificateStorage* certificatePolicyStorage =
            [[CRWSessionCertificateStorage alloc]
                initWithDeprecatedSerialization:serialiazation];
        if (certificatePolicyStorage)
          [certificateStorages addObject:certificatePolicyStorage];
      }
      _certificateStorages = certificateStorages;
    }
  }
  return self;
}

- (void)encodeWithCoder:(NSCoder*)aCoder {
  [aCoder encodeObject:self.certificateStorages
                forKey:web::kCertificateStoragesKey];
}

@end
