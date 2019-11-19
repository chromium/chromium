// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_RECORD_RDATA_H_
#define NET_DNS_RECORD_RDATA_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/net_export.h"
#include "net/dns/public/dns_protocol.h"

namespace net {

class DnsRecordParser;

// Parsed represenation of the extra data in a record. Does not include standard
// DNS record data such as TTL, Name, Type and Class.
class NET_EXPORT RecordRdata {
 public:
  virtual ~RecordRdata() {}

  // Return true if |data| represents RDATA in the wire format with a valid size
  // for the give |type|.
  static bool HasValidSize(const base::StringPiece& data, uint16_t type);

  virtual bool IsEqual(const RecordRdata* other) const = 0;
  virtual uint16_t Type() const = 0;
};

// SRV record format (http://www.ietf.org/rfc/rfc2782.txt):
// 2 bytes network-order unsigned priority
// 2 bytes network-order unsigned weight
// 2 bytes network-order unsigned port
// target: domain name (on-the-wire representation)
class NET_EXPORT_PRIVATE SrvRecordRdata : public RecordRdata {
 public:
  static const uint16_t kType = dns_protocol::kTypeSRV;

  ~SrvRecordRdata() override;
  static std::unique_ptr<SrvRecordRdata> Create(const base::StringPiece& data,
                                                const DnsRecordParser& parser);

  bool IsEqual(const RecordRdata* other) const override;
  uint16_t Type() const override;

  uint16_t priority() const { return priority_; }
  uint16_t weight() const { return weight_; }
  uint16_t port() const { return port_; }

  const std::string& target() const { return target_; }

 private:
  SrvRecordRdata();

  uint16_t priority_;
  uint16_t weight_;
  uint16_t port_;

  std::string target_;

  DISALLOW_COPY_AND_ASSIGN(SrvRecordRdata);
};

// A Record format (http://www.ietf.org/rfc/rfc1035.txt):
// 4 bytes for IP address.
class NET_EXPORT ARecordRdata : public RecordRdata {
 public:
  static const uint16_t kType = dns_protocol::kTypeA;

  ~ARecordRdata() override;
  static std::unique_ptr<ARecordRdata> Create(const base::StringPiece& data,
                                              const DnsRecordParser& parser);
  bool IsEqual(const RecordRdata* other) const override;
  uint16_t Type() const override;

  const IPAddress& address() const { return address_; }

 private:
  ARecordRdata();

  IPAddress address_;

  DISALLOW_COPY_AND_ASSIGN(ARecordRdata);
};

// AAAA Record format (http://www.ietf.org/rfc/rfc1035.txt):
// 16 bytes for IP address.
class NET_EXPORT AAAARecordRdata : public RecordRdata {
 public:
  static const uint16_t kType = dns_protocol::kTypeAAAA;

  ~AAAARecordRdata() override;
  static std::unique_ptr<AAAARecordRdata> Create(const base::StringPiece& data,
                                                 const DnsRecordParser& parser);
  bool IsEqual(const RecordRdata* other) const override;
  uint16_t Type() const override;

  const IPAddress& address() const { return address_; }

 private:
  AAAARecordRdata();

  IPAddress address_;

  DISALLOW_COPY_AND_ASSIGN(AAAARecordRdata);
};

// CNAME record format (http://www.ietf.org/rfc/rfc1035.txt):
// cname: On the wire representation of domain name.
class NET_EXPORT_PRIVATE CnameRecordRdata : public RecordRdata {
 public:
  static const uint16_t kType = dns_protocol::kTypeCNAME;

  ~CnameRecordRdata() override;
  static std::unique_ptr<CnameRecordRdata> Create(
      const base::StringPiece& data,
      const DnsRecordParser& parser);
  bool IsEqual(const RecordRdata* other) const override;
  uint16_t Type() const override;

  std::string cname() const { return cname_; }

 private:
  CnameRecordRdata();

  std::string cname_;

  DISALLOW_COPY_AND_ASSIGN(CnameRecordRdata);
};

// PTR record format (http://www.ietf.org/rfc/rfc1035.txt):
// domain: On the wire representation of domain name.
class NET_EXPORT_PRIVATE PtrRecordRdata : public RecordRdata {
 public:
  static const uint16_t kType = dns_protocol::kTypePTR;

  ~PtrRecordRdata() override;
  static std::unique_ptr<PtrRecordRdata> Create(const base::StringPiece& data,
                                                const DnsRecordParser& parser);
  bool IsEqual(const RecordRdata* other) const override;
  uint16_t Type() const override;

  std::string ptrdomain() const { return ptrdomain_; }

 private:
  PtrRecordRdata();

  std::string ptrdomain_;

  DISALLOW_COPY_AND_ASSIGN(PtrRecordRdata);
};

// TXT record format (http://www.ietf.org/rfc/rfc1035.txt):
// texts: One or more <character-string>s.
// a <character-string> is a length octet followed by as many characters.
class NET_EXPORT_PRIVATE TxtRecordRdata : public RecordRdata {
 public:
  static const uint16_t kType = dns_protocol::kTypeTXT;

  ~TxtRecordRdata() override;
  static std::unique_ptr<TxtRecordRdata> Create(const base::StringPiece& data,
                                                const DnsRecordParser& parser);
  bool IsEqual(const RecordRdata* other) const override;
  uint16_t Type() const override;

  const std::vector<std::string>& texts() const { return texts_; }

 private:
  TxtRecordRdata();

  std::vector<std::string> texts_;

  DISALLOW_COPY_AND_ASSIGN(TxtRecordRdata);
};

// Only the subset of the NSEC record format required by mDNS is supported.
// Nsec record format is described in http://www.ietf.org/rfc/rfc3845.txt and
// the limited version required for mDNS described in
// http://www.rfc-editor.org/rfc/rfc6762.txt Section 6.1.
class NET_EXPORT_PRIVATE NsecRecordRdata : public RecordRdata {
 public:
  static const uint16_t kType = dns_protocol::kTypeNSEC;

  ~NsecRecordRdata() override;
  static std::unique_ptr<NsecRecordRdata> Create(const base::StringPiece& data,
                                                 const DnsRecordParser& parser);
  bool IsEqual(const RecordRdata* other) const override;
  uint16_t Type() const override;

  // Length of the bitmap in bits.
  // This will be between 8 and 256, per RFC 3845, Section 2.1.2.
  uint16_t bitmap_length() const {
    DCHECK_LE(bitmap_.size(), 32u);
    return static_cast<uint16_t>(bitmap_.size() * 8);
  }

  // Returns bit i-th bit in the bitmap, where bits withing a byte are organized
  // most to least significant. If it is set, a record with rrtype i exists for
  // the domain name of this nsec record.
  bool GetBit(unsigned i) const;

 private:
  NsecRecordRdata();

  std::vector<uint8_t> bitmap_;

  DISALLOW_COPY_AND_ASSIGN(NsecRecordRdata);
};

// OPT record format (https://tools.ietf.org/html/rfc6891):
class NET_EXPORT_PRIVATE OptRecordRdata : public RecordRdata {
 public:
  class NET_EXPORT_PRIVATE Opt {
   public:
    static constexpr size_t kHeaderSize = 4;  // sizeof(code) + sizeof(size)

    Opt(uint16_t code, base::StringPiece data);

    bool operator==(const Opt& other) const;

    uint16_t code() const { return code_; }
    base::StringPiece data() const { return data_; }

   private:
    uint16_t code_;
    std::string data_;
  };

  static const uint16_t kType = dns_protocol::kTypeOPT;

  OptRecordRdata();
  OptRecordRdata(OptRecordRdata&& other);
  ~OptRecordRdata() override;

  OptRecordRdata& operator=(OptRecordRdata&& other);

  static std::unique_ptr<OptRecordRdata> Create(const base::StringPiece& data,
                                                const DnsRecordParser& parser);
  bool IsEqual(const RecordRdata* other) const override;
  uint16_t Type() const override;

  const std::vector<char>& buf() const { return buf_; }

  const std::vector<Opt>& opts() const { return opts_; }
  void AddOpt(const Opt& opt);

  // Add all Opts from |other| to |this|.
  void AddOpts(const OptRecordRdata& other);

  bool ContainsOptCode(uint16_t opt_code) const;

 private:
  std::vector<Opt> opts_;
  std::vector<char> buf_;

  DISALLOW_COPY_AND_ASSIGN(OptRecordRdata);
};

// TLS 1.3 Encrypted Server Name Indication
// record format (https://tools.ietf.org/id/draft-ietf-tls-esni-04.txt)
// struct {
//   ESNIKeys esni_keys;  // see spec
//   Extension dns_extensions<0..2 ^ 16 - 1>;
// } ESNIRecord;
class NET_EXPORT EsniRecordRdata : public RecordRdata {
 public:
  static constexpr uint16_t kType = dns_protocol::kExperimentalTypeEsniDraft4;
  static constexpr uint16_t kAddressSetExtensionType = 0x1001u;

  ~EsniRecordRdata() override;

  // Parsing an ESNIRecord Rdata succeeds when all of the following hold:
  // 1. The esni_keys field is well-formed.
  // 2. The dns_extensions field is well-formed and, additionally, valid
  // in the sense that its enum members have values allowed by the spec.
  // 3. The Rdata field contains no data beyond the ESNIKeys and, optionally,
  // one DNS extension of type address_set.
  static std::unique_ptr<EsniRecordRdata> Create(base::StringPiece data,
                                                 const DnsRecordParser& parser);

  // Two EsniRecordRdatas compare equal if their ESNIKeys fields agree
  // and their address sets contain the same addresses in the same order.
  bool IsEqual(const RecordRdata* other) const override;
  uint16_t Type() const override;

  // Returns the ESNIKeys field of the record. This is an opaque bitstring
  // passed to the SSL library.
  base::StringPiece esni_keys() const { return esni_keys_; }

  // Returns the IP addresses parsed from the address_set DNS extension, if any.
  const std::vector<IPAddress>& addresses() const { return addresses_; }

 private:
  EsniRecordRdata();

  std::string esni_keys_;
  std::vector<IPAddress> addresses_;

  DISALLOW_COPY_AND_ASSIGN(EsniRecordRdata);
};

}  // namespace net

#endif  // NET_DNS_RECORD_RDATA_H_
