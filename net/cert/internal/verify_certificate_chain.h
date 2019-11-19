// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_VERIFY_CERTIFICATE_CHAIN_H_
#define NET_CERT_INTERNAL_VERIFY_CERTIFICATE_CHAIN_H_

#include <set>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_export.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/internal/parsed_certificate.h"
#include "net/der/input.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace net {

namespace der {
struct GeneralizedTime;
}

struct CertificateTrust;

// The key purpose (extended key usage) to check for during verification.
enum class KeyPurpose {
  ANY_EKU,
  SERVER_AUTH,
  CLIENT_AUTH,
};

enum class InitialExplicitPolicy {
  kFalse,
  kTrue,
};

enum class InitialPolicyMappingInhibit {
  kFalse,
  kTrue,
};

enum class InitialAnyPolicyInhibit {
  kFalse,
  kTrue,
};

// VerifyCertificateChainDelegate exposes delegate methods used when verifying a
// chain.
class NET_EXPORT VerifyCertificateChainDelegate {
 public:
  // Implementations should return true if |signature_algorithm| is allowed for
  // certificate signing, false otherwise. When returning false implementations
  // can optionally add high-severity errors to |errors| with details on why it
  // was rejected.
  virtual bool IsSignatureAlgorithmAcceptable(
      const SignatureAlgorithm& signature_algorithm,
      CertErrors* errors) = 0;

  // Implementations should return true if |public_key| is acceptable. This is
  // called for each certificate in the chain, including the target certificate.
  // When returning false implementations can optionally add high-severity
  // errors to |errors| with details on why it was rejected.
  //
  // |public_key| can be assumed to be non-null.
  virtual bool IsPublicKeyAcceptable(EVP_PKEY* public_key,
                                     CertErrors* errors) = 0;

  virtual ~VerifyCertificateChainDelegate();
};

// VerifyCertificateChain() verifies an ordered certificate path in accordance
// with RFC 5280's "Certification Path Validation" algorithm (section 6).
//
// -----------------------------------------
// Deviations from RFC 5280
// -----------------------------------------
//
//   * If Extended Key Usage appears on intermediates, it is treated as
//     a restriction on subordinate certificates.
//   * No revocation checking is performed.
//
// -----------------------------------------
// Additional responsibilities of the caller
// -----------------------------------------
//
// After successful path verification, the caller is responsible for
// subsequently checking:
//
//  * The end-entity's KeyUsage before using its SPKI.
//  * The end-entity's name/subjectAltName. Name constraints from intermediates
//    will have already been applied, so it is sufficient to check the
//    end-entity for a match.
//
// ---------
// Inputs
// ---------
//
//   certs:
//     A non-empty chain of DER-encoded certificates, listed in the
//     "forward" direction. The first certificate is the target
//     certificate to verify, and the last certificate has trustedness
//     given by |last_cert_trust| (generally a trust anchor).
//
//      * certs[0] is the target certificate to verify.
//      * certs[i+1] holds the certificate that issued cert_chain[i].
//      * certs[N-1] the root certificate
//
//     Note that THIS IS NOT identical in meaning to the same named
//     "certs" input defined in RFC 5280 section 6.1.1.a. The differences
//     are:
//
//      * The order of certificates is reversed
//      * In RFC 5280 "certs" DOES NOT include the trust anchor
//
//   last_cert_trust:
//     Trustedness of |certs.back()|. The trustedness of |certs.back()|
//     MUST BE decided by the caller -- this function takes it purely as
//     an input. Moreover, the CertificateTrust can be used to specify
//     trust anchor constraints.
//
//     This combined with |certs.back()| (the root certificate) fills a
//     similar role to "trust anchor information" defined in RFC 5280
//     section 6.1.1.d.
//
//   delegate:
//     |delegate| must be non-null. It is used to answer policy questions such
//     as whether a signature algorithm is acceptable, or a public key is strong
//     enough.
//
//   time:
//     The UTC time to use for expiration checks. This is equivalent to
//     the input from RFC 5280 section 6.1.1:
//
//       (b)  the current date/time.
//
//   required_key_purpose:
//     The key purpose that the target certificate needs to be valid for.
//
//   user_initial_policy_set:
//     This is equivalent to the same named input in RFC 5280 section
//     6.1.1:
//
//       (c)  user-initial-policy-set: A set of certificate policy
//            identifiers naming the policies that are acceptable to the
//            certificate user. The user-initial-policy-set contains the
//            special value any-policy if the user is not concerned about
//            certificate policy.
//
//   initial_policy_mapping_inhibit:
//     This is equivalent to the same named input in RFC 5280 section
//     6.1.1:
//
//       (e)  initial-policy-mapping-inhibit, which indicates if policy
//            mapping is allowed in the certification path.
//
//   initial_explicit_policy:
//     This is equivalent to the same named input in RFC 5280 section
//     6.1.1:
//
//       (f)  initial-explicit-policy, which indicates if the path must be
//            valid for at least one of the certificate policies in the
//            user-initial-policy-set.
//
//   initial_any_policy_inhibit:
//     This is equivalent to the same named input in RFC 5280 section
//     6.1.1:
//
//       (g)  initial-any-policy-inhibit, which indicates whether the
//            anyPolicy OID should be processed if it is included in a
//            certificate.
//
// ---------
// Outputs
// ---------
//
//   user_constrained_policy_set:
//     Can be null. If non-null, |user_constrained_policy_set| will be filled
//     with the matching policies (intersected with user_initial_policy_set).
//     This is equivalent to the same named output in X.509 section 10.2.
//     Note that it is OK for this to point to input user_initial_policy_set.
//
//   errors:
//     Must be non-null. The set of errors/warnings encountered while
//     validating the path are appended to this structure. If verification
//     failed, then there is guaranteed to be at least 1 high severity error
//     written to |errors|.
//
// -------------------------
// Trust Anchor constraints
// -------------------------
//
// Conceptually, VerifyCertificateChain() sets RFC 5937's
// "enforceTrustAnchorConstraints" to true.
//
// One specifies trust anchor constraints using the |last_cert_trust|
// parameter in conjunction with extensions appearing in |certs.back()|.
//
// The trust anchor |certs.back()| is always passed as a certificate to
// this function, however the manner in which that certificate is
// interpreted depends on |last_cert_trust|:
//
// TRUSTED_ANCHOR:
//
// No properties from the root certificate, other than its Subject and
// SPKI, are checked during verification. This is the usual
// interpretation for a "trust anchor".
//
// TRUSTED_ANCHOR_WITH_CONSTRAINTS:
//
// Only a subset of extensions and properties from the certificate are checked,
// as described by RFC 5937.
//
//  * Signature:             No
//  * Validity (expiration): No
//  * Key usage:             No
//  * Extended key usage:    Yes (not part of RFC 5937)
//  * Basic constraints:     Yes, but only the pathlen (CA=false is accepted)
//  * Name constraints:      Yes
//  * Certificate policies:  Not currently, TODO(crbug.com/634453)
//  * Policy Mappings:       No
//  * inhibitAnyPolicy:      Not currently, TODO(crbug.com/634453)
//  * PolicyConstraints:     Not currently, TODO(crbug.com/634452)
//
// The presence of any other unrecognized extension marked as critical fails
// validation.
NET_EXPORT void VerifyCertificateChain(
    const ParsedCertificateList& certs,
    const CertificateTrust& last_cert_trust,
    VerifyCertificateChainDelegate* delegate,
    const der::GeneralizedTime& time,
    KeyPurpose required_key_purpose,
    InitialExplicitPolicy initial_explicit_policy,
    const std::set<der::Input>& user_initial_policy_set,
    InitialPolicyMappingInhibit initial_policy_mapping_inhibit,
    InitialAnyPolicyInhibit initial_any_policy_inhibit,
    std::set<der::Input>* user_constrained_policy_set,
    CertPathErrors* errors);

}  // namespace net

#endif  // NET_CERT_INTERNAL_VERIFY_CERTIFICATE_CHAIN_H_
