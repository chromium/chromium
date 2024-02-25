// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_serialization.h"

#include <string_view>

#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "crypto/sha2.h"
#include "net/cert/merkle_tree_leaf.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/signed_tree_head.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"

namespace net::ct {

namespace {

const size_t kLogIdLength = crypto::kSHA256Length;

enum SignatureType {
  SIGNATURE_TYPE_CERTIFICATE_TIMESTAMP = 0,
  TREE_HASH = 1,
};

// Reads a variable-length SCT list that has been TLS encoded.
// The bytes read from |in| are discarded (i.e. |in|'s prefix removed)
// |max_list_length| contains the overall length of the encoded list.
// |max_item_length| contains the maximum length of a single item.
// On success, returns true and updates |*out| with the encoded list.
bool ReadSCTList(CBS* in, std::vector<std::string_view>* out) {
  std::vector<std::string_view> result;

  CBS sct_list_data;

  if (!CBS_get_u16_length_prefixed(in, &sct_list_data))
    return false;

  while (CBS_len(&sct_list_data) != 0) {
    CBS sct_list_item;
    if (!CBS_get_u16_length_prefixed(&sct_list_data, &sct_list_item) ||
        CBS_len(&sct_list_item) == 0) {
      return false;
    }

    result.emplace_back(reinterpret_cast<const char*>(CBS_data(&sct_list_item)),
                        CBS_len(&sct_list_item));
  }

  result.swap(*out);
  return true;
}

// Checks and converts a hash algorithm.
// |in| is the numeric representation of the algorithm.
// If the hash algorithm value is in a set of known values, fills in |out| and
// returns true. Otherwise, returns false.
bool ConvertHashAlgorithm(unsigned in, DigitallySigned::HashAlgorithm* out) {
  switch (in) {
    case DigitallySigned::HASH_ALGO_NONE:
    case DigitallySigned::HASH_ALGO_MD5:
    case DigitallySigned::HASH_ALGO_SHA1:
    case DigitallySigned::HASH_ALGO_SHA224:
    case DigitallySigned::HASH_ALGO_SHA256:
    case DigitallySigned::HASH_ALGO_SHA384:
    case DigitallySigned::HASH_ALGO_SHA512:
      break;
    default:
      return false;
  }
  *out = static_cast<DigitallySigned::HashAlgorithm>(in);
  return true;
}

// Checks and converts a signing algorithm.
// |in| is the numeric representation of the algorithm.
// If the signing algorithm value is in a set of known values, fills in |out|
// and returns true. Otherwise, returns false.
bool ConvertSignatureAlgorithm(
    unsigned in,
    DigitallySigned::SignatureAlgorithm* out) {
  switch (in) {
    case DigitallySigned::SIG_ALGO_ANONYMOUS:
    case DigitallySigned::SIG_ALGO_RSA:
    case DigitallySigned::SIG_ALGO_DSA:
    case DigitallySigned::SIG_ALGO_ECDSA:
      break;
    default:
      return false;
  }
  *out = static_cast<DigitallySigned::SignatureAlgorithm>(in);
  return true;
}

// Writes a SignedEntryData of type X.509 cert to |*output|.
// |input| is the SignedEntryData containing the certificate.
// Returns true if the leaf_certificate in the SignedEntryData does not exceed
// kMaxAsn1CertificateLength and so can be written to |output|.
bool EncodeAsn1CertSignedEntry(const SignedEntryData& input, CBB* output) {
  CBB child;
  return CBB_add_u24_length_prefixed(output, &child) &&
         CBB_add_bytes(
             &child,
             reinterpret_cast<const uint8_t*>(input.leaf_certificate.data()),
             input.leaf_certificate.size()) &&
         CBB_flush(output);
}

// Writes a SignedEntryData of type PreCertificate to |*output|.
// |input| is the SignedEntryData containing the TBSCertificate and issuer key
// hash. Returns true if the TBSCertificate component in the SignedEntryData
// does not exceed kMaxTbsCertificateLength and so can be written to |output|.
bool EncodePrecertSignedEntry(const SignedEntryData& input, CBB* output) {
  CBB child;
  return CBB_add_bytes(
             output,
             reinterpret_cast<const uint8_t*>(input.issuer_key_hash.data),
             kLogIdLength) &&
         CBB_add_u24_length_prefixed(output, &child) &&
         CBB_add_bytes(
             &child,
             reinterpret_cast<const uint8_t*>(input.tbs_certificate.data()),
             input.tbs_certificate.size()) &&
         CBB_flush(output);
}

}  // namespace

bool EncodeDigitallySigned(const DigitallySigned& input, CBB* output_cbb) {
  CBB child;
  return CBB_add_u8(output_cbb, input.hash_algorithm) &&
         CBB_add_u8(output_cbb, input.signature_algorithm) &&
         CBB_add_u16_length_prefixed(output_cbb, &child) &&
         CBB_add_bytes(
             &child,
             reinterpret_cast<const uint8_t*>(input.signature_data.data()),
             input.signature_data.size()) &&
         CBB_flush(output_cbb);
}

bool EncodeDigitallySigned(const DigitallySigned& input,
                           std::string* output) {
  bssl::ScopedCBB output_cbb;
  if (!CBB_init(output_cbb.get(), 64) ||
      !EncodeDigitallySigned(input, output_cbb.get()) ||
      !CBB_flush(output_cbb.get())) {
    return false;
  }

  output->append(reinterpret_cast<const char*>(CBB_data(output_cbb.get())),
                 CBB_len(output_cbb.get()));
  return true;
}

bool DecodeDigitallySigned(CBS* input, DigitallySigned* output) {
  uint8_t hash_algo;
  uint8_t sig_algo;
  CBS sig_data;

  if (!CBS_get_u8(input, &hash_algo) || !CBS_get_u8(input, &sig_algo) ||
      !CBS_get_u16_length_prefixed(input, &sig_data)) {
    return false;
  }

  DigitallySigned result;
  if (!ConvertHashAlgorithm(hash_algo, &result.hash_algorithm) ||
      !ConvertSignatureAlgorithm(sig_algo, &result.signature_algorithm)) {
    return false;
  }

  result.signature_data.assign(
      reinterpret_cast<const char*>(CBS_data(&sig_data)), CBS_len(&sig_data));

  *output = result;
  return true;
}

bool DecodeDigitallySigned(std::string_view* input, DigitallySigned* output) {
  CBS input_cbs;
  CBS_init(&input_cbs, reinterpret_cast<const uint8_t*>(input->data()),
           input->size());
  bool result = DecodeDigitallySigned(&input_cbs, output);
  input->remove_prefix(input->size() - CBS_len(&input_cbs));
  return result;
}

static bool EncodeSignedEntry(const SignedEntryData& input, CBB* output) {
  if (!CBB_add_u16(output, input.type)) {
    return false;
  }
  switch (input.type) {
    case SignedEntryData::LOG_ENTRY_TYPE_X509:
      return EncodeAsn1CertSignedEntry(input, output);
    case SignedEntryData::LOG_ENTRY_TYPE_PRECERT:
      return EncodePrecertSignedEntry(input, output);
  }
  return false;
}

bool EncodeSignedEntry(const SignedEntryData& input, std::string* output) {
  bssl::ScopedCBB output_cbb;

  if (!CBB_init(output_cbb.get(), 64) ||
      !EncodeSignedEntry(input, output_cbb.get()) ||
      !CBB_flush(output_cbb.get())) {
    return false;
  }

  output->append(reinterpret_cast<const char*>(CBB_data(output_cbb.get())),
                 CBB_len(output_cbb.get()));
  return true;
}

static bool ReadTimeSinceEpoch(CBS* input, base::Time* output) {
  uint64_t time_since_epoch = 0;
  if (!CBS_get_u64(input, &time_since_epoch))
    return false;

  base::CheckedNumeric<int64_t> time_since_epoch_signed = time_since_epoch;

  if (!time_since_epoch_signed.IsValid()) {
    return false;
  }

  *output = base::Time::UnixEpoch() +
            base::Milliseconds(int64_t{time_since_epoch_signed.ValueOrDie()});

  return true;
}

static bool WriteTimeSinceEpoch(const base::Time& timestamp, CBB* output) {
  base::TimeDelta time_since_epoch = timestamp - base::Time::UnixEpoch();
  return CBB_add_u64(output, time_since_epoch.InMilliseconds());
}

bool EncodeTreeLeaf(const MerkleTreeLeaf& leaf, std::string* output) {
  bssl::ScopedCBB output_cbb;
  CBB child;
  if (!CBB_init(output_cbb.get(), 64) ||
      !CBB_add_u8(output_cbb.get(), 0) ||  // version: 1
      !CBB_add_u8(output_cbb.get(), 0) ||  // type: timestamped entry
      !WriteTimeSinceEpoch(leaf.timestamp, output_cbb.get()) ||
      !EncodeSignedEntry(leaf.signed_entry, output_cbb.get()) ||
      !CBB_add_u16_length_prefixed(output_cbb.get(), &child) ||
      !CBB_add_bytes(&child,
                     reinterpret_cast<const uint8_t*>(leaf.extensions.data()),
                     leaf.extensions.size()) ||
      !CBB_flush(output_cbb.get())) {
    return false;
  }
  output->append(reinterpret_cast<const char*>(CBB_data(output_cbb.get())),
                 CBB_len(output_cbb.get()));
  return true;
}

bool EncodeV1SCTSignedData(const base::Time& timestamp,
                           const std::string& serialized_log_entry,
                           const std::string& extensions,
                           std::string* output) {
  bssl::ScopedCBB output_cbb;
  CBB child;
  if (!CBB_init(output_cbb.get(), 64) ||
      !CBB_add_u8(output_cbb.get(), SignedCertificateTimestamp::V1) ||
      !CBB_add_u8(output_cbb.get(), SIGNATURE_TYPE_CERTIFICATE_TIMESTAMP) ||
      !WriteTimeSinceEpoch(timestamp, output_cbb.get()) ||
      // NOTE: serialized_log_entry must already be serialized and contain the
      // length as the prefix.
      !CBB_add_bytes(
          output_cbb.get(),
          reinterpret_cast<const uint8_t*>(serialized_log_entry.data()),
          serialized_log_entry.size()) ||
      !CBB_add_u16_length_prefixed(output_cbb.get(), &child) ||
      !CBB_add_bytes(&child,
                     reinterpret_cast<const uint8_t*>(extensions.data()),
                     extensions.size()) ||
      !CBB_flush(output_cbb.get())) {
    return false;
  }
  output->append(reinterpret_cast<const char*>(CBB_data(output_cbb.get())),
                 CBB_len(output_cbb.get()));
  return true;
}

bool EncodeTreeHeadSignature(const SignedTreeHead& signed_tree_head,
                             std::string* output) {
  bssl::ScopedCBB output_cbb;
  if (!CBB_init(output_cbb.get(), 64) ||
      !CBB_add_u8(output_cbb.get(), signed_tree_head.version) ||
      !CBB_add_u8(output_cbb.get(), TREE_HASH) ||
      !WriteTimeSinceEpoch(signed_tree_head.timestamp, output_cbb.get()) ||
      !CBB_add_u64(output_cbb.get(), signed_tree_head.tree_size) ||
      !CBB_add_bytes(
          output_cbb.get(),
          reinterpret_cast<const uint8_t*>(signed_tree_head.sha256_root_hash),
          kSthRootHashLength)) {
    return false;
  }
  output->append(reinterpret_cast<const char*>(CBB_data(output_cbb.get())),
                 CBB_len(output_cbb.get()));
  return true;
}

bool DecodeSCTList(std::string_view input,
                   std::vector<std::string_view>* output) {
  std::vector<std::string_view> result;
  CBS input_cbs;
  CBS_init(&input_cbs, reinterpret_cast<const uint8_t*>(input.data()),
           input.size());
  if (!ReadSCTList(&input_cbs, &result) || CBS_len(&input_cbs) != 0 ||
      result.empty()) {
    return false;
  }

  output->swap(result);
  return true;
}

bool DecodeSignedCertificateTimestamp(
    std::string_view* input,
    scoped_refptr<SignedCertificateTimestamp>* output) {
  auto result = base::MakeRefCounted<SignedCertificateTimestamp>();
  uint8_t version;
  CBS input_cbs;
  CBS_init(&input_cbs, reinterpret_cast<const uint8_t*>(input->data()),
           input->size());
  if (!CBS_get_u8(&input_cbs, &version) ||
      version != SignedCertificateTimestamp::V1) {
    return false;
  }

  result->version = SignedCertificateTimestamp::V1;
  CBS log_id;
  CBS extensions;
  if (!CBS_get_bytes(&input_cbs, &log_id, kLogIdLength) ||
      !ReadTimeSinceEpoch(&input_cbs, &result->timestamp) ||
      !CBS_get_u16_length_prefixed(&input_cbs, &extensions) ||
      !DecodeDigitallySigned(&input_cbs, &result->signature)) {
    return false;
  }

  result->log_id.assign(reinterpret_cast<const char*>(CBS_data(&log_id)),
                        CBS_len(&log_id));
  result->extensions.assign(
      reinterpret_cast<const char*>(CBS_data(&extensions)),
      CBS_len(&extensions));
  output->swap(result);
  input->remove_prefix(input->size() - CBS_len(&input_cbs));
  return true;
}

bool EncodeSignedCertificateTimestamp(
    const scoped_refptr<ct::SignedCertificateTimestamp>& input,
    std::string* output) {
  // This function only supports serialization of V1 SCTs.
  DCHECK_EQ(SignedCertificateTimestamp::V1, input->version);
  DCHECK_EQ(kLogIdLength, input->log_id.size());

  bssl::ScopedCBB output_cbb;
  CBB child;
  if (!CBB_init(output_cbb.get(), 64) ||
      !CBB_add_u8(output_cbb.get(), input->version) ||
      !CBB_add_bytes(output_cbb.get(),
                     reinterpret_cast<const uint8_t*>(input->log_id.data()),
                     kLogIdLength) ||
      !WriteTimeSinceEpoch(input->timestamp, output_cbb.get()) ||
      !CBB_add_u16_length_prefixed(output_cbb.get(), &child) ||
      !CBB_add_bytes(&child,
                     reinterpret_cast<const uint8_t*>(input->extensions.data()),
                     input->extensions.size()) ||
      !EncodeDigitallySigned(input->signature, output_cbb.get()) ||
      !CBB_flush(output_cbb.get())) {
    return false;
  }
  output->append(reinterpret_cast<const char*>(CBB_data(output_cbb.get())),
                 CBB_len(output_cbb.get()));
  return true;
}

bool EncodeSCTListForTesting(const std::vector<std::string>& scts,
                             std::string* output) {
  bssl::ScopedCBB output_cbb;
  CBB output_child;
  if (!CBB_init(output_cbb.get(), 64) ||
      !CBB_add_u16_length_prefixed(output_cbb.get(), &output_child)) {
    return false;
  }

  for (const std::string& sct : scts) {
    bssl::ScopedCBB encoded_sct;
    CBB encoded_sct_child;
    if (!CBB_init(encoded_sct.get(), 64) ||
        !CBB_add_u16_length_prefixed(encoded_sct.get(), &encoded_sct_child) ||
        !CBB_add_bytes(&encoded_sct_child,
                       reinterpret_cast<const uint8_t*>(sct.data()),
                       sct.size()) ||
        !CBB_flush(encoded_sct.get()) ||
        !CBB_add_bytes(&output_child, CBB_data(encoded_sct.get()),
                       CBB_len(encoded_sct.get()))) {
      return false;
    }
  }

  if (!CBB_flush(output_cbb.get())) {
    return false;
  }
  output->append(reinterpret_cast<const char*>(CBB_data(output_cbb.get())),
                 CBB_len(output_cbb.get()));
  return true;
}

}  // namespace net::ct
