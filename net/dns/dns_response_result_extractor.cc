// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_response_result_extractor.h"

#include <limits.h>
#include <stdint.h>

#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/check.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/optional.h"
#include "base/rand_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_util.h"
#include "net/dns/host_cache.h"
#include "net/dns/https_record_rdata.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/record_parsed.h"
#include "net/dns/record_rdata.h"

namespace net {

namespace {

using AliasMap = std::map<std::string, std::string, DomainNameComparator>;
using ExtractionError = DnsResponseResultExtractor::ExtractionError;

void SaveMetricsForAdditionalHttpsRecord(const RecordParsed& record,
                                         bool is_unsolicited) {
  const HttpsRecordRdata* rdata = record.rdata<HttpsRecordRdata>();

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class UnsolicitedHttpsRecordStatus {
    kMalformed = 0,
    kAlias = 1,
    kService = 2,
    kMaxValue = kService
  } status;

  if (!rdata || rdata->IsMalformed()) {
    status = UnsolicitedHttpsRecordStatus::kMalformed;
  } else if (rdata->IsAlias()) {
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
    const std::vector<std::unique_ptr<const RecordParsed>>& results,
    std::vector<std::string>* out_ordered_aliases) {
  DCHECK(out_ordered_aliases);

  // Validate that all aliases form a single non-looping chain, starting from
  // `query_name`.
  size_t aliases_in_chain = 0;
  base::StringPiece final_chain_name = query_name;
  std::vector<std::string> reordered_aliases;
  reordered_aliases.push_back(std::string(query_name));
  auto alias = aliases.find(std::string(query_name));
  while (alias != aliases.end() && aliases_in_chain <= aliases.size()) {
    aliases_in_chain++;
    final_chain_name = alias->second;
    reordered_aliases.emplace_back(alias->second);
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

  // Reverse the ordered aliases so that `final_chain_name` is first and
  // `query_name` is last.
  using iter_t = std::vector<std::string>::reverse_iterator;
  std::vector<std::string> reversed_aliases;
  reversed_aliases.insert(
      reversed_aliases.end(),
      std::move_iterator<iter_t>(reordered_aliases.rbegin()),
      std::move_iterator<iter_t>(reordered_aliases.rend()));
  *out_ordered_aliases = reversed_aliases;

  return ExtractionError::kOk;
}

ExtractionError ExtractResponseRecords(
    const DnsResponse& response,
    uint16_t result_qtype,
    std::vector<std::unique_ptr<const RecordParsed>>* out_records,
    base::Optional<base::TimeDelta>* out_response_ttl,
    std::vector<std::string>* out_aliases) {
  DCHECK(out_records);
  DCHECK(out_response_ttl);

  std::vector<std::unique_ptr<const RecordParsed>> records;
  base::Optional<base::TimeDelta> response_ttl;

  DnsRecordParser parser = response.Parser();

  // Expected to be validated by DnsTransaction.
  DCHECK_EQ(result_qtype, response.qtype());

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

      base::TimeDelta ttl = base::TimeDelta::FromSeconds(record->ttl());
      response_ttl =
          std::min(response_ttl.value_or(base::TimeDelta::Max()), ttl);

      bool added = aliases.emplace(record->name(), cname_data->cname()).second;
      DCHECK(added);
    } else if (record->klass() == dns_protocol::kClassIN &&
               record->type() == result_qtype) {
      base::TimeDelta ttl = base::TimeDelta::FromSeconds(record->ttl());
      response_ttl =
          std::min(response_ttl.value_or(base::TimeDelta::Max()), ttl);

      records.push_back(std::move(record));
    }
  }

  std::vector<std::string> out_ordered_aliases;
  ExtractionError name_and_alias_validation_error = ValidateNamesAndAliases(
      response.GetDottedName(), aliases, records, &out_ordered_aliases);
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
        base::TimeDelta ttl = base::TimeDelta::FromSeconds(record.ttl);
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
  if (out_aliases)
    *out_aliases = std::move(out_ordered_aliases);

  return ExtractionError::kOk;
}

ExtractionError ExtractAddressResults(const DnsResponse& response,
                                      uint16_t address_qtype,
                                      HostCache::Entry* out_results) {
  DCHECK(address_qtype == dns_protocol::kTypeA ||
         address_qtype == dns_protocol::kTypeAAAA);
  DCHECK(out_results);

  std::vector<std::unique_ptr<const RecordParsed>> records;
  base::Optional<base::TimeDelta> response_ttl;
  std::vector<std::string> aliases;
  ExtractionError extraction_error = ExtractResponseRecords(
      response, address_qtype, &records, &response_ttl, &aliases);

  if (extraction_error != ExtractionError::kOk) {
    *out_results = HostCache::Entry(ERR_DNS_MALFORMED_RESPONSE,
                                    HostCache::Entry::SOURCE_DNS);
    return extraction_error;
  }

  AddressList addresses;
  std::string canonical_name;
  for (const auto& record : records) {
    if (addresses.empty())
      canonical_name = record->name();

    // Expect that ExtractResponseRecords validates that all results correctly
    // have the same name.
    DCHECK_EQ(canonical_name, record->name());

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
    addresses.push_back(IPEndPoint(address, 0 /* port */));
  }

  // If addresses were found, then a canonical name exists. Verify that the
  // canonical name is the first entry in the alias vector to be stored in
  // `addresses.dns_aliases_`. The alias chain order should have been preserved
  // from canonical name (i.e. record name) through to query name.
  if (!addresses.empty()) {
    DCHECK(!aliases.empty());
    DCHECK(aliases.front() == canonical_name);
    DCHECK(aliases.back() == response.GetDottedName());
    addresses.SetDnsAliases(std::move(aliases));
  }

  *out_results = HostCache::Entry(
      addresses.empty() ? ERR_NAME_NOT_RESOLVED : OK, std::move(addresses),
      HostCache::Entry::SOURCE_DNS, response_ttl);
  return ExtractionError::kOk;
}

ExtractionError ExtractTxtResults(const DnsResponse& response,
                                  HostCache::Entry* out_results) {
  DCHECK(out_results);

  std::vector<std::unique_ptr<const RecordParsed>> records;
  base::Optional<base::TimeDelta> response_ttl;
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
  base::Optional<base::TimeDelta> response_ttl;
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
  base::Optional<base::TimeDelta> response_ttl;
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

ExtractionError ExtractIntegrityResults(const DnsResponse& response,
                                        HostCache::Entry* out_results) {
  DCHECK(out_results);

  base::Optional<base::TimeDelta> response_ttl;
  std::vector<std::unique_ptr<const RecordParsed>> records;
  ExtractionError extraction_error = ExtractResponseRecords(
      response, dns_protocol::kExperimentalTypeIntegrity, &records,
      &response_ttl, nullptr /* out_aliases */);

  // If the response couldn't be parsed, assume no INTEGRITY records, and
  // pretend success. This is a temporary hack to keep errors with INTEGRITY
  // (which is only used for experiments) from affecting non-experimental
  // results, e.g. due to a parse error being treated as fatal for the whole
  // HostResolver request.
  //
  // TODO(crbug.com/1138620): Cleanup handling of fatal vs non-fatal errors and
  // the organization responsibility for handling them so that this extractor
  // can more sensibly return honest results rather than lying to try to be
  // helpful to HostResolverManager.
  if (extraction_error != ExtractionError::kOk) {
    *out_results =
        DnsResponseResultExtractor::CreateEmptyResult(DnsQueryType::INTEGRITY);
    return ExtractionError::kOk;
  }

  // Condense results into a list of booleans. We do not cache the results,
  // but this enables us to write some unit tests.
  std::vector<bool> condensed_results;
  for (const auto& record : records) {
    const IntegrityRecordRdata& rdata = *record->rdata<IntegrityRecordRdata>();
    condensed_results.push_back(rdata.IsIntact());
  }

  // As another temporary hack for the experimental nature of INTEGRITY, always
  // claim no results, even on success.  This will let it merge with address
  // results since an address request should be considered successful overall
  // only with A/AAAA results, not INTEGRITY results.
  //
  // TODO(crbug.com/1138620): Remove and handle the merging more intelligently
  // in HostResolverManager.
  *out_results =
      HostCache::Entry(ERR_NAME_NOT_RESOLVED, std::move(condensed_results),
                       HostCache::Entry::SOURCE_DNS, response_ttl);
  DCHECK_EQ(extraction_error, ExtractionError::kOk);
  return extraction_error;
}

ExtractionError ExtractHttpsResults(const DnsResponse& response,
                                    HostCache::Entry* out_results) {
  DCHECK(out_results);

  base::Optional<base::TimeDelta> response_ttl;
  std::vector<std::unique_ptr<const RecordParsed>> records;
  ExtractionError extraction_error =
      ExtractResponseRecords(response, dns_protocol::kTypeHttps, &records,
                             &response_ttl, nullptr /* out_aliases */);

  // If the response couldn't be parsed, assume no HTTPS records, and pretend
  // success. This is a temporary hack to keep errors with HTTPS (which is
  // currently only used for experiments) from affecting non-experimental
  // results, e.g. due to a parse error being treated as fatal for the whole
  // HostResolver request.
  //
  // TODO(crbug.com/1138620): Cleanup handling of fatal vs non-fatal errors and
  // the organization responsibility for handling them so that this extractor
  // can more sensibly return honest results rather than lying to try to be
  // helpful to HostResolverManager.
  if (extraction_error != ExtractionError::kOk) {
    *out_results =
        DnsResponseResultExtractor::CreateEmptyResult(DnsQueryType::HTTPS);
    return ExtractionError::kOk;
  }

  // Condense results into a list of booleans. We do not cache the results,
  // but this enables us to write some unit tests.
  std::vector<bool> condensed_results;
  for (const auto& record : records) {
    const HttpsRecordRdata& rdata = *record->rdata<HttpsRecordRdata>();
    condensed_results.push_back(!rdata.IsMalformed());
  }

  // As another temporary hack for experimental usage of HTTPS, always claim no
  // results, even on success.  This will let it merge with address results
  // since an address request should be considered successful overall only with
  // A/AAAA results, not HTTPS results.
  //
  // TODO(crbug.com/1138620): Remove and handle the merging more intelligently
  // in HostResolverManager.
  *out_results =
      HostCache::Entry(ERR_NAME_NOT_RESOLVED, std::move(condensed_results),
                       HostCache::Entry::SOURCE_DNS, response_ttl);
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
    HostCache::Entry* out_results) const {
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
    case DnsQueryType::INTEGRITY:
      return ExtractIntegrityResults(*response_, out_results);
    case DnsQueryType::HTTPS:
      return ExtractHttpsResults(*response_, out_results);
  }
}

// static
HostCache::Entry DnsResponseResultExtractor::CreateEmptyResult(
    DnsQueryType query_type) {
  if (query_type != DnsQueryType::INTEGRITY &&
      query_type != DnsQueryType::HTTPS) {
    // Currently only used for INTEGRITY/HTTPS.
    NOTIMPLEMENTED();
    return HostCache::Entry(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
  }

  return HostCache::Entry(ERR_NAME_NOT_RESOLVED, std::vector<bool>(),
                          HostCache::Entry::SOURCE_DNS);
}

}  // namespace net
