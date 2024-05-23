// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/dns/record_rdata.h"

#include <algorithm>
#include <numeric>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "net/base/ip_address.h"
#include "net/dns/dns_response.h"
#include "net/dns/public/dns_protocol.h"

namespace net {

static const size_t kSrvRecordMinimumSize = 6;

// Minimal HTTPS rdata is 2 octets priority + 1 octet empty name.
static constexpr size_t kHttpsRdataMinimumSize = 3;

bool RecordRdata::HasValidSize(std::string_view data, uint16_t type) {
  switch (type) {
    case dns_protocol::kTypeSRV:
      return data.size() >= kSrvRecordMinimumSize;
    case dns_protocol::kTypeA:
      return data.size() == IPAddress::kIPv4AddressSize;
    case dns_protocol::kTypeAAAA:
      return data.size() == IPAddress::kIPv6AddressSize;
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

SrvRecordRdata::SrvRecordRdata() = default;

SrvRecordRdata::~SrvRecordRdata() = default;

// static
std::unique_ptr<SrvRecordRdata> SrvRecordRdata::Create(
    std::string_view data,
    const DnsRecordParser& parser) {
  if (!HasValidSize(data, kType))
    return nullptr;

  auto rdata = base::WrapUnique(new SrvRecordRdata());

  auto reader = base::SpanReader(base::as_byte_span(data));
  // 2 bytes for priority, 2 bytes for weight, 2 bytes for port.
  reader.ReadU16BigEndian(rdata->priority_);
  reader.ReadU16BigEndian(rdata->weight_);
  reader.ReadU16BigEndian(rdata->port_);

  if (!parser.ReadName(data.substr(kSrvRecordMinimumSize).data(),
                       &rdata->target_)) {
    return nullptr;
  }

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
    std::string_view data,
    const DnsRecordParser& parser) {
  if (!HasValidSize(data, kType))
    return nullptr;

  auto rdata = base::WrapUnique(new ARecordRdata());
  rdata->address_ = IPAddress(base::as_byte_span(data));
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
    std::string_view data,
    const DnsRecordParser& parser) {
  if (!HasValidSize(data, kType))
    return nullptr;

  auto rdata = base::WrapUnique(new AAAARecordRdata());
  rdata->address_ = IPAddress(base::as_byte_span(data));
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
    std::string_view data,
    const DnsRecordParser& parser) {
  auto rdata = base::WrapUnique(new CnameRecordRdata());

  if (!parser.ReadName(data.data(), &rdata->cname_)) {
    return nullptr;
  }

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
    std::string_view data,
    const DnsRecordParser& parser) {
  auto rdata = base::WrapUnique(new PtrRecordRdata());

  if (!parser.ReadName(data.data(), &rdata->ptrdomain_)) {
    return nullptr;
  }

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
    std::string_view data,
    const DnsRecordParser& parser) {
  auto rdata = base::WrapUnique(new TxtRecordRdata());

  for (size_t i = 0; i < data.size(); ) {
    uint8_t length = data[i];

    if (i + length >= data.size())
      return nullptr;

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
    std::string_view data,
    const DnsRecordParser& parser) {
  auto rdata = base::WrapUnique(new NsecRecordRdata());

  // Read the "next domain". This part for the NSEC record format is
  // ignored for mDNS, since it has no semantic meaning.
  unsigned next_domain_length = parser.ReadName(data.data(), nullptr);

  // If we did not succeed in getting the next domain or the data length
  // is too short for reading the bitmap header, return.
  if (next_domain_length == 0 || data.length() < next_domain_length + 2)
    return nullptr;

  struct BitmapHeader {
    uint8_t block_number;  // The block number should be zero.
    uint8_t length;        // Bitmap length in bytes. Between 1 and 32.
  };

  const BitmapHeader* header = reinterpret_cast<const BitmapHeader*>(
      data.data() + next_domain_length);

  // The block number must be zero in mDns-specific NSEC records. The bitmap
  // length must be between 1 and 32.
  if (header->block_number != 0 || header->length == 0 || header->length > 32)
    return nullptr;

  std::string_view bitmap_data = data.substr(next_domain_length + 2);

  // Since we may only have one block, the data length must be exactly equal to
  // the domain length plus bitmap size.
  if (bitmap_data.length() != header->length)
    return nullptr;

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

}  // namespace net
