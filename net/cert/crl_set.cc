// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/crl_set.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "crypto/sha2.h"
#include "net/base/trace_constants.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/mem.h"

namespace net {

namespace {

// CRLSet format:
//
// uint16le header_len
// byte[header_len] header_bytes
// repeated {
//   byte[32] parent_spki_sha256
//   uint32le num_serials
//   [num_serials] {
//     uint8_t serial_length;
//     byte[serial_length] serial;
//   }
//
// header_bytes consists of a JSON dictionary with the following keys:
//   Version (int): currently 0
//   ContentType (string): "CRLSet" or "CRLSetDelta" (magic value)
//   DeltaFrom (int32_t): if this is a delta update (see below), then this
//       contains the sequence number of the base CRLSet.
//   Sequence (int32_t): the monotonic sequence number of this CRL set.
//
// ReadHeader reads the header (including length prefix) from |data| and
// updates |data| to remove the header on return. Caller takes ownership of the
// returned pointer.
base::DictionaryValue* ReadHeader(base::StringPiece* data) {
  uint16_t header_len;
  if (data->size() < sizeof(header_len))
    return nullptr;
  // Assumes little-endian.
  memcpy(&header_len, data->data(), sizeof(header_len));
  data->remove_prefix(sizeof(header_len));

  if (data->size() < header_len)
    return nullptr;

  const base::StringPiece header_bytes(data->data(), header_len);
  data->remove_prefix(header_len);

  std::unique_ptr<base::Value> header =
      base::JSONReader::Read(header_bytes, base::JSON_ALLOW_TRAILING_COMMAS);
  if (header.get() == nullptr)
    return nullptr;

  if (!header->is_dict())
    return nullptr;
  return static_cast<base::DictionaryValue*>(header.release());
}

// kCurrentFileVersion is the version of the CRLSet file format that we
// currently implement.
static const int kCurrentFileVersion = 0;

bool ReadCRL(base::StringPiece* data,
             std::string* out_parent_spki_hash,
             std::vector<std::string>* out_serials) {
  if (data->size() < crypto::kSHA256Length)
    return false;
  out_parent_spki_hash->assign(data->data(), crypto::kSHA256Length);
  data->remove_prefix(crypto::kSHA256Length);

  uint32_t num_serials;
  if (data->size() < sizeof(num_serials))
    return false;
  // Assumes little endian.
  memcpy(&num_serials, data->data(), sizeof(num_serials));
  data->remove_prefix(sizeof(num_serials));

  if (num_serials > 32 * 1024 * 1024)  // Sanity check.
    return false;

  out_serials->reserve(num_serials);

  for (uint32_t i = 0; i < num_serials; ++i) {
    if (data->size() < sizeof(uint8_t))
      return false;

    uint8_t serial_length = data->data()[0];
    data->remove_prefix(sizeof(uint8_t));

    if (data->size() < serial_length)
      return false;

    out_serials->push_back(std::string());
    out_serials->back().assign(data->data(), serial_length);
    data->remove_prefix(serial_length);
  }

  return true;
}

// CopyHashListFromHeader parses a list of base64-encoded, SHA-256 hashes from
// the given |key| in |header_dict| and sets |*out| to the decoded values. It's
// not an error if |key| is not found in |header_dict|.
bool CopyHashListFromHeader(base::DictionaryValue* header_dict,
                            const char* key,
                            std::vector<std::string>* out) {
  base::ListValue* list = nullptr;
  if (!header_dict->GetList(key, &list)) {
    // Hash lists are optional so it's not an error if not present.
    return true;
  }

  out->clear();
  out->reserve(list->GetSize());

  std::string sha256_base64;

  for (size_t i = 0; i < list->GetSize(); ++i) {
    sha256_base64.clear();

    if (!list->GetString(i, &sha256_base64))
      return false;

    out->push_back(std::string());
    if (!base::Base64Decode(sha256_base64, &out->back())) {
      out->pop_back();
      return false;
    }
  }

  return true;
}

// CopyHashToHashesMapFromHeader parse a map from base64-encoded, SHA-256
// hashes to lists of the same, from the given |key| in |header_dict|. It
// copies the map data into |out| (after base64-decoding).
bool CopyHashToHashesMapFromHeader(
    base::DictionaryValue* header_dict,
    const char* key,
    std::unordered_map<std::string, std::vector<std::string>>* out) {
  out->clear();

  base::Value* const dict =
      header_dict->FindKeyOfType(key, base::Value::Type::DICTIONARY);
  if (dict == nullptr) {
    // Maps are optional so it's not an error if not present.
    return true;
  }

  for (const auto& i : dict->DictItems()) {
    if (!i.second.is_list()) {
      return false;
    }

    std::vector<std::string> allowed_spkis;
    for (const auto& j : i.second.GetList()) {
      allowed_spkis.push_back(std::string());
      if (!j.is_string() ||
          !base::Base64Decode(j.GetString(), &allowed_spkis.back())) {
        return false;
      }
    }

    std::string subject_hash;
    if (!base::Base64Decode(i.first, &subject_hash)) {
      return false;
    }

    (*out)[subject_hash] = allowed_spkis;
  }

  return true;
}

}  // namespace

CRLSet::CRLSet()
    : sequence_(0),
      not_after_(0) {
}

CRLSet::~CRLSet() = default;

// static
bool CRLSet::Parse(base::StringPiece data, scoped_refptr<CRLSet>* out_crl_set) {
  TRACE_EVENT0(kNetTracingCategory, "CRLSet::Parse");
// Other parts of Chrome assume that we're little endian, so we don't lose
// anything by doing this.
#if defined(__BYTE_ORDER)
  // Linux check
  static_assert(__BYTE_ORDER == __LITTLE_ENDIAN, "assumes little endian");
#elif defined(__BIG_ENDIAN__)
// Mac check
#error assumes little endian
#endif

  std::unique_ptr<base::DictionaryValue> header_dict(ReadHeader(&data));
  if (!header_dict.get())
    return false;

  std::string contents;
  if (!header_dict->GetString("ContentType", &contents))
    return false;
  if (contents != "CRLSet")
    return false;

  int version;
  if (!header_dict->GetInteger("Version", &version) ||
      version != kCurrentFileVersion) {
    return false;
  }

  int sequence;
  if (!header_dict->GetInteger("Sequence", &sequence))
    return false;

  double not_after;
  if (!header_dict->GetDouble("NotAfter", &not_after)) {
    // NotAfter is optional for now.
    not_after = 0;
  }
  if (not_after < 0)
    return false;

  scoped_refptr<CRLSet> crl_set(new CRLSet());
  crl_set->sequence_ = static_cast<uint32_t>(sequence);
  crl_set->not_after_ = static_cast<uint64_t>(not_after);
  crl_set->crls_.reserve(64);  // Value observed experimentally.

  for (size_t crl_index = 0; !data.empty(); crl_index++) {
    std::string spki_hash;
    std::vector<std::string> blocked_serials;

    if (!ReadCRL(&data, &spki_hash, &blocked_serials)) {
      return false;
    }
    crl_set->crls_[std::move(spki_hash)] = std::move(blocked_serials);
  }

  if (!CopyHashListFromHeader(header_dict.get(), "BlockedSPKIs",
                              &crl_set->blocked_spkis_) ||
      !CopyHashToHashesMapFromHeader(header_dict.get(), "LimitedSubjects",
                                     &crl_set->limited_subjects_)) {
    return false;
  }

  *out_crl_set = std::move(crl_set);
  return true;
}

CRLSet::Result CRLSet::CheckSPKI(const base::StringPiece& spki_hash) const {
  for (auto i = blocked_spkis_.begin(); i != blocked_spkis_.end(); ++i) {
    if (spki_hash.size() == i->size() &&
        memcmp(spki_hash.data(), i->data(), i->size()) == 0) {
      return REVOKED;
    }
  }

  return GOOD;
}

CRLSet::Result CRLSet::CheckSubject(const base::StringPiece& encoded_subject,
                                    const base::StringPiece& spki_hash) const {
  const std::string digest(crypto::SHA256HashString(encoded_subject));
  const auto i = limited_subjects_.find(digest);
  if (i == limited_subjects_.end()) {
    return GOOD;
  }

  for (const auto& j : i->second) {
    if (spki_hash == j) {
      return GOOD;
    }
  }

  return REVOKED;
}

CRLSet::Result CRLSet::CheckSerial(
    const base::StringPiece& serial_number,
    const base::StringPiece& issuer_spki_hash) const {
  base::StringPiece serial(serial_number);

  if (!serial.empty() && (serial[0] & 0x80) != 0) {
    // This serial number is negative but the process which generates CRL sets
    // will reject any certificates with negative serial numbers as invalid.
    return UNKNOWN;
  }

  // Remove any leading zero bytes.
  while (serial.size() > 1 && serial[0] == 0x00)
    serial.remove_prefix(1);

  auto it = crls_.find(issuer_spki_hash.as_string());
  if (it == crls_.end())
    return UNKNOWN;

  for (const auto& issuer_serial : it->second) {
    if (issuer_serial == serial)
      return REVOKED;
  }

  return GOOD;
}

bool CRLSet::IsExpired() const {
  if (not_after_ == 0)
    return false;

  uint64_t now = base::Time::Now().ToTimeT();
  return now > not_after_;
}

uint32_t CRLSet::sequence() const {
  return sequence_;
}

const CRLSet::CRLList& CRLSet::CrlsForTesting() const {
  return crls_;
}

// static
scoped_refptr<CRLSet> CRLSet::EmptyCRLSetForTesting() {
  return ForTesting(false, nullptr, "", "", {});
}

// static
scoped_refptr<CRLSet> CRLSet::ExpiredCRLSetForTesting() {
  return ForTesting(true, nullptr, "", "", {});
}

// static
scoped_refptr<CRLSet> CRLSet::ForTesting(
    bool is_expired,
    const SHA256HashValue* issuer_spki,
    const std::string& serial_number,
    const std::string common_name,
    const std::vector<std::string> acceptable_spki_hashes_for_cn) {
  std::string subject_hash;
  if (!common_name.empty()) {
    CBB cbb, top_level, set, inner_seq, oid, cn;
    uint8_t* x501_data;
    size_t x501_len;
    static const uint8_t kCommonNameOID[] = {0x55, 0x04, 0x03};  // 2.5.4.3

    CBB_zero(&cbb);

    if (!CBB_init(&cbb, 32) ||
        !CBB_add_asn1(&cbb, &top_level, CBS_ASN1_SEQUENCE) ||
        !CBB_add_asn1(&top_level, &set, CBS_ASN1_SET) ||
        !CBB_add_asn1(&set, &inner_seq, CBS_ASN1_SEQUENCE) ||
        !CBB_add_asn1(&inner_seq, &oid, CBS_ASN1_OBJECT) ||
        !CBB_add_bytes(&oid, kCommonNameOID, sizeof(kCommonNameOID)) ||
        !CBB_add_asn1(&inner_seq, &cn, CBS_ASN1_PRINTABLESTRING) ||
        !CBB_add_bytes(&cn,
                       reinterpret_cast<const uint8_t*>(common_name.data()),
                       common_name.size()) ||
        !CBB_finish(&cbb, &x501_data, &x501_len)) {
      CBB_cleanup(&cbb);
      return nullptr;
    }

    subject_hash.assign(crypto::SHA256HashString(
        base::StringPiece(reinterpret_cast<char*>(x501_data), x501_len)));
    OPENSSL_free(x501_data);
  }

  scoped_refptr<CRLSet> crl_set(new CRLSet);
  crl_set->sequence_ = 0;
  if (is_expired)
    crl_set->not_after_ = 1;

  if (issuer_spki) {
    const std::string spki(reinterpret_cast<const char*>(issuer_spki->data),
                           sizeof(issuer_spki->data));
    std::vector<std::string> serials;
    if (!serial_number.empty())
      serials.push_back(serial_number);

    crl_set->crls_.emplace(std::move(spki), std::move(serials));
  }

  if (!subject_hash.empty())
    crl_set->limited_subjects_[subject_hash] = acceptable_spki_hashes_for_cn;

  return crl_set;
}

}  // namespace net
