// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_RESPONSE_H_
#define NET_DNS_DNS_RESPONSE_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/dns/public/dns_protocol.h"

namespace base {
class BigEndianWriter;
}  // namespace base

namespace net {

class AddressList;
class DnsQuery;
class IOBuffer;

namespace dns_protocol {
struct Header;
}  // namespace dns_protocol

// Structure representing a Resource Record as specified in RFC 1035, Section
// 4.1.3.
struct NET_EXPORT_PRIVATE DnsResourceRecord {
  DnsResourceRecord();
  explicit DnsResourceRecord(const DnsResourceRecord& other);
  DnsResourceRecord(DnsResourceRecord&& other);
  ~DnsResourceRecord();

  DnsResourceRecord& operator=(const DnsResourceRecord& other);
  DnsResourceRecord& operator=(DnsResourceRecord&& other);

  // A helper to set |owned_rdata| that also sets |rdata| to point to it.
  // See the definition of |owned_rdata| below.
  void SetOwnedRdata(std::string value);

  // NAME (variable length) + TYPE (2 bytes) + CLASS (2 bytes) + TTL (4 bytes) +
  // RDLENGTH (2 bytes) + RDATA (variable length)
  //
  // Uses |owned_rdata| for RDATA if non-empty.
  size_t CalculateRecordSize() const;

  std::string name;  // in dotted form
  uint16_t type = 0;
  uint16_t klass = 0;
  uint32_t ttl = 0;
  // Points to the original response buffer or otherwise to |owned_rdata|.
  base::StringPiece rdata;
  // Used to construct a DnsResponse from data. This field is empty if |rdata|
  // points to the response buffer.
  std::string owned_rdata;
};

// Iterator to walk over resource records of the DNS response packet.
class NET_EXPORT_PRIVATE DnsRecordParser {
 public:
  // Construct an uninitialized iterator.
  DnsRecordParser();

  // Construct an iterator to process the |packet| of given |length|.
  // |offset| points to the beginning of the answer section.
  DnsRecordParser(const void* packet, size_t length, size_t offset);

  // Returns |true| if initialized.
  bool IsValid() const { return packet_ != nullptr; }

  // Returns |true| if no more bytes remain in the packet.
  bool AtEnd() const { return cur_ == packet_ + length_; }

  // Returns current offset into the packet.
  size_t GetOffset() const { return cur_ - packet_; }

  // Parses a (possibly compressed) DNS name from the packet starting at
  // |pos|. Stores output (even partial) in |out| unless |out| is NULL. |out|
  // is stored in the dotted form, e.g., "example.com". Returns number of bytes
  // consumed or 0 on failure.
  // This is exposed to allow parsing compressed names within RRDATA for TYPEs
  // such as NS, CNAME, PTR, MX, SOA.
  // See RFC 1035 section 4.1.4.
  unsigned ReadName(const void* pos, std::string* out) const;

  // Parses the next resource record into |record|. Returns true if succeeded.
  bool ReadRecord(DnsResourceRecord* record);

  // Skip a question section, returns true if succeeded.
  bool SkipQuestion();

 private:
  const char* packet_;
  size_t length_;
  // Current offset within the packet.
  const char* cur_;
};

// Buffer-holder for the DNS response allowing easy access to the header fields
// and resource records. After reading into |io_buffer| must call InitParse to
// position the RR parser.
class NET_EXPORT_PRIVATE DnsResponse {
 public:
  // Possible results from ParseToAddressList.
  enum Result {
    DNS_PARSE_OK = 0,
    DNS_MALFORMED_RESPONSE,    // DnsRecordParser failed before the end of
                               // packet.
    DNS_MALFORMED_CNAME,       // Could not parse CNAME out of RRDATA.
    DNS_NAME_MISMATCH,         // Got an address but no ordered chain of CNAMEs
                               // leads there.
    DNS_SIZE_MISMATCH,         // Got an address but size does not match.
    DNS_CNAME_AFTER_ADDRESS,   // Found CNAME after an address record.
    DNS_ADDRESS_TTL_MISMATCH,  // OBSOLETE. No longer used.
    DNS_NO_ADDRESSES,          // OBSOLETE. No longer used.
    // Only add new values here.
    DNS_PARSE_RESULT_MAX,      // Bounding value for histograms.
  };

  // Constructs a response buffer large enough to store one byte more than
  // largest possible response, to detect malformed responses.
  DnsResponse();

  // Constructs a response message from |answers| and the originating |query|.
  // After the successful construction, and the parser is also initialized.
  DnsResponse(uint16_t id,
              bool is_authoritative,
              const std::vector<DnsResourceRecord>& answers,
              const std::vector<DnsResourceRecord>& authority_records,
              const std::vector<DnsResourceRecord>& additional_records,
              const base::Optional<DnsQuery>& query,
              uint8_t rcode = dns_protocol::kRcodeNOERROR);

  // Constructs a response buffer of given length. Used for TCP transactions.
  explicit DnsResponse(size_t length);

  // Constructs a response from the passed buffer.
  DnsResponse(scoped_refptr<IOBuffer> buffer, size_t size);

  // Constructs a response from |data|. Used for testing purposes only!
  DnsResponse(const void* data, size_t length, size_t answer_offset);

  ~DnsResponse();

  // Internal buffer accessor into which actual bytes of response will be
  // read.
  IOBuffer* io_buffer() { return io_buffer_.get(); }

  // Size of the internal buffer.
  size_t io_buffer_size() const { return io_buffer_size_; }

  // Assuming the internal buffer holds |nbytes| bytes, returns true iff the
  // packet matches the |query| id and question. This should only be called if
  // the response is constructed from a raw buffer.
  bool InitParse(size_t nbytes, const DnsQuery& query);

  // Assuming the internal buffer holds |nbytes| bytes, initialize the parser
  // without matching it against an existing query. This should only be called
  // if the response is constructed from a raw buffer.
  bool InitParseWithoutQuery(size_t nbytes);

  // Returns true if response is valid, that is, after successful InitParse, or
  // after successful construction of a new response from data.
  bool IsValid() const;

  // All of the methods below are valid only if the response is valid.

  // Accessors for the header.
  uint16_t flags() const;  // excluding rcode
  uint8_t rcode() const;

  unsigned answer_count() const;
  unsigned additional_answer_count() const;

  // Accessors to the question. The qname is unparsed.
  base::StringPiece qname() const;
  uint16_t qtype() const;

  // Returns qname in dotted format.
  std::string GetDottedName() const;

  // Returns an iterator to the resource records in the answer section.
  // The iterator is valid only in the scope of the DnsResponse.
  // This operation is idempotent.
  DnsRecordParser Parser() const;

  // Extracts an AddressList from this response. Returns SUCCESS if succeeded.
  // Otherwise returns a detailed error number.
  Result ParseToAddressList(AddressList* addr_list, base::TimeDelta* ttl) const;

 private:
  bool WriteHeader(base::BigEndianWriter* writer,
                   const dns_protocol::Header& header);
  bool WriteQuestion(base::BigEndianWriter* writer, const DnsQuery& query);
  bool WriteRecord(base::BigEndianWriter* wirter,
                   const DnsResourceRecord& record);
  bool WriteAnswer(base::BigEndianWriter* wirter,
                   const DnsResourceRecord& answer,
                   const base::Optional<DnsQuery>& query);

  // Convenience for header access.
  const dns_protocol::Header* header() const;

  // Buffer into which response bytes are read.
  scoped_refptr<IOBuffer> io_buffer_;

  // Size of the buffer.
  size_t io_buffer_size_;

  // Iterator constructed after InitParse positioned at the answer section.
  // It is never updated afterwards, so can be used in accessors.
  DnsRecordParser parser_;

  DISALLOW_COPY_AND_ASSIGN(DnsResponse);
};

}  // namespace net

#endif  // NET_DNS_DNS_RESPONSE_H_
