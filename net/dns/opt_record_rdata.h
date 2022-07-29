// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_OPT_RECORD_RDATA_H_
#define NET_DNS_OPT_RECORD_RDATA_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/strings/string_piece.h"
#include "net/base/net_export.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/record_rdata.h"

namespace net {

class DnsRecordParser;

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

  OptRecordRdata(const OptRecordRdata&) = delete;
  OptRecordRdata& operator=(const OptRecordRdata&) = delete;

  OptRecordRdata(OptRecordRdata&& other);

  ~OptRecordRdata() override;

  OptRecordRdata& operator=(OptRecordRdata&& other);

  static std::unique_ptr<OptRecordRdata> Create(const base::StringPiece& data,
                                                const DnsRecordParser& parser);
  bool IsEqual(const RecordRdata* other) const override;
  uint16_t Type() const override;

  const std::vector<char>& buf() const { return buf_; }
  const std::multimap<uint16_t, Opt>& opts() { return opts_; }

  void AddOpt(const Opt& opt);

  // Add all Opts from |other| to |this|.
  void AddOpts(const OptRecordRdata& other);

  // Checks if an Opt with the specified opt_code is in opts_.
  bool ContainsOptCode(uint16_t opt_code) const;

 private:
  // Opt objects are stored in a multimap; key is the opt code.
  std::multimap<uint16_t, Opt> opts_;
  std::vector<char> buf_;
};

}  // namespace net

#endif  // NET_DNS_OPT_RECORD_RDATA_H_
