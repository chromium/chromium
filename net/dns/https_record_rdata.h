// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_HTTPS_RECORD_RDATA_H_
#define NET_DNS_HTTPS_RECORD_RDATA_H_

#include <stdint.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "net/base/ip_address.h"
#include "net/base/net_export.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/record_rdata.h"

namespace net {

class AliasFormHttpsRecordRdata;
class ServiceFormHttpsRecordRdata;

class NET_EXPORT_PRIVATE HttpsRecordRdata : public RecordRdata {
 public:
  static const uint16_t kType = dns_protocol::kTypeHttps;

  static std::unique_ptr<HttpsRecordRdata> Parse(base::StringPiece data);

  HttpsRecordRdata(const HttpsRecordRdata& rdata) = delete;
  HttpsRecordRdata& operator=(const HttpsRecordRdata& rdata) = delete;

  ~HttpsRecordRdata() override;

  bool IsEqual(const RecordRdata* other) const override;
  virtual bool IsEqual(const HttpsRecordRdata* other) const = 0;
  uint16_t Type() const override;

  virtual bool IsAlias() const = 0;
  AliasFormHttpsRecordRdata* AsAliasForm();
  const AliasFormHttpsRecordRdata* AsAliasForm() const;
  ServiceFormHttpsRecordRdata* AsServiceForm();
  const ServiceFormHttpsRecordRdata* AsServiceForm() const;

 protected:
  HttpsRecordRdata() = default;
};

class NET_EXPORT_PRIVATE AliasFormHttpsRecordRdata : public HttpsRecordRdata {
 public:
  explicit AliasFormHttpsRecordRdata(std::string alias_name);
  static std::unique_ptr<AliasFormHttpsRecordRdata> Parse(
      base::StringPiece data);

  bool IsEqual(const HttpsRecordRdata* other) const override;
  bool IsAlias() const override;

  base::StringPiece alias_name() { return alias_name_; }

 private:
  AliasFormHttpsRecordRdata() = default;

  const std::string alias_name_;
};

class NET_EXPORT_PRIVATE ServiceFormHttpsRecordRdata : public HttpsRecordRdata {
 public:
  ServiceFormHttpsRecordRdata(uint16_t priority,
                              std::string service_name,
                              std::vector<std::string> alpn_ids,
                              bool default_alpn,
                              base::Optional<uint16_t> port,
                              std::vector<IPAddress> ipv4_hint,
                              std::string ech_config,
                              std::vector<IPAddress> ipv6_hint,
                              std::map<uint16_t, std::string> unparsed_params);
  static std::unique_ptr<ServiceFormHttpsRecordRdata> Parse(
      base::StringPiece data);

  ~ServiceFormHttpsRecordRdata() override;

  bool IsEqual(const HttpsRecordRdata* other) const override;
  bool IsAlias() const override;

  uint16_t priority() { return priority_; }
  base::StringPiece service_name() { return service_name_; }
  const std::vector<std::string>& alpn_ids() { return alpn_ids_; }
  bool default_alpn() { return default_alpn_; }
  base::Optional<uint16_t> port() { return port_; }
  const std::vector<IPAddress>& ipv4_hint() { return ipv4_hint_; }
  base::StringPiece ech_config() { return ech_config_; }
  const std::vector<IPAddress>& ipv6_hint() { return ipv6_hint_; }
  const std::map<uint16_t, std::string>& unparsed_params() {
    return unparsed_params_;
  }

 private:
  const uint16_t priority_;
  const std::string service_name_;

  // Supported service parameters.
  const std::vector<std::string> alpn_ids_;
  const bool default_alpn_;
  const base::Optional<uint16_t> port_;
  const std::vector<IPAddress> ipv4_hint_;
  const std::string ech_config_;
  const std::vector<IPAddress> ipv6_hint_;

  const std::map<uint16_t, std::string> unparsed_params_;
};

}  // namespace net

#endif  // NET_DNS_HTTPS_RECORD_RDATA_H_
