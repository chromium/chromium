// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_log_verifier.h"

#include <string.h>

#include <vector>

#include "base/logging.h"
#include "crypto/openssl_util.h"
#include "crypto/sha2.h"
#include "net/cert/ct_log_verifier_util.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/merkle_audit_proof.h"
#include "net/cert/merkle_consistency_proof.h"
#include "net/cert/signed_tree_head.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace net {

namespace {

// The SHA-256 hash of the empty string.
const unsigned char kSHA256EmptyStringHash[ct::kSthRootHashLength] = {
    0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14, 0x9a, 0xfb, 0xf4,
    0xc8, 0x99, 0x6f, 0xb9, 0x24, 0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b,
    0x93, 0x4c, 0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55};

bool IsPowerOfTwo(uint64_t n) {
  return n != 0 && (n & (n - 1)) == 0;
}

const EVP_MD* GetEvpAlg(ct::DigitallySigned::HashAlgorithm alg) {
  switch (alg) {
    case ct::DigitallySigned::HASH_ALGO_MD5:
      return EVP_md5();
    case ct::DigitallySigned::HASH_ALGO_SHA1:
      return EVP_sha1();
    case ct::DigitallySigned::HASH_ALGO_SHA224:
      return EVP_sha224();
    case ct::DigitallySigned::HASH_ALGO_SHA256:
      return EVP_sha256();
    case ct::DigitallySigned::HASH_ALGO_SHA384:
      return EVP_sha384();
    case ct::DigitallySigned::HASH_ALGO_SHA512:
      return EVP_sha512();
    case ct::DigitallySigned::HASH_ALGO_NONE:
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace

// static
scoped_refptr<const CTLogVerifier> CTLogVerifier::Create(
    const base::StringPiece& public_key,
    std::string description) {
  scoped_refptr<CTLogVerifier> result(
      new CTLogVerifier(std::move(description)));
  if (!result->Init(public_key))
    return nullptr;
  return result;
}

CTLogVerifier::CTLogVerifier(std::string description)
    : description_(std::move(description)),
      hash_algorithm_(ct::DigitallySigned::HASH_ALGO_NONE),
      signature_algorithm_(ct::DigitallySigned::SIG_ALGO_ANONYMOUS),
      public_key_(nullptr) {}

bool CTLogVerifier::Verify(const ct::SignedEntryData& entry,
                           const ct::SignedCertificateTimestamp& sct) const {
  if (sct.log_id != key_id()) {
    DVLOG(1) << "SCT is not signed by this log.";
    return false;
  }

  if (!SignatureParametersMatch(sct.signature))
    return false;

  std::string serialized_log_entry;
  if (!ct::EncodeSignedEntry(entry, &serialized_log_entry)) {
    DVLOG(1) << "Unable to serialize entry.";
    return false;
  }
  std::string serialized_data;
  if (!ct::EncodeV1SCTSignedData(sct.timestamp, serialized_log_entry,
                                 sct.extensions, &serialized_data)) {
    DVLOG(1) << "Unable to create SCT to verify.";
    return false;
  }

  return VerifySignature(serialized_data, sct.signature.signature_data);
}

bool CTLogVerifier::VerifySignedTreeHead(
    const ct::SignedTreeHead& signed_tree_head) const {
  if (!SignatureParametersMatch(signed_tree_head.signature))
    return false;

  std::string serialized_data;
  if (!ct::EncodeTreeHeadSignature(signed_tree_head, &serialized_data))
    return false;

  if (VerifySignature(serialized_data,
                      signed_tree_head.signature.signature_data)) {
    if (signed_tree_head.tree_size == 0) {
      // Root hash must equate SHA256 hash of the empty string.
      return (memcmp(signed_tree_head.sha256_root_hash, kSHA256EmptyStringHash,
                     ct::kSthRootHashLength) == 0);
    }
    return true;
  }
  return false;
}

bool CTLogVerifier::SignatureParametersMatch(
    const ct::DigitallySigned& signature) const {
  if (!signature.SignatureParametersMatch(hash_algorithm_,
                                          signature_algorithm_)) {
    DVLOG(1) << "Mismatched hash or signature algorithm. Hash: "
             << hash_algorithm_ << " vs " << signature.hash_algorithm
             << " Signature: " << signature_algorithm_ << " vs "
             << signature.signature_algorithm << ".";
    return false;
  }

  return true;
}

bool CTLogVerifier::VerifyConsistencyProof(
    const ct::MerkleConsistencyProof& proof,
    const std::string& old_tree_hash,
    const std::string& new_tree_hash) const {
  // Proof does not originate from this log.
  if (key_id_ != proof.log_id)
    return false;

  // Cannot prove consistency from a tree of a certain size to a tree smaller
  // than that - only the other way around.
  if (proof.first_tree_size > proof.second_tree_size)
    return false;

  // If the proof is between trees of the same size, then the 'proof'
  // is really just a statement that the tree hasn't changed. If this
  // is the case, there should be no proof nodes, and both the old
  // and new hash should be equivalent.
  if (proof.first_tree_size == proof.second_tree_size)
    return proof.nodes.empty() && old_tree_hash == new_tree_hash;

  // It is possible to call this method to prove consistency between the
  // initial state of a log (i.e. an empty tree) and a later root. In that
  // case, the only valid proof is an empty proof.
  if (proof.first_tree_size == 0)
    return proof.nodes.empty();

  // Implement the algorithm described in
  // https://tools.ietf.org/html/draft-ietf-trans-rfc6962-bis-12#section-9.4.2
  //
  // It maintains a pair of hashes |fr| and |sr|, initialized to the same
  // value. Each node in |proof| will be hashed to the left of both |fr| and
  // |sr| or to the right of only |sr|. The proof is then valid if |fr| is
  // |old_tree_hash| and |sr| is |new_tree_hash|, proving that tree nodes which
  // make up |old_tree_hash| are a prefix of |new_tree_hash|.

  // At this point, the algorithm's preconditions must be satisfied.
  DCHECK_LT(0u, proof.first_tree_size);
  DCHECK_LT(proof.first_tree_size, proof.second_tree_size);

  // 1. If "first" is an exact power of 2, then prepend "first_hash" to the
  // "consistency_path" array.
  base::StringPiece first_proof_node = old_tree_hash;
  auto iter = proof.nodes.begin();
  if (!IsPowerOfTwo(proof.first_tree_size)) {
    if (iter == proof.nodes.end())
      return false;
    first_proof_node = *iter;
    ++iter;
  }
  // iter now points to the second node in the modified proof.nodes.

  // 2. Set "fn" to "first - 1" and "sn" to "second - 1".
  uint64_t fn = proof.first_tree_size - 1;
  uint64_t sn = proof.second_tree_size - 1;

  // 3. If "LSB(fn)" is set, then right-shift both "fn" and "sn" equally until
  // "LSB(fn)" is not set.
  while (fn & 1) {
    fn >>= 1;
    sn >>= 1;
  }

  // 4. Set both "fr" and "sr" to the first value in the "consistency_path"
  // array.
  std::string fr = first_proof_node.as_string();
  std::string sr = first_proof_node.as_string();

  // 5. For each subsequent value "c" in the "consistency_path" array:
  for (; iter != proof.nodes.end(); ++iter) {
    // If "sn" is 0, stop the iteration and fail the proof verification.
    if (sn == 0)
      return false;
    // If "LSB(fn)" is set, or if "fn" is equal to "sn", then:
    if ((fn & 1) || fn == sn) {
      // 1. Set "fr" to "HASH(0x01 || c || fr)"
      //    Set "sr" to "HASH(0x01 || c || sr)"
      fr = ct::internal::HashNodes(*iter, fr);
      sr = ct::internal::HashNodes(*iter, sr);

      // 2. If "LSB(fn)" is not set, then right-shift both "fn" and "sn" equally
      // until either "LSB(fn)" is set or "fn" is "0".
      while (!(fn & 1) && fn != 0) {
        fn >>= 1;
        sn >>= 1;
      }
    } else {  // Otherwise:
      // Set "sr" to "HASH(0x01 || sr || c)"
      sr = ct::internal::HashNodes(sr, *iter);
    }

    // Finally, right-shift both "fn" and "sn" one time.
    fn >>= 1;
    sn >>= 1;
  }

  // 6. After completing iterating through the "consistency_path" array as
  // described above, verify that the "fr" calculated is equal to the
  // "first_hash" supplied, that the "sr" calculated is equal to the
  // "second_hash" supplied and that "sn" is 0.
  return fr == old_tree_hash && sr == new_tree_hash && sn == 0;
}

bool CTLogVerifier::VerifyAuditProof(const ct::MerkleAuditProof& proof,
                                     const std::string& root_hash,
                                     const std::string& leaf_hash) const {
  // Implements the algorithm described in
  // https://tools.ietf.org/html/draft-ietf-trans-rfc6962-bis-19#section-10.4.1
  //
  // It maintains a hash |r|, initialized to |leaf_hash|, and hashes nodes from
  // |proof| into it. The proof is then valid if |r| is |root_hash|, proving
  // that |root_hash| includes |leaf_hash|.

  // 1.  Compare "leaf_index" against "tree_size".  If "leaf_index" is
  //     greater than or equal to "tree_size" fail the proof verification.
  if (proof.leaf_index >= proof.tree_size)
    return false;

  // 2.  Set "fn" to "leaf_index" and "sn" to "tree_size - 1".
  uint64_t fn = proof.leaf_index;
  uint64_t sn = proof.tree_size - 1;
  // 3.  Set "r" to "hash".
  std::string r = leaf_hash;

  // 4.  For each value "p" in the "inclusion_path" array:
  for (const std::string& p : proof.nodes) {
    // If "sn" is 0, stop the iteration and fail the proof verification.
    if (sn == 0)
      return false;

    // If "LSB(fn)" is set, or if "fn" is equal to "sn", then:
    if ((fn & 1) || fn == sn) {
      // 1.  Set "r" to "HASH(0x01 || p || r)"
      r = ct::internal::HashNodes(p, r);

      // 2.  If "LSB(fn)" is not set, then right-shift both "fn" and "sn"
      //     equally until either "LSB(fn)" is set or "fn" is "0".
      while (!(fn & 1) && fn != 0) {
        fn >>= 1;
        sn >>= 1;
      }
    } else {  // Otherwise:
      // Set "r" to "HASH(0x01 || r || p)"
      r = ct::internal::HashNodes(r, p);
    }

    // Finally, right-shift both "fn" and "sn" one time.
    fn >>= 1;
    sn >>= 1;
  }

  // 5.  Compare "sn" to 0.  Compare "r" against the "root_hash".  If "sn"
  //     is equal to 0, and "r" and the "root_hash" are equal, then the
  //     log has proven the inclusion of "hash".  Otherwise, fail the
  //     proof verification.
  return sn == 0 && r == root_hash;
}

CTLogVerifier::~CTLogVerifier() {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  if (public_key_)
    EVP_PKEY_free(public_key_);
}

bool CTLogVerifier::Init(const base::StringPiece& public_key) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  CBS cbs;
  CBS_init(&cbs, reinterpret_cast<const uint8_t*>(public_key.data()),
           public_key.size());
  public_key_ = EVP_parse_public_key(&cbs);
  if (!public_key_ || CBS_len(&cbs) != 0)
    return false;

  key_id_ = crypto::SHA256HashString(public_key);

  // Right now, only RSASSA-PKCS1v15 with SHA-256 and ECDSA with SHA-256 are
  // supported.
  switch (EVP_PKEY_type(public_key_->type)) {
    case EVP_PKEY_RSA:
      hash_algorithm_ = ct::DigitallySigned::HASH_ALGO_SHA256;
      signature_algorithm_ = ct::DigitallySigned::SIG_ALGO_RSA;
      break;
    case EVP_PKEY_EC:
      hash_algorithm_ = ct::DigitallySigned::HASH_ALGO_SHA256;
      signature_algorithm_ = ct::DigitallySigned::SIG_ALGO_ECDSA;
      break;
    default:
      DVLOG(1) << "Unsupported key type: " << EVP_PKEY_type(public_key_->type);
      return false;
  }

  // Extra sanity check: Require RSA keys of at least 2048 bits.
  // EVP_PKEY_size returns the size in bytes. 256 = 2048-bit RSA key.
  if (signature_algorithm_ == ct::DigitallySigned::SIG_ALGO_RSA &&
      EVP_PKEY_size(public_key_) < 256) {
    DVLOG(1) << "Too small a public key.";
    return false;
  }

  return true;
}

bool CTLogVerifier::VerifySignature(const base::StringPiece& data_to_sign,
                                    const base::StringPiece& signature) const {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  const EVP_MD* hash_alg = GetEvpAlg(hash_algorithm_);
  if (hash_alg == nullptr)
    return false;

  bssl::ScopedEVP_MD_CTX ctx;
  return EVP_DigestVerifyInit(ctx.get(), nullptr, hash_alg, nullptr,
                              public_key_) &&
         EVP_DigestVerifyUpdate(ctx.get(), data_to_sign.data(),
                                data_to_sign.size()) &&
         EVP_DigestVerifyFinal(
             ctx.get(), reinterpret_cast<const uint8_t*>(signature.data()),
             signature.size());
}

}  // namespace net
