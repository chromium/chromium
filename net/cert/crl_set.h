// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CRL_SET_H_
#define NET_CERT_CRL_SET_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/memory/ref_counted.h"
#include "net/base/hash_value.h"
#include "net/base/net_export.h"

namespace net {

// A CRLSet is a structure that lists the serial numbers of revoked
// certificates from a number of issuers where issuers are identified by the
// SHA256 of their SubjectPublicKeyInfo.
class NET_EXPORT CRLSet : public base::RefCountedThreadSafe<CRLSet> {
 public:
  enum Result {
    REVOKED,  // the certificate should be rejected.
    UNKNOWN,  // the CRL for the certificate is not included in the set.
    GOOD,     // the certificate is not listed.
  };

  // Parses the bytes in |data| and, on success, puts a new CRLSet in
  // |out_crl_set| and returns true.
  static bool Parse(std::string_view data, scoped_refptr<CRLSet>* out_crl_set);

  // CheckSPKI checks whether the given SPKI has been listed as blocked.
  //   spki_hash: the SHA256 of the SubjectPublicKeyInfo of the certificate.
  Result CheckSPKI(std::string_view spki_hash) const;

  // CheckSerial returns the information contained in the set for a given
  // certificate:
  //   serial_number: the serial number of the certificate, as the DER-encoded
  //       value
  //   issuer_spki_hash: the SHA256 of the SubjectPublicKeyInfo of the CRL
  //       signer
  Result CheckSerial(std::string_view serial_number,
                     std::string_view issuer_spki_hash) const;

  // CheckSubject returns the information contained in the set for a given,
  // encoded subject name and SPKI SHA-256 hash. The subject name is encoded as
  // a DER X.501 Name (see https://tools.ietf.org/html/rfc5280#section-4.1.2.4).
  Result CheckSubject(std::string_view asn1_subject,
                      std::string_view spki_hash) const;

  // Returns true if |spki_hash|, the SHA256 of the SubjectPublicKeyInfo,
  // is known to be used for interception by a party other than the device
  // or machine owner.
  bool IsKnownInterceptionKey(std::string_view spki_hash) const;

  // IsExpired returns true iff the current time is past the NotAfter time
  // specified in the CRLSet.
  bool IsExpired() const;

  // sequence returns the sequence number of this CRL set. CRL sets generated
  // by the same source are given strictly monotonically increasing sequence
  // numbers.
  uint32_t sequence() const;

  // CRLList contains a map of (issuer SPKI hash, revoked serial numbers)
  // pairs.
  using CRLList = std::unordered_map<std::string, std::vector<std::string>>;

  // crls returns the internal state of this CRLSet. It should only be used in
  // testing.
  const CRLList& CrlsForTesting() const;

  // BuiltinCRLSet() returns the default CRLSet, to be used when no CRLSet is
  // available from the network.  The default CRLSet includes a statically-
  // configured block list.
  static scoped_refptr<CRLSet> BuiltinCRLSet();

  // EmptyCRLSetForTesting returns a valid, but empty, CRLSet for unit tests.
  static scoped_refptr<CRLSet> EmptyCRLSetForTesting();

  // ExpiredCRLSetForTesting returns a expired, empty CRLSet for unit tests.
  static scoped_refptr<CRLSet> ExpiredCRLSetForTesting();

  // ForTesting returns a CRLSet for testing. If |is_expired| is true, calling
  // IsExpired on the result will return true. If |issuer_spki| is not NULL,
  // the CRLSet will cover certificates issued by that SPKI. If |serial_number|
  // is not empty, then that DER-encoded serial number will be considered to
  // have been revoked by |issuer_spki|. If |utf8_common_name| is not empty
  // then the CRLSet will consider certificates with a subject consisting only
  // of that common name as a UTF8String to be revoked unless they match an
  // SPKI hash from |acceptable_spki_hashes_for_cn|.
  static scoped_refptr<CRLSet> ForTesting(
      bool is_expired,
      const SHA256HashValue* issuer_spki,
      std::string_view serial_number,
      std::string_view utf8_common_name,
      const std::vector<std::string>& acceptable_spki_hashes_for_cn);

 private:
  CRLSet();
  ~CRLSet();

  friend class base::RefCountedThreadSafe<CRLSet>;

  uint32_t sequence_ = 0;
  // not_after_ contains the time, in UNIX epoch seconds, after which the
  // CRLSet should be considered stale, or 0 if no such time was given.
  uint64_t not_after_ = 0;
  // crls_ is a map from the SHA-256 hash of an X.501 subject name to a list
  // of revoked serial numbers.
  CRLList crls_;
  // blocked_spkis_ contains the SHA256 hashes of SPKIs which are to be blocked
  // no matter where in a certificate chain they might appear.
  std::vector<std::string> blocked_spkis_;
  // known_interception_spkis_ contains the SHA256 hashes of SPKIs which are
  // known to be used for interception by a party other than the device or
  // machine owner.
  std::vector<std::string> known_interception_spkis_;
  // limited_subjects_ is a map from the SHA256 hash of an X.501 subject name
  // to a list of allowed SPKI hashes for certificates with that subject name.
  std::unordered_map<std::string, std::vector<std::string>> limited_subjects_;
};

}  // namespace net

#endif  // NET_CERT_CRL_SET_H_
