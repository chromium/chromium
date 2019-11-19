// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_response.h"

#include <algorithm>
#include <limits>
#include <numeric>
#include <utility>

#include "base/big_endian.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/sys_byteorder.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_query.h"
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
  owned_rdata = std::move(value);
  rdata = owned_rdata;
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

DnsRecordParser::DnsRecordParser()
    : packet_(nullptr), length_(0), cur_(nullptr) {}

DnsRecordParser::DnsRecordParser(const void* packet,
                                 size_t length,
                                 size_t offset)
    : packet_(reinterpret_cast<const char*>(packet)),
      length_(length),
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
    out->reserve(dns_protocol::kMaxNameLength);
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
        base::ReadBigEndian<uint16_t>(p, &offset);
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
        // Add one octet for the length and |label_len| for the number of
        // following octets.
        encoded_name_len += 1 + label_len;
        if (encoded_name_len > dns_protocol::kMaxNameLength) {
          VLOG(1) << kAbortMsg << " Name is too long.";
          return 0;
        }
        // Note: root domain (".") is NOT included.
        if (label_len == 0) {
          if (consumed == 0) {
            consumed = p - pos;
          }  // else we set |consumed| before first jump
          return consumed;
        }
        if (p + label_len >= end) {
          VLOG(1) << kAbortMsg << " Truncated or missing label.";
          return 0;  // Truncated or missing label.
        }
        if (out) {
          if (!out->empty())
            out->append(".");
          out->append(p, label_len);
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
  size_t consumed = ReadName(cur_, &out->name);
  if (!consumed)
    return false;
  base::BigEndianReader reader(cur_ + consumed,
                               packet_ + length_ - (cur_ + consumed));
  uint16_t rdlen;
  if (reader.ReadU16(&out->type) &&
      reader.ReadU16(&out->klass) &&
      reader.ReadU32(&out->ttl) &&
      reader.ReadU16(&rdlen) &&
      reader.ReadPiece(&out->rdata, rdlen)) {
    cur_ = reader.ptr();
    return true;
  }
  return false;
}

bool DnsRecordParser::SkipQuestion() {
  size_t consumed = ReadName(cur_, nullptr);
  if (!consumed)
    return false;

  const char* next = cur_ + consumed + 2 * sizeof(uint16_t);  // QTYPE + QCLASS
  if (next > packet_ + length_)
    return false;

  cur_ = next;

  return true;
}

DnsResponse::DnsResponse(
    uint16_t id,
    bool is_authoritative,
    const std::vector<DnsResourceRecord>& answers,
    const std::vector<DnsResourceRecord>& authority_records,
    const std::vector<DnsResourceRecord>& additional_records,
    const base::Optional<DnsQuery>& query,
    uint8_t rcode) {
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

  io_buffer_ = base::MakeRefCounted<IOBuffer>(response_size);
  io_buffer_size_ = response_size;
  base::BigEndianWriter writer(io_buffer_->data(), io_buffer_size_);
  success &= WriteHeader(&writer, header);
  DCHECK(success);
  if (has_query) {
    success &= WriteQuestion(&writer, query.value());
    DCHECK(success);
  }
  // Start the Answer section.
  for (const auto& answer : answers) {
    success &= WriteAnswer(&writer, answer, query);
    DCHECK(success);
  }
  // Start the Authority section.
  for (const auto& record : authority_records) {
    success &= WriteRecord(&writer, record);
    DCHECK(success);
  }
  // Start the Additional section.
  for (const auto& record : additional_records) {
    success &= WriteRecord(&writer, record);
    DCHECK(success);
  }
  if (!success) {
    io_buffer_.reset();
    io_buffer_size_ = 0;
    return;
  }
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
      parser_(io_buffer_->data(), length, answer_offset) {
  DCHECK(data);
  memcpy(io_buffer_->data(), data, length);
}

DnsResponse::~DnsResponse() = default;

bool DnsResponse::InitParse(size_t nbytes, const DnsQuery& query) {
  const base::StringPiece question = query.question();

  // Response includes question, it should be at least that size.
  if (nbytes < kHeaderSize + question.size() || nbytes > io_buffer_size_) {
    return false;
  }

  // Match the query id.
  if (base::NetToHost16(header()->id) != query.id())
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

  // Construct the parser.
  parser_ = DnsRecordParser(io_buffer_->data(), nbytes,
                            kHeaderSize + question.size());
  return true;
}

bool DnsResponse::InitParseWithoutQuery(size_t nbytes) {
  if (nbytes < kHeaderSize || nbytes > io_buffer_size_) {
    return false;
  }

  parser_ = DnsRecordParser(io_buffer_->data(), nbytes, kHeaderSize);

  // Not a response?
  if ((base::NetToHost16(header()->flags) & dns_protocol::kFlagResponse) == 0)
    return false;

  unsigned qdcount = base::NetToHost16(header()->qdcount);
  for (unsigned i = 0; i < qdcount; ++i) {
    if (!parser_.SkipQuestion()) {
      parser_ = DnsRecordParser();  // Make parser invalid again.
      return false;
    }
  }

  return true;
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

unsigned DnsResponse::answer_count() const {
  DCHECK(parser_.IsValid());
  return base::NetToHost16(header()->ancount);
}

unsigned DnsResponse::additional_answer_count() const {
  DCHECK(parser_.IsValid());
  return base::NetToHost16(header()->arcount);
}

base::StringPiece DnsResponse::qname() const {
  DCHECK(parser_.IsValid());
  // The response is HEADER QNAME QTYPE QCLASS ANSWER.
  // |parser_| is positioned at the beginning of ANSWER, so the end of QNAME is
  // two uint16_ts before it.
  const size_t qname_size =
      parser_.GetOffset() - 2 * sizeof(uint16_t) - kHeaderSize;
  return base::StringPiece(io_buffer_->data() + kHeaderSize, qname_size);
}

uint16_t DnsResponse::qtype() const {
  DCHECK(parser_.IsValid());
  // QTYPE starts where QNAME ends.
  const size_t type_offset = parser_.GetOffset() - 2 * sizeof(uint16_t);
  uint16_t type;
  base::ReadBigEndian<uint16_t>(io_buffer_->data() + type_offset, &type);
  return type;
}

std::string DnsResponse::GetDottedName() const {
  return DNSDomainToString(qname());
}

DnsRecordParser DnsResponse::Parser() const {
  DCHECK(parser_.IsValid());
  // Return a copy of the parser.
  return parser_;
}

const dns_protocol::Header* DnsResponse::header() const {
  return reinterpret_cast<const dns_protocol::Header*>(io_buffer_->data());
}

DnsResponse::Result DnsResponse::ParseToAddressList(
    AddressList* addr_list,
    base::TimeDelta* ttl) const {
  DCHECK(IsValid());
  // DnsTransaction already verified that |response| matches the issued query.
  // We still need to determine if there is a valid chain of CNAMEs from the
  // query name to the RR owner name.
  // We err on the side of caution with the assumption that if we are too picky,
  // we can always fall back to the system getaddrinfo.

  // Expected owner of record. No trailing dot.
  std::string expected_name = GetDottedName();

  uint16_t expected_type = qtype();
  DCHECK(expected_type == dns_protocol::kTypeA ||
         expected_type == dns_protocol::kTypeAAAA);

  size_t expected_size = (expected_type == dns_protocol::kTypeAAAA)
                             ? IPAddress::kIPv6AddressSize
                             : IPAddress::kIPv4AddressSize;

  uint32_t ttl_sec = std::numeric_limits<uint32_t>::max();
  IPAddressList ip_addresses;
  DnsRecordParser parser = Parser();
  DnsResourceRecord record;
  unsigned ancount = answer_count();

  for (unsigned i = 0; i < ancount; ++i) {
    if (!parser.ReadRecord(&record))
      return DNS_MALFORMED_RESPONSE;

    if (record.type == dns_protocol::kTypeCNAME) {
      // Following the CNAME chain, only if no addresses seen.
      if (!ip_addresses.empty())
        return DNS_CNAME_AFTER_ADDRESS;

      if (!base::EqualsCaseInsensitiveASCII(record.name, expected_name))
        return DNS_NAME_MISMATCH;

      if (record.rdata.size() !=
          parser.ReadName(record.rdata.begin(), &expected_name))
        return DNS_MALFORMED_CNAME;

      ttl_sec = std::min(ttl_sec, record.ttl);
    } else if (record.type == expected_type) {
      if (record.rdata.size() != expected_size)
        return DNS_SIZE_MISMATCH;

      if (!base::EqualsCaseInsensitiveASCII(record.name, expected_name))
        return DNS_NAME_MISMATCH;

      ttl_sec = std::min(ttl_sec, record.ttl);
      ip_addresses.push_back(
          IPAddress(reinterpret_cast<const uint8_t*>(record.rdata.data()),
                    record.rdata.length()));
    }
  }

  // NXDOMAIN or NODATA cases respectively.
  if (rcode() == dns_protocol::kRcodeNXDOMAIN ||
      (ancount == 0 && rcode() == dns_protocol::kRcodeNOERROR)) {
    unsigned nscount = base::NetToHost16(header()->nscount);
    for (unsigned i = 0; i < nscount; ++i) {
      if (parser.ReadRecord(&record) && record.type == dns_protocol::kTypeSOA)
        ttl_sec = std::min(ttl_sec, record.ttl);
    }
  }

  // getcanonname in eglibc returns the first owner name of an A or AAAA RR.
  // If the response passed all the checks so far, then |expected_name| is it.
  *addr_list = AddressList::CreateFromIPAddressList(ip_addresses,
                                                    expected_name);
  *ttl = base::TimeDelta::FromSeconds(ttl_sec);
  return DNS_PARSE_OK;
}

bool DnsResponse::WriteHeader(base::BigEndianWriter* writer,
                              const dns_protocol::Header& header) {
  return writer->WriteU16(header.id) && writer->WriteU16(header.flags) &&
         writer->WriteU16(header.qdcount) && writer->WriteU16(header.ancount) &&
         writer->WriteU16(header.nscount) && writer->WriteU16(header.arcount);
}

bool DnsResponse::WriteQuestion(base::BigEndianWriter* writer,
                                const DnsQuery& query) {
  const base::StringPiece& question = query.question();
  return writer->WriteBytes(question.data(), question.size());
}

bool DnsResponse::WriteRecord(base::BigEndianWriter* writer,
                              const DnsResourceRecord& record) {
  if (record.rdata.data() != record.owned_rdata.data() ||
      record.rdata.size() != record.owned_rdata.size()) {
    VLOG(1) << "record.rdata should point to record.owned_rdata.";
    return false;
  }

  if (!RecordRdata::HasValidSize(record.owned_rdata, record.type)) {
    VLOG(1) << "Invalid RDATA size for a record.";
    return false;
  }
  std::string domain_name;
  if (!DNSDomainFromDot(record.name, &domain_name)) {
    VLOG(1) << "Invalid dotted name.";
    return false;
  }
  return writer->WriteBytes(domain_name.data(), domain_name.size()) &&
         writer->WriteU16(record.type) && writer->WriteU16(record.klass) &&
         writer->WriteU32(record.ttl) &&
         writer->WriteU16(record.owned_rdata.size()) &&
         // Use the owned RDATA in the record to construct the response.
         writer->WriteBytes(record.owned_rdata.data(),
                            record.owned_rdata.size());
}

bool DnsResponse::WriteAnswer(base::BigEndianWriter* writer,
                              const DnsResourceRecord& answer,
                              const base::Optional<DnsQuery>& query) {
  // Generally assumed to be a mistake if we write answers that don't match the
  // query type, except CNAME answers which can always be added.
  if (query.has_value() && answer.type != query.value().qtype() &&
      answer.type != dns_protocol::kTypeCNAME) {
    VLOG(1) << "Mismatched answer resource record type and qtype.";
    return false;
  }
  return WriteRecord(writer, answer);
}

}  // namespace net
