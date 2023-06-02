// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_SESSION_CRW_SESSION_CERTIFICATE_POLICY_CACHE_STORAGE_H_
#define IOS_WEB_PUBLIC_SESSION_CRW_SESSION_CERTIFICATE_POLICY_CACHE_STORAGE_H_

#import <Foundation/Foundation.h>

#include "base/memory/ref_counted.h"
#include "net/cert/cert_status_flags.h"

namespace net {
class X509Certificate;
}

#pragma mark - CRWSessionCertificateStorage

namespace web {
namespace proto {
class CertificateStorage;
class CertificatesCacheStorage;
}  // namespace proto

// Serialization keys used in CRWSessionCertificateStorage's NSCoding
// implementation.
extern NSString* const kCertificateSerializationKey;
extern NSString* const kHostSerializationKey;
extern NSString* const kStatusSerializationKey;

// Total bytes serialized during CRWSessionCertificateStorage encoding since the
// uptime.
size_t GetCertPolicyBytesEncoded();

}  // namespace web

// A serializable representation of a certificate.
@interface CRWSessionCertificateStorage : NSObject <NSCoding>

// Designated initializer.
- (instancetype)initWithCertificate:(scoped_refptr<net::X509Certificate>)cert
                               host:(const std::string&)host
                             status:(net::CertStatus)status
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Convenience initializer that creates an instance from proto representation.
- (instancetype)initWithProto:(const web::proto::CertificateStorage&)storage;

// Serializes the CRWSessionCertificateStorage into `storage`.
- (void)serializeToProto:(web::proto::CertificateStorage&)storage;

// The certificate represented by this storage.
@property(nonatomic, readonly) net::X509Certificate* certificate;
// The hostname of the page that issued `certificate`.
@property(nonatomic, readonly) std::string& host;
// The allowance chosen for the certificate.
@property(nonatomic, readonly) net::CertStatus status;

@end

#pragma mark - CRWSessionCertificatePolicyCacheStorage

namespace web {
// Serialization key used in CRWSessionCertificatePolicyCacheStorage's NSCoding
// implementation.
extern NSString* const kCertificateStoragesKey;
}  // namespace web

// A serializable representation of a list of allowed certificates.
@interface CRWSessionCertificatePolicyCacheStorage : NSObject <NSCoding>

// Convenience initializer that creates an instance from proto representation.
- (instancetype)initWithProto:
    (const web::proto::CertificatesCacheStorage&)storage;

// Serializes the CRWSessionCertificatePolicyCacheStorage into `storage`.
- (void)serializeToProto:(web::proto::CertificatesCacheStorage&)storage;

// The certificate policy storages for this session.
@property(nonatomic, strong)
    NSSet<CRWSessionCertificateStorage*>* certificateStorages;

@end

#endif  // IOS_WEB_PUBLIC_SESSION_CRW_SESSION_CERTIFICATE_POLICY_CACHE_STORAGE_H_
