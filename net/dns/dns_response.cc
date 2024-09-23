// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_response.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/big_endian.h"
#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/containers/span_writer.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/sys_byteorder.h"
#include "base/types/optional_util.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_names_util.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_response_result_extractor.h"
#include "net/dns/dns_util.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/record_rdata.h"

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

DnsRecordParser::~DnsRecordParser() = default;

DnsRecordParser::DnsRecordParser(const DnsRecordParser&) = default;

DnsRecordParser::DnsRecordParser(DnsRecordParser&&) = default;

DnsRecordParser& DnsRecordParser::operator=(const DnsRecordParser&) = default;

DnsRecordParser& DnsRecordParser::operator=(DnsRecordParser&&) = default;

DnsRecordParser::DnsRecordParser(base::span<const uint8_t> packet,
                                 size_t offset,
                                 size_t num_records)
    : packet_(packet), num_records_(num_records), cur_(offset) {
  CHECK_LE(offset, packet_.size());
}

unsigned DnsRecordParser::ReadName(const void* const vpos,
                                   std::string* out) const {
  static const char kAbortMsg[] = "Abort parsing of noncompliant DNS record.";

  CHECK_LE(packet_.data(), vpos);
  CHECK_LE(vpos, packet_.last(0u).data());
  const size_t initial_offset =
      // SAFETY: `vpos` points into the span, as verified by the CHECKs above,
      // so subtracting the data pointer is well-defined and gives an offset
      // into the span.
      //
      // TODO(danakj): Since we need an offset anyway, no unsafe pointer usage
      // would be required, and fewer CHECKs, if this function took an offset
      // instead of a pointer.
      UNSAFE_BUFFERS(static_cast<const uint8_t*>(vpos) - packet_.data());

  if (initial_offset == packet_.size()) {
    return 0;
  }

  size_t offset = initial_offset;
  // Count number of seen bytes to detect loops.
  unsigned seen = 0u;
  // Remember how many bytes were consumed before first jump.
  unsigned consumed = 0u;
  // The length of the encoded name (sum of label octets and label lengths).
  // For context, RFC 1034 states that the total number of octets representing a
  // domain name (the sum of all label octets and label lengths) is limited to
  // 255. RFC 1035 introduces message compression as a way to reduce packet size
  // on the wire, not to increase the maximum domain name length.
  unsigned encoded_name_len = 0u;

  if (out) {
    out->clear();
    out->reserve(dns_protocol::kMaxCharNameLength);
  }

  for (;;) {
    // The first two bits of the length give the type of the length. It's
    // either a direct length or a pointer to the remainder of the name.
    switch (packet_[offset] & dns_protocol::kLabelMask) {
      case dns_protocol::kLabelPointer: {
        if (packet_.size() < sizeof(uint16_t) ||
            offset > packet_.size() - sizeof(uint16_t)) {
          VLOG(1) << kAbortMsg << " Truncated or missing label pointer.";
          return 0;
        }
        if (consumed == 0u) {
          consumed = offset - initial_offset + sizeof(uint16_t);
          if (!out) {
            return consumed;  // If name is not stored, that's all we need.
          }
        }
        seen += sizeof(uint16_t);
        // If seen the whole packet, then we must be in a loop.
        if (seen > packet_.size()) {
          VLOG(1) << kAbortMsg << " Detected loop in label pointers.";
          return 0;
        }
        uint16_t new_offset =
            base::U16FromBigEndian(packet_.subspan(offset).first<2u>());
        offset = new_offset & dns_protocol::kOffsetMask;
        if (offset >= packet_.size()) {
          VLOG(1) << kAbortMsg << " Label pointer points outside packet.";
          return 0;
        }
        break;
      }
      case dns_protocol::kLabelDirect: {
        uint8_t label_len = packet_[offset];
        ++offset;
        // Note: root domain (".") is NOT included.
        if (label_len == 0) {
          if (consumed == 0) {
            consumed = offset - initial_offset;
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
        if (label_len >= packet_.size() - offset) {
          VLOG(1) << kAbortMsg << " Truncated or missing label.";
          return 0;  // Truncated or missing label.
        }
        if (out) {
          if (!out->empty())
            out->append(".");
          // TODO(danakj): Use append_range() in C++23.
          auto range = packet_.subspan(offset, label_len);
          out->append(range.begin(), range.end());
          CHECK_LE(out->size(), dns_protocol::kMaxCharNameLength);
        }
        offset += label_len;
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
  CHECK(!packet_.empty());

  // Disallow parsing any more than the claimed number of records.
  if (num_records_parsed_ >= num_records_)
    return false;

  size_t consumed = ReadName(packet_.subspan(cur_).data(), &out->name);
  if (!consumed) {
    return false;
  }
  auto reader = base::SpanReader(packet_.subspan(cur_ + consumed));
  uint16_t rdlen;
  if (reader.ReadU16BigEndian(out->type) &&
      reader.ReadU16BigEndian(out->klass) &&
      reader.ReadU32BigEndian(out->ttl) &&  //
      reader.ReadU16BigEndian(rdlen) &&
      base::OptionalUnwrapTo(reader.Read(rdlen), out->rdata, [](auto span) {
        return base::as_string_view(span);
      })) {
    cur_ += consumed + 2u + 2u + 4u + 2u + rdlen;
    ++num_records_parsed_;
    return true;
  }
  return false;
}

bool DnsRecordParser::ReadQuestion(std::string& out_dotted_qname,
                                   uint16_t& out_qtype) {
  size_t consumed = ReadName(packet_.subspan(cur_).data(), &out_dotted_qname);
  if (!consumed)
    return false;

  if (consumed + 2 * sizeof(uint16_t) > packet_.size() - cur_) {
    return false;
  }

  out_qtype = base::U16FromBigEndian(
      packet_.subspan(cur_ + consumed).first<sizeof(uint16_t)>());

  cur_ += consumed + 2 * sizeof(uint16_t);  // QTYPE + QCLASS

  return true;
}

DnsResponse::DnsResponse(
    uint16_t id,
    bool is_authoritative,
    const std::vector<DnsResourceRecord>& answers,
    const std::vector<DnsResourceRecord>& authority_records,
    const std::vector<DnsResourceRecord>& additional_records,
    const std::optional<DnsQuery>& query,
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

  auto io_buffer = base::MakeRefCounted<IOBufferWithSize>(response_size);
  auto writer = base::SpanWriter(io_buffer->span());
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
  DCHECK_EQ(writer.remaining(), 0u);
  std::ranges::fill(writer.remaining_span(), uint8_t{0});
  if (has_query)
    InitParse(io_buffer_size_, query.value());
  else
    InitParseWithoutQuery(io_buffer_size_);
}

DnsResponse::DnsResponse()
    : io_buffer_(base::MakeRefCounted<IOBufferWithSize>(
          dns_protocol::kMaxUDPSize + 1)),
      io_buffer_size_(dns_protocol::kMaxUDPSize + 1) {}

DnsResponse::DnsResponse(scoped_refptr<IOBuffer> buffer, size_t size)
    : io_buffer_(std::move(buffer)), io_buffer_size_(size) {}

DnsResponse::DnsResponse(size_t length)
    : io_buffer_(base::MakeRefCounted<IOBufferWithSize>(length)),
      io_buffer_size_(length) {}

DnsResponse::DnsResponse(base::span<const uint8_t> data, size_t answer_offset)
    : io_buffer_(base::MakeRefCounted<IOBufferWithSize>(data.size())),
      io_buffer_size_(data.size()),
      parser_(io_buffer_->span(),
              answer_offset,
              std::numeric_limits<size_t>::max()) {
  io_buffer_->span().copy_from(data);
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
  const std::string_view question = query.question();

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

  base::span<const uint8_t> subspan =
      io_buffer_->span().subspan(kHeaderSize, question.size());
  // Match the question section.
  if (question != base::as_string_view(subspan)) {
    return false;
  }

  std::optional<std::string> dotted_qname =
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
  parser_ = DnsRecordParser(io_buffer_->span().first(nbytes),
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
  parser_ = DnsRecordParser(io_buffer_->span().first(nbytes), kHeaderSize,
                            num_records);

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

std::optional<uint16_t> DnsResponse::id() const {
  if (!id_available_)
    return std::nullopt;

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

std::string_view DnsResponse::GetSingleDottedName() const {
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

bool DnsResponse::WriteHeader(base::SpanWriter<uint8_t>* writer,
                              const dns_protocol::Header& header) {
  return writer->WriteU16BigEndian(header.id) &&
         writer->WriteU16BigEndian(header.flags) &&
         writer->WriteU16BigEndian(header.qdcount) &&
         writer->WriteU16BigEndian(header.ancount) &&
         writer->WriteU16BigEndian(header.nscount) &&
         writer->WriteU16BigEndian(header.arcount);
}

bool DnsResponse::WriteQuestion(base::SpanWriter<uint8_t>* writer,
                                const DnsQuery& query) {
  return writer->Write(base::as_byte_span(query.question()));
}

bool DnsResponse::WriteRecord(base::SpanWriter<uint8_t>* writer,
                              const DnsResourceRecord& record,
                              bool validate_record,
                              bool validate_name_as_internet_hostname) {
  if (record.rdata != std::string_view(record.owned_rdata)) {
    VLOG(1) << "record.rdata should point to record.owned_rdata.";
    return false;
  }

  if (validate_record &&
      !RecordRdata::HasValidSize(record.owned_rdata, record.type)) {
    VLOG(1) << "Invalid RDATA size for a record.";
    return false;
  }

  std::optional<std::vector<uint8_t>> domain_name =
      dns_names_util::DottedNameToNetwork(record.name,
                                          validate_name_as_internet_hostname);
  if (!domain_name.has_value()) {
    VLOG(1) << "Invalid dotted name (as "
            << (validate_name_as_internet_hostname ? "Internet hostname)."
                                                   : "DNS name).");
    return false;
  }

  return writer->Write(domain_name.value()) &&
         writer->WriteU16BigEndian(record.type) &&
         writer->WriteU16BigEndian(record.klass) &&
         writer->WriteU32BigEndian(record.ttl) &&
         writer->WriteU16BigEndian(record.owned_rdata.size()) &&
         // Use the owned RDATA in the record to construct the response.
         writer->Write(base::as_byte_span(record.owned_rdata));
}

bool DnsResponse::WriteAnswer(base::SpanWriter<uint8_t>* writer,
                              const DnsResourceRecord& answer,
                              const std::optional<DnsQuery>& query,
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
