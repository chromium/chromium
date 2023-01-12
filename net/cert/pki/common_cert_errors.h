// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_PKI_COMMON_CERT_ERRORS_H_
#define NET_CERT_PKI_COMMON_CERT_ERRORS_H_

#include "net/base/net_export.h"
#include "net/cert/pki/cert_errors.h"

// This file contains the set of "default" certificate errors (those
// defined by the core verification/path building code).
//
// Errors may be defined for other domains.
namespace net::cert_errors {

// An internal error occurred which prevented path building or verification
// from finishing.
NET_EXPORT extern const CertErrorId kInternalError;

// The verification time is after the certificate's notAfter time.
NET_EXPORT extern const CertErrorId kValidityFailedNotAfter;

// The verification time is before the certificate's notBefore time.
NET_EXPORT extern const CertErrorId kValidityFailedNotBefore;

// The certificate is actively distrusted by the trust store (this is separate
// from other revocation mechanisms).
NET_EXPORT extern const CertErrorId kDistrustedByTrustStore;

// The certificate disagrees on what the signature algorithm was
// (Certificate.signatureAlgorithm != TBSCertificate.signature).
NET_EXPORT extern const CertErrorId kSignatureAlgorithmMismatch;

// Certificate verification was called with an empty chain.
NET_EXPORT extern const CertErrorId kChainIsEmpty;

// The certificate contains an unknown extension which is marked as critical.
NET_EXPORT extern const CertErrorId kUnconsumedCriticalExtension;

// The target certificate appears to be a CA (has Basic Constraints CA=true)
// but is being used for TLS client or server authentication.
NET_EXPORT extern const CertErrorId kTargetCertShouldNotBeCa;

// The certificate is being used to sign other certificates, however the
// keyCertSign KeyUsage was not set.
NET_EXPORT extern const CertErrorId kKeyCertSignBitNotSet;

// The chain violates the max_path_length from BasicConstraints.
NET_EXPORT extern const CertErrorId kMaxPathLengthViolated;

// The certificate being used to sign other certificates has a
// BasicConstraints extension, however it sets CA=false
NET_EXPORT extern const CertErrorId kBasicConstraintsIndicatesNotCa;

// The certificate being used to sign other certificates does not include a
// BasicConstraints extension.
NET_EXPORT extern const CertErrorId kMissingBasicConstraints;

// The certificate has a subject or subjectAltName that violates an issuer's
// name constraints.
NET_EXPORT extern const CertErrorId kNotPermittedByNameConstraints;

// The chain has an excessive number of names and/or name constraints.
NET_EXPORT extern const CertErrorId kTooManyNameConstraintChecks;

// The certificate's issuer field does not match the subject of its alleged
// issuer.
NET_EXPORT extern const CertErrorId kSubjectDoesNotMatchIssuer;

// Failed to verify the certificate's signature using its issuer's public key.
NET_EXPORT extern const CertErrorId kVerifySignedDataFailed;

// The certificate encodes its signature differently between
// Certificate.algorithm and TBSCertificate.signature, but it appears
// to be the same algorithm.
NET_EXPORT extern const CertErrorId kSignatureAlgorithmsDifferentEncoding;

// The certificate verification is being done for serverAuth, however the
// certificate lacks serverAuth in its ExtendedKeyUsages.
NET_EXPORT extern const CertErrorId kEkuLacksServerAuth;

// The certificate verification is being done for clientAuth, however the
// certificate lacks clientAuth in its ExtendedKeyUsages.
NET_EXPORT extern const CertErrorId kEkuLacksClientAuth;

// The root certificate in a chain is not trusted.
NET_EXPORT extern const CertErrorId kCertIsNotTrustAnchor;

// The chain is not valid for any policy, and an explicit policy was required.
// (Either because the relying party requested it during verificaiton, or it was
// requrested by a PolicyConstraints extension).
NET_EXPORT extern const CertErrorId kNoValidPolicy;

// The certificate is trying to map to, or from, anyPolicy.
NET_EXPORT extern const CertErrorId kPolicyMappingAnyPolicy;

// The public key in this certificate could not be parsed.
NET_EXPORT extern const CertErrorId kFailedParsingSpki;

// The certificate's signature algorithm (used to verify its
// signature) is not acceptable by the consumer. What constitutes as
// "acceptable" is determined by the verification delegate.
NET_EXPORT extern const CertErrorId kUnacceptableSignatureAlgorithm;

// The certificate's public key is not acceptable by the consumer.
// What constitutes as "acceptable" is determined by the verification delegate.
NET_EXPORT extern const CertErrorId kUnacceptablePublicKey;

// The certificate's EKU is missing serverAuth. However Netscape Server Gated
// Crypto is present instead.
NET_EXPORT extern const CertErrorId kEkuLacksServerAuthButHasGatedCrypto;

// The certificate's EKU is missing serverAuth. However EKU ANY is present
// instead.
NET_EXPORT extern const CertErrorId kEkuLacksServerAuthButHasAnyEKU;

// The certificate's EKU is missing clientAuth. However EKU ANY is present
// instead.
NET_EXPORT extern const CertErrorId kEkuLacksClientAuthButHasAnyEKU;

// The certificate's EKU is missing both clientAuth and serverAuth.
NET_EXPORT extern const CertErrorId kEkuLacksClientAuthOrServerAuth;

// The certificate's EKU has OSCP Signing when it should not.
NET_EXPORT extern const CertErrorId kEkuHasProhibitedOCSPSigning;

// The certificate's EKU has Time Stamping when it should not.
NET_EXPORT extern const CertErrorId kEkuHasProhibitedTimeStamping;

// The certificate's EKU has Code Signing when it should not.
NET_EXPORT extern const CertErrorId kEkuHasProhibitedCodeSigning;

// The certificate does not have EKU.
NET_EXPORT extern const CertErrorId kEkuNotPresent;

// The certificate has been revoked.
NET_EXPORT extern const CertErrorId kCertificateRevoked;

// The certificate lacks a recognized revocation mechanism (i.e. OCSP/CRL).
// Emitted as an error when revocation checking expects certificates to have
// such info.
NET_EXPORT extern const CertErrorId kNoRevocationMechanism;

// The certificate had a revocation mechanism, but when used it was unable to
// affirmatively say whether the certificate was unrevoked.
NET_EXPORT extern const CertErrorId kUnableToCheckRevocation;

// Path building was unable to find any issuers for the certificate.
NET_EXPORT extern const CertErrorId kNoIssuersFound;

// Deadline was reached during path building.
NET_EXPORT extern const CertErrorId kDeadlineExceeded;

// Iteration limit was reached during path building.
NET_EXPORT extern const CertErrorId kIterationLimitExceeded;

// Depth limit was reached during path building.
NET_EXPORT extern const CertErrorId kDepthLimitExceeded;

}  // namespace net::cert_errors

#endif  // NET_CERT_PKI_COMMON_CERT_ERRORS_H_
