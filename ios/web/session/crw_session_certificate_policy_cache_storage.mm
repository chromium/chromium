// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/session/crw_session_certificate_policy_cache_storage.h"

#import <string_view>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/web/public/session/proto/session.pb.h"
#import "ios/web/session/hash_util.h"
#import "net/base/hash_value.h"
#import "net/cert/x509_certificate.h"
#import "net/cert/x509_util.h"

namespace {

// Total bytes serialized during CRWSessionCertificateStorage encoding since the
// uptime.
static size_t gBytesEncoded = 0;

// The deprecated serialization technique serialized each certificate policy as
// an NSArray, where the necessary information is stored at the following
// indices.
typedef NS_ENUM(NSInteger, DeprecatedSerializationIndices) {
  CertificateDataIndex = 0,
  HostStringIndex,
  StatusIndex,
  DeprecatedSerializationIndexCount,
};

// Converts `certificate` to NSData for serialization.
NSData* CertificateToNSData(net::X509Certificate* certificate) {
  std::string_view cert_string =
      net::x509_util::CryptoBufferAsStringPiece(certificate->cert_buffer());
  return [NSData dataWithBytes:cert_string.data() length:cert_string.length()];
}

// Converts serialized NSData to a certificate.
scoped_refptr<net::X509Certificate> NSDataToCertificate(NSData* data) {
  return net::X509Certificate::CreateFromBytes(
      base::make_span(static_cast<const uint8_t*>(data.bytes), data.length));
}

}  // namespace

namespace web {

// CRWSessionCertificateStorage serialization keys.
NSString* const kCertificateSerializationKey = @"CertificateSerializationKey";
NSString* const kHostSerializationKey = @"HostSerializationKey";
NSString* const kStatusSerializationKey = @"StatusSerializationKey";

// CRWSessionCertificatePolicyCacheStorage serialization keys.
NSString* const kCertificateStoragesKey = @"kCertificateStoragesKey";
NSString* const kCertificateStoragesDeprecatedKey = @"allowedCertificates";

size_t GetCertPolicyBytesEncoded() {
  return gBytesEncoded;
}

}  // namespace web

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

- (instancetype)initWithProto:(const web::proto::CertificateStorage&)storage {
  const std::string& certString = storage.certificate();
  scoped_refptr<net::X509Certificate> cert =
      net::X509Certificate::CreateFromBytes(base::as_byte_span(certString));

  // Return nil if the cert cannot be decoded or the host is empty.
  if (!cert || storage.host().empty()) {
    return nil;
  }

  return [self initWithCertificate:cert
                              host:storage.host()
                            status:storage.status()];
}

- (void)serializeToProto:(web::proto::CertificateStorage&)storage {
  const std::string_view certString =
      net::x509_util::CryptoBufferAsStringPiece(_certificate->cert_buffer());

  storage.set_certificate(certString.data(), certString.size());
  storage.set_host(_host);
  storage.set_status(_status);
}

#pragma mark NSObject

- (NSUInteger)hash {
  return web::session::ComputeHash(_certificate, _host, _status);
}

- (BOOL)isEqual:(NSObject*)object {
  CRWSessionCertificateStorage* other =
      base::apple::ObjCCast<CRWSessionCertificateStorage>(object);

  return [other cr_isEqualSameClass:self];
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
  NSData* certData = CertificateToNSData(_certificate.get());
  [aCoder encodeObject:certData forKey:web::kCertificateSerializationKey];
  [aCoder encodeObject:base::SysUTF8ToNSString(_host)
                forKey:web::kHostSerializationKey];
  [aCoder encodeObject:@(_status) forKey:web::kStatusSerializationKey];

  gBytesEncoded += certData.length + _host.size() + sizeof(_status);
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

- (BOOL)cr_isEqualSameClass:(CRWSessionCertificateStorage*)other {
  if (_host != other.host) {
    return NO;
  }

  if (_status != other.status) {
    return NO;
  }

  return net::x509_util::CryptoBufferEqual(_certificate->cert_buffer(),
                                           other.certificate->cert_buffer());
}

@end

#pragma mark - CRWSessionCertificatePolicyCacheStorage

@implementation CRWSessionCertificatePolicyCacheStorage

@synthesize certificateStorages = _certificateStorages;

- (instancetype)initWithProto:
    (const web::proto::CertificatesCacheStorage&)storage {
  if ((self = [super init])) {
    NSMutableSet<CRWSessionCertificateStorage*>* certificates =
        [[NSMutableSet alloc] initWithCapacity:storage.certs_size()];
    for (const web::proto::CertificateStorage& certStorage : storage.certs()) {
      CRWSessionCertificateStorage* cert =
          [[CRWSessionCertificateStorage alloc] initWithProto:certStorage];

      if (cert) {
        [certificates addObject:cert];
      }
    }
    _certificateStorages = [certificates copy];
  }
  return self;
}

- (void)serializeToProto:(web::proto::CertificatesCacheStorage&)storage {
  for (CRWSessionCertificateStorage* cert in _certificateStorages) {
    [cert serializeToProto:*storage.add_certs()];
  }
}

#pragma mark NSObject

- (BOOL)isEqual:(NSObject*)object {
  CRWSessionCertificatePolicyCacheStorage* other =
      base::apple::ObjCCast<CRWSessionCertificatePolicyCacheStorage>(object);

  return [other cr_isEqualSameClass:self];
}

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

#pragma mark Private

- (BOOL)cr_isEqualSameClass:(CRWSessionCertificatePolicyCacheStorage*)other {
  if (_certificateStorages.count != other.certificateStorages.count) {
    return NO;
  }

  for (CRWSessionCertificateStorage* cert in other.certificateStorages) {
    if (![_certificateStorages containsObject:cert]) {
      return NO;
    }
  }

  return YES;
}

@end
