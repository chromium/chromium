// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_response.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <numeric>
#include <utility>
#include <vector>

#include "base/big_endian.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/sys_byteorder.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_names_util.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_response_result_extractor.h"
#include "net/dns/dns_util.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/record_rdata.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

namespace {

const size_t kHeaderSize = sizeof(dns_protocol::Header);

const uint8_t kRcodeMask = 0xf;

}  // namespace

DnsResourceRecord::DnsResourceRecord() = default;

DnsResourceRecord::DnsResourceRecord(const DnsResourceRecord& other)
    : name(other.name),
      type(other.type),
      klass(other.klass),
      ttl(other.ttl),
      owned_rdata(other.owned_rdata) {
  if (!owned_rdata.empty())
    rdata = owned_rdata;
  else
    rdata = other.rdata;
}

DnsResourceRecord::DnsResourceRecord(DnsResourceRecord&& other)
    : name(std::move(other.name)),
      type(other.type),
      klass(other.klass),
      ttl(other.ttl),
      owned_rdata(std::move(other.owned_rdata)) {
  if (!owned_rdata.empty())
    rdata = owned_rdata;
  else
    rdata = other.rdata;
}

DnsResourceRecord::~DnsResourceRecord() = default;

DnsResourceRecord& DnsResourceRecord::operator=(
    const DnsResourceRecord& other) {
  name = other.name;
  type = other.type;
  klass = other.klass;
  ttl = other.ttl;
  owned_rdata = other.owned_rdata;

  if (!owned_rdata.empty())
    rdata = owned_rdata;
  else
    rdata = other.rdata;

  return *this;
}

DnsResourceRecord& DnsResourceRecord::operator=(DnsResourceRecord&& other) {
  name = std::move(other.name);
  type = other.type;
  klass = other.klass;
  ttl = other.ttl;
  owned_rdata = std::move(other.owned_rdata);

  if (!owned_rdata.empty())
    rdata = owned_rdata;
  else
    rdata = other.rdata;

  return *this;
}

void DnsResourceRecord::SetOwnedRdata(std::string value) {
  DCHECK(!value.empty());
  owned_rdata = std::move(value);
  rdata = owned_rdata;
  DCHECK_EQ(owned_rdata.data(), rdata.data());
}

size_t DnsResourceRecord::CalculateRecordSize() const {
  bool has_final_dot = name.back() == '.';
  // Depending on if |name| in the dotted format has the final dot for the root
  // domain or not, the corresponding wire data in the DNS domain name format is
  // 1 byte (with dot) or 2 bytes larger in size. See RFC 1035, Section 3.1 and
  // DNSDomainFromDot.
  return name.size() + (has_final_dot ? 1 : 2) +
         net::dns_protocol::kResourceRecordSizeInBytesWithoutNameAndRData +
         (owned_rdata.empty() ? rdata.size() : owned_rdata.size());
}

DnsRecordParser::DnsRecordParser() = default;

DnsRecordParser::DnsRecordParser(const void* packet,
                                 size_t length,
                                 size_t offset,
                                 size_t num_records)
    : packet_(reinterpret_cast<const char*>(packet)),
      length_(length),
      num_records_(num_records),
      cur_(packet_ + offset) {
  DCHECK_LE(offset, length);
}

unsigned DnsRecordParser::ReadName(const void* const vpos,
                                   std::string* out) const {
  static const char kAbortMsg[] = "Abort parsing of noncompliant DNS record.";

  const char* const pos = reinterpret_cast<const char*>(vpos);
  DCHECK(packet_);
  DCHECK_LE(packet_, pos);
  DCHECK_LE(pos, packet_ + length_);

  const char* p = pos;
  const char* end = packet_ + length_;
  // Count number of seen bytes to detect loops.
  unsigned seen = 0;
  // Remember how many bytes were consumed before first jump.
  unsigned consumed = 0;
  // The length of the encoded name (sum of label octets and label lengths).
  // For context, RFC 1034 states that the total number of octets representing a
  // domain name (the sum of all label octets and label lengths) is limited to
  // 255. RFC 1035 introduces message compression as a way to reduce packet size
  // on the wire, not to increase the maximum domain name length.
  unsigned encoded_name_len = 0;

  if (pos >= end)
    return 0;

  if (out) {
    out->clear();
    out->reserve(dns_protocol::kMaxCharNameLength);
  }

  for (;;) {
    // The first two bits of the length give the type of the length. It's
    // either a direct length or a pointer to the remainder of the name.
    switch (*p & dns_protocol::kLabelMask) {
      case dns_protocol::kLabelPointer: {
        if (p + sizeof(uint16_t) > end) {
          VLOG(1) << kAbortMsg << " Truncated or missing label pointer.";
          return 0;
        }
        if (consumed == 0) {
          consumed = p - pos + sizeof(uint16_t);
          if (!out)
            return consumed;  // If name is not stored, that's all we need.
        }
        seen += sizeof(uint16_t);
        // If seen the whole packet, then we must be in a loop.
        if (seen > length_) {
          VLOG(1) << kAbortMsg << " Detected loop in label pointers.";
          return 0;
        }
        uint16_t offset;
        base::ReadBigEndian(reinterpret_cast<const uint8_t*>(p), &offset);
        offset &= dns_protocol::kOffsetMask;
        p = packet_ + offset;
        if (p >= end) {
          VLOG(1) << kAbortMsg << " Label pointer points outside packet.";
          return 0;
        }
        break;
      }
      case dns_protocol::kLabelDirect: {
        uint8_t label_len = *p;
        ++p;
        // Note: root domain (".") is NOT included.
        if (label_len == 0) {
          if (consumed == 0) {
            consumed = p - pos;
          }  // else we set |consumed| before first jump
          return consumed;
        }
        // Add one octet for the length and |label_len| for the number of
        // following octets.
        encoded_name_len += 1 + label_len;
        if (encoded_name_len > dns_protocol::kMaxNameLength) {
          VLOG(1) << kAbortMsg << " Name is too long.";
          return 0;
        }
        if (p + label_len >= end) {
          VLOG(1) << kAbortMsg << " Truncated or missing label.";
          return 0;  // Truncated or missing label.
        }
        if (out) {
          if (!out->empty())
            out->append(".");
          out->append(p, label_len);
          DCHECK_LE(out->size(), dns_protocol::kMaxCharNameLength);
        }
        p += label_len;
        seen += 1 + label_len;
        break;
      }
      default:
        // unhandled label type
        VLOG(1) << kAbortMsg << " Unhandled label type.";
        return 0;
    }
  }
}

bool DnsRecordParser::ReadRecord(DnsResourceRecord* out) {
  DCHECK(packet_);

  // Disallow parsing any more than the claimed number of records.
  if (num_records_parsed_ >= num_records_)
    return false;

  size_t consumed = ReadName(cur_, &out->name);
  if (!consumed)
    return false;
  base::BigEndianReader reader(
      reinterpret_cast<const uint8_t*>(cur_ + consumed),
      packet_ + length_ - (cur_ + consumed));
  uint16_t rdlen;
  if (reader.ReadU16(&out->type) &&
      reader.ReadU16(&out->klass) &&
      reader.ReadU32(&out->ttl) &&
      reader.ReadU16(&rdlen) &&
      reader.ReadPiece(&out->rdata, rdlen)) {
    cur_ = reinterpret_cast<const char*>(reader.ptr());
    ++num_records_parsed_;
    return true;
  }
  return false;
}

bool DnsRecordParser::ReadQuestion(std::string& out_dotted_qname,
                                   uint16_t& out_qtype) {
  size_t consumed = ReadName(cur_, &out_dotted_qname);
  if (!consumed)
    return false;

  const char* next = cur_ + consumed + 2 * sizeof(uint16_t);  // QTYPE + QCLASS
  if (next > packet_ + length_)
    return false;

  base::ReadBigEndian(reinterpret_cast<const uint8_t*>(cur_ + consumed),
                      &out_qtype);

  cur_ = next;

  return true;
}

DnsResponse::DnsResponse(
    uint16_t id,
    bool is_authoritative,
    const std::vector<DnsResourceRecord>& answers,
    const std::vector<DnsResourceRecord>& authority_records,
    const std::vector<DnsResourceRecord>& additional_records,
    const absl::optional<DnsQuery>& query,
    uint8_t rcode,
    bool validate_records,
    bool validate_names_as_internet_hostnames) {
  bool has_query = query.has_value();
  dns_protocol::Header header;
  header.id = id;
  bool success = true;
  if (has_query) {
    success &= (id == query.value().id());
    DCHECK(success);
    // DnsQuery only supports a single question.
    header.qdcount = 1;
  }
  header.flags |= dns_protocol::kFlagResponse;
  if (is_authoritative)
    header.flags |= dns_protocol::kFlagAA;
  DCHECK_EQ(0, rcode & ~kRcodeMask);
  header.flags |= rcode;

  header.ancount = answers.size();
  header.nscount = authority_records.size();
  header.arcount = additional_records.size();

  // Response starts with the header and the question section (if any).
  size_t response_size = has_query
                             ? sizeof(header) + query.value().question_size()
                             : sizeof(header);
  // Add the size of all answers and additional records.
  auto do_accumulation = [](size_t cur_size, const DnsResourceRecord& record) {
    return cur_size + record.CalculateRecordSize();
  };
  response_size = std::accumulate(answers.begin(), answers.end(), response_size,
                                  do_accumulation);
  response_size =
      std::accumulate(authority_records.begin(), authority_records.end(),
                      response_size, do_accumulation);
  response_size =
      std::accumulate(additional_records.begin(), additional_records.end(),
                      response_size, do_accumulation);

  auto io_buffer = base::MakeRefCounted<IOBuffer>(response_size);
  base::BigEndianWriter writer(io_buffer->data(), response_size);
  success &= WriteHeader(&writer, header);
  DCHECK(success);
  if (has_query) {
    success &= WriteQuestion(&writer, query.value());
    DCHECK(success);
  }
  // Start the Answer section.
  for (const auto& answer : answers) {
    success &= WriteAnswer(&writer, answer, query, validate_records,
                           validate_names_as_internet_hostnames);
    DCHECK(success);
  }
  // Start the Authority section.
  for (const auto& record : authority_records) {
    success &= WriteRecord(&writer, record, validate_records,
                           validate_names_as_internet_hostnames);
    DCHECK(success);
  }
  // Start the Additional section.
  for (const auto& record : additional_records) {
    success &= WriteRecord(&writer, record, validate_records,
                           validate_names_as_internet_hostnames);
    DCHECK(success);
  }
  if (!success) {
    return;
  }
  io_buffer_ = io_buffer;
  io_buffer_size_ = response_size;
  // Ensure we don't have any remaining uninitialized bytes in the buffer.
  DCHECK(!writer.remaining());
  memset(writer.ptr(), 0, writer.remaining());
  if (has_query)
    InitParse(io_buffer_size_, query.value());
  else
    InitParseWithoutQuery(io_buffer_size_);
}

DnsResponse::DnsResponse()
    : io_buffer_(base::MakeRefCounted<IOBuffer>(dns_protocol::kMaxUDPSize + 1)),
      io_buffer_size_(dns_protocol::kMaxUDPSize + 1) {}

DnsResponse::DnsResponse(scoped_refptr<IOBuffer> buffer, size_t size)
    : io_buffer_(std::move(buffer)), io_buffer_size_(size) {}

DnsResponse::DnsResponse(size_t length)
    : io_buffer_(base::MakeRefCounted<IOBuffer>(length)),
      io_buffer_size_(length) {}

DnsResponse::DnsResponse(const void* data, size_t length, size_t answer_offset)
    : io_buffer_(base::MakeRefCounted<IOBufferWithSize>(length)),
      io_buffer_size_(length),
      parser_(io_buffer_->data(),
              length,
              answer_offset,
              std::numeric_limits<size_t>::max()) {
  DCHECK(data);
  memcpy(io_buffer_->data(), data, length);
}

// static
DnsResponse DnsResponse::CreateEmptyNoDataResponse(
    uint16_t id,
    bool is_authoritative,
    base::span<const uint8_t> qname,
    uint16_t qtype) {
  return DnsResponse(id, is_authoritative,
                     /*answers=*/{},
                     /*authority_records=*/{},
                     /*additional_records=*/{}, DnsQuery(id, qname, qtype));
}

DnsResponse::DnsResponse(DnsResponse&& other) = default;
DnsResponse& DnsResponse::operator=(DnsResponse&& other) = default;

DnsResponse::~DnsResponse() = default;

bool DnsResponse::InitParse(size_t nbytes, const DnsQuery& query) {
  const base::StringPiece question = query.question();

  // Response includes question, it should be at least that size.
  if (nbytes < kHeaderSize + question.size() || nbytes > io_buffer_size_) {
    return false;
  }

  // At this point, it has been validated that the response is at least large
  // enough to read the ID field.
  id_available_ = true;

  // Match the query id.
  DCHECK(id());
  if (id().value() != query.id())
    return false;

  // Not a response?
  if ((base::NetToHost16(header()->flags) & dns_protocol::kFlagResponse) == 0)
    return false;

  // Match question count.
  if (base::NetToHost16(header()->qdcount) != 1)
    return false;

  // Match the question section.
  if (question !=
      base::StringPiece(io_buffer_->data() + kHeaderSize, question.size())) {
    return false;
  }

  absl::optional<std::string> dotted_qname =
      dns_names_util::NetworkToDottedName(query.qname());
  if (!dotted_qname.has_value())
    return false;
  dotted_qnames_.push_back(std::move(dotted_qname).value());
  qtypes_.push_back(query.qtype());

  size_t num_records = base::NetToHost16(header()->ancount) +
                       base::NetToHost16(header()->nscount) +
                       base::NetToHost16(header()->arcount);

  // Construct the parser. Only allow parsing up to `num_records` records. If
  // more records are present in the buffer, it's just garbage extra data after
  // the formal end of the response and should be ignored.
  parser_ = DnsRecordParser(io_buffer_->data(), nbytes,
                            kHeaderSize + question.size(), num_records);
  return true;
}

bool DnsResponse::InitParseWithoutQuery(size_t nbytes) {
  if (nbytes < kHeaderSize || nbytes > io_buffer_size_) {
    return false;
  }
  id_available_ = true;

  // Not a response?
  if ((base::NetToHost16(header()->flags) & dns_protocol::kFlagResponse) == 0)
    return false;

  size_t num_records = base::NetToHost16(header()->ancount) +
                       base::NetToHost16(header()->nscount) +
                       base::NetToHost16(header()->arcount);
  // Only allow parsing up to `num_records` records. If more records are present
  // in the buffer, it's just garbage extra data after the formal end of the
  // response and should be ignored.
  parser_ =
      DnsRecordParser(io_buffer_->data(), nbytes, kHeaderSize, num_records);

  unsigned qdcount = base::NetToHost16(header()->qdcount);
  for (unsigned i = 0; i < qdcount; ++i) {
    std::string dotted_qname;
    uint16_t qtype;
    if (!parser_.ReadQuestion(dotted_qname, qtype)) {
      parser_ = DnsRecordParser();  // Make parser invalid again.
      return false;
    }
    dotted_qnames_.push_back(std::move(dotted_qname));
    qtypes_.push_back(qtype);
  }

  return true;
}

absl::optional<uint16_t> DnsResponse::id() const {
  if (!id_available_)
    return absl::nullopt;

  return base::NetToHost16(header()->id);
}

bool DnsResponse::IsValid() const {
  return parser_.IsValid();
}

uint16_t DnsResponse::flags() const {
  DCHECK(parser_.IsValid());
  return base::NetToHost16(header()->flags) & ~(kRcodeMask);
}

uint8_t DnsResponse::rcode() const {
  DCHECK(parser_.IsValid());
  return base::NetToHost16(header()->flags) & kRcodeMask;
}

unsigned DnsResponse::question_count() const {
  DCHECK(parser_.IsValid());
  return base::NetToHost16(header()->qdcount);
}

unsigned DnsResponse::answer_count() const {
  DCHECK(parser_.IsValid());
  return base::NetToHost16(header()->ancount);
}

unsigned DnsResponse::authority_count() const {
  DCHECK(parser_.IsValid());
  return base::NetToHost16(header()->nscount);
}

unsigned DnsResponse::additional_answer_count() const {
  DCHECK(parser_.IsValid());
  return base::NetToHost16(header()->arcount);
}

uint16_t DnsResponse::GetSingleQType() const {
  DCHECK_EQ(qtypes().size(), 1u);
  return qtypes().front();
}

base::StringPiece DnsResponse::GetSingleDottedName() const {
  DCHECK_EQ(dotted_qnames().size(), 1u);
  return dotted_qnames().front();
}

DnsRecordParser DnsResponse::Parser() const {
  DCHECK(parser_.IsValid());
  // Return a copy of the parser.
  return parser_;
}

const dns_protocol::Header* DnsResponse::header() const {
  return reinterpret_cast<const dns_protocol::Header*>(io_buffer_->data());
}

bool DnsResponse::WriteHeader(base::BigEndianWriter* writer,
                              const dns_protocol::Header& header) {
  return writer->WriteU16(header.id) && writer->WriteU16(header.flags) &&
         writer->WriteU16(header.qdcount) && writer->WriteU16(header.ancount) &&
         writer->WriteU16(header.nscount) && writer->WriteU16(header.arcount);
}

bool DnsResponse::WriteQuestion(base::BigEndianWriter* writer,
                                const DnsQuery& query) {
  base::StringPiece question = query.question();
  return writer->WriteBytes(question.data(), question.size());
}

bool DnsResponse::WriteRecord(base::BigEndianWriter* writer,
                              const DnsResourceRecord& record,
                              bool validate_record,
                              bool validate_name_as_internet_hostname) {
  if (record.rdata != base::StringPiece(record.owned_rdata)) {
    VLOG(1) << "record.rdata should point to record.owned_rdata.";
    return false;
  }

  if (validate_record &&
      !RecordRdata::HasValidSize(record.owned_rdata, record.type)) {
    VLOG(1) << "Invalid RDATA size for a record.";
    return false;
  }

  absl::optional<std::vector<uint8_t>> domain_name =
      dns_names_util::DottedNameToNetwork(record.name,
                                          validate_name_as_internet_hostname);
  if (!domain_name.has_value()) {
    VLOG(1) << "Invalid dotted name (as "
            << (validate_name_as_internet_hostname ? "Internet hostname)."
                                                   : "DNS name).");
    return false;
  }

  return writer->WriteBytes(domain_name.value().data(),
                            domain_name.value().size()) &&
         writer->WriteU16(record.type) && writer->WriteU16(record.klass) &&
         writer->WriteU32(record.ttl) &&
         writer->WriteU16(record.owned_rdata.size()) &&
         // Use the owned RDATA in the record to construct the response.
         writer->WriteBytes(record.owned_rdata.data(),
                            record.owned_rdata.size());
}

bool DnsResponse::WriteAnswer(base::BigEndianWriter* writer,
                              const DnsResourceRecord& answer,
                              const absl::optional<DnsQuery>& query,
                              bool validate_record,
                              bool validate_name_as_internet_hostname) {
  // Generally assumed to be a mistake if we write answers that don't match the
  // query type, except CNAME answers which can always be added.
  if (validate_record && query.has_value() &&
      answer.type != query.value().qtype() &&
      answer.type != dns_protocol::kTypeCNAME) {
    VLOG(1) << "Mismatched answer resource record type and qtype.";
    return false;
  }
  return WriteRecord(writer, answer, validate_record,
                     validate_name_as_internet_hostname);
}

}  // namespace net
