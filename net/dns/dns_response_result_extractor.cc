// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_response_result_extractor.h"

#include <limits.h>
#include <stdint.h>

#include <iterator>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <string>
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
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
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
#include "net/dns/https_record_rdata.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/record_parsed.h"
#include "net/dns/record_rdata.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

namespace {

using AliasMap =
    std::map<std::string, std::string, dns_names_util::DomainNameComparator>;
using ExtractionError = DnsResponseResultExtractor::ExtractionError;

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
  for (const SrvRecordRdata* rdata : rdatas)
    ordered_by_priority[rdata->priority()].insert(rdata);

  std::vector<HostPortPair> sorted_targets;
  for (auto& priority : ordered_by_priority) {
    // With (num results) <= UINT16_MAX (and in practice, much less) and
    // (weight per result) <= UINT16_MAX, then it should be the case that
    // (total weight) <= UINT32_MAX, but use CheckedNumeric for extra safety.
    auto total_weight = base::MakeCheckedNum<uint32_t>(0);
    for (const SrvRecordRdata* rdata : priority.second)
      total_weight += rdata->weight();

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

ExtractionError ValidateNamesAndAliases(
    base::StringPiece query_name,
    const AliasMap& aliases,
    const std::vector<std::unique_ptr<const RecordParsed>>& results) {
  // Validate that all aliases form a single non-looping chain, starting from
  // `query_name`.
  size_t aliases_in_chain = 0;
  base::StringPiece final_chain_name = query_name;
  auto alias = aliases.find(std::string(query_name));
  while (alias != aliases.end() && aliases_in_chain <= aliases.size()) {
    aliases_in_chain++;
    final_chain_name = alias->second;
    alias = aliases.find(alias->second);
  }

  if (aliases_in_chain != aliases.size())
    return ExtractionError::kBadAliasChain;

  // All results must match final alias name.
  for (const auto& result : results) {
    DCHECK_NE(result->type(), dns_protocol::kTypeCNAME);
    if (!base::EqualsCaseInsensitiveASCII(final_chain_name, result->name())) {
      return ExtractionError::kNameMismatch;
    }
  }

  return ExtractionError::kOk;
}

ExtractionError ExtractResponseRecords(
    const DnsResponse& response,
    uint16_t result_qtype,
    std::vector<std::unique_ptr<const RecordParsed>>* out_records,
    absl::optional<base::TimeDelta>* out_response_ttl,
    std::set<std::string>* out_aliases) {
  DCHECK_EQ(response.question_count(), 1u);
  DCHECK(out_records);
  DCHECK(out_response_ttl);

  std::vector<std::unique_ptr<const RecordParsed>> records;
  absl::optional<base::TimeDelta> response_ttl;

  DnsRecordParser parser = response.Parser();

  // Expected to be validated by DnsTransaction.
  DCHECK_EQ(result_qtype, response.GetSingleQType());

  AliasMap aliases;
  for (unsigned i = 0; i < response.answer_count(); ++i) {
    std::unique_ptr<const RecordParsed> record =
        RecordParsed::CreateFrom(&parser, base::Time::Now());

    if (!record)
      return ExtractionError::kMalformedRecord;

    DCHECK_NE(result_qtype, dns_protocol::kTypeCNAME);
    if (record->klass() == dns_protocol::kClassIN &&
        record->type() == dns_protocol::kTypeCNAME) {
      // Per RFC2181, multiple CNAME records are not allowed for the same name.
      if (aliases.find(record->name()) != aliases.end())
        return ExtractionError::kMultipleCnames;

      const CnameRecordRdata* cname_data = record->rdata<CnameRecordRdata>();
      if (!cname_data)
        return ExtractionError::kMalformedCname;

      base::TimeDelta ttl = base::Seconds(record->ttl());
      response_ttl =
          std::min(response_ttl.value_or(base::TimeDelta::Max()), ttl);

      bool added = aliases.emplace(record->name(), cname_data->cname()).second;
      DCHECK(added);
    } else if (record->klass() == dns_protocol::kClassIN &&
               record->type() == result_qtype) {
      base::TimeDelta ttl = base::Seconds(record->ttl());
      response_ttl =
          std::min(response_ttl.value_or(base::TimeDelta::Max()), ttl);

      records.push_back(std::move(record));
    }
  }

  ExtractionError name_and_alias_validation_error =
      ValidateNamesAndAliases(response.GetSingleDottedName(), aliases, records);
  if (name_and_alias_validation_error != ExtractionError::kOk)
    return name_and_alias_validation_error;

  // For NXDOMAIN or NODATA (NOERROR with 0 answers), attempt to find a TTL
  // via an SOA record.
  if (response.rcode() == dns_protocol::kRcodeNXDOMAIN ||
      (response.answer_count() == 0 &&
       response.rcode() == dns_protocol::kRcodeNOERROR)) {
    bool soa_found = false;
    for (unsigned i = 0; i < response.authority_count(); ++i) {
      DnsResourceRecord record;
      if (parser.ReadRecord(&record) && record.type == dns_protocol::kTypeSOA) {
        soa_found = true;
        base::TimeDelta ttl = base::Seconds(record.ttl);
        response_ttl =
            std::min(response_ttl.value_or(base::TimeDelta::Max()), ttl);
      }
    }

    // Per RFC2308, section 5, never cache negative results unless an SOA
    // record is found.
    if (!soa_found)
      response_ttl.reset();
  }

  for (unsigned i = 0; i < response.additional_answer_count(); ++i) {
    std::unique_ptr<const RecordParsed> record =
        RecordParsed::CreateFrom(&parser, base::Time::Now());
    if (record && record->klass() == dns_protocol::kClassIN &&
        record->type() == dns_protocol::kTypeHttps) {
      bool is_unsolicited = result_qtype != dns_protocol::kTypeHttps;
      SaveMetricsForAdditionalHttpsRecord(*record, is_unsolicited);
    }
  }

  *out_records = std::move(records);
  *out_response_ttl = response_ttl;

  if (out_aliases) {
    out_aliases->clear();
    for (const auto& alias : aliases) {
      std::string canonicalized_alias =
          dns_names_util::UrlCanonicalizeNameIfAble(alias.second);
      if (dns_names_util::IsValidDnsRecordName(canonicalized_alias)) {
        out_aliases->insert(std::move(canonicalized_alias));
      }
    }
    std::string canonicalized_query = dns_names_util::UrlCanonicalizeNameIfAble(
        response.GetSingleDottedName());
    if (dns_names_util::IsValidDnsRecordName(canonicalized_query)) {
      out_aliases->insert(std::move(canonicalized_query));
    }
  }

  return ExtractionError::kOk;
}

ExtractionError ExtractAddressResults(const DnsResponse& response,
                                      uint16_t address_qtype,
                                      HostCache::Entry* out_results) {
  DCHECK_EQ(response.question_count(), 1u);
  DCHECK(address_qtype == dns_protocol::kTypeA ||
         address_qtype == dns_protocol::kTypeAAAA);
  DCHECK(out_results);

  std::vector<std::unique_ptr<const RecordParsed>> records;
  absl::optional<base::TimeDelta> response_ttl;
  std::set<std::string> aliases;
  ExtractionError extraction_error = ExtractResponseRecords(
      response, address_qtype, &records, &response_ttl, &aliases);

  if (extraction_error != ExtractionError::kOk) {
    *out_results = HostCache::Entry(ERR_DNS_MALFORMED_RESPONSE,
                                    HostCache::Entry::SOURCE_DNS);
    return extraction_error;
  }

  std::vector<IPEndPoint> ip_endpoints;
  std::string canonical_name;
  for (const auto& record : records) {
    if (ip_endpoints.empty())
      canonical_name = record->name();

    // Expect that ExtractResponseRecords validates that all results correctly
    // have the same name.
    DCHECK(base::EqualsCaseInsensitiveASCII(canonical_name, record->name()))
        << "canonical_name: " << canonical_name
        << "\nrecord->name(): " << record->name();

    IPAddress address;
    if (address_qtype == dns_protocol::kTypeA) {
      const ARecordRdata* rdata = record->rdata<ARecordRdata>();
      address = rdata->address();
      DCHECK(address.IsIPv4());
    } else {
      DCHECK_EQ(address_qtype, dns_protocol::kTypeAAAA);
      const AAAARecordRdata* rdata = record->rdata<AAAARecordRdata>();
      address = rdata->address();
      DCHECK(address.IsIPv6());
    }
    ip_endpoints.emplace_back(address, /*port=*/0);
  }
  int error_result = ip_endpoints.empty() ? ERR_NAME_NOT_RESOLVED : OK;

  HostCache::Entry results(error_result, std::move(ip_endpoints),
                           std::move(aliases), HostCache::Entry::SOURCE_DNS,
                           response_ttl);

  if (!canonical_name.empty()) {
    results.set_canonical_names(std::set<std::string>({canonical_name}));
  }

  *out_results = std::move(results);
  return ExtractionError::kOk;
}

ExtractionError ExtractTxtResults(const DnsResponse& response,
                                  HostCache::Entry* out_results) {
  DCHECK(out_results);

  std::vector<std::unique_ptr<const RecordParsed>> records;
  absl::optional<base::TimeDelta> response_ttl;
  ExtractionError extraction_error =
      ExtractResponseRecords(response, dns_protocol::kTypeTXT, &records,
                             &response_ttl, nullptr /* out_aliases */);

  if (extraction_error != ExtractionError::kOk) {
    *out_results = HostCache::Entry(ERR_DNS_MALFORMED_RESPONSE,
                                    HostCache::Entry::SOURCE_DNS);
    return extraction_error;
  }

  std::vector<std::string> text_records;
  for (const auto& record : records) {
    const TxtRecordRdata* rdata = record->rdata<net::TxtRecordRdata>();
    text_records.insert(text_records.end(), rdata->texts().begin(),
                        rdata->texts().end());
  }

  *out_results = HostCache::Entry(
      text_records.empty() ? ERR_NAME_NOT_RESOLVED : OK,
      std::move(text_records), HostCache::Entry::SOURCE_DNS, response_ttl);
  return ExtractionError::kOk;
}

ExtractionError ExtractPointerResults(const DnsResponse& response,
                                      HostCache::Entry* out_results) {
  DCHECK(out_results);

  std::vector<std::unique_ptr<const RecordParsed>> records;
  absl::optional<base::TimeDelta> response_ttl;
  ExtractionError extraction_error =
      ExtractResponseRecords(response, dns_protocol::kTypePTR, &records,
                             &response_ttl, nullptr /* out_aliases */);

  if (extraction_error != ExtractionError::kOk) {
    *out_results = HostCache::Entry(ERR_DNS_MALFORMED_RESPONSE,
                                    HostCache::Entry::SOURCE_DNS);
    return extraction_error;
  }

  std::vector<HostPortPair> pointers;
  for (const auto& record : records) {
    const PtrRecordRdata* rdata = record->rdata<net::PtrRecordRdata>();
    std::string pointer = rdata->ptrdomain();

    // Skip pointers to the root domain.
    if (!pointer.empty())
      pointers.emplace_back(std::move(pointer), 0);
  }

  *out_results = HostCache::Entry(pointers.empty() ? ERR_NAME_NOT_RESOLVED : OK,
                                  std::move(pointers),
                                  HostCache::Entry::SOURCE_DNS, response_ttl);
  return ExtractionError::kOk;
}

ExtractionError ExtractServiceResults(const DnsResponse& response,
                                      HostCache::Entry* out_results) {
  DCHECK(out_results);

  std::vector<std::unique_ptr<const RecordParsed>> records;
  absl::optional<base::TimeDelta> response_ttl;
  ExtractionError extraction_error =
      ExtractResponseRecords(response, dns_protocol::kTypeSRV, &records,
                             &response_ttl, nullptr /* out_aliases */);

  if (extraction_error != ExtractionError::kOk) {
    *out_results = HostCache::Entry(ERR_DNS_MALFORMED_RESPONSE,
                                    HostCache::Entry::SOURCE_DNS);
    return extraction_error;
  }

  std::vector<const SrvRecordRdata*> fitered_rdatas;
  for (const auto& record : records) {
    const SrvRecordRdata* rdata = record->rdata<net::SrvRecordRdata>();

    // Skip pointers to the root domain.
    if (!rdata->target().empty())
      fitered_rdatas.push_back(rdata);
  }

  std::vector<HostPortPair> ordered_service_targets =
      SortServiceTargets(fitered_rdatas);

  *out_results = HostCache::Entry(
      ordered_service_targets.empty() ? ERR_NAME_NOT_RESOLVED : OK,
      std::move(ordered_service_targets), HostCache::Entry::SOURCE_DNS,
      response_ttl);
  return ExtractionError::kOk;
}

const RecordParsed* UnwrapRecordPtr(
    const std::unique_ptr<const RecordParsed>& ptr) {
  return ptr.get();
}

bool RecordIsAlias(const RecordParsed* record) {
  DCHECK(record->rdata<HttpsRecordRdata>());
  return record->rdata<HttpsRecordRdata>()->IsAlias();
}

ExtractionError ExtractHttpsResults(const DnsResponse& response,
                                    base::StringPiece original_domain_name,
                                    uint16_t request_port,
                                    HostCache::Entry* out_results) {
  DCHECK(!original_domain_name.empty());
  DCHECK(out_results);

  absl::optional<base::TimeDelta> response_ttl;
  std::vector<std::unique_ptr<const RecordParsed>> records;
  ExtractionError extraction_error =
      ExtractResponseRecords(response, dns_protocol::kTypeHttps, &records,
                             &response_ttl, nullptr /* out_aliases */);

  if (extraction_error != ExtractionError::kOk) {
    *out_results = HostCache::Entry(ERR_DNS_MALFORMED_RESPONSE,
                                    HostCache::Entry::SOURCE_DNS);
    return extraction_error;
  }

  std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata> results;
  std::vector<bool> record_compatibility;
  bool default_alpn_found = false;
#if DCHECK_IS_ON()
  std::string canonical_name;
#endif  // DCHECK_IS_ON()
  for (const auto& record : records) {
#if DCHECK_IS_ON()
    if (canonical_name.empty()) {
      canonical_name = record->name();
    } else {
      DCHECK(record->name() == canonical_name);
    }
#endif  // DCHECK_IS_ON()

    const HttpsRecordRdata* rdata = record->rdata<HttpsRecordRdata>();
    DCHECK(rdata);

    // Chrome does not yet support alias records.
    if (rdata->IsAlias()) {
      // Alias records are always considered compatible because they do not
      // support "mandatory" params.
      record_compatibility.push_back(true);
      continue;
    }

    const ServiceFormHttpsRecordRdata* service = rdata->AsServiceForm();
    record_compatibility.push_back(service->IsCompatible());

    // Ignore services incompatible with Chrome's HTTPS record parser.
    // draft-ietf-dnsop-svcb-https-08#section-8
    if (!service->IsCompatible())
      continue;

    base::StringPiece target_name = service->service_name().empty()
                                        ? record->name()
                                        : service->service_name();

    // Chrome does not yet support followup queries. So only support services at
    // the original domain name or the canonical name (the record name).
    // Note: HostCache::Entry::GetEndpoints() will not return metadatas which
    // target name is different from the canonical name of A/AAAA query results.
    if ((target_name != original_domain_name) &&
        (target_name != record->name())) {
      continue;
    }

    // Ignore services at a different port from the request port. Chrome does
    // not yet support endpoints diverging by port.  Note that before supporting
    // port redirects, Chrome must ensure redirects to the "bad port list" are
    // disallowed. Unclear if such logic would belong here or in socket
    // connection logic.
    if (service->port().has_value() && service->port().value() != request_port)
      continue;

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
    // draft-ietf-dnsop-svcb-https-08#section-7.1.1 and
    // draft-ietf-dnsop-svcb-https-08#section-2.4.3.
    if (metadata.supported_protocol_alpns.empty())
      continue;

    metadata.ech_config_list = ConnectionEndpointMetadata::EchConfigList(
        service->ech_config().cbegin(), service->ech_config().cend());

    metadata.target_name = base::ToLowerASCII(target_name);

    results.emplace(service->priority(), std::move(metadata));

    if (service->default_alpn())
      default_alpn_found = true;
  }

  // Ignore all records if any are an alias record. Chrome does not yet support
  // alias records, but aliases take precedence over any other records.
  if (base::ranges::any_of(records, &RecordIsAlias, &UnwrapRecordPtr)) {
    records.clear();
    results.clear();
  }

  // Ignore all records if they all mark "no-default-alpn". Domains should
  // always provide at least one endpoint allowing default ALPN to ensure a
  // reasonable expectation of connection success.
  // draft-ietf-dnsop-svcb-https-08#section-7.1.2
  if (!default_alpn_found) {
    records.clear();
    results.clear();
  }

  *out_results = HostCache::Entry(results.empty() ? ERR_NAME_NOT_RESOLVED : OK,
                                  std::move(results),
                                  HostCache::Entry::SOURCE_DNS, response_ttl);
  out_results->set_https_record_compatibility(std::move(record_compatibility));
  DCHECK_EQ(extraction_error, ExtractionError::kOk);
  return extraction_error;
}

}  // namespace

DnsResponseResultExtractor::DnsResponseResultExtractor(
    const DnsResponse* response)
    : response_(response) {
  DCHECK(response_);
}

DnsResponseResultExtractor::~DnsResponseResultExtractor() = default;

DnsResponseResultExtractor::ExtractionError
DnsResponseResultExtractor::ExtractDnsResults(
    DnsQueryType query_type,
    base::StringPiece original_domain_name,
    uint16_t request_port,
    HostCache::Entry* out_results) const {
  DCHECK(!original_domain_name.empty());
  DCHECK(out_results);

  switch (query_type) {
    case DnsQueryType::UNSPECIFIED:
      // Should create multiple transactions with specified types.
      NOTREACHED();
      return ExtractionError::kUnexpected;
    case DnsQueryType::A:
    case DnsQueryType::AAAA:
      return ExtractAddressResults(*response_, DnsQueryTypeToQtype(query_type),
                                   out_results);
    case DnsQueryType::TXT:
      return ExtractTxtResults(*response_, out_results);
    case DnsQueryType::PTR:
      return ExtractPointerResults(*response_, out_results);
    case DnsQueryType::SRV:
      return ExtractServiceResults(*response_, out_results);
    case DnsQueryType::HTTPS:
      return ExtractHttpsResults(*response_, original_domain_name, request_port,
                                 out_results);
  }
}

// static
HostCache::Entry DnsResponseResultExtractor::CreateEmptyResult(
    DnsQueryType query_type) {
  if (query_type != DnsQueryType::HTTPS) {
    // Currently only used for HTTPS.
    NOTIMPLEMENTED();
    return HostCache::Entry(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  }

  return HostCache::Entry(ERR_NAME_NOT_RESOLVED, std::vector<bool>(),
                          HostCache::Entry::SOURCE_DNS);
}

}  // namespace net
