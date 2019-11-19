// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/record_rdata.h"

#include <algorithm>
#include <numeric>

#include "base/big_endian.h"
#include "base/memory/ptr_util.h"
#include "net/base/ip_address.h"
#include "net/dns/dns_response.h"
#include "net/dns/public/dns_protocol.h"

namespace net {

namespace {

// Helper function for parsing ESNI (TLS 1.3 Encrypted
// Server Name Indication, draft 4) RDATA.
//
// Precondition: |reader| points to the beginning of an
// incoming ESNI-type RecordRdata's data
//
// If the ESNIRecord contains a well-formed
// ESNIKeys field, advances |reader| immediately past the field
// and returns true. Otherwise, returns false.
WARN_UNUSED_RESULT bool AdvancePastEsniKeysField(
    base::BigEndianReader* reader) {
  DCHECK(reader);

  // Skip |esni_keys.version|.
  if (!reader->Skip(2))
    return false;

  // Within esni_keys, skip |public_name|,
  // |keys|, and |cipher_suites|.
  base::StringPiece piece_for_skipping;
  for (int i = 0; i < 3; ++i) {
    if (!reader->ReadU16LengthPrefixed(&piece_for_skipping))
      return false;
  }

  // Skip the |esni_keys.padded_length| field.
  if (!reader->Skip(2))
    return false;

  // Skip the |esni_keys.extensions| field.
  return reader->ReadU16LengthPrefixed(&piece_for_skipping);
}

// Parses a single ESNI address set, appending the addresses to |out|.
WARN_UNUSED_RESULT bool ParseEsniAddressSet(base::StringPiece address_set,
                                            std::vector<IPAddress>* out) {
  DCHECK(out);

  IPAddressBytes address_bytes;
  base::BigEndianReader address_set_reader(address_set.data(),
                                           address_set.size());
  while (address_set_reader.remaining()) {
    // enum AddressType, section 4.2.1 of ESNI draft 4
    // values: 4 (IPv4), 6 (IPv6)
    uint8_t address_type = 0;
    if (!address_set_reader.ReadU8(&address_type))
      return false;

    switch (address_type) {
      case 4: {
        address_bytes.Resize(IPAddress::kIPv4AddressSize);
        break;
      }
      case 6: {
        address_bytes.Resize(IPAddress::kIPv6AddressSize);
        break;
      }
      default:  // invalid address type
        return false;
    }
    if (!address_set_reader.ReadBytes(address_bytes.data(),
                                      address_bytes.size()))
      return false;
    out->emplace_back(address_bytes);
  }
  return true;
}

}  // namespace

static const size_t kSrvRecordMinimumSize = 6;

// Source: https://tools.ietf.org/html/draft-ietf-tls-esni-04, section 4.1
// (This isn't necessarily a tight bound, but it doesn't need to be:
// |HasValidSize| is just used for sanity-check validation.)
// - ESNIKeys field: 8 bytes of length prefixes, 4 bytes of mandatory u16
// fields, >=7 bytes of length-prefixed fields' contents
// - ESNIRecord field = ESNIKeys field + 2 bytes for |dns_extensions|'s
// length prefix
static const size_t kEsniDraft4MinimumSize = 21;

bool RecordRdata::HasValidSize(const base::StringPiece& data, uint16_t type) {
  switch (type) {
    case dns_protocol::kTypeSRV:
      return data.size() >= kSrvRecordMinimumSize;
    case dns_protocol::kTypeA:
      return data.size() == IPAddress::kIPv4AddressSize;
    case dns_protocol::kTypeAAAA:
      return data.size() == IPAddress::kIPv6AddressSize;
    case dns_protocol::kExperimentalTypeEsniDraft4:
      return data.size() >= kEsniDraft4MinimumSize;
    case dns_protocol::kTypeCNAME:
    case dns_protocol::kTypePTR:
    case dns_protocol::kTypeTXT:
    case dns_protocol::kTypeNSEC:
    case dns_protocol::kTypeOPT:
    case dns_protocol::kTypeSOA:
      return true;
    default:
      VLOG(1) << "Unsupported RDATA type.";
      return false;
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

    rdata->texts_.push_back(data.substr(i + 1, length).as_string());

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

OptRecordRdata::Opt::Opt(uint16_t code, base::StringPiece data) : code_(code) {
  data.CopyToString(&data_);
}

bool OptRecordRdata::Opt::operator==(const OptRecordRdata::Opt& other) const {
  return code_ == other.code_ && data_ == other.data_;
}

EsniRecordRdata::EsniRecordRdata() = default;

EsniRecordRdata::~EsniRecordRdata() = default;

// static
std::unique_ptr<EsniRecordRdata> EsniRecordRdata::Create(
    base::StringPiece data,
    const DnsRecordParser& parser) {
  base::BigEndianReader reader(data.data(), data.size());

  // TODO: Once BoringSSL CL 37704 lands, replace
  // this with Boring's SSL_parse_esni_record,
  // which does the same thing.
  if (!AdvancePastEsniKeysField(&reader))
    return nullptr;

  size_t esni_keys_len = reader.ptr() - data.data();

  base::StringPiece dns_extensions;
  if (!reader.ReadU16LengthPrefixed(&dns_extensions) || reader.remaining() > 0)
    return nullptr;

  // Check defensively that we're not about to read OOB.
  CHECK_LT(esni_keys_len, data.size());
  auto rdata = base::WrapUnique(new EsniRecordRdata);
  rdata->esni_keys_ = std::string(data.begin(), esni_keys_len);

  base::BigEndianReader dns_extensions_reader(dns_extensions.data(),
                                              dns_extensions.size());
  if (dns_extensions_reader.remaining() == 0)
    return rdata;

  // ESNI Draft 4 only permits one extension type, address_set,
  // so reject if we see any other extension type.
  uint16_t dns_extension_type = 0;
  if (!dns_extensions_reader.ReadU16(&dns_extension_type) ||
      dns_extension_type != kAddressSetExtensionType)
    return nullptr;

  base::StringPiece address_set;
  if (!dns_extensions_reader.ReadU16LengthPrefixed(&address_set) ||
      !ParseEsniAddressSet(address_set, &rdata->addresses_))
    return nullptr;

  // In TLS, it's forbidden to send the same extension more than once in an
  // extension block; assuming that the same restriction applies here, the
  // record is ill-formed if any bytes follow the first (and only) extension.
  if (dns_extensions_reader.remaining() > 0)
    return nullptr;

  return rdata;
}

uint16_t EsniRecordRdata::Type() const {
  return EsniRecordRdata::kType;
}

bool EsniRecordRdata::IsEqual(const RecordRdata* other) const {
  if (other->Type() != Type())
    return false;
  const EsniRecordRdata* esni_other =
      static_cast<const EsniRecordRdata*>(other);
  return esni_keys_ == esni_other->esni_keys_ &&
         addresses_ == esni_other->addresses_;
}

}  // namespace net
