// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_RESPONSE_RESULT_EXTRACTOR_H_
#define NET_DNS_DNS_RESPONSE_RESULT_EXTRACTOR_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece.h"
#include "net/base/net_export.h"
#include "net/dns/host_cache.h"
#include "net/dns/public/dns_query_type.h"

namespace net {

class DnsResponse;

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

  explicit DnsResponseResultExtractor(const DnsResponse* response);
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
  ExtractionError ExtractDnsResults(DnsQueryType query_type,
                                    base::StringPiece original_domain_name,
                                    uint16_t request_port,
                                    HostCache::Entry* out_results) const;

  // Creates the results of a NODATA response (successfully parsed but without
  // any results) appropriate for `query_type`.
  static HostCache::Entry CreateEmptyResult(DnsQueryType query_type);

 private:
  const raw_ptr<const DnsResponse, DanglingUntriaged> response_;
};

}  // namespace net

#endif  // NET_DNS_DNS_RESPONSE_RESULT_EXTRACTOR_H_
