// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_internal_result.h"

#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/json/values_util.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/dns/https_record_rdata.h"
#include "net/dns/public/dns_query_type.h"
#include "url/url_canon.h"
#include "url/url_canon_stdstring.h"

namespace net {

namespace {

// base::Value keys
constexpr std::string_view kValueDomainNameKey = "domain_name";
constexpr std::string_view kValueQueryTypeKey = "query_type";
constexpr std::string_view kValueTypeKey = "type";
constexpr std::string_view kValueSourceKey = "source";
constexpr std::string_view kValueTimedExpirationKey = "timed_expiration";
constexpr std::string_view kValueEndpointsKey = "endpoints";
constexpr std::string_view kValueStringsKey = "strings";
constexpr std::string_view kValueHostsKey = "hosts";
constexpr std::string_view kValueMetadatasKey = "metadatas";
constexpr std::string_view kValueMetadataWeightKey = "metadata_weight";
constexpr std::string_view kValueMetadataValueKey = "metadata_value";
constexpr std::string_view kValueErrorKey = "error";
constexpr std::string_view kValueAliasTargetKey = "alias_target";

// Returns `domain_name` as-is if it could not be canonicalized.
std::string MaybeCanonicalizeName(std::string domain_name) {
  std::string canonicalized;
  url::StdStringCanonOutput output(&canonicalized);
  url::CanonHostInfo host_info;

  url::CanonicalizeHostVerbose(domain_name.data(),
                               url::Component(0, domain_name.size()), &output,
                               &host_info);

  if (host_info.family == url::CanonHostInfo::Family::NEUTRAL) {
    output.Complete();
    return canonicalized;
  } else {
    return domain_name;
  }
}

base::Value EndpointMetadataPairToValue(
    const std::pair<HttpsRecordPriority, ConnectionEndpointMetadata>& pair) {
  base::Value::Dict dictionary;
  dictionary.Set(kValueMetadataWeightKey, pair.first);
  dictionary.Set(kValueMetadataValueKey, pair.second.ToValue());
  return base::Value(std::move(dictionary));
}

std::optional<std::pair<HttpsRecordPriority, ConnectionEndpointMetadata>>
EndpointMetadataPairFromValue(const base::Value& value) {
  const base::Value::Dict* dict = value.GetIfDict();
  if (!dict)
    return std::nullopt;

  std::optional<int> weight = dict->FindInt(kValueMetadataWeightKey);
  if (!weight || !base::IsValueInRangeForNumericType<HttpsRecordPriority>(
                     weight.value())) {
    return std::nullopt;
  }

  const base::Value* metadata_value = dict->Find(kValueMetadataValueKey);
  if (!metadata_value)
    return std::nullopt;
  std::optional<ConnectionEndpointMetadata> metadata =
      ConnectionEndpointMetadata::FromValue(*metadata_value);
  if (!metadata)
    return std::nullopt;

  return std::pair(base::checked_cast<HttpsRecordPriority>(weight.value()),
                   std::move(metadata).value());
}

std::optional<DnsQueryType> QueryTypeFromValue(const base::Value& value) {
  const std::string* query_type_string = value.GetIfString();
  if (!query_type_string)
    return std::nullopt;
  const auto query_type_it =
      base::ranges::find(kDnsQueryTypes, *query_type_string,
                         &decltype(kDnsQueryTypes)::value_type::second);
  if (query_type_it == kDnsQueryTypes.end())
    return std::nullopt;

  return query_type_it->first;
}

base::Value TypeToValue(HostResolverInternalResult::Type type) {
  switch (type) {
    case HostResolverInternalResult::Type::kData:
      return base::Value("data");
    case HostResolverInternalResult::Type::kMetadata:
      return base::Value("metadata");
    case HostResolverInternalResult::Type::kError:
      return base::Value("error");
    case HostResolverInternalResult::Type::kAlias:
      return base::Value("alias");
  }
}

std::optional<HostResolverInternalResult::Type> TypeFromValue(
    const base::Value& value) {
  const std::string* string = value.GetIfString();
  if (!string)
    return std::nullopt;

  if (*string == "data") {
    return HostResolverInternalResult::Type::kData;
  } else if (*string == "metadata") {
    return HostResolverInternalResult::Type::kMetadata;
  } else if (*string == "error") {
    return HostResolverInternalResult::Type::kError;
  } else if (*string == "alias") {
    return HostResolverInternalResult::Type::kAlias;
  } else {
    return std::nullopt;
  }
}

base::Value SourceToValue(HostResolverInternalResult::Source source) {
  switch (source) {
    case HostResolverInternalResult::Source::kDns:
      return base::Value("dns");
    case HostResolverInternalResult::Source::kHosts:
      return base::Value("hosts");
    case HostResolverInternalResult::Source::kUnknown:
      return base::Value("unknown");
  }
}

std::optional<HostResolverInternalResult::Source> SourceFromValue(
    const base::Value& value) {
  const std::string* string = value.GetIfString();
  if (!string)
    return std::nullopt;

  if (*string == "dns") {
    return HostResolverInternalResult::Source::kDns;
  } else if (*string == "hosts") {
    return HostResolverInternalResult::Source::kHosts;
  } else if (*string == "unknown") {
    return HostResolverInternalResult::Source::kUnknown;
  } else {
    return std::nullopt;
  }
}

}  // namespace

// static
std::unique_ptr<HostResolverInternalResult>
HostResolverInternalResult::FromValue(const base::Value& value) {
  const base::Value::Dict* dict = value.GetIfDict();
  if (!dict)
    return nullptr;

  const base::Value* type_value = dict->Find(kValueTypeKey);
  if (!type_value)
    return nullptr;
  std::optional<Type> type = TypeFromValue(*type_value);
  if (!type.has_value())
    return nullptr;

  switch (type.value()) {
    case Type::kData:
      return HostResolverInternalDataResult::FromValue(value);
    case Type::kMetadata:
      return HostResolverInternalMetadataResult::FromValue(value);
    case Type::kError:
      return HostResolverInternalErrorResult::FromValue(value);
    case Type::kAlias:
      return HostResolverInternalAliasResult::FromValue(value);
  }
}

const HostResolverInternalDataResult& HostResolverInternalResult::AsData()
    const {
  CHECK_EQ(type_, Type::kData);
  return *static_cast<const HostResolverInternalDataResult*>(this);
}

HostResolverInternalDataResult& HostResolverInternalResult::AsData() {
  CHECK_EQ(type_, Type::kData);
  return *static_cast<HostResolverInternalDataResult*>(this);
}

const HostResolverInternalMetadataResult&
HostResolverInternalResult::AsMetadata() const {
  CHECK_EQ(type_, Type::kMetadata);
  return *static_cast<const HostResolverInternalMetadataResult*>(this);
}

HostResolverInternalMetadataResult& HostResolverInternalResult::AsMetadata() {
  CHECK_EQ(type_, Type::kMetadata);
  return *static_cast<HostResolverInternalMetadataResult*>(this);
}

const HostResolverInternalErrorResult& HostResolverInternalResult::AsError()
    const {
  CHECK_EQ(type_, Type::kError);
  return *static_cast<const HostResolverInternalErrorResult*>(this);
}

HostResolverInternalErrorResult& HostResolverInternalResult::AsError() {
  CHECK_EQ(type_, Type::kError);
  return *static_cast<HostResolverInternalErrorResult*>(this);
}

const HostResolverInternalAliasResult& HostResolverInternalResult::AsAlias()
    const {
  CHECK_EQ(type_, Type::kAlias);
  return *static_cast<const HostResolverInternalAliasResult*>(this);
}

HostResolverInternalAliasResult& HostResolverInternalResult::AsAlias() {
  CHECK_EQ(type_, Type::kAlias);
  return *static_cast<HostResolverInternalAliasResult*>(this);
}

HostResolverInternalResult::HostResolverInternalResult(
    std::string domain_name,
    DnsQueryType query_type,
    std::optional<base::TimeTicks> expiration,
    std::optional<base::Time> timed_expiration,
    Type type,
    Source source)
    : domain_name_(MaybeCanonicalizeName(std::move(domain_name))),
      query_type_(query_type),
      type_(type),
      source_(source),
      expiration_(expiration),
      timed_expiration_(timed_expiration) {
  DCHECK(!domain_name_.empty());
  // If `expiration` has a value, `timed_expiration` must too.
  DCHECK(!expiration_.has_value() || timed_expiration.has_value());
}

HostResolverInternalResult::HostResolverInternalResult(
    const base::Value::Dict& dict)
    : domain_name_(*dict.FindString(kValueDomainNameKey)),
      query_type_(QueryTypeFromValue(*dict.Find(kValueQueryTypeKey)).value()),
      type_(TypeFromValue(*dict.Find(kValueTypeKey)).value()),
      source_(SourceFromValue(*dict.Find(kValueSourceKey)).value()),
      timed_expiration_(
          dict.contains(kValueTimedExpirationKey)
              ? base::ValueToTime(*dict.Find(kValueTimedExpirationKey))
              : std::optional<base::Time>()) {}

// static
bool HostResolverInternalResult::ValidateValueBaseDict(
    const base::Value::Dict& dict,
    bool require_timed_expiration) {
  const std::string* domain_name = dict.FindString(kValueDomainNameKey);
  if (!domain_name)
    return false;

  const std::string* query_type_string = dict.FindString(kValueQueryTypeKey);
  if (!query_type_string)
    return false;
  const auto query_type_it =
      base::ranges::find(kDnsQueryTypes, *query_type_string,
                         &decltype(kDnsQueryTypes)::value_type::second);
  if (query_type_it == kDnsQueryTypes.end())
    return false;

  const base::Value* type_value = dict.Find(kValueTypeKey);
  if (!type_value)
    return false;
  std::optional<Type> type = TypeFromValue(*type_value);
  if (!type.has_value())
    return false;

  const base::Value* source_value = dict.Find(kValueSourceKey);
  if (!source_value)
    return false;
  std::optional<Source> source = SourceFromValue(*source_value);
  if (!source.has_value())
    return false;

  std::optional<base::Time> timed_expiration;
  const base::Value* timed_expiration_value =
      dict.Find(kValueTimedExpirationKey);
  if (require_timed_expiration && !timed_expiration_value)
    return false;
  if (timed_expiration_value) {
    timed_expiration = base::ValueToTime(timed_expiration_value);
    if (!timed_expiration.has_value())
      return false;
  }

  return true;
}

base::Value::Dict HostResolverInternalResult::ToValueBaseDict() const {
  base::Value::Dict dict;

  dict.Set(kValueDomainNameKey, domain_name_);
  dict.Set(kValueQueryTypeKey, kDnsQueryTypes.at(query_type_));
  dict.Set(kValueTypeKey, TypeToValue(type_));
  dict.Set(kValueSourceKey, SourceToValue(source_));

  // `expiration_` is not serialized because it is TimeTicks.

  if (timed_expiration_.has_value()) {
    dict.Set(kValueTimedExpirationKey,
             base::TimeToValue(timed_expiration_.value()));
  }

  return dict;
}

// static
std::unique_ptr<HostResolverInternalDataResult>
HostResolverInternalDataResult::FromValue(const base::Value& value) {
  const base::Value::Dict* dict = value.GetIfDict();
  if (!dict || !ValidateValueBaseDict(*dict, /*require_timed_expiration=*/true))
    return nullptr;

  const base::Value::List* endpoint_values = dict->FindList(kValueEndpointsKey);
  if (!endpoint_values)
    return nullptr;

  std::vector<IPEndPoint> endpoints;
  endpoints.reserve(endpoint_values->size());
  for (const base::Value& endpoint_value : *endpoint_values) {
    std::optional<IPEndPoint> endpoint = IPEndPoint::FromValue(endpoint_value);
    if (!endpoint.has_value())
      return nullptr;

    endpoints.push_back(std::move(endpoint).value());
  }

  const base::Value::List* string_values = dict->FindList(kValueStringsKey);
  if (!string_values)
    return nullptr;

  std::vector<std::string> strings;
  strings.reserve(string_values->size());
  for (const base::Value& string_value : *string_values) {
    const std::string* string = string_value.GetIfString();
    if (!string)
      return nullptr;

    strings.push_back(*string);
  }

  const base::Value::List* host_values = dict->FindList(kValueHostsKey);
  if (!host_values)
    return nullptr;

  std::vector<HostPortPair> hosts;
  hosts.reserve(host_values->size());
  for (const base::Value& host_value : *host_values) {
    std::optional<HostPortPair> host = HostPortPair::FromValue(host_value);
    if (!host.has_value())
      return nullptr;

    hosts.push_back(std::move(host).value());
  }

  // WrapUnique due to private constructor.
  return base::WrapUnique(new HostResolverInternalDataResult(
      *dict, std::move(endpoints), std::move(strings), std::move(hosts)));
}

HostResolverInternalDataResult::HostResolverInternalDataResult(
    std::string domain_name,
    DnsQueryType query_type,
    std::optional<base::TimeTicks> expiration,
    base::Time timed_expiration,
    Source source,
    std::vector<IPEndPoint> endpoints,
    std::vector<std::string> strings,
    std::vector<HostPortPair> hosts)
    : HostResolverInternalResult(std::move(domain_name),
                                 query_type,
                                 expiration,
                                 timed_expiration,
                                 Type::kData,
                                 source),
      endpoints_(std::move(endpoints)),
      strings_(std::move(strings)),
      hosts_(std::move(hosts)) {
  DCHECK(!endpoints_.empty() || !strings_.empty() || !hosts_.empty());
}

HostResolverInternalDataResult::~HostResolverInternalDataResult() = default;

std::unique_ptr<HostResolverInternalResult>
HostResolverInternalDataResult::Clone() const {
  CHECK(timed_expiration().has_value());
  return std::make_unique<HostResolverInternalDataResult>(
      domain_name(), query_type(), expiration(), timed_expiration().value(),
      source(), endpoints(), strings(), hosts());
}

base::Value HostResolverInternalDataResult::ToValue() const {
  base::Value::Dict dict = ToValueBaseDict();

  base::Value::List endpoints_list;
  endpoints_list.reserve(endpoints_.size());
  for (IPEndPoint endpoint : endpoints_) {
    endpoints_list.Append(endpoint.ToValue());
  }
  dict.Set(kValueEndpointsKey, std::move(endpoints_list));

  base::Value::List strings_list;
  strings_list.reserve(strings_.size());
  for (const std::string& string : strings_) {
    strings_list.Append(string);
  }
  dict.Set(kValueStringsKey, std::move(strings_list));

  base::Value::List hosts_list;
  hosts_list.reserve(hosts_.size());
  for (const HostPortPair& host : hosts_) {
    hosts_list.Append(host.ToValue());
  }
  dict.Set(kValueHostsKey, std::move(hosts_list));

  return base::Value(std::move(dict));
}

HostResolverInternalDataResult::HostResolverInternalDataResult(
    const base::Value::Dict& dict,
    std::vector<IPEndPoint> endpoints,
    std::vector<std::string> strings,
    std::vector<HostPortPair> hosts)
    : HostResolverInternalResult(dict),
      endpoints_(std::move(endpoints)),
      strings_(std::move(strings)),
      hosts_(std::move(hosts)) {}

// static
std::unique_ptr<HostResolverInternalMetadataResult>
HostResolverInternalMetadataResult::FromValue(const base::Value& value) {
  const base::Value::Dict* dict = value.GetIfDict();
  if (!dict || !ValidateValueBaseDict(*dict, /*require_timed_expiration=*/true))
    return nullptr;

  const base::Value::List* metadata_values = dict->FindList(kValueMetadatasKey);
  if (!metadata_values)
    return nullptr;

  std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata> metadatas;
  for (const base::Value& metadata_value : *metadata_values) {
    std::optional<std::pair<HttpsRecordPriority, ConnectionEndpointMetadata>>
        metadata = EndpointMetadataPairFromValue(metadata_value);
    if (!metadata.has_value())
      return nullptr;
    metadatas.insert(std::move(metadata).value());
  }

  // WrapUnique due to private constructor.
  return base::WrapUnique(
      new HostResolverInternalMetadataResult(*dict, std::move(metadatas)));
}

HostResolverInternalMetadataResult::HostResolverInternalMetadataResult(
    std::string domain_name,
    DnsQueryType query_type,
    std::optional<base::TimeTicks> expiration,
    base::Time timed_expiration,
    Source source,
    std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata> metadatas)
    : HostResolverInternalResult(std::move(domain_name),
                                 query_type,
                                 expiration,
                                 timed_expiration,
                                 Type::kMetadata,
                                 source),
      metadatas_(std::move(metadatas)) {}

HostResolverInternalMetadataResult::~HostResolverInternalMetadataResult() =
    default;

std::unique_ptr<HostResolverInternalResult>
HostResolverInternalMetadataResult::Clone() const {
  CHECK(timed_expiration().has_value());
  return std::make_unique<HostResolverInternalMetadataResult>(
      domain_name(), query_type(), expiration(), timed_expiration().value(),
      source(), metadatas());
}

base::Value HostResolverInternalMetadataResult::ToValue() const {
  base::Value::Dict dict = ToValueBaseDict();

  base::Value::List metadatas_list;
  metadatas_list.reserve(metadatas_.size());
  for (const std::pair<const HttpsRecordPriority, ConnectionEndpointMetadata>&
           metadata_pair : metadatas_) {
    metadatas_list.Append(EndpointMetadataPairToValue(metadata_pair));
  }
  dict.Set(kValueMetadatasKey, std::move(metadatas_list));

  return base::Value(std::move(dict));
}

HostResolverInternalMetadataResult::HostResolverInternalMetadataResult(
    const base::Value::Dict& dict,
    std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata> metadatas)
    : HostResolverInternalResult(dict), metadatas_(std::move(metadatas)) {}

// static
std::unique_ptr<HostResolverInternalErrorResult>
HostResolverInternalErrorResult::FromValue(const base::Value& value) {
  const base::Value::Dict* dict = value.GetIfDict();
  if (!dict ||
      !ValidateValueBaseDict(*dict, /*require_timed_expiration=*/false)) {
    return nullptr;
  }

  std::optional<int> error = dict->FindInt(kValueErrorKey);
  if (!error.has_value())
    return nullptr;

  // WrapUnique due to private constructor.
  return base::WrapUnique(
      new HostResolverInternalErrorResult(*dict, error.value()));
}

HostResolverInternalErrorResult::HostResolverInternalErrorResult(
    std::string domain_name,
    DnsQueryType query_type,
    std::optional<base::TimeTicks> expiration,
    std::optional<base::Time> timed_expiration,
    Source source,
    int error)
    : HostResolverInternalResult(std::move(domain_name),
                                 query_type,
                                 expiration,
                                 timed_expiration,
                                 Type::kError,
                                 source),
      error_(error) {}

std::unique_ptr<HostResolverInternalResult>
HostResolverInternalErrorResult::Clone() const {
  return std::make_unique<HostResolverInternalErrorResult>(
      domain_name(), query_type(), expiration(), timed_expiration(), source(),
      error());
}

base::Value HostResolverInternalErrorResult::ToValue() const {
  base::Value::Dict dict = ToValueBaseDict();

  dict.Set(kValueErrorKey, error_);

  return base::Value(std::move(dict));
}

HostResolverInternalErrorResult::HostResolverInternalErrorResult(
    const base::Value::Dict& dict,
    int error)
    : HostResolverInternalResult(dict), error_(error) {
  DCHECK_NE(error_, OK);
}

// static
std::unique_ptr<HostResolverInternalAliasResult>
HostResolverInternalAliasResult::FromValue(const base::Value& value) {
  const base::Value::Dict* dict = value.GetIfDict();
  if (!dict || !ValidateValueBaseDict(*dict, /*require_timed_expiration=*/true))
    return nullptr;

  const std::string* target = dict->FindString(kValueAliasTargetKey);
  if (!target)
    return nullptr;

  // WrapUnique due to private constructor.
  return base::WrapUnique(new HostResolverInternalAliasResult(*dict, *target));
}

HostResolverInternalAliasResult::HostResolverInternalAliasResult(
    std::string domain_name,
    DnsQueryType query_type,
    std::optional<base::TimeTicks> expiration,
    base::Time timed_expiration,
    Source source,
    std::string alias_target)
    : HostResolverInternalResult(std::move(domain_name),
                                 query_type,
                                 expiration,
                                 timed_expiration,
                                 Type::kAlias,
                                 source),
      alias_target_(MaybeCanonicalizeName(std::move(alias_target))) {
  DCHECK(!alias_target_.empty());
}

std::unique_ptr<HostResolverInternalResult>
HostResolverInternalAliasResult::Clone() const {
  CHECK(timed_expiration().has_value());
  return std::make_unique<HostResolverInternalAliasResult>(
      domain_name(), query_type(), expiration(), timed_expiration().value(),
      source(), alias_target());
}

base::Value HostResolverInternalAliasResult::ToValue() const {
  base::Value::Dict dict = ToValueBaseDict();

  dict.Set(kValueAliasTargetKey, alias_target_);

  return base::Value(std::move(dict));
}

HostResolverInternalAliasResult::HostResolverInternalAliasResult(
    const base::Value::Dict& dict,
    std::string alias_target)
    : HostResolverInternalResult(dict),
      alias_target_(MaybeCanonicalizeName(std::move(alias_target))) {}

}  // namespace net
