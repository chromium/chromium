// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_REPORTING_API_ENDPOINT_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_REPORTING_API_ENDPOINT_MOJOM_TRAITS_H_

#include <optional>

#include "base/values.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/network_anonymization_key.h"
#include "net/reporting/reporting_endpoint.h"
#include "services/network/public/cpp/network_anonymization_key_mojom_traits.h"
#include "services/network/public/mojom/reporting_service.mojom-shared.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<network::mojom::ReportingApiEndpointDataView,
                    net::ReportingEndpoint> {
  static const GURL& url(const net::ReportingEndpoint& endpoint) {
    return endpoint.info.url;
  }

  static int32_t attempted_uploads(const net::ReportingEndpoint& endpoint) {
    return endpoint.stats.attempted_uploads;
  }

  static int32_t successful_uploads(const net::ReportingEndpoint& endpoint) {
    return endpoint.stats.successful_uploads;
  }

  static int32_t attempted_reports(const net::ReportingEndpoint& endpoint) {
    return endpoint.stats.attempted_reports;
  }

  static int32_t successful_reports(const net::ReportingEndpoint& endpoint) {
    return endpoint.stats.successful_reports;
  }

  static int32_t priority(const net::ReportingEndpoint& endpoint) {
    return endpoint.info.priority;
  }

  static int32_t weight(const net::ReportingEndpoint& endpoint) {
    return endpoint.info.weight;
  }

  static const std::optional<url::Origin>& origin(
      const net::ReportingEndpoint& endpoint) {
    return endpoint.group_key.origin;
  }

  static const std::string& group_name(const net::ReportingEndpoint& endpoint) {
    return endpoint.group_key.group_name;
  }

  static const net::NetworkAnonymizationKey& network_anonymization_key(
      const net::ReportingEndpoint& endpoint) {
    return endpoint.group_key.network_anonymization_key;
  }

  static const std::optional<base::UnguessableToken>& reporting_source(
      const net::ReportingEndpoint& endpoint) {
    return endpoint.group_key.reporting_source;
  }

  static bool Read(network::mojom::ReportingApiEndpointDataView data,
                   net::ReportingEndpoint* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_REPORTING_API_ENDPOINT_MOJOM_TRAITS_H_
