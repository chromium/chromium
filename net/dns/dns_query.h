// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_QUERY_H_
#define NET_DNS_DNS_QUERY_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "net/base/net_export.h"

namespace base {
class BigEndianReader;
}  // namespace base

namespace net {

class OptRecordRdata;

namespace dns_protocol {
struct Header;
}  // namespace dns_protocol

class IOBufferWithSize;

// Represents on-the-wire DNS query message as an object.
class NET_EXPORT_PRIVATE DnsQuery {
 public:
  enum class PaddingStrategy {
    // Query will not be padded. Recommended strategy when query will not be
    // encrypted.
    NONE,

    // Query will be padded to the next multiple of 128 octets. Recommended
    // strategy (per RFC 8467) when query will be encrypted, e.g. through
    // DNS-over-HTTPS.
    BLOCK_LENGTH_128,
  };

  // Constructs a query message from |qname| which *MUST* be in a valid
  // DNS name format, and |qtype|. The qclass is set to IN.
  // If |opt_rdata| is not null, an OPT record will be added to the "Additional"
  // section of the query.
  DnsQuery(uint16_t id,
           const base::StringPiece& qname,
           uint16_t qtype,
           const OptRecordRdata* opt_rdata = nullptr,
           PaddingStrategy padding_strategy = PaddingStrategy::NONE);

  // Constructs an empty query from a raw packet in |buffer|. If the raw packet
  // represents a valid DNS query in the wire format (RFC 1035), Parse() will
  // populate the empty query.
  explicit DnsQuery(scoped_refptr<IOBufferWithSize> buffer);

  ~DnsQuery();

  // Clones |this| verbatim, with ID field of the header set to |id|.
  std::unique_ptr<DnsQuery> CloneWithNewId(uint16_t id) const;

  // Returns true and populates the query if the internally stored raw packet
  // can be parsed. This should only be called when DnsQuery is constructed from
  // the raw buffer.
  // |valid_bytes| indicates the number of initialized bytes in the raw buffer.
  // E.g. if the buffer holds a packet received from the network, the buffer may
  // be allocated with the maximum size of a UDP packet, but |valid_bytes|
  // indicates the number of bytes actually received from the network. If the
  // parsing requires reading more than the number of initialized bytes, this
  // method fails and returns false.
  bool Parse(size_t valid_bytes);

  // DnsQuery field accessors.
  uint16_t id() const;
  base::StringPiece qname() const;
  uint16_t qtype() const;

  // Returns the Question section of the query.  Used when matching the
  // response.
  base::StringPiece question() const;

  // Returns the size of the question section.
  size_t question_size() const;

  // IOBuffer accessor to be used for writing out the query. The buffer has
  // the same byte layout as the DNS query wire format.
  IOBufferWithSize* io_buffer() const { return io_buffer_.get(); }

  void set_flags(uint16_t flags);

 private:
  DnsQuery(const DnsQuery& orig, uint16_t id);

  bool ReadHeader(base::BigEndianReader* reader, dns_protocol::Header* out);
  // After read, |out| is in the DNS format, e.g.
  // "\x03""www""\x08""chromium""\x03""com""\x00". Use DNSDomainToString to
  // convert to the dotted format "www.chromium.com" with no trailing dot.
  bool ReadName(base::BigEndianReader* reader, std::string* out);

  // Size of the DNS name (*NOT* hostname) we are trying to resolve; used
  // to calculate offsets.
  size_t qname_size_ = 0;

  // Contains query bytes to be consumed by higher level Write() call.
  scoped_refptr<IOBufferWithSize> io_buffer_;

  // Pointer to the dns header section.
  dns_protocol::Header* header_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(DnsQuery);
};

}  // namespace net

#endif  // NET_DNS_DNS_QUERY_H_
