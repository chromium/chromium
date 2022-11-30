// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains a set of root CAs which are required to disclose
// all certificates via Certificate Transparency, as well as exceptions
// for independent and disclosed sub-CAs.
//
// It is meant to be directly included in transport_security_state.cc
// within an unnamed namespace.

struct CTRequiredPolicy {
  // A certificate MUST be disclosed via Certificate Transparency if it
  // chains to or through one of the values contained in |roots|, which
  // contains the SHA-256 hash of the issuing CA's SubjectPublicKeyInfo,
  // the same format as HTTP Public Key Pinning.
  const SHA256HashValue* roots;

  // The number of entries in |roots|.
  size_t roots_length;

  // The date at which enforcement should begin, relative to the Unix
  // Epoch. If equivalent to zero (base::TimeDelta()), then it is enforced
  // for all certificates.
  base::TimeDelta effective_date;

  // However, if a certificate ALSO chains to or through one of
  // |exceptions|, which also contains the SHA-256 hashes of the
  // issuing CA's SubjectPublicKeyInfo, then even though it chained
  // through |roots|, it will be exempt from CT requirements.
  const SHA256HashValue* exceptions;

  // The number of entries in |exceptions|.
  size_t exceptions_length;
};

typedef CTRequiredPolicy CTRequiredPolicies[2];

const CTRequiredPolicies& GetCTRequiredPolicies() {
  static const CTRequiredPolicy kCTRequiredPolicies[] = {
      // See net/data/ssl/symantec/README.md
      {
          kSymantecRoots, kSymantecRootsLength,
          // 1 June 2016, 00:00:00 GMT.
          base::Seconds(1464739200),
          kSymantecExceptions, kSymantecExceptionsLength,
      },
      {
          kSymantecManagedCAs, kSymantecManagedCAsLength,
          base::TimeDelta(), nullptr, 0
      },
  };

  return kCTRequiredPolicies;
}
