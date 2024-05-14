// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_response_result_extractor.h"

#include <limits.h>
#include <stdint.h>

#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/dcheck_is_on.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/ostream_operators.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "net/base/address_list.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_alias_utility.h"
#include "net/dns/dns_names_util.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_util.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver_internal_result.h"
#include "net/dns/https_record_rdata.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/record_parsed.h"
#include "net/dns/record_rdata.h"

namespace net {

namespace {

using AliasMap = std::map<std::string,
                          std::unique_ptr<const RecordParsed>,
                          dns_names_util::DomainNameComparator>;
using ExtractionError = DnsResponseResultExtractor::ExtractionError;
using RecordsOrError =
    base::expected<std::vector<std::unique_ptr<const RecordParsed>>,
                   ExtractionError>;
using ResultsOrError = DnsResponseResultExtractor::ResultsOrError;
using Source = HostResolverInternalResult::Source;

void SaveMetricsForAdditionalHttpsRecord(const RecordParsed& record,
                                         bool is_unsolicited) {
  const HttpsRecordRdata* rdata = record.rdata<HttpsRecordRdata>();
  DCHECK(rdata);

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class UnsolicitedHttpsRecordStatus {
    kMalformed = 0,  // No longer recorded.
    kAlias = 1,
    kService = 2,
    kMaxValue = kService
  } status;

  if (rdata->IsAlias()) {
    status = UnsolicitedHttpsRecordStatus::kAlias;
  } else {
    status = UnsolicitedHttpsRecordStatus::kService;
  }

  if (is_unsolicited) {
    UMA_HISTOGRAM_ENUMERATION("Net.DNS.DnsTask.AdditionalHttps.Unsolicited",
                              status);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Net.DNS.DnsTask.AdditionalHttps.Requested",
                              status);
  }
}

// Sort service targets per RFC2782.  In summary, sort first by `priority`,
// lowest first.  For targets with the same priority, secondary sort randomly
// using `weight` with higher weighted objects more likely to go first.
std::vector<HostPortPair> SortServiceTargets(
    const std::vector<const SrvRecordRdata*>& rdatas) {
  std::map<uint16_t, std::unordered_set<const SrvRecordRdata*>>
      ordered_by_priority;
  for (const SrvRecordRdata* rdata : rdatas) {
    ordered_by_priority[rdata->priority()].insert(rdata);
  }

  std::vector<HostPortPair> sorted_targets;
  for (auto& priority : ordered_by_priority) {
    // With (num results) <= UINT16_MAX (and in practice, much less) and
    // (weight per result) <= UINT16_MAX, then it should be the case that
    // (total weight) <= UINT32_MAX, but use CheckedNumeric for extra safety.
    auto total_weight = base::MakeCheckedNum<uint32_t>(0);
    for (const SrvRecordRdata* rdata : priority.second) {
      total_weight += rdata->weight();
    }

    // Add 1 to total weight because, to deal with 0-weight targets, we want
    // our random selection to be inclusive [0, total].
    total_weight++;

    // Order by weighted random. Make such random selections, removing from
    // |priority.second| until |priority.second| only contains 1 rdata.
    while (priority.second.size() >= 2) {
      uint32_t random_selection =
          base::RandGenerator(total_weight.ValueOrDie());
      const SrvRecordRdata* selected_rdata = nullptr;
      for (const SrvRecordRdata* rdata : priority.second) {
        // >= to always select the first target on |random_selection| == 0,
        // even if its weight is 0.
        if (rdata->weight() >= random_selection) {
          selected_rdata = rdata;
          break;
        }
        random_selection -= rdata->weight();
      }

      DCHECK(selected_rdata);
      sorted_targets.emplace_back(selected_rdata->target(),
                                  selected_rdata->port());
      total_weight -= selected_rdata->weight();
      size_t removed = priority.second.erase(selected_rdata);
      DCHECK_EQ(1u, removed);
    }

    DCHECK_EQ(1u, priority.second.size());
    DCHECK_EQ((total_weight - 1).ValueOrDie(),
              (*priority.second.begin())->weight());
    const SrvRecordRdata* rdata = *priority.second.begin();
    sorted_targets.emplace_back(rdata->target(), rdata->port());
  }

  return sorted_targets;
}

// Validates that all `aliases` form a single non-looping chain, starting from
// `query_name` and that all alias records are valid. Also validates that all
// `data_records` are at the final name at the end of the alias chain.
// TODO(crbug.com/40245250): Consider altering chain TTLs so that each TTL is
// less than or equal to all previous links in the chain.
ExtractionError ValidateNamesAndAliases(
    std::string_view query_name,
    const AliasMap& aliases,
    const std::vector<std::unique_ptr<const RecordParsed>>& data_records,
    std::string& out_final_chain_name) {
  // Validate that all aliases form a single non-looping chain, starting from
  // `query_name`.
  size_t aliases_in_chain = 0;
  std::string target_name =
      dns_names_util::UrlCanonicalizeNameIfAble(query_name);
  for (auto alias = aliases.find(target_name);
       alias != aliases.end() && aliases_in_chain <= aliases.size();
       alias = aliases.find(target_name)) {
    aliases_in_chain++;

    const CnameRecordRdata* cname_data =
        alias->second->rdata<CnameRecordRdata>();
    if (!cname_data) {
      return ExtractionError::kMalformedCname;
    }

    target_name =
        dns_names_util::UrlCanonicalizeNameIfAble(cname_data->cname());
    if (!dns_names_util::IsValidDnsRecordName(target_name)) {
      return ExtractionError::kMalformedCname;
    }
  }

  if (aliases_in_chain != aliases.size()) {
    return ExtractionError::kBadAliasChain;
  }

  // All records must match final alias name.
  for (const auto& record : data_records) {
    DCHECK_NE(record->type(), dns_protocol::kTypeCNAME);
    if (!base::EqualsCaseInsensitiveASCII(
            target_name,
            dns_names_util::UrlCanonicalizeNameIfAble(record->name()))) {
      return ExtractionError::kNameMismatch;
    }
  }

  out_final_chain_name = std::move(target_name);
  return ExtractionError::kOk;
}

// Common results (aliases and errors) are extracted into
// `out_non_data_results`.
RecordsOrError ExtractResponseRecords(
    const DnsResponse& response,
    DnsQueryType query_type,
    base::Time now,
    base::TimeTicks now_ticks,
    std::set<std::unique_ptr<HostResolverInternalResult>>&
        out_non_data_results) {
  DCHECK_EQ(response.question_count(), 1u);

  std::vector<std::unique_ptr<const RecordParsed>> data_records;
  std::optional<base::TimeDelta> response_ttl;

  DnsRecordParser parser = response.Parser();

  // Expected to be validated by DnsTransaction.
  DCHECK_EQ(DnsQueryTypeToQtype(query_type), response.GetSingleQType());

  AliasMap aliases;
  for (unsigned i = 0; i < response.answer_count(); ++i) {
    std::unique_ptr<const RecordParsed> record =
        RecordParsed::CreateFrom(&parser, now);

    if (!record || !dns_names_util::IsValidDnsRecordName(record->name())) {
      return base::unexpected(ExtractionError::kMalformedRecord);
    }

    if (record->klass() == dns_protocol::kClassIN &&
        record->type() == dns_protocol::kTypeCNAME) {
      std::string canonicalized_name =
          dns_names_util::UrlCanonicalizeNameIfAble(record->name());
      DCHECK(dns_names_util::IsValidDnsRecordName(canonicalized_name));

      bool added =
          aliases.emplace(canonicalized_name, std::move(record)).second;
      // Per RFC2181, multiple CNAME records are not allowed for the same name.
      if (!added) {
        return base::unexpected(ExtractionError::kMultipleCnames);
      }
    } else if (record->klass() == dns_protocol::kClassIN &&
               record->type() == DnsQueryTypeToQtype(query_type)) {
      base::TimeDelta ttl = base::Seconds(record->ttl());
      response_ttl =
          std::min(response_ttl.value_or(base::TimeDelta::Max()), ttl);

      data_records.push_back(std::move(record));
    }
  }

  std::string final_chain_name;
  ExtractionError name_and_alias_validation_error = ValidateNamesAndAliases(
      response.GetSingleDottedName(), aliases, data_records, final_chain_name);
  if (name_and_alias_validation_error != ExtractionError::kOk) {
    return base::unexpected(name_and_alias_validation_error);
  }

  std::set<std::unique_ptr<HostResolverInternalResult>> non_data_results;
  for (const auto& alias : aliases) {
    DCHECK(alias.second->rdata<CnameRecordRdata>());
    non_data_results.insert(std::make_unique<HostResolverInternalAliasResult>(
        alias.first, query_type, now_ticks + base::Seconds(alias.second->ttl()),
        now + base::Seconds(alias.second->ttl()), Source::kDns,
        alias.second->rdata<CnameRecordRdata>()->cname()));
  }

  std::optional<base::TimeDelta> error_ttl;
  for (unsigned i = 0; i < response.authority_count(); ++i) {
    DnsResourceRecord record;
    if (!parser.ReadRecord(&record)) {
      // Stop trying to process records if things get malformed in the authority
      // section.
      break;
    }

    if (record.type == dns_protocol::kTypeSOA) {
      base::TimeDelta ttl = base::Seconds(record.ttl);
      error_ttl = std::min(error_ttl.value_or(base::TimeDelta::Max()), ttl);
    }
  }

  // For NXDOMAIN or NODATA (NOERROR with 0 answers matching the qtype), cache
  // an error if an error TTL was found from SOA records. Also, ignore the error
  // if we somehow have result records (most likely if the server incorrectly
  // sends NXDOMAIN with results). Note that, per the weird QNAME definition in
  // RFC2308, section 1, as well as the clarifications in RFC6604, section 3,
  // and in RFC8020, section 2, the cached error is specific to the final chain
  // name, not the query name.
  //
  // TODO(ericorth@chromium.org): Differentiate nxdomain errors by making it
  // cacheable across any query type (per RFC2308, Section 5).
  bool is_cachable_error = data_records.empty() &&
                           (response.rcode() == dns_protocol::kRcodeNXDOMAIN ||
                            response.rcode() == dns_protocol::kRcodeNOERROR);
  if (is_cachable_error && error_ttl.has_value()) {
    non_data_results.insert(std::make_unique<HostResolverInternalErrorResult>(
        final_chain_name, query_type, now_ticks + error_ttl.value(),
        now + error_ttl.value(), Source::kDns, ERR_NAME_NOT_RESOLVED));
  }

  for (unsigned i = 0; i < response.additional_answer_count(); ++i) {
    std::unique_ptr<const RecordParsed> record =
        RecordParsed::CreateFrom(&parser, base::Time::Now());
    if (record && record->klass() == dns_protocol::kClassIN &&
        record->type() == dns_protocol::kTypeHttps) {
      bool is_unsolicited = query_type != DnsQueryType::HTTPS;
      SaveMetricsForAdditionalHttpsRecord(*record, is_unsolicited);
    }
  }

  out_non_data_results = std::move(non_data_results);
  return data_records;
}

ResultsOrError ExtractAddressResults(const DnsResponse& response,
                                     DnsQueryType query_type,
                                     base::Time now,
                                     base::TimeTicks now_ticks) {
  DCHECK_EQ(response.question_count(), 1u);
  DCHECK(query_type == DnsQueryType::A || query_type == DnsQueryType::AAAA);

  std::set<std::unique_ptr<HostResolverInternalResult>> results;
  RecordsOrError records =
      ExtractResponseRecords(response, query_type, now, now_ticks, results);
  if (!records.has_value()) {
    return base::unexpected(records.error());
  }

  std::vector<IPEndPoint> ip_endpoints;
  auto min_ttl = base::TimeDelta::Max();
  for (const auto& record : records.value()) {
    IPAddress address;
    if (query_type == DnsQueryType::A) {
      const ARecordRdata* rdata = record->rdata<ARecordRdata>();
      DCHECK(rdata);
      address = rdata->address();
      DCHECK(address.IsIPv4());
    } else {
      DCHECK_EQ(query_type, DnsQueryType::AAAA);
      const AAAARecordRdata* rdata = record->rdata<AAAARecordRdata>();
      DCHECK(rdata);
      address = rdata->address();
      DCHECK(address.IsIPv6());
    }
    ip_endpoints.emplace_back(address, /*port=*/0);

    base::TimeDelta ttl = base::Seconds(record->ttl());
    min_ttl = std::min(ttl, min_ttl);
  }

  if (!ip_endpoints.empty()) {
    results.insert(std::make_unique<HostResolverInternalDataResult>(
        records->front()->name(), query_type, now_ticks + min_ttl,
        now + min_ttl, Source::kDns, std::move(ip_endpoints),
        std::vector<std::string>{}, std::vector<HostPortPair>{}));
  }

  return results;
}

ResultsOrError ExtractTxtResults(const DnsResponse& response,
                                 base::Time now,
                                 base::TimeTicks now_ticks) {
  std::set<std::unique_ptr<HostResolverInternalResult>> results;
  RecordsOrError txt_records = ExtractResponseRecords(
      response, DnsQueryType::TXT, now, now_ticks, results);
  if (!txt_records.has_value()) {
    return base::unexpected(txt_records.error());
  }

  std::vector<std::string> strings;
  base::TimeDelta min_ttl = base::TimeDelta::Max();
  for (const auto& record : txt_records.value()) {
    const TxtRecordRdata* rdata = record->rdata<net::TxtRecordRdata>();
    DCHECK(rdata);
    strings.insert(strings.end(), rdata->texts().begin(), rdata->texts().end());

    base::TimeDelta ttl = base::Seconds(record->ttl());
    min_ttl = std::min(ttl, min_ttl);
  }

  if (!strings.empty()) {
    results.insert(std::make_unique<HostResolverInternalDataResult>(
        txt_records->front()->name(), DnsQueryType::TXT, now_ticks + min_ttl,
        now + min_ttl, Source::kDns, std::vector<IPEndPoint>{},
        std::move(strings), std::vector<HostPortPair>{}));
  }

  return results;
}

ResultsOrError ExtractPointerResults(const DnsResponse& response,
                                     base::Time now,
                                     base::TimeTicks now_ticks) {
  std::set<std::unique_ptr<HostResolverInternalResult>> results;
  RecordsOrError ptr_records = ExtractResponseRecords(
      response, DnsQueryType::PTR, now, now_ticks, results);
  if (!ptr_records.has_value()) {
    return base::unexpected(ptr_records.error());
  }

  std::vector<HostPortPair> pointers;
  auto min_ttl = base::TimeDelta::Max();
  for (const auto& record : ptr_records.value()) {
    const PtrRecordRdata* rdata = record->rdata<net::PtrRecordRdata>();
    DCHECK(rdata);
    std::string pointer = rdata->ptrdomain();

    // Skip pointers to the root domain.
    if (!pointer.empty()) {
      pointers.emplace_back(std::move(pointer), 0);

      base::TimeDelta ttl = base::Seconds(record->ttl());
      min_ttl = std::min(ttl, min_ttl);
    }
  }

  if (!pointers.empty()) {
    results.insert(std::make_unique<HostResolverInternalDataResult>(
        ptr_records->front()->name(), DnsQueryType::PTR, now_ticks + min_ttl,
        now + min_ttl, Source::kDns, std::vector<IPEndPoint>{},
        std::vector<std::string>{}, std::move(pointers)));
  }

  return results;
}

ResultsOrError ExtractServiceResults(const DnsResponse& response,
                                     base::Time now,
                                     base::TimeTicks now_ticks) {
  std::set<std::unique_ptr<HostResolverInternalResult>> results;
  RecordsOrError srv_records = ExtractResponseRecords(
      response, DnsQueryType::SRV, now, now_ticks, results);
  if (!srv_records.has_value()) {
    return base::unexpected(srv_records.error());
  }

  std::vector<const SrvRecordRdata*> fitered_rdatas;
  auto min_ttl = base::TimeDelta::Max();
  for (const auto& record : srv_records.value()) {
    const SrvRecordRdata* rdata = record->rdata<net::SrvRecordRdata>();
    DCHECK(rdata);

    // Skip pointers to the root domain.
    if (!rdata->target().empty()) {
      fitered_rdatas.push_back(rdata);

      base::TimeDelta ttl = base::Seconds(record->ttl());
      min_ttl = std::min(ttl, min_ttl);
    }
  }

  std::vector<HostPortPair> ordered_service_targets =
      SortServiceTargets(fitered_rdatas);

  if (!ordered_service_targets.empty()) {
    results.insert(std::make_unique<HostResolverInternalDataResult>(
        srv_records->front()->name(), DnsQueryType::SRV, now_ticks + min_ttl,
        now + min_ttl, Source::kDns, std::vector<IPEndPoint>{},
        std::vector<std::string>{}, std::move(ordered_service_targets)));
  }

  return results;
}

const RecordParsed* UnwrapRecordPtr(
    const std::unique_ptr<const RecordParsed>& ptr) {
  return ptr.get();
}

bool RecordIsAlias(const RecordParsed* record) {
  DCHECK(record->rdata<HttpsRecordRdata>());
  return record->rdata<HttpsRecordRdata>()->IsAlias();
}

ResultsOrError ExtractHttpsResults(const DnsResponse& response,
                                   std::string_view original_domain_name,
                                   uint16_t request_port,
                                   base::Time now,
                                   base::TimeTicks now_ticks) {
  DCHECK(!original_domain_name.empty());

  std::set<std::unique_ptr<HostResolverInternalResult>> results;
  RecordsOrError https_records = ExtractResponseRecords(
      response, DnsQueryType::HTTPS, now, now_ticks, results);
  if (!https_records.has_value()) {
    return base::unexpected(https_records.error());
  }

  // Min TTL among records of full use to Chrome.
  std::optional<base::TimeDelta> min_ttl;

  // Min TTL among all records considered compatible with Chrome, per
  // RFC9460#section-8.
  std::optional<base::TimeDelta> min_compatible_ttl;

  std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata> metadatas;
  bool compatible_record_found = false;
  bool default_alpn_found = false;
  for (const auto& record : https_records.value()) {
    const HttpsRecordRdata* rdata = record->rdata<HttpsRecordRdata>();
    DCHECK(rdata);

    base::TimeDelta ttl = base::Seconds(record->ttl());

    // Chrome does not yet support alias records.
    if (rdata->IsAlias()) {
      // Alias records are always considered compatible because they do not
      // support "mandatory" params.
      compatible_record_found = true;
      min_compatible_ttl =
          std::min(ttl, min_compatible_ttl.value_or(base::TimeDelta::Max()));

      continue;
    }

    const ServiceFormHttpsRecordRdata* service = rdata->AsServiceForm();
    if (service->IsCompatible()) {
      compatible_record_found = true;
      min_compatible_ttl =
          std::min(ttl, min_compatible_ttl.value_or(base::TimeDelta::Max()));
    } else {
      // Ignore services incompatible with Chrome's HTTPS record parser.
      // draft-ietf-dnsop-svcb-https-12#section-8
      continue;
    }

    std::string target_name = dns_names_util::UrlCanonicalizeNameIfAble(
        service->service_name().empty() ? record->name()
                                        : service->service_name());

    // Chrome does not yet support followup queries. So only support services at
    // the original domain name or the canonical name (the record name).
    // Note: HostCache::Entry::GetEndpoints() will not return metadatas which
    // target name is different from the canonical name of A/AAAA query results.
    if (!base::EqualsCaseInsensitiveASCII(
            target_name,
            dns_names_util::UrlCanonicalizeNameIfAble(original_domain_name)) &&
        !base::EqualsCaseInsensitiveASCII(
            target_name,
            dns_names_util::UrlCanonicalizeNameIfAble(record->name()))) {
      continue;
    }

    // Ignore services at a different port from the request port. Chrome does
    // not yet support endpoints diverging by port.  Note that before supporting
    // port redirects, Chrome must ensure redirects to the "bad port list" are
    // disallowed. Unclear if such logic would belong here or in socket
    // connection logic.
    if (service->port().has_value() &&
        service->port().value() != request_port) {
      continue;
    }

    ConnectionEndpointMetadata metadata;

    metadata.supported_protocol_alpns = service->alpn_ids();
    if (service->default_alpn() &&
        !base::Contains(metadata.supported_protocol_alpns,
                        dns_protocol::kHttpsServiceDefaultAlpn)) {
      metadata.supported_protocol_alpns.push_back(
          dns_protocol::kHttpsServiceDefaultAlpn);
    }

    // Services with no supported ALPNs (those with "no-default-alpn" and no or
    // empty "alpn") are not self-consistent and are rejected.
    // draft-ietf-dnsop-svcb-https-12#section-7.1.1 and
    // draft-ietf-dnsop-svcb-https-12#section-2.4.3.
    if (metadata.supported_protocol_alpns.empty()) {
      continue;
    }

    metadata.ech_config_list = ConnectionEndpointMetadata::EchConfigList(
        service->ech_config().cbegin(), service->ech_config().cend());

    metadata.target_name = std::move(target_name);

    metadatas.emplace(service->priority(), std::move(metadata));

    min_ttl = std::min(ttl, min_ttl.value_or(base::TimeDelta::Max()));

    if (service->default_alpn()) {
      default_alpn_found = true;
    }
  }

  // Ignore all records if any are an alias record. Chrome does not yet support
  // alias records, but aliases take precedence over any other records.
  if (base::ranges::any_of(https_records.value(), &RecordIsAlias,
                           &UnwrapRecordPtr)) {
    metadatas.clear();
  }

  // Ignore all records if they all mark "no-default-alpn". Domains should
  // always provide at least one endpoint allowing default ALPN to ensure a
  // reasonable expectation of connection success.
  // draft-ietf-dnsop-svcb-https-12#section-7.1.2
  if (!default_alpn_found) {
    metadatas.clear();
  }

  if (metadatas.empty() && compatible_record_found) {
    // Empty metadata result signifies that compatible HTTPS records were
    // received but with no contained metadata of use to Chrome. Use the min TTL
    // of all compatible records.
    CHECK(min_compatible_ttl.has_value());
    results.insert(std::make_unique<HostResolverInternalMetadataResult>(
        https_records->front()->name(), DnsQueryType::HTTPS,
        now_ticks + min_compatible_ttl.value(),
        now + min_compatible_ttl.value(), Source::kDns,
        /*metadatas=*/
        std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata>{}));
  } else if (!metadatas.empty()) {
    // Use min TTL only of those records contributing useful metadata.
    CHECK(min_ttl.has_value());
    results.insert(std::make_unique<HostResolverInternalMetadataResult>(
        https_records->front()->name(), DnsQueryType::HTTPS,
        now_ticks + min_ttl.value(), now + min_ttl.value(), Source::kDns,
        std::move(metadatas)));
  }

  return results;
}

}  // namespace

DnsResponseResultExtractor::DnsResponseResultExtractor(
    const DnsResponse& response,
    const base::Clock& clock,
    const base::TickClock& tick_clock)
    : response_(response), clock_(clock), tick_clock_(tick_clock) {}

DnsResponseResultExtractor::~DnsResponseResultExtractor() = default;

ResultsOrError DnsResponseResultExtractor::ExtractDnsResults(
    DnsQueryType query_type,
    std::string_view original_domain_name,
    uint16_t request_port) const {
  DCHECK(!original_domain_name.empty());

  switch (query_type) {
    case DnsQueryType::UNSPECIFIED:
      // Should create multiple transactions with specified types.
      NOTREACHED_IN_MIGRATION();
      return base::unexpected(ExtractionError::kUnexpected);
    case DnsQueryType::A:
    case DnsQueryType::AAAA:
      return ExtractAddressResults(*response_, query_type, clock_->Now(),
                                   tick_clock_->NowTicks());
    case DnsQueryType::TXT:
      return ExtractTxtResults(*response_, clock_->Now(),
                               tick_clock_->NowTicks());
    case DnsQueryType::PTR:
      return ExtractPointerResults(*response_, clock_->Now(),
                                   tick_clock_->NowTicks());
    case DnsQueryType::SRV:
      return ExtractServiceResults(*response_, clock_->Now(),
                                   tick_clock_->NowTicks());
    case DnsQueryType::HTTPS:
      return ExtractHttpsResults(*response_, original_domain_name, request_port,
                                 clock_->Now(), tick_clock_->NowTicks());
  }
}

}  // namespace net
