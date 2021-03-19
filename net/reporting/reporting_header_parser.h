// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_REPORTING_REPORTING_HEADER_PARSER_H_
#define NET_REPORTING_REPORTING_HEADER_PARSER_H_

#include <memory>

#include "base/macros.h"
#include "net/base/net_export.h"
#include "net/http/structured_headers.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace base {
class Value;
}  // namespace base

namespace net {

class NetworkIsolationKey;
class ReportingContext;

class NET_EXPORT ReportingHeaderParser {
 public:
  static void ParseReportToHeader(
      ReportingContext* context,
      const NetworkIsolationKey& network_isolation_key,
      const GURL& url,
      std::unique_ptr<base::Value> value);

  static void ParseReportingEndpointsHeader(
      ReportingContext* context,
      const NetworkIsolationKey& network_isolation_key,
      const url::Origin& origin,
      std::unique_ptr<structured_headers::Dictionary> value);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ReportingHeaderParser);
};

}  // namespace net

#endif  // NET_REPORTING_REPORTING_HEADER_PARSER_H_
