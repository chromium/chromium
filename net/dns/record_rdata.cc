// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/record_rdata.h"

#include <algorithm>
#include <numeric>
#include <string_view>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/strings/string_view_util.h"
#include "net/base/ip_address.h"
#include "net/dns/dns_response.h"
#include "net/dns/public/dns_protocol.h"

namespace net {

static const size_t kSrvRecordMinimumSize = 6;

// Minimal HTTPS rdata is 2 octets priority + 1 octet empty name.
static constexpr size_t kHttpsRdataMinimumSize = 3;

bool RecordRdata::HasValidSize(base::span<const uint8_t> data, uint16_t type) {
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

std::unique_ptr<SrvRecordRdata> SrvRecordRdata::CreateInstance() {
  return base::WrapUnique(new SrvRecordRdata());
}

// static
std::unique_ptr<SrvRecordRdata> SrvRecordRdata::Create(
    base::span<const uint8_t> data,
    const DnsRecordParser& parser) {
  if (!HasValidSize(data, kType))
    return nullptr;

  auto rdata = SrvRecordRdata::CreateInstance();

  auto reader = base::SpanReader(data);
  // 2 bytes for priority, 2 bytes for weight, 2 bytes for port.
  reader.ReadU16BigEndian(rdata->priority_);
  reader.ReadU16BigEndian(rdata->weight_);
  reader.ReadU16BigEndian(rdata->port_);

  if (!parser.ReadName(data.subspan(kSrvRecordMinimumSize).data(),
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
  return weight_ == srv_other->weight_ && port_ == srv_other->port_ &&
         priority_ == srv_other->priority_ && target_ == srv_other->target_;
}

ARecordRdata::ARecordRdata() = default;

ARecordRdata::~ARecordRdata() = default;

std::unique_ptr<ARecordRdata> ARecordRdata::CreateInstance() {
  return base::WrapUnique(new ARecordRdata());
}

// static
std::unique_ptr<ARecordRdata> ARecordRdata::Create(
    base::span<const uint8_t> data,
    const DnsRecordParser& parser) {
  if (!HasValidSize(data, kType))
    return nullptr;

  auto rdata = ARecordRdata::CreateInstance();
  rdata->address_ = IPAddress(data);
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

std::unique_ptr<AAAARecordRdata> AAAARecordRdata::CreateInstance() {
  return base::WrapUnique(new AAAARecordRdata());
}

// static
std::unique_ptr<AAAARecordRdata> AAAARecordRdata::Create(
    base::span<const uint8_t> data,
    const DnsRecordParser& parser) {
  if (!HasValidSize(data, kType))
    return nullptr;

  auto rdata = AAAARecordRdata::CreateInstance();
  rdata->address_ = IPAddress(base::as_byte_span(data));
  return rdata;
}

uint16_t AAAARecordRdata::Type() const {
  return AAAARecordRdata::kType;
}

bool AAAARecordRdata::IsEqual(const RecordRdata* other) const {
  if (other->Type() != Type()) {
    return false;
  }
  const AAAARecordRdata* a_other = static_cast<const AAAARecordRdata*>(other);
  return address_ == a_other->address_;
}

CnameRecordRdata::CnameRecordRdata() = default;

CnameRecordRdata::~CnameRecordRdata() = default;

std::unique_ptr<CnameRecordRdata> CnameRecordRdata::CreateInstance() {
  return base::WrapUnique(new CnameRecordRdata());
}

// static
std::unique_ptr<CnameRecordRdata> CnameRecordRdata::Create(
    base::span<const uint8_t> data,
    const DnsRecordParser& parser) {
  auto rdata = CnameRecordRdata::CreateInstance();

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

std::unique_ptr<PtrRecordRdata> PtrRecordRdata::CreateInstance() {
  return base::WrapUnique(new PtrRecordRdata());
}

// static
std::unique_ptr<PtrRecordRdata> PtrRecordRdata::Create(
    base::span<const uint8_t> data,
    const DnsRecordParser& parser) {
  auto rdata = PtrRecordRdata::CreateInstance();

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

std::unique_ptr<TxtRecordRdata> TxtRecordRdata::CreateInstance() {
  return base::WrapUnique(new TxtRecordRdata());
}
// static
std::unique_ptr<TxtRecordRdata> TxtRecordRdata::Create(
    base::span<const uint8_t> data,
    const DnsRecordParser& parser) {
  auto rdata = TxtRecordRdata::CreateInstance();

  if (data.empty()) {
    // Per RFC1035-3.3.14, a TXT record must contain at least one string entry.
    return nullptr;
  }

  for (size_t i = 0; i < data.size();) {
    uint8_t length = data[i];

    if (i + length >= data.size()) {
      return nullptr;
    }

    rdata->texts_.emplace_back(
        base::as_string_view(base::as_chars(data.subspan(i + 1, length))));
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

std::unique_ptr<NsecRecordRdata> NsecRecordRdata::CreateInstance() {
  return base::WrapUnique(new NsecRecordRdata());
}

// static
std::unique_ptr<NsecRecordRdata> NsecRecordRdata::Create(
    base::span<const uint8_t> data,
    const DnsRecordParser& parser) {
  auto rdata = NsecRecordRdata::CreateInstance();

  // Read the "next domain". This part for the NSEC record format is
  // ignored for mDNS, since it has no semantic meaning.
  unsigned next_domain_length = parser.ReadName(data.data(), nullptr);

  // If we did not succeed in getting the next domain or the data length
  // is too short for reading the bitmap header, return.
  if (next_domain_length == 0 || data.size() < next_domain_length + 2) {
    return nullptr;
  }

  struct BitmapHeader {
    uint8_t block_number;  // The block number should be zero.
    uint8_t length;        // Bitmap length in bytes. Between 1 and 32.
  };

  const BitmapHeader* header = reinterpret_cast<const BitmapHeader*>(
      UNSAFE_TODO(data.data() + next_domain_length));

  // The block number must be zero in mDns-specific NSEC records. The bitmap
  // length must be between 1 and 32.
  if (header->block_number != 0 || header->length == 0 || header->length > 32)
    return nullptr;

  base::span<const uint8_t> bitmap_data = data.subspan(next_domain_length + 2);

  // Since we may only have one block, the data length must be exactly equal
  // to the domain length plus bitmap size.
  if (bitmap_data.size() != header->length) {
    return nullptr;
  }

  rdata->bitmap_.assign(bitmap_data.begin(), bitmap_data.end());

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
  unsigned byte_num = i / 8;
  if (bitmap_.size() < byte_num + 1)
    return false;

  unsigned bit_num = 7 - i % 8;
  return (bitmap_[byte_num] & (1 << bit_num)) != 0;
}

}  // namespace net
