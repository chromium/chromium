// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_REPORTING_REPORTING_HEADER_PARSER_H_
#define NET_REPORTING_REPORTING_HEADER_PARSER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "net/base/net_export.h"
#include "net/http/structured_headers.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace base {
class Value;
}  // namespace base

namespace net {

class NetworkIsolationKey;
class ReportingContext;

// Tries to parse a Reporting-Endpoints header. Returns base::nullopt if parsing
// failed and the header should be ignored; otherwise returns a (possibly
// empty) mapping of endpoint names to URLs.
NET_EXPORT
absl::optional<base::flat_map<std::string, std::string>>
ParseReportingEndpoints(const std::string& header);

class NET_EXPORT ReportingHeaderParser {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ReportingHeaderType {
    kReportTo = 0,
    kReportToInvalid = 1,
    kMaxValue = kReportToInvalid,
  };

  static void ParseReportToHeader(
      ReportingContext* context,
      const NetworkIsolationKey& network_isolation_key,
      const GURL& url,
      std::unique_ptr<base::Value> value);

  static void ProcessParsedReportingEndpointsHeader(
      ReportingContext* context,
      const NetworkIsolationKey& network_isolation_key,
      const url::Origin& origin,
      base::flat_map<std::string, std::string> parsed_header);

  static void RecordReportingHeaderType(ReportingHeaderType header_type);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ReportingHeaderParser);
};

}  // namespace net

#endif  // NET_REPORTING_REPORTING_HEADER_PARSER_H_
