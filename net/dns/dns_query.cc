// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_query.h"

#include <utility>

#include "base/big_endian.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "base/sys_byteorder.h"
#include "net/base/io_buffer.h"
#include "net/dns/dns_util.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/record_rdata.h"

namespace net {

namespace {

const size_t kHeaderSize = sizeof(dns_protocol::Header);

// Size of the fixed part of an OPT RR:
// https://tools.ietf.org/html/rfc6891#section-6.1.2
static const size_t kOptRRFixedSize = 11;

// https://tools.ietf.org/html/rfc6891#section-6.2.5
// TODO(robpercival): Determine a good value for this programmatically.
const uint16_t kMaxUdpPayloadSize = 4096;

size_t QuestionSize(size_t qname_size) {
  // QNAME + QTYPE + QCLASS
  return qname_size + sizeof(uint16_t) + sizeof(uint16_t);
}

// Buffer size of Opt record for |rdata| (does not include Opt record or RData
// added for padding).
size_t OptRecordSize(const OptRecordRdata* rdata) {
  return rdata == nullptr ? 0 : kOptRRFixedSize + rdata->buf().size();
}

// Padding size includes Opt header for the padding.  Does not include OptRecord
// header (kOptRRFixedSize) even when added just for padding.
size_t DeterminePaddingSize(size_t unpadded_size,
                            DnsQuery::PaddingStrategy padding_strategy) {
  switch (padding_strategy) {
    case DnsQuery::PaddingStrategy::NONE:
      return 0;
    case DnsQuery::PaddingStrategy::BLOCK_LENGTH_128:
      size_t padding_size = OptRecordRdata::Opt::kHeaderSize;
      size_t remainder = (padding_size + unpadded_size) % 128;
      padding_size += (128 - remainder) % 128;
      DCHECK_EQ((unpadded_size + padding_size) % 128, 0u);
      return padding_size;
  }
}

base::Optional<OptRecordRdata> AddPaddingIfNecessary(
    const OptRecordRdata* opt_rdata,
    DnsQuery::PaddingStrategy padding_strategy,
    size_t no_opt_buffer_size) {
  // If no input OPT record rdata and no padding, no OPT record rdata needed.
  if (!opt_rdata && padding_strategy == DnsQuery::PaddingStrategy::NONE)
    return base::nullopt;

  OptRecordRdata merged_opt_rdata;
  if (opt_rdata)
    merged_opt_rdata.AddOpts(*opt_rdata);

  size_t unpadded_size = no_opt_buffer_size + OptRecordSize(&merged_opt_rdata);
  size_t padding_size = DeterminePaddingSize(unpadded_size, padding_strategy);

  if (padding_size > 0) {
    // |opt_rdata| must not already contain padding if DnsQuery is to add
    // padding.
    DCHECK(!merged_opt_rdata.ContainsOptCode(dns_protocol::kEdnsPadding));
    // OPT header is the minimum amount of padding.
    DCHECK(padding_size >= OptRecordRdata::Opt::kHeaderSize);

    merged_opt_rdata.AddOpt(OptRecordRdata::Opt(
        dns_protocol::kEdnsPadding,
        std::string(padding_size - OptRecordRdata::Opt::kHeaderSize, 0)));
  }

  return merged_opt_rdata;
}

}  // namespace

// DNS query consists of a 12-byte header followed by a question section.
// For details, see RFC 1035 section 4.1.1.  This header template sets RD
// bit, which directs the name server to pursue query recursively, and sets
// the QDCOUNT to 1, meaning the question section has a single entry.
DnsQuery::DnsQuery(uint16_t id,
                   const base::StringPiece& qname,
                   uint16_t qtype,
                   const OptRecordRdata* opt_rdata,
                   PaddingStrategy padding_strategy)
    : qname_size_(qname.size()) {
  DCHECK(!DNSDomainToString(qname).empty());

  size_t buffer_size = kHeaderSize + QuestionSize(qname_size_);
  base::Optional<OptRecordRdata> merged_opt_rdata =
      AddPaddingIfNecessary(opt_rdata, padding_strategy, buffer_size);
  if (merged_opt_rdata)
    buffer_size += OptRecordSize(&merged_opt_rdata.value());

  io_buffer_ = base::MakeRefCounted<IOBufferWithSize>(buffer_size);

  header_ = reinterpret_cast<dns_protocol::Header*>(io_buffer_->data());
  *header_ = {};
  header_->id = base::HostToNet16(id);
  header_->flags = base::HostToNet16(dns_protocol::kFlagRD);
  header_->qdcount = base::HostToNet16(1);

  // Write question section after the header.
  base::BigEndianWriter writer(io_buffer_->data() + kHeaderSize,
                               io_buffer_->size() - kHeaderSize);
  writer.WriteBytes(qname.data(), qname.size());
  writer.WriteU16(qtype);
  writer.WriteU16(dns_protocol::kClassIN);

  if (merged_opt_rdata) {
    DCHECK(!merged_opt_rdata.value().opts().empty());

    header_->arcount = base::HostToNet16(1);
    // Write OPT pseudo-resource record.
    writer.WriteU8(0);                       // empty domain name (root domain)
    writer.WriteU16(OptRecordRdata::kType);  // type
    writer.WriteU16(kMaxUdpPayloadSize);     // class
    // ttl (next 3 fields)
    writer.WriteU8(0);  // rcode does not apply to requests
    writer.WriteU8(0);  // version
    // TODO(robpercival): Set "DNSSEC OK" flag if/when DNSSEC is supported:
    // https://tools.ietf.org/html/rfc3225#section-3
    writer.WriteU16(0);  // flags

    // rdata
    writer.WriteU16(merged_opt_rdata.value().buf().size());  // rdata length
    writer.WriteBytes(merged_opt_rdata.value().buf().data(),
                      merged_opt_rdata.value().buf().size());
  }
}

DnsQuery::DnsQuery(scoped_refptr<IOBufferWithSize> buffer)
    : io_buffer_(std::move(buffer)) {}

DnsQuery::~DnsQuery() = default;

std::unique_ptr<DnsQuery> DnsQuery::CloneWithNewId(uint16_t id) const {
  return base::WrapUnique(new DnsQuery(*this, id));
}

bool DnsQuery::Parse(size_t valid_bytes) {
  if (io_buffer_ == nullptr || io_buffer_->data() == nullptr) {
    return false;
  }
  CHECK(valid_bytes <= base::checked_cast<size_t>(io_buffer_->size()));
  // We should only parse the query once if the query is constructed from a raw
  // buffer. If we have constructed the query from data or the query is already
  // parsed after constructed from a raw buffer, |header_| is not null.
  DCHECK(header_ == nullptr);
  base::BigEndianReader reader(io_buffer_->data(), valid_bytes);
  dns_protocol::Header header;
  if (!ReadHeader(&reader, &header)) {
    return false;
  }
  if (header.flags & dns_protocol::kFlagResponse) {
    return false;
  }
  if (header.qdcount > 1) {
    VLOG(1) << "Not supporting parsing a DNS query with multiple questions.";
    return false;
  }
  std::string qname;
  if (!ReadName(&reader, &qname)) {
    return false;
  }
  uint16_t qtype;
  uint16_t qclass;
  if (!reader.ReadU16(&qtype) || !reader.ReadU16(&qclass) ||
      qclass != dns_protocol::kClassIN) {
    return false;
  }
  // |io_buffer_| now contains the raw packet of a valid DNS query, we just
  // need to properly initialize |qname_size_| and |header_|.
  qname_size_ = qname.size();
  header_ = reinterpret_cast<dns_protocol::Header*>(io_buffer_->data());
  return true;
}

uint16_t DnsQuery::id() const {
  return base::NetToHost16(header_->id);
}

base::StringPiece DnsQuery::qname() const {
  return base::StringPiece(io_buffer_->data() + kHeaderSize, qname_size_);
}

uint16_t DnsQuery::qtype() const {
  uint16_t type;
  base::ReadBigEndian<uint16_t>(io_buffer_->data() + kHeaderSize + qname_size_,
                                &type);
  return type;
}

base::StringPiece DnsQuery::question() const {
  return base::StringPiece(io_buffer_->data() + kHeaderSize,
                           QuestionSize(qname_size_));
}

size_t DnsQuery::question_size() const {
  return QuestionSize(qname_size_);
}

void DnsQuery::set_flags(uint16_t flags) {
  header_->flags = flags;
}

DnsQuery::DnsQuery(const DnsQuery& orig, uint16_t id) {
  qname_size_ = orig.qname_size_;
  io_buffer_ = base::MakeRefCounted<IOBufferWithSize>(orig.io_buffer()->size());
  memcpy(io_buffer_.get()->data(), orig.io_buffer()->data(),
         io_buffer_.get()->size());
  header_ = reinterpret_cast<dns_protocol::Header*>(io_buffer_->data());
  header_->id = base::HostToNet16(id);
}

bool DnsQuery::ReadHeader(base::BigEndianReader* reader,
                          dns_protocol::Header* header) {
  return (
      reader->ReadU16(&header->id) && reader->ReadU16(&header->flags) &&
      reader->ReadU16(&header->qdcount) && reader->ReadU16(&header->ancount) &&
      reader->ReadU16(&header->nscount) && reader->ReadU16(&header->arcount));
}

bool DnsQuery::ReadName(base::BigEndianReader* reader, std::string* out) {
  DCHECK(out != nullptr);
  out->clear();
  out->reserve(dns_protocol::kMaxNameLength);
  uint8_t label_length;
  if (!reader->ReadU8(&label_length)) {
    return false;
  }
  out->append(reinterpret_cast<char*>(&label_length), 1);
  while (label_length) {
    base::StringPiece label;
    if (!reader->ReadPiece(&label, label_length)) {
      return false;
    }
    out->append(label.data(), label.size());
    if (!reader->ReadU8(&label_length)) {
      return false;
    }
    out->append(reinterpret_cast<char*>(&label_length), 1);
  }
  return true;
}

}  // namespace net
