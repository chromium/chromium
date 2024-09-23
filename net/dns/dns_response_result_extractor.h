// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_RESPONSE_RESULT_EXTRACTOR_H_
#define NET_DNS_DNS_RESPONSE_RESULT_EXTRACTOR_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/types/expected.h"
#include "net/base/net_export.h"
#include "net/dns/host_cache.h"
#include "net/dns/public/dns_query_type.h"

namespace net {

class DnsResponse;
class HostResolverInternalResult;

// Higher-level parser to take a DnsResponse and extract results.
class NET_EXPORT_PRIVATE DnsResponseResultExtractor {
 public:
  enum class ExtractionError {
    kOk = 0,
    // Record failed to parse.
    kMalformedRecord,
    // Malformed CNAME
    kMalformedCname,
    // Found CNAME or result record with an unexpected name.
    kNameMismatch,
    // Malformed result record
    kMalformedResult,
    // CNAME record after a result record
    kCnameAfterResult,
    // Multiple CNAME records for the same owner name.
    kMultipleCnames,
    // Invalid alias chain, e.g. contains loops or disjoint aliases.
    kBadAliasChain,
    // Not expected. Used for DCHECKs.
    kUnexpected,
  };

  using ResultsOrError =
      base::expected<std::set<std::unique_ptr<HostResolverInternalResult>>,
                     ExtractionError>;

  // References must stay alive for the life of the created extractor.
  explicit DnsResponseResultExtractor(
      const DnsResponse& response,
      const base::Clock& clock = *base::DefaultClock::GetInstance(),
      const base::TickClock& tick_clock =
          *base::DefaultTickClock::GetInstance());
  ~DnsResponseResultExtractor();

  DnsResponseResultExtractor(const DnsResponseResultExtractor&) = delete;
  DnsResponseResultExtractor& operator=(const DnsResponseResultExtractor&) =
      delete;

  // Extract results from the response. `query_type` must match the qtype from
  // the DNS query, and it must have already been validated (expected to be done
  // by DnsTransaction) that the response matches the query.
  //
  // `original_domain_name` is the query name (in dotted form) before any
  // aliasing or prepending port/scheme. It is expected to be the name under
  // which any basic query types, e.g. A or AAAA, are queried.
  //
  // May have the side effect of recording metrics about DnsResponses as they
  // are parsed, so while not an absolute requirement, any given DnsResponse
  // should only be used and extracted from at most once.
  ResultsOrError ExtractDnsResults(DnsQueryType query_type,
                                   std::string_view original_domain_name,
                                   uint16_t request_port) const;

 private:
  const raw_ref<const DnsResponse> response_;
  const raw_ref<const base::Clock> clock_;
  const raw_ref<const base::TickClock> tick_clock_;
};

}  // namespace net

#endif  // NET_DNS_DNS_RESPONSE_RESULT_EXTRACTOR_H_
