// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_query.h"

#include <optional>
#include <utility>

#include "base/big_endian.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "base/sys_byteorder.h"
#include "net/base/io_buffer.h"
#include "net/dns/dns_names_util.h"
#include "net/dns/opt_record_rdata.h"
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

std::unique_ptr<OptRecordRdata> AddPaddingIfNecessary(
    const OptRecordRdata* opt_rdata,
    DnsQuery::PaddingStrategy padding_strategy,
    size_t no_opt_buffer_size) {
  // If no input OPT record rdata and no padding, no OPT record rdata needed.
  if (!opt_rdata && padding_strategy == DnsQuery::PaddingStrategy::NONE)
    return nullptr;

  std::unique_ptr<OptRecordRdata> merged_opt_rdata;
  if (opt_rdata) {
    merged_opt_rdata = OptRecordRdata::Create(
        base::StringPiece(opt_rdata->buf().data(), opt_rdata->buf().size()));
  } else {
    merged_opt_rdata = std::make_unique<OptRecordRdata>();
  }
  DCHECK(merged_opt_rdata);

  size_t unpadded_size =
      no_opt_buffer_size + OptRecordSize(merged_opt_rdata.get());
  size_t padding_size = DeterminePaddingSize(unpadded_size, padding_strategy);

  if (padding_size > 0) {
    // |opt_rdata| must not already contain padding if DnsQuery is to add
    // padding.
    DCHECK(!merged_opt_rdata->ContainsOptCode(dns_protocol::kEdnsPadding));
    // OPT header is the minimum amount of padding.
    DCHECK(padding_size >= OptRecordRdata::Opt::kHeaderSize);

    merged_opt_rdata->AddOpt(std::make_unique<OptRecordRdata::PaddingOpt>(
        padding_size - OptRecordRdata::Opt::kHeaderSize));
  }

  return merged_opt_rdata;
}

}  // namespace

// DNS query consists of a 12-byte header followed by a question section.
// For details, see RFC 1035 section 4.1.1.  This header template sets RD
// bit, which directs the name server to pursue query recursively, and sets
// the QDCOUNT to 1, meaning the question section has a single entry.
DnsQuery::DnsQuery(uint16_t id,
                   base::span<const uint8_t> qname,
                   uint16_t qtype,
                   const OptRecordRdata* opt_rdata,
                   PaddingStrategy padding_strategy)
    : qname_size_(qname.size()) {
#if DCHECK_IS_ON()
  std::optional<std::string> dotted_name =
      dns_names_util::NetworkToDottedName(qname);
  DCHECK(dotted_name && !dotted_name.value().empty());
#endif  // DCHECK_IS_ON()

  size_t buffer_size = kHeaderSize + QuestionSize(qname_size_);
  std::unique_ptr<OptRecordRdata> merged_opt_rdata =
      AddPaddingIfNecessary(opt_rdata, padding_strategy, buffer_size);
  if (merged_opt_rdata)
    buffer_size += OptRecordSize(merged_opt_rdata.get());

  io_buffer_ = base::MakeRefCounted<IOBufferWithSize>(buffer_size);

  dns_protocol::Header* header = header_in_io_buffer();
  *header = {};
  header->id = base::HostToNet16(id);
  header->flags = base::HostToNet16(dns_protocol::kFlagRD);
  header->qdcount = base::HostToNet16(1);

  // Write question section after the header.
  base::BigEndianWriter writer(
      base::as_writable_bytes(io_buffer_->span()).subspan(kHeaderSize));
  writer.WriteBytes(qname.data(), qname.size());
  writer.WriteU16(qtype);
  writer.WriteU16(dns_protocol::kClassIN);

  if (merged_opt_rdata) {
    DCHECK_NE(merged_opt_rdata->OptCount(), 0u);

    header->arcount = base::HostToNet16(1);
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
    writer.WriteU16(merged_opt_rdata->buf().size());  // rdata length
    writer.WriteBytes(merged_opt_rdata->buf().data(),
                      merged_opt_rdata->buf().size());
  }
}

DnsQuery::DnsQuery(scoped_refptr<IOBufferWithSize> buffer)
    : io_buffer_(std::move(buffer)) {}

DnsQuery::DnsQuery(const DnsQuery& query) {
  CopyFrom(query);
}

DnsQuery& DnsQuery::operator=(const DnsQuery& query) {
  CopyFrom(query);
  return *this;
}

DnsQuery::~DnsQuery() = default;

std::unique_ptr<DnsQuery> DnsQuery::CloneWithNewId(uint16_t id) const {
  return base::WrapUnique(new DnsQuery(*this, id));
}

bool DnsQuery::Parse(size_t valid_bytes) {
  if (io_buffer_ == nullptr || io_buffer_->span().empty()) {
    return false;
  }
  base::BigEndianReader reader(
      base::as_bytes(io_buffer_->span()).first(valid_bytes));
  dns_protocol::Header header;
  if (!ReadHeader(&reader, &header)) {
    return false;
  }
  if (header.flags & dns_protocol::kFlagResponse) {
    return false;
  }
  if (header.qdcount != 1) {
    VLOG(1) << "Not supporting parsing a DNS query with multiple (or zero) "
               "questions.";
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
  // need to properly initialize |qname_size_|.
  qname_size_ = qname.size();
  return true;
}

uint16_t DnsQuery::id() const {
  return base::NetToHost16(header_in_io_buffer()->id);
}

base::span<const uint8_t> DnsQuery::qname() const {
  return base::as_bytes(io_buffer_->span()).subspan(kHeaderSize, qname_size_);
}

uint16_t DnsQuery::qtype() const {
  return base::numerics::U16FromBigEndian(
      base::as_bytes(io_buffer_->span())
          .subspan(kHeaderSize + qname_size_)
          .first<2u>());
}

base::StringPiece DnsQuery::question() const {
  auto s = io_buffer_->span().subspan(kHeaderSize, QuestionSize(qname_size_));
  return base::StringPiece(s.begin(), s.end());
}

size_t DnsQuery::question_size() const {
  return QuestionSize(qname_size_);
}

void DnsQuery::set_flags(uint16_t flags) {
  header_in_io_buffer()->flags = flags;
}

DnsQuery::DnsQuery(const DnsQuery& orig, uint16_t id) {
  CopyFrom(orig);
  header_in_io_buffer()->id = base::HostToNet16(id);
}

void DnsQuery::CopyFrom(const DnsQuery& orig) {
  qname_size_ = orig.qname_size_;
  io_buffer_ = base::MakeRefCounted<IOBufferWithSize>(orig.io_buffer()->size());
  io_buffer_->span().copy_from(orig.io_buffer()->span());
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
  out->reserve(dns_protocol::kMaxNameLength + 1);
  uint8_t label_length;
  if (!reader->ReadU8(&label_length)) {
    return false;
  }
  while (label_length) {
    if (out->size() + 1 + label_length > dns_protocol::kMaxNameLength) {
      return false;
    }

    out->append(reinterpret_cast<char*>(&label_length), 1);

    base::StringPiece label;
    if (!reader->ReadPiece(&label, label_length)) {
      return false;
    }
    out->append(label);
    if (!reader->ReadU8(&label_length)) {
      return false;
    }
  }
  DCHECK_LE(out->size(), static_cast<size_t>(dns_protocol::kMaxNameLength));
  out->append(1, '\0');
  return true;
}

}  // namespace net
