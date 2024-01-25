// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_HOST_RESOLVER_INTERNAL_RESULT_H_
#define NET_DNS_HOST_RESOLVER_INTERNAL_RESULT_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "base/time/time.h"
#include "base/values.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/dns/https_record_rdata.h"
#include "net/dns/public/dns_query_type.h"

namespace net {

class HostResolverInternalDataResult;
class HostResolverInternalMetadataResult;
class HostResolverInternalErrorResult;
class HostResolverInternalAliasResult;

// Parsed and extracted result type for use internally to HostResolver code.
class NET_EXPORT_PRIVATE HostResolverInternalResult {
 public:
  enum class Type { kData, kMetadata, kError, kAlias };
  enum class Source { kDns, kHosts, kUnknown };

  // Nullptr if `value` is malformed to be deserialized.
  static std::unique_ptr<HostResolverInternalResult> FromValue(
      const base::Value& value);

  virtual ~HostResolverInternalResult() = default;

  const std::string& domain_name() const { return domain_name_; }
  DnsQueryType query_type() const { return query_type_; }
  Type type() const { return type_; }
  Source source() const { return source_; }
  std::optional<base::TimeTicks> expiration() const { return expiration_; }
  std::optional<base::Time> timed_expiration() const {
    return timed_expiration_;
  }

  const HostResolverInternalDataResult& AsData() const;
  HostResolverInternalDataResult& AsData();
  const HostResolverInternalMetadataResult& AsMetadata() const;
  HostResolverInternalMetadataResult& AsMetadata();
  const HostResolverInternalErrorResult& AsError() const;
  HostResolverInternalErrorResult& AsError();
  const HostResolverInternalAliasResult& AsAlias() const;
  HostResolverInternalAliasResult& AsAlias();

  virtual std::unique_ptr<HostResolverInternalResult> Clone() const = 0;

  virtual base::Value ToValue() const = 0;

 protected:
  HostResolverInternalResult(std::string domain_name,
                             DnsQueryType query_type,
                             std::optional<base::TimeTicks> expiration,
                             std::optional<base::Time> timed_expiration,
                             Type type,
                             Source source);
  // Expect to only be called with a `dict` well-formed for deserialization. Can
  // be checked via ValidateValueBaseDict().
  explicit HostResolverInternalResult(const base::Value::Dict& dict);

  bool operator==(const HostResolverInternalResult& other) const {
    return std::tie(domain_name_, query_type_, type_, source_, expiration_,
                    timed_expiration_) ==
           std::tie(other.domain_name_, other.query_type_, other.type_,
                    other.source_, other.expiration_, other.timed_expiration_);
  }

  static bool ValidateValueBaseDict(const base::Value::Dict& dict,
                                    bool require_timed_expiration);
  base::Value::Dict ToValueBaseDict() const;

 private:
  const std::string domain_name_;
  const DnsQueryType query_type_;
  const Type type_;
  const Source source_;

  // Expiration logic should prefer to be based on `expiration_` for correctness
  // through system time changes. But if result has been serialized to disk, it
  // may be that only `timed_expiration_` is available.
  const std::optional<base::TimeTicks> expiration_;
  const std::optional<base::Time> timed_expiration_;
};

// Parsed and extracted result containing result data.
class NET_EXPORT_PRIVATE HostResolverInternalDataResult final
    : public HostResolverInternalResult {
 public:
  static std::unique_ptr<HostResolverInternalDataResult> FromValue(
      const base::Value& value);

  // `domain_name` is dotted form.
  HostResolverInternalDataResult(std::string domain_name,
                                 DnsQueryType query_type,
                                 std::optional<base::TimeTicks> expiration,
                                 base::Time timed_expiration,
                                 Source source,
                                 std::vector<IPEndPoint> endpoints,
                                 std::vector<std::string> strings,
                                 std::vector<HostPortPair> hosts);
  ~HostResolverInternalDataResult() override;

  HostResolverInternalDataResult(const HostResolverInternalDataResult&) =
      delete;
  HostResolverInternalDataResult& operator=(
      const HostResolverInternalDataResult&) = delete;

  bool operator==(const HostResolverInternalDataResult& other) const {
    return HostResolverInternalResult::operator==(other) &&
           std::tie(endpoints_, strings_, hosts_) ==
               std::tie(other.endpoints_, other.strings_, other.hosts_);
  }

  const std::vector<IPEndPoint>& endpoints() const { return endpoints_; }
  void set_endpoints(std::vector<IPEndPoint> endpoints) {
    endpoints_ = std::move(endpoints);
  }
  const std::vector<std::string>& strings() const { return strings_; }
  void set_strings(std::vector<std::string> strings) {
    strings_ = std::move(strings);
  }
  const std::vector<HostPortPair>& hosts() const { return hosts_; }
  void set_hosts(std::vector<HostPortPair> hosts) { hosts_ = std::move(hosts); }

  std::unique_ptr<HostResolverInternalResult> Clone() const override;

  base::Value ToValue() const override;

 private:
  HostResolverInternalDataResult(const base::Value::Dict& dict,
                                 std::vector<IPEndPoint> endpoints,
                                 std::vector<std::string> strings,
                                 std::vector<HostPortPair> hosts);

  // Corresponds to the `HostResolverEndpointResult::ip_endpoints` portion of
  // `HostResolver::ResolveHostRequest::GetEndpointResults()`.
  std::vector<IPEndPoint> endpoints_;

  // Corresponds to `HostResolver::ResolveHostRequest::GetTextResults()`.
  std::vector<std::string> strings_;

  // Corresponds to `HostResolver::ResolveHostRequest::GetHostnameResults()`.
  std::vector<HostPortPair> hosts_;
};

// Parsed and extracted connection metadata, but not usable on its own without
// being paired with separate HostResolverInternalDataResult data (for the
// domain name specified by `ConnectionEndpointMetadata::target_name`). An empty
// metadata result signifies that compatible HTTPS records were received but
// with no contained metadata of use to Chrome.
class NET_EXPORT_PRIVATE HostResolverInternalMetadataResult final
    : public HostResolverInternalResult {
 public:
  static std::unique_ptr<HostResolverInternalMetadataResult> FromValue(
      const base::Value& value);

  // `domain_name` and `data_domain` are dotted form domain names.
  HostResolverInternalMetadataResult(
      std::string domain_name,
      DnsQueryType query_type,
      std::optional<base::TimeTicks> expiration,
      base::Time timed_expiration,
      Source source,
      std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata> metadatas);
  ~HostResolverInternalMetadataResult() override;

  HostResolverInternalMetadataResult(
      const HostResolverInternalMetadataResult&) = delete;
  HostResolverInternalMetadataResult& operator=(
      const HostResolverInternalMetadataResult&) = delete;

  bool operator==(const HostResolverInternalMetadataResult& other) const {
    return HostResolverInternalResult::operator==(other) &&
           metadatas_ == other.metadatas_;
  }

  const std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata>&
  metadatas() const {
    return metadatas_;
  }

  std::unique_ptr<HostResolverInternalResult> Clone() const override;

  base::Value ToValue() const override;

 private:
  HostResolverInternalMetadataResult(
      const base::Value::Dict& dict,
      std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata> metadatas);

  std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata> metadatas_;
};

// Parsed and extracted error.
class NET_EXPORT_PRIVATE HostResolverInternalErrorResult final
    : public HostResolverInternalResult {
 public:
  static std::unique_ptr<HostResolverInternalErrorResult> FromValue(
      const base::Value& value);

  // `domain_name` is dotted form. `timed_expiration` may be `nullopt` for
  // non-cacheable errors.
  HostResolverInternalErrorResult(std::string domain_name,
                                  DnsQueryType query_type,
                                  std::optional<base::TimeTicks> expiration,
                                  std::optional<base::Time> timed_expiration,
                                  Source source,
                                  int error);
  ~HostResolverInternalErrorResult() override = default;

  HostResolverInternalErrorResult(const HostResolverInternalErrorResult&) =
      delete;
  HostResolverInternalErrorResult& operator=(
      const HostResolverInternalErrorResult&) = delete;

  bool operator==(const HostResolverInternalErrorResult& other) const {
    return HostResolverInternalResult::operator==(other) &&
           error_ == other.error_;
  }

  int error() const { return error_; }

  std::unique_ptr<HostResolverInternalResult> Clone() const override;

  base::Value ToValue() const override;

 private:
  HostResolverInternalErrorResult(const base::Value::Dict& dict, int error);

  const int error_;
};

// Parsed and extracted alias (CNAME or alias-type HTTPS).
class NET_EXPORT_PRIVATE HostResolverInternalAliasResult final
    : public HostResolverInternalResult {
 public:
  static std::unique_ptr<HostResolverInternalAliasResult> FromValue(
      const base::Value& value);

  // `domain_name` and `alias_target` are dotted form domain names.
  HostResolverInternalAliasResult(std::string domain_name,
                                  DnsQueryType query_type,
                                  std::optional<base::TimeTicks> expiration,
                                  base::Time timed_expiration,
                                  Source source,
                                  std::string alias_target);
  ~HostResolverInternalAliasResult() override = default;

  HostResolverInternalAliasResult(const HostResolverInternalAliasResult&) =
      delete;
  HostResolverInternalAliasResult& operator=(
      const HostResolverInternalAliasResult&) = delete;

  bool operator==(const HostResolverInternalAliasResult& other) const {
    return HostResolverInternalResult::operator==(other) &&
           alias_target_ == other.alias_target_;
  }

  const std::string& alias_target() const { return alias_target_; }

  std::unique_ptr<HostResolverInternalResult> Clone() const override;

  base::Value ToValue() const override;

 private:
  HostResolverInternalAliasResult(const base::Value::Dict& dict,
                                  std::string alias_target);

  const std::string alias_target_;
};

}  // namespace net

#endif  // NET_DNS_HOST_RESOLVER_INTERNAL_RESULT_H_
