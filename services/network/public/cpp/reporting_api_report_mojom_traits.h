// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_REPORTING_API_REPORT_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_REPORTING_API_REPORT_MOJOM_TRAITS_H_

#include "base/values.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "mojo/public/cpp/base/values_mojom_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/reporting/reporting_report.h"
#include "services/network/public/mojom/reporting_service.mojom-shared.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

template <>
struct EnumTraits<network::mojom::ReportingApiReportStatus,
                  net::ReportingReport::Status> {
  static network::mojom::ReportingApiReportStatus ToMojom(
      net::ReportingReport::Status input);
  static bool FromMojom(network::mojom::ReportingApiReportStatus input,
                        net::ReportingReport::Status* output);
};

template <>
struct StructTraits<network::mojom::ReportingApiReportDataView,
                    net::ReportingReport> {
  static base::UnguessableToken id(const net::ReportingReport& report) {
    return report.id;
  }

  static const GURL& url(const net::ReportingReport& report) {
    return report.url;
  }

  static const std::string& group(const net::ReportingReport& report) {
    return report.group;
  }

  static const std::string& type(const net::ReportingReport& report) {
    return report.type;
  }

  static base::TimeTicks timestamp(const net::ReportingReport& report) {
    return report.queued;
  }

  static int32_t depth(const net::ReportingReport& report) {
    return report.depth;
  }

  static int32_t attempts(const net::ReportingReport& report) {
    return report.attempts;
  }

  static const base::Value::Dict& body(const net::ReportingReport& report) {
    return report.body;
  }

  static net::ReportingReport::Status status(
      const net::ReportingReport& report) {
    return report.status;
  }

  static bool Read(network::mojom::ReportingApiReportDataView data,
                   net::ReportingReport* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_REPORTING_API_REPORT_MOJOM_TRAITS_H_
