// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_REPORTING_REPORTING_HEADER_PARSER_H_
#define NET_REPORTING_REPORTING_HEADER_PARSER_H_

#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/values.h"
#include "net/base/net_export.h"
#include "net/http/structured_headers.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

class IsolationInfo;
class NetworkAnonymizationKey;
class ReportingContext;

// Tries to parse a Reporting-Endpoints header. Returns base::nullopt if parsing
// failed and the header should be ignored; otherwise returns a (possibly
// empty) mapping of endpoint names to URLs.
NET_EXPORT
std::optional<base::flat_map<std::string, std::string>> ParseReportingEndpoints(
    const std::string& header);

class NET_EXPORT ReportingHeaderParser {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // They should also be kept in sync with the NetReportingHeaderType enum
  // in tools/metrics/histograms/enums.xml
  enum class ReportingHeaderType {
    kReportTo = 0,
    kReportToInvalid = 1,
    kReportingEndpoints = 2,
    kReportingEndpointsInvalid = 3,
    kMaxValue = kReportingEndpointsInvalid,
  };

  ReportingHeaderParser() = delete;
  ReportingHeaderParser(const ReportingHeaderParser&) = delete;
  ReportingHeaderParser& operator=(const ReportingHeaderParser&) = delete;

  static void ParseReportToHeader(
      ReportingContext* context,
      const NetworkAnonymizationKey& network_anonymization_key,
      const url::Origin& origin,
      const base::Value::List& list);

  // `isolation_info` here will be stored in the cache, associated with the
  // `reporting_source`. `network_anonymization_key` is the NAK which will be
  // passed in with reports to be queued. This must match the NAK from
  // `isolation_source`, unless it is empty (which will be the case if the
  // network state partitioning is disabled).
  static void ProcessParsedReportingEndpointsHeader(
      ReportingContext* context,
      const base::UnguessableToken& reporting_source,
      const IsolationInfo& isolation_info,
      const NetworkAnonymizationKey& network_anonymization_key,
      const url::Origin& origin,
      base::flat_map<std::string, std::string> parsed_header);

  static void RecordReportingHeaderType(ReportingHeaderType header_type);
};

}  // namespace net

#endif  // NET_REPORTING_REPORTING_HEADER_PARSER_H_
