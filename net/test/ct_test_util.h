// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_CT_TEST_UTIL_H_
#define NET_TEST_CT_TEST_UTIL_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"

namespace net::ct {

struct DigitallySigned;
struct MerkleTreeLeaf;
struct SignedEntryData;
struct SignedTreeHead;

// Note: unless specified otherwise, all test data is taken from Certificate
// Transparency test data repository.

// Fills |entry| with test data for an X.509 entry.
void GetX509CertSignedEntry(SignedEntryData* entry);

// Fills |tree_leaf| with test data for an X.509 Merkle tree leaf.
void GetX509CertTreeLeaf(MerkleTreeLeaf* tree_leaf);

// Returns a DER-encoded X509 cert. The SCT provided by
// GetX509CertSCT is signed over this certificate.
std::string GetDerEncodedX509Cert();

// Fills |entry| with test data for a Precertificate entry.
void GetPrecertSignedEntry(SignedEntryData* entry);

// Fills |tree_leaf| with test data for a Precertificate Merkle tree leaf.
void GetPrecertTreeLeaf(MerkleTreeLeaf* tree_leaf);

// Returns the binary representation of a test DigitallySigned
std::string GetTestDigitallySigned();

// Returns the binary representation of a test serialized SCT.
std::string GetTestSignedCertificateTimestamp();

// Test log key
std::string GetTestPublicKey();

// ID of test log key
std::string GetTestPublicKeyId();

// SCT for the X509Certificate provided above.
void GetX509CertSCT(scoped_refptr<SignedCertificateTimestamp>* sct);

// SCT for the Precertificate log entry provided above.
void GetPrecertSCT(scoped_refptr<SignedCertificateTimestamp>* sct);

// Issuer key hash
std::string GetDefaultIssuerKeyHash();

// Fake OCSP response with an embedded SCT list.
std::string GetDerEncodedFakeOCSPResponse();

// The SCT list embedded in the response above.
std::string GetFakeOCSPExtensionValue();

// The cert the OCSP response is for.
std::string GetDerEncodedFakeOCSPResponseCert();

// The issuer of the previous cert.
std::string GetDerEncodedFakeOCSPResponseIssuerCert();

// A sample, valid STH.
bool GetSampleSignedTreeHead(SignedTreeHead* sth);

// A valid STH for the empty tree.
bool GetSampleEmptySignedTreeHead(SignedTreeHead* sth);

// An STH for an empty tree where the root hash is not the hash of the empty
// string, but the signature over the STH is valid. Such an STH is not valid
// according to RFC6962.
bool GetBadEmptySignedTreeHead(SignedTreeHead* sth);

// The SHA256 root hash for the sample STH.
std::string GetSampleSTHSHA256RootHash();

// The tree head signature for the sample STH.
std::string GetSampleSTHTreeHeadSignature();

// The same signature as GetSampleSTHTreeHeadSignature, decoded.
bool GetSampleSTHTreeHeadDecodedSignature(DigitallySigned* signature);

// The sample STH in JSON form.
std::string GetSampleSTHAsJson();

// Assembles, and returns, a sample STH in JSON format using
// the provided parameters.
std::string CreateSignedTreeHeadJsonString(size_t tree_size,
                                           int64_t timestamp,
                                           std::string sha256_root_hash,
                                           std::string tree_head_signature);

// Assembles, and returns, a sample consistency proof in JSON format using
// the provided raw nodes (i.e. the raw nodes will be base64-encoded).
std::string CreateConsistencyProofJsonString(
    const std::vector<std::string>& raw_nodes);

// Returns SCTList for testing.
std::string GetSCTListForTesting();

// Returns a corrupted SCTList. This is done by changing a byte inside the
// Log ID part of the SCT so it does not match the log used in the tests.
std::string GetSCTListWithInvalidSCT();

// Returns true if |log_description| is in the |result|'s |verified_scts| and
// number of |verified_scts| in |result| is equal to 1.
bool CheckForSingleVerifiedSCTInResult(
    const SignedCertificateTimestampAndStatusList& scts,
    const std::string& log_description);

// Returns true if |origin| is in the |result|'s |verified_scts|.
bool CheckForSCTOrigin(const SignedCertificateTimestampAndStatusList& scts,
                       SignedCertificateTimestamp::Origin origin);

}  // namespace net::ct

#endif  // NET_TEST_CT_TEST_UTIL_H_
