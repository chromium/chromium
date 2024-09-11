// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/crl_set.h"

#include <algorithm>
#include <string_view>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/time/time.h"
#include "base/values.h"
#include "crypto/sha2.h"
#include "net/base/trace_constants.h"
#include "net/base/tracing.h"
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
//   ContentType (string): "CRLSet" (magic value)
//   Sequence (int32_t): the monotonic sequence number of this CRL set.
//   NotAfter (optional) (double/int64_t): The number of seconds since the
//     Unix epoch, after which, this CRLSet is expired.
//   BlockedSPKIs (array of string): An array of Base64 encoded, SHA-256 hashed
//     SubjectPublicKeyInfos that should be blocked.
//   LimitedSubjects (object/map of string -> array of string): A map between
//     the Base64-encoded SHA-256 hash of the DER-encoded Subject and the
//     Base64-encoded SHA-256 hashes of the SubjectPublicKeyInfos that are
//     allowed for that subject.
//   KnownInterceptionSPKIs (array of string): An array of Base64-encoded
//     SHA-256 hashed SubjectPublicKeyInfos known to be used for interception.
//   BlockedInterceptionSPKIs (array of string): An array of Base64-encoded
//     SHA-256 hashed SubjectPublicKeyInfos known to be used for interception
//     and that should be actively blocked.
//
// ReadHeader reads the header (including length prefix) from |data| and
// updates |data| to remove the header on return. Caller takes ownership of the
// returned pointer.
std::optional<base::Value> ReadHeader(std::string_view* data) {
  uint16_t header_len;
  if (data->size() < sizeof(header_len)) {
    return std::nullopt;
  }
  // Assumes little-endian.
  memcpy(&header_len, data->data(), sizeof(header_len));
  data->remove_prefix(sizeof(header_len));

  if (data->size() < header_len) {
    return std::nullopt;
  }

  const std::string_view header_bytes = data->substr(0, header_len);
  data->remove_prefix(header_len);

  std::optional<base::Value> header =
      base::JSONReader::Read(header_bytes, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!header || !header->is_dict()) {
    return std::nullopt;
  }

  return header;
}

// kCurrentFileVersion is the version of the CRLSet file format that we
// currently implement.
static const int kCurrentFileVersion = 0;

bool ReadCRL(std::string_view* data,
             std::string* out_parent_spki_hash,
             std::vector<std::string>* out_serials) {
  if (data->size() < crypto::kSHA256Length)
    return false;
  *out_parent_spki_hash = std::string(data->substr(0, crypto::kSHA256Length));
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

    uint8_t serial_length = (*data)[0];
    data->remove_prefix(sizeof(uint8_t));

    if (data->size() < serial_length)
      return false;

    out_serials->push_back(std::string());
    out_serials->back() = std::string(data->substr(0, serial_length));
    data->remove_prefix(serial_length);
  }

  return true;
}

// CopyHashListFromHeader parses a list of base64-encoded, SHA-256 hashes from
// the given |key| (without path expansion) in |header_dict| and sets |*out|
// to the decoded values. It's not an error if |key| is not found in
// |header_dict|.
bool CopyHashListFromHeader(const base::Value::Dict& header_dict,
                            const char* key,
                            std::vector<std::string>* out) {
  const base::Value::List* list = header_dict.FindList(key);
  if (!list) {
    // Hash lists are optional so it's not an error if not present.
    return true;
  }

  out->clear();
  out->reserve(list->size());

  std::string sha256_base64;

  for (const base::Value& i : *list) {
    sha256_base64.clear();

    if (!i.is_string())
      return false;
    sha256_base64 = i.GetString();

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
    const base::Value::Dict& header_dict,
    const char* key,
    std::unordered_map<std::string, std::vector<std::string>>* out) {
  out->clear();

  const base::Value::Dict* dict = header_dict.FindDict(key);
  if (dict == nullptr) {
    // Maps are optional so it's not an error if not present.
    return true;
  }

  for (auto i : *dict) {
    if (!i.second.is_list()) {
      return false;
    }

    std::vector<std::string> allowed_spkis;
    for (const auto& j : i.second.GetList()) {
      allowed_spkis.emplace_back();
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

CRLSet::CRLSet() = default;

CRLSet::~CRLSet() = default;

// static
bool CRLSet::Parse(std::string_view data, scoped_refptr<CRLSet>* out_crl_set) {
  TRACE_EVENT0(NetTracingCategory(), "CRLSet::Parse");
// Other parts of Chrome assume that we're little endian, so we don't lose
// anything by doing this.
#if defined(__BYTE_ORDER)
  // Linux check
  static_assert(__BYTE_ORDER == __LITTLE_ENDIAN, "assumes little endian");
#elif defined(__BIG_ENDIAN__)
// Mac check
#error assumes little endian
#endif

  std::optional<base::Value> header_value = ReadHeader(&data);
  if (!header_value) {
    return false;
  }

  const base::Value::Dict& header_dict = header_value->GetDict();

  const std::string* contents = header_dict.FindString("ContentType");
  if (!contents || (*contents != "CRLSet"))
    return false;

  if (header_dict.FindInt("Version") != kCurrentFileVersion)
    return false;

  std::optional<int> sequence = header_dict.FindInt("Sequence");
  if (!sequence)
    return false;

  // NotAfter is optional for now.
  double not_after = header_dict.FindDouble("NotAfter").value_or(0);
  if (not_after < 0)
    return false;

  auto crl_set = base::WrapRefCounted(new CRLSet());
  crl_set->sequence_ = static_cast<uint32_t>(*sequence);
  crl_set->not_after_ = static_cast<uint64_t>(not_after);
  crl_set->crls_.reserve(64);  // Value observed experimentally.

  while (!data.empty()) {
    std::string spki_hash;
    std::vector<std::string> blocked_serials;

    if (!ReadCRL(&data, &spki_hash, &blocked_serials)) {
      return false;
    }
    crl_set->crls_[std::move(spki_hash)] = std::move(blocked_serials);
  }

  std::vector<std::string> blocked_interception_spkis;
  if (!CopyHashListFromHeader(header_dict, "BlockedSPKIs",
                              &crl_set->blocked_spkis_) ||
      !CopyHashToHashesMapFromHeader(header_dict, "LimitedSubjects",
                                     &crl_set->limited_subjects_) ||
      !CopyHashListFromHeader(header_dict, "KnownInterceptionSPKIs",
                              &crl_set->known_interception_spkis_) ||
      !CopyHashListFromHeader(header_dict, "BlockedInterceptionSPKIs",
                              &blocked_interception_spkis)) {
    return false;
  }

  // Add the BlockedInterceptionSPKIs to both lists; these are provided as
  // a separate list to allow less data to be sent over the wire, even though
  // they are duplicated in-memory.
  crl_set->blocked_spkis_.insert(crl_set->blocked_spkis_.end(),
                                 blocked_interception_spkis.begin(),
                                 blocked_interception_spkis.end());
  crl_set->known_interception_spkis_.insert(
      crl_set->known_interception_spkis_.end(),
      blocked_interception_spkis.begin(), blocked_interception_spkis.end());

  // Defines kSPKIBlockList and kKnownInterceptionList
#include "net/cert/cert_verify_proc_blocklist.inc"
  for (const auto& hash : kSPKIBlockList) {
    crl_set->blocked_spkis_.emplace_back(reinterpret_cast<const char*>(hash),
                                         crypto::kSHA256Length);
  }

  for (const auto& hash : kKnownInterceptionList) {
    crl_set->known_interception_spkis_.emplace_back(
        reinterpret_cast<const char*>(hash), crypto::kSHA256Length);
  }

  // Sort, as these will be std::binary_search()'d.
  std::sort(crl_set->blocked_spkis_.begin(), crl_set->blocked_spkis_.end());
  std::sort(crl_set->known_interception_spkis_.begin(),
            crl_set->known_interception_spkis_.end());

  *out_crl_set = std::move(crl_set);
  return true;
}

CRLSet::Result CRLSet::CheckSPKI(std::string_view spki_hash) const {
  if (std::binary_search(blocked_spkis_.begin(), blocked_spkis_.end(),
                         spki_hash))
    return REVOKED;
  return GOOD;
}

CRLSet::Result CRLSet::CheckSubject(std::string_view encoded_subject,
                                    std::string_view spki_hash) const {
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

CRLSet::Result CRLSet::CheckSerial(std::string_view serial_number,
                                   std::string_view issuer_spki_hash) const {
  std::string_view serial(serial_number);

  if (!serial.empty() && (serial[0] & 0x80) != 0) {
    // This serial number is negative but the process which generates CRL sets
    // will reject any certificates with negative serial numbers as invalid.
    return UNKNOWN;
  }

  // Remove any leading zero bytes.
  while (serial.size() > 1 && serial[0] == 0x00)
    serial.remove_prefix(1);

  auto it = crls_.find(std::string(issuer_spki_hash));
  if (it == crls_.end())
    return UNKNOWN;

  for (const auto& issuer_serial : it->second) {
    if (issuer_serial == serial)
      return REVOKED;
  }

  return GOOD;
}

bool CRLSet::IsKnownInterceptionKey(std::string_view spki_hash) const {
  return std::binary_search(known_interception_spkis_.begin(),
                            known_interception_spkis_.end(), spki_hash);
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
scoped_refptr<CRLSet> CRLSet::BuiltinCRLSet() {
  constexpr char kCRLSet[] =
      "\x31\x00{\"ContentType\":\"CRLSet\",\"Sequence\":0,\"Version\":0}";
  scoped_refptr<CRLSet> ret;
  bool parsed = CRLSet::Parse({kCRLSet, sizeof(kCRLSet) - 1}, &ret);
  DCHECK(parsed);
  return ret;
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
    std::string_view serial_number,
    std::string_view utf8_common_name,
    const std::vector<std::string>& acceptable_spki_hashes_for_cn) {
  std::string subject_hash;
  if (!utf8_common_name.empty()) {
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
        !CBB_add_asn1(&inner_seq, &cn, CBS_ASN1_UTF8STRING) ||
        !CBB_add_bytes(
            &cn, reinterpret_cast<const uint8_t*>(utf8_common_name.data()),
            utf8_common_name.size()) ||
        !CBB_finish(&cbb, &x501_data, &x501_len)) {
      CBB_cleanup(&cbb);
      return nullptr;
    }

    subject_hash.assign(crypto::SHA256HashString(
        std::string_view(reinterpret_cast<char*>(x501_data), x501_len)));
    OPENSSL_free(x501_data);
  }

  auto crl_set = base::WrapRefCounted(new CRLSet());
  crl_set->sequence_ = 0;
  if (is_expired)
    crl_set->not_after_ = 1;

  if (issuer_spki) {
    std::string spki(reinterpret_cast<const char*>(issuer_spki->data),
                     sizeof(issuer_spki->data));
    std::vector<std::string> serials;
    if (!serial_number.empty()) {
      serials.push_back(std::string(serial_number));
      // |serial_number| is in DER-encoded form, which means it may have a
      // leading 0x00 to indicate it is a positive INTEGER. CRLSets are stored
      // without these leading 0x00, as handled in CheckSerial(), so remove
      // that here. As DER-encoding means that any sequences of leading zeroes
      // should be omitted, except to indicate sign, there should only ever
      // be one, and the next byte should have the high bit set.
      DCHECK_EQ(serials[0][0] & 0x80, 0);  // Negative serials are not allowed.
      if (serials[0][0] == 0x00) {
        serials[0].erase(0, 1);
        // If there was a leading 0x00, then the high-bit of the next byte
        // should have been set.
        DCHECK(!serials[0].empty() && serials[0][0] & 0x80);
      }
    }

    crl_set->crls_.emplace(std::move(spki), std::move(serials));
  }

  if (!subject_hash.empty())
    crl_set->limited_subjects_[subject_hash] = acceptable_spki_hashes_for_cn;

  return crl_set;
}

}  // namespace net
