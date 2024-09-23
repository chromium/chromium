// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_HTTPS_RECORD_RDATA_H_
#define NET_DNS_HTTPS_RECORD_RDATA_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "net/base/ip_address.h"
#include "net/base/net_export.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/record_rdata.h"

namespace net {

using HttpsRecordPriority = uint16_t;

class AliasFormHttpsRecordRdata;
class ServiceFormHttpsRecordRdata;

class NET_EXPORT_PRIVATE HttpsRecordRdata : public RecordRdata {
 public:
  static const uint16_t kType = dns_protocol::kTypeHttps;

  // Returns `nullptr` on malformed input.
  static std::unique_ptr<HttpsRecordRdata> Parse(std::string_view data);

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
      std::string_view data);

  bool IsEqual(const HttpsRecordRdata* other) const override;
  bool IsAlias() const override;

  std::string_view alias_name() const { return alias_name_; }

 private:
  AliasFormHttpsRecordRdata() = default;

  const std::string alias_name_;
};

class NET_EXPORT_PRIVATE ServiceFormHttpsRecordRdata : public HttpsRecordRdata {
 public:
  static constexpr uint16_t kSupportedKeys[] = {
      dns_protocol::kHttpsServiceParamKeyMandatory,
      dns_protocol::kHttpsServiceParamKeyAlpn,
      dns_protocol::kHttpsServiceParamKeyNoDefaultAlpn,
      dns_protocol::kHttpsServiceParamKeyPort,
      dns_protocol::kHttpsServiceParamKeyIpv4Hint,
      dns_protocol::kHttpsServiceParamKeyEchConfig,
      dns_protocol::kHttpsServiceParamKeyIpv6Hint};

  ServiceFormHttpsRecordRdata(uint16_t priority,
                              std::string service_name,
                              std::set<uint16_t> mandatory_keys,
                              std::vector<std::string> alpn_ids,
                              bool default_alpn,
                              std::optional<uint16_t> port,
                              std::vector<IPAddress> ipv4_hint,
                              std::string ech_config,
                              std::vector<IPAddress> ipv6_hint,
                              std::map<uint16_t, std::string> unparsed_params);
  static std::unique_ptr<ServiceFormHttpsRecordRdata> Parse(
      std::string_view data);

  ~ServiceFormHttpsRecordRdata() override;

  bool IsEqual(const HttpsRecordRdata* other) const override;
  bool IsAlias() const override;

  HttpsRecordPriority priority() const { return priority_; }
  std::string_view service_name() const { return service_name_; }
  const std::set<uint16_t>& mandatory_keys() const { return mandatory_keys_; }
  const std::vector<std::string>& alpn_ids() const { return alpn_ids_; }
  bool default_alpn() const { return default_alpn_; }
  std::optional<uint16_t> port() const { return port_; }
  const std::vector<IPAddress>& ipv4_hint() const { return ipv4_hint_; }
  std::string_view ech_config() const { return ech_config_; }
  const std::vector<IPAddress>& ipv6_hint() const { return ipv6_hint_; }
  const std::map<uint16_t, std::string>& unparsed_params() const {
    return unparsed_params_;
  }

  // Returns whether or not this rdata parser is considered "compatible" with
  // the parsed rdata. That is that all keys listed by mandatory_keys() (and all
  // keys considered default mandatory for HTTPS records) are parsable by this
  // parser.
  bool IsCompatible() const;

 private:
  static bool IsSupportedKey(uint16_t key);

  const HttpsRecordPriority priority_;
  const std::string service_name_;

  // Supported service parameters.
  const std::set<uint16_t> mandatory_keys_;
  const std::vector<std::string> alpn_ids_;
  const bool default_alpn_;
  const std::optional<uint16_t> port_;
  const std::vector<IPAddress> ipv4_hint_;
  const std::string ech_config_;
  const std::vector<IPAddress> ipv6_hint_;

  const std::map<uint16_t, std::string> unparsed_params_;
};

}  // namespace net

#endif  // NET_DNS_HTTPS_RECORD_RDATA_H_
