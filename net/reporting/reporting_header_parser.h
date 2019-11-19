// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_REPORTING_REPORTING_HEADER_PARSER_H_
#define NET_REPORTING_REPORTING_HEADER_PARSER_H_

#include <memory>

#include "base/macros.h"
#include "net/base/net_export.h"
#include "url/gurl.h"

namespace base {
class Value;
}  // namespace base

namespace net {

class ReportingContext;

class NET_EXPORT ReportingHeaderParser {
 public:
  // Histograms.  These are mainly used in test cases to verify that interesting
  // events occurred.

  static const char kHeaderOutcomeHistogram[];
  static const char kHeaderEndpointGroupOutcomeHistogram[];
  static const char kHeaderEndpointOutcomeHistogram[];

  enum class HeaderOutcome {
    DISCARDED_NO_REPORTING_SERVICE = 0,
    DISCARDED_INVALID_SSL_INFO = 1,
    DISCARDED_CERT_STATUS_ERROR = 2,
    DISCARDED_JSON_TOO_BIG = 3,
    DISCARDED_JSON_INVALID = 4,
    PARSED = 5,
    REMOVED_EMPTY = 6,
    MAX
  };

  enum class HeaderEndpointGroupOutcome {
    DISCARDED_NOT_DICTIONARY = 0,
    DISCARDED_GROUP_NOT_STRING = 1,
    DISCARDED_TTL_MISSING = 2,
    DISCARDED_TTL_NOT_INTEGER = 3,
    DISCARDED_TTL_NEGATIVE = 4,
    DISCARDED_ENDPOINTS_MISSING = 5,
    DISCARDED_ENDPOINTS_NOT_LIST = 6,

    PARSED = 7,
    REMOVED_TTL_ZERO = 8,
    REMOVED_EMPTY = 9,
    DISCARDED_INCLUDE_SUBDOMAINS_NOT_ALLOWED = 10,
    MAX
  };

  enum class HeaderEndpointOutcome {
    DISCARDED_NOT_DICTIONARY = 0,
    DISCARDED_URL_MISSING = 1,
    DISCARDED_URL_NOT_STRING = 2,
    DISCARDED_URL_INVALID = 3,
    DISCARDED_URL_INSECURE = 4,
    DISCARDED_PRIORITY_NOT_INTEGER = 5,
    DISCARDED_WEIGHT_NOT_INTEGER = 6,
    DISCARDED_WEIGHT_NEGATIVE = 7,

    REMOVED = 8,  // Obsolete: removing for max_age: 0 is done on a group basis.
    SET_REJECTED_BY_DELEGATE = 9,
    SET = 10,
    DISCARDED_PRIORITY_NEGATIVE = 11,

    MAX
  };

  static void RecordHeaderDiscardedForNoReportingService();
  static void RecordHeaderDiscardedForInvalidSSLInfo();
  static void RecordHeaderDiscardedForCertStatusError();
  static void RecordHeaderDiscardedForJsonInvalid();
  static void RecordHeaderDiscardedForJsonTooBig();

  static void ParseHeader(ReportingContext* context,
                          const GURL& url,
                          std::unique_ptr<base::Value> value);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ReportingHeaderParser);
};

}  // namespace net

#endif  // NET_REPORTING_REPORTING_HEADER_PARSER_H_
