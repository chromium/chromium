// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_RECORD_RDATA_H_
#define NET_DNS_RECORD_RDATA_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/net_export.h"
#include "net/dns/public/dns_protocol.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace net {

class DnsRecordParser;

// Parsed represenation of the extra data in a record. Does not include standard
// DNS record data such as TTL, Name, Type and Class.
class NET_EXPORT RecordRdata {
 public:
  virtual ~RecordRdata() = default;

  // Return true if `data` represents RDATA in the wire format with a valid size
  // for the give `type`. Always returns true for unrecognized `type`s as the
  // size is never known to be invalid.
  static bool HasValidSize(std::string_view data, uint16_t type);

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

  SrvRecordRdata(const SrvRecordRdata&) = delete;
  SrvRecordRdata& operator=(const SrvRecordRdata&) = delete;

  ~SrvRecordRdata() override;
  static std::unique_ptr<SrvRecordRdata> Create(std::string_view data,
                                                const DnsRecordParser& parser);

  bool IsEqual(const RecordRdata* other) const override;
  uint16_t Type() const override;

  uint16_t priority() const { return priority_; }
  uint16_t weight() const { return weight_; }
  uint16_t port() const { return port_; }

  const std::string& target() const { return target_; }

 private:
  SrvRecordRdata();

  uint16_t priority_ = 0;
  uint16_t weight_ = 0;
  uint16_t port_ = 0;

  std::string target_;
};

// A Record format (http://www.ietf.org/rfc/rfc1035.txt):
// 4 bytes for IP address.
class NET_EXPORT ARecordRdata : public RecordRdata {
 public:
  static const uint16_t kType = dns_protocol::kTypeA;

  ARecordRdata(const ARecordRdata&) = delete;
  ARecordRdata& operator=(const ARecordRdata&) = delete;

  ~ARecordRdata() override;
  static std::unique_ptr<ARecordRdata> Create(std::string_view data,
                                              const DnsRecordParser& parser);
  bool IsEqual(const RecordRdata* other) const override;
  uint16_t Type() const override;

  const IPAddress& address() const { return address_; }

 private:
  ARecordRdata();

  IPAddress address_;
};

// AAAA Record format (http://www.ietf.org/rfc/rfc1035.txt):
// 16 bytes for IP address.
class NET_EXPORT AAAARecordRdata : public RecordRdata {
 public:
  static const uint16_t kType = dns_protocol::kTypeAAAA;

  AAAARecordRdata(const AAAARecordRdata&) = delete;
  AAAARecordRdata& operator=(const AAAARecordRdata&) = delete;

  ~AAAARecordRdata() override;
  static std::unique_ptr<AAAARecordRdata> Create(std::string_view data,
                                                 const DnsRecordParser& parser);
  bool IsEqual(const RecordRdata* other) const override;
  uint16_t Type() const override;

  const IPAddress& address() const { return address_; }

 private:
  AAAARecordRdata();

  IPAddress address_;
};

// CNAME record format (http://www.ietf.org/rfc/rfc1035.txt):
// cname: On the wire representation of domain name.
class NET_EXPORT_PRIVATE CnameRecordRdata : public RecordRdata {
 public:
  static const uint16_t kType = dns_protocol::kTypeCNAME;

  CnameRecordRdata(const CnameRecordRdata&) = delete;
  CnameRecordRdata& operator=(const CnameRecordRdata&) = delete;

  ~CnameRecordRdata() override;
  static std::unique_ptr<CnameRecordRdata> Create(
      std::string_view data,
      const DnsRecordParser& parser);
  bool IsEqual(const RecordRdata* other) const override;
  uint16_t Type() const override;

  const std::string& cname() const { return cname_; }

 private:
  CnameRecordRdata();

  std::string cname_;
};

// PTR record format (http://www.ietf.org/rfc/rfc1035.txt):
// domain: On the wire representation of domain name.
class NET_EXPORT_PRIVATE PtrRecordRdata : public RecordRdata {
 public:
  static const uint16_t kType = dns_protocol::kTypePTR;

  PtrRecordRdata(const PtrRecordRdata&) = delete;
  PtrRecordRdata& operator=(const PtrRecordRdata&) = delete;

  ~PtrRecordRdata() override;
  static std::unique_ptr<PtrRecordRdata> Create(std::string_view data,
                                                const DnsRecordParser& parser);
  bool IsEqual(const RecordRdata* other) const override;
  uint16_t Type() const override;

  std::string ptrdomain() const { return ptrdomain_; }

 private:
  PtrRecordRdata();

  std::string ptrdomain_;
};

// TXT record format (http://www.ietf.org/rfc/rfc1035.txt):
// texts: One or more <character-string>s.
// a <character-string> is a length octet followed by as many characters.
class NET_EXPORT_PRIVATE TxtRecordRdata : public RecordRdata {
 public:
  static const uint16_t kType = dns_protocol::kTypeTXT;

  TxtRecordRdata(const TxtRecordRdata&) = delete;
  TxtRecordRdata& operator=(const TxtRecordRdata&) = delete;

  ~TxtRecordRdata() override;
  static std::unique_ptr<TxtRecordRdata> Create(std::string_view data,
                                                const DnsRecordParser& parser);
  bool IsEqual(const RecordRdata* other) const override;
  uint16_t Type() const override;

  const std::vector<std::string>& texts() const { return texts_; }

 private:
  TxtRecordRdata();

  std::vector<std::string> texts_;
};

// Only the subset of the NSEC record format required by mDNS is supported.
// Nsec record format is described in http://www.ietf.org/rfc/rfc3845.txt and
// the limited version required for mDNS described in
// http://www.rfc-editor.org/rfc/rfc6762.txt Section 6.1.
class NET_EXPORT_PRIVATE NsecRecordRdata : public RecordRdata {
 public:
  static const uint16_t kType = dns_protocol::kTypeNSEC;

  NsecRecordRdata(const NsecRecordRdata&) = delete;
  NsecRecordRdata& operator=(const NsecRecordRdata&) = delete;

  ~NsecRecordRdata() override;
  static std::unique_ptr<NsecRecordRdata> Create(std::string_view data,
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
};

}  // namespace net

#endif  // NET_DNS_RECORD_RDATA_H_
