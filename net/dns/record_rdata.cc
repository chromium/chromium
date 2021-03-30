// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/record_rdata.h"

#include <algorithm>
#include <numeric>
#include <utility>

#include "base/big_endian.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/strings/string_piece.h"
#include "net/base/ip_address.h"
#include "net/dns/dns_response.h"
#include "net/dns/public/dns_protocol.h"

namespace net {

static const size_t kSrvRecordMinimumSize = 6;

// The simplest INTEGRITY record is a U16-length-prefixed nonce (containing zero
// bytes) followed by its SHA256 digest.
static constexpr size_t kIntegrityMinimumSize =
    sizeof(uint16_t) + IntegrityRecordRdata::kDigestLen;

// Minimal HTTPS rdata is 2 octets priority + 1 octet empty name.
static constexpr size_t kHttpsRdataMinimumSize = 3;

bool RecordRdata::HasValidSize(const base::StringPiece& data, uint16_t type) {
  switch (type) {
    case dns_protocol::kTypeSRV:
      return data.size() >= kSrvRecordMinimumSize;
    case dns_protocol::kTypeA:
      return data.size() == IPAddress::kIPv4AddressSize;
    case dns_protocol::kTypeAAAA:
      return data.size() == IPAddress::kIPv6AddressSize;
    case dns_protocol::kExperimentalTypeIntegrity:
      return data.size() >= kIntegrityMinimumSize;
    case dns_protocol::kTypeHttps:
      return data.size() >= kHttpsRdataMinimumSize;
    case dns_protocol::kTypeCNAME:
    case dns_protocol::kTypePTR:
    case dns_protocol::kTypeTXT:
    case dns_protocol::kTypeNSEC:
    case dns_protocol::kTypeOPT:
    case dns_protocol::kTypeSOA:
      return true;
    default:
      VLOG(1) << "Unrecognized RDATA type.";
      return true;
  }
}

SrvRecordRdata::SrvRecordRdata() : priority_(0), weight_(0), port_(0) {
}

SrvRecordRdata::~SrvRecordRdata() = default;

// static
std::unique_ptr<SrvRecordRdata> SrvRecordRdata::Create(
    const base::StringPiece& data,
    const DnsRecordParser& parser) {
  if (!HasValidSize(data, kType))
    return std::unique_ptr<SrvRecordRdata>();

  std::unique_ptr<SrvRecordRdata> rdata(new SrvRecordRdata);

  base::BigEndianReader reader(data.data(), data.size());
  // 2 bytes for priority, 2 bytes for weight, 2 bytes for port.
  reader.ReadU16(&rdata->priority_);
  reader.ReadU16(&rdata->weight_);
  reader.ReadU16(&rdata->port_);

  if (!parser.ReadName(data.substr(kSrvRecordMinimumSize).begin(),
                       &rdata->target_))
    return std::unique_ptr<SrvRecordRdata>();

  return rdata;
}

uint16_t SrvRecordRdata::Type() const {
  return SrvRecordRdata::kType;
}

bool SrvRecordRdata::IsEqual(const RecordRdata* other) const {
  if (other->Type() != Type()) return false;
  const SrvRecordRdata* srv_other = static_cast<const SrvRecordRdata*>(other);
  return weight_ == srv_other->weight_ &&
      port_ == srv_other->port_ &&
      priority_ == srv_other->priority_ &&
      target_ == srv_other->target_;
}

ARecordRdata::ARecordRdata() = default;

ARecordRdata::~ARecordRdata() = default;

// static
std::unique_ptr<ARecordRdata> ARecordRdata::Create(
    const base::StringPiece& data,
    const DnsRecordParser& parser) {
  if (!HasValidSize(data, kType))
    return std::unique_ptr<ARecordRdata>();

  std::unique_ptr<ARecordRdata> rdata(new ARecordRdata);
  rdata->address_ =
      IPAddress(reinterpret_cast<const uint8_t*>(data.data()), data.length());
  return rdata;
}

uint16_t ARecordRdata::Type() const {
  return ARecordRdata::kType;
}

bool ARecordRdata::IsEqual(const RecordRdata* other) const {
  if (other->Type() != Type()) return false;
  const ARecordRdata* a_other = static_cast<const ARecordRdata*>(other);
  return address_ == a_other->address_;
}

AAAARecordRdata::AAAARecordRdata() = default;

AAAARecordRdata::~AAAARecordRdata() = default;

// static
std::unique_ptr<AAAARecordRdata> AAAARecordRdata::Create(
    const base::StringPiece& data,
    const DnsRecordParser& parser) {
  if (!HasValidSize(data, kType))
    return std::unique_ptr<AAAARecordRdata>();

  std::unique_ptr<AAAARecordRdata> rdata(new AAAARecordRdata);
  rdata->address_ =
      IPAddress(reinterpret_cast<const uint8_t*>(data.data()), data.length());
  return rdata;
}

uint16_t AAAARecordRdata::Type() const {
  return AAAARecordRdata::kType;
}

bool AAAARecordRdata::IsEqual(const RecordRdata* other) const {
  if (other->Type() != Type()) return false;
  const AAAARecordRdata* a_other = static_cast<const AAAARecordRdata*>(other);
  return address_ == a_other->address_;
}

CnameRecordRdata::CnameRecordRdata() = default;

CnameRecordRdata::~CnameRecordRdata() = default;

// static
std::unique_ptr<CnameRecordRdata> CnameRecordRdata::Create(
    const base::StringPiece& data,
    const DnsRecordParser& parser) {
  std::unique_ptr<CnameRecordRdata> rdata(new CnameRecordRdata);

  if (!parser.ReadName(data.begin(), &rdata->cname_))
    return std::unique_ptr<CnameRecordRdata>();

  return rdata;
}

uint16_t CnameRecordRdata::Type() const {
  return CnameRecordRdata::kType;
}

bool CnameRecordRdata::IsEqual(const RecordRdata* other) const {
  if (other->Type() != Type()) return false;
  const CnameRecordRdata* cname_other =
      static_cast<const CnameRecordRdata*>(other);
  return cname_ == cname_other->cname_;
}

PtrRecordRdata::PtrRecordRdata() = default;

PtrRecordRdata::~PtrRecordRdata() = default;

// static
std::unique_ptr<PtrRecordRdata> PtrRecordRdata::Create(
    const base::StringPiece& data,
    const DnsRecordParser& parser) {
  std::unique_ptr<PtrRecordRdata> rdata(new PtrRecordRdata);

  if (!parser.ReadName(data.begin(), &rdata->ptrdomain_))
    return std::unique_ptr<PtrRecordRdata>();

  return rdata;
}

uint16_t PtrRecordRdata::Type() const {
  return PtrRecordRdata::kType;
}

bool PtrRecordRdata::IsEqual(const RecordRdata* other) const {
  if (other->Type() != Type()) return false;
  const PtrRecordRdata* ptr_other = static_cast<const PtrRecordRdata*>(other);
  return ptrdomain_ == ptr_other->ptrdomain_;
}

TxtRecordRdata::TxtRecordRdata() = default;

TxtRecordRdata::~TxtRecordRdata() = default;

// static
std::unique_ptr<TxtRecordRdata> TxtRecordRdata::Create(
    const base::StringPiece& data,
    const DnsRecordParser& parser) {
  std::unique_ptr<TxtRecordRdata> rdata(new TxtRecordRdata);

  for (size_t i = 0; i < data.size(); ) {
    uint8_t length = data[i];

    if (i + length >= data.size())
      return std::unique_ptr<TxtRecordRdata>();

    rdata->texts_.push_back(std::string(data.substr(i + 1, length)));

    // Move to the next string.
    i += length + 1;
  }

  return rdata;
}

uint16_t TxtRecordRdata::Type() const {
  return TxtRecordRdata::kType;
}

bool TxtRecordRdata::IsEqual(const RecordRdata* other) const {
  if (other->Type() != Type()) return false;
  const TxtRecordRdata* txt_other = static_cast<const TxtRecordRdata*>(other);
  return texts_ == txt_other->texts_;
}

NsecRecordRdata::NsecRecordRdata() = default;

NsecRecordRdata::~NsecRecordRdata() = default;

// static
std::unique_ptr<NsecRecordRdata> NsecRecordRdata::Create(
    const base::StringPiece& data,
    const DnsRecordParser& parser) {
  std::unique_ptr<NsecRecordRdata> rdata(new NsecRecordRdata);

  // Read the "next domain". This part for the NSEC record format is
  // ignored for mDNS, since it has no semantic meaning.
  unsigned next_domain_length = parser.ReadName(data.data(), nullptr);

  // If we did not succeed in getting the next domain or the data length
  // is too short for reading the bitmap header, return.
  if (next_domain_length == 0 || data.length() < next_domain_length + 2)
    return std::unique_ptr<NsecRecordRdata>();

  struct BitmapHeader {
    uint8_t block_number;  // The block number should be zero.
    uint8_t length;        // Bitmap length in bytes. Between 1 and 32.
  };

  const BitmapHeader* header = reinterpret_cast<const BitmapHeader*>(
      data.data() + next_domain_length);

  // The block number must be zero in mDns-specific NSEC records. The bitmap
  // length must be between 1 and 32.
  if (header->block_number != 0 || header->length == 0 || header->length > 32)
    return std::unique_ptr<NsecRecordRdata>();

  base::StringPiece bitmap_data = data.substr(next_domain_length + 2);

  // Since we may only have one block, the data length must be exactly equal to
  // the domain length plus bitmap size.
  if (bitmap_data.length() != header->length)
    return std::unique_ptr<NsecRecordRdata>();

  rdata->bitmap_.insert(rdata->bitmap_.begin(),
                        bitmap_data.begin(),
                        bitmap_data.end());

  return rdata;
}

uint16_t NsecRecordRdata::Type() const {
  return NsecRecordRdata::kType;
}

bool NsecRecordRdata::IsEqual(const RecordRdata* other) const {
  if (other->Type() != Type())
    return false;
  const NsecRecordRdata* nsec_other =
      static_cast<const NsecRecordRdata*>(other);
  return bitmap_ == nsec_other->bitmap_;
}

bool NsecRecordRdata::GetBit(unsigned i) const {
  unsigned byte_num = i/8;
  if (bitmap_.size() < byte_num + 1)
    return false;

  unsigned bit_num = 7 - i % 8;
  return (bitmap_[byte_num] & (1 << bit_num)) != 0;
}

OptRecordRdata::OptRecordRdata() = default;

OptRecordRdata::OptRecordRdata(OptRecordRdata&& other) = default;

OptRecordRdata::~OptRecordRdata() = default;

OptRecordRdata& OptRecordRdata::operator=(OptRecordRdata&& other) = default;

// static
std::unique_ptr<OptRecordRdata> OptRecordRdata::Create(
    const base::StringPiece& data,
    const DnsRecordParser& parser) {
  std::unique_ptr<OptRecordRdata> rdata(new OptRecordRdata);
  rdata->buf_.assign(data.begin(), data.end());

  base::BigEndianReader reader(data.data(), data.size());
  while (reader.remaining() > 0) {
    uint16_t opt_code, opt_data_size;
    base::StringPiece opt_data;

    if (!(reader.ReadU16(&opt_code) && reader.ReadU16(&opt_data_size) &&
          reader.ReadPiece(&opt_data, opt_data_size))) {
      return std::unique_ptr<OptRecordRdata>();
    }
    rdata->opts_.push_back(Opt(opt_code, opt_data));
  }

  return rdata;
}

uint16_t OptRecordRdata::Type() const {
  return OptRecordRdata::kType;
}

bool OptRecordRdata::IsEqual(const RecordRdata* other) const {
  if (other->Type() != Type())
    return false;
  const OptRecordRdata* opt_other = static_cast<const OptRecordRdata*>(other);
  return opt_other->opts_ == opts_;
}

void OptRecordRdata::AddOpt(const Opt& opt) {
  base::StringPiece opt_data = opt.data();

  // Resize buffer to accommodate new OPT.
  const size_t orig_rdata_size = buf_.size();
  buf_.resize(orig_rdata_size + Opt::kHeaderSize + opt_data.size());

  // Start writing from the end of the existing rdata.
  base::BigEndianWriter writer(buf_.data() + orig_rdata_size, buf_.size());
  bool success = writer.WriteU16(opt.code()) &&
                 writer.WriteU16(opt_data.size()) &&
                 writer.WriteBytes(opt_data.data(), opt_data.size());
  DCHECK(success);

  opts_.push_back(opt);
}

void OptRecordRdata::AddOpts(const OptRecordRdata& other) {
  buf_.insert(buf_.end(), other.buf_.begin(), other.buf_.end());
  opts_.insert(opts_.end(), other.opts_.begin(), other.opts_.end());
}

bool OptRecordRdata::ContainsOptCode(uint16_t opt_code) const {
  return std::any_of(
      opts_.begin(), opts_.end(),
      [=](const OptRecordRdata::Opt& opt) { return opt.code() == opt_code; });
}

OptRecordRdata::Opt::Opt(uint16_t code, base::StringPiece data)
    : code_(code), data_(data) {}

bool OptRecordRdata::Opt::operator==(const OptRecordRdata::Opt& other) const {
  return code_ == other.code_ && data_ == other.data_;
}

IntegrityRecordRdata::IntegrityRecordRdata(Nonce nonce)
    : nonce_(std::move(nonce)), digest_(Hash(nonce_)), is_intact_(true) {}

IntegrityRecordRdata::IntegrityRecordRdata(Nonce nonce,
                                           Digest digest,
                                           size_t rdata_len)
    : nonce_(std::move(nonce)),
      digest_(digest),
      is_intact_(rdata_len == LengthForSerialization(nonce_) &&
                 Hash(nonce_) == digest_) {}

IntegrityRecordRdata::IntegrityRecordRdata(IntegrityRecordRdata&&) = default;
IntegrityRecordRdata::IntegrityRecordRdata(const IntegrityRecordRdata&) =
    default;
IntegrityRecordRdata::~IntegrityRecordRdata() = default;

bool IntegrityRecordRdata::IsEqual(const RecordRdata* other) const {
  if (other->Type() != Type())
    return false;
  const IntegrityRecordRdata* integrity_other =
      static_cast<const IntegrityRecordRdata*>(other);
  return is_intact_ && integrity_other->is_intact_ &&
         nonce_ == integrity_other->nonce_ &&
         digest_ == integrity_other->digest_;
}

uint16_t IntegrityRecordRdata::Type() const {
  return kType;
}

// static
std::unique_ptr<IntegrityRecordRdata> IntegrityRecordRdata::Create(
    const base::StringPiece& data) {
  base::BigEndianReader reader(data.data(), data.size());
  // Parse a U16-prefixed |Nonce| followed by a |Digest|.
  base::StringPiece parsed_nonce, parsed_digest;

  // Note that even if this parse fails, we still want to create a record.
  bool parse_success = reader.ReadU16LengthPrefixed(&parsed_nonce) &&
                       reader.ReadPiece(&parsed_digest, kDigestLen);

  const std::string kZeroDigest = std::string(kDigestLen, 0);
  if (!parse_success) {
    parsed_nonce = base::StringPiece();
    parsed_digest = base::StringPiece(kZeroDigest);
  }

  Digest digest_copy{};
  CHECK_EQ(parsed_digest.size(), digest_copy.size());
  std::copy_n(parsed_digest.begin(), parsed_digest.size(), digest_copy.begin());

  auto record = base::WrapUnique(
      new IntegrityRecordRdata(Nonce(parsed_nonce.begin(), parsed_nonce.end()),
                               digest_copy, data.size()));

  // A failed parse implies |!IsIntact()|, though the converse is not true. The
  // record may be considered not intact if there were trailing bytes in |data|
  // or if |parsed_digest| is not the hash of |parsed_nonce|.
  if (!parse_success)
    DCHECK(!record->IsIntact());
  return record;
}

// static
IntegrityRecordRdata IntegrityRecordRdata::Random() {
  constexpr uint16_t kMinNonceLen = 32;
  constexpr uint16_t kMaxNonceLen = 512;

  // Construct random nonce.
  const uint16_t nonce_len = base::RandInt(kMinNonceLen, kMaxNonceLen);
  Nonce nonce(nonce_len);
  base::RandBytes(nonce.data(), nonce.size());

  return IntegrityRecordRdata(std::move(nonce));
}

base::Optional<std::vector<uint8_t>> IntegrityRecordRdata::Serialize() const {
  if (!is_intact_) {
    return base::nullopt;
  }

  // Create backing buffer and writer.
  std::vector<uint8_t> serialized(LengthForSerialization(nonce_));
  base::BigEndianWriter writer(reinterpret_cast<char*>(serialized.data()),
                               serialized.size());

  // Writes will only fail if the buffer is too small. We are asserting here
  // that our buffer is exactly the right size, which is expected to always be
  // true if |is_intact_|.
  CHECK(writer.WriteU16(nonce_.size()));
  CHECK(writer.WriteBytes(nonce_.data(), nonce_.size()));
  CHECK(writer.WriteBytes(digest_.data(), digest_.size()));
  CHECK_EQ(writer.remaining(), 0u);

  return serialized;
}

// static
IntegrityRecordRdata::Digest IntegrityRecordRdata::Hash(const Nonce& nonce) {
  Digest digest{};
  SHA256(nonce.data(), nonce.size(), digest.data());
  return digest;
}

// static
size_t IntegrityRecordRdata::LengthForSerialization(const Nonce& nonce) {
  // A serialized INTEGRITY record consists of a U16-prefixed |nonce_|, followed
  // by the bytes of |digest_|.
  return sizeof(uint16_t) + nonce.size() + kDigestLen;
}

}  // namespace net
