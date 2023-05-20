// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_ATTRIBUTION_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_ATTRIBUTION_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/network/public/cpp/attribution_reporting_runtime_features.h"
#include "services/network/public/cpp/trigger_verification.h"
#include "services/network/public/mojom/attribution.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_ATTRIBUTION_MOJOM_TRAITS)
    StructTraits<network::mojom::TriggerVerificationDataView,
                 network::TriggerVerification> {
  static const std::string& token(
      const network::TriggerVerification& verification) {
    return verification.token();
  }

  static std::string aggregatable_report_id(
      const network::TriggerVerification& verification) {
    return verification.aggregatable_report_id().AsLowercaseString();
  }

  static bool Read(network::mojom::TriggerVerificationDataView data,
                   network::TriggerVerification* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_ATTRIBUTION_MOJOM_TRAITS)
    StructTraits<network::mojom::AttributionReportingRuntimeFeaturesDataView,
                 network::AttributionReportingRuntimeFeatures> {
  static bool cross_app_web_enabled(
      network::AttributionReportingRuntimeFeatures runtime_features) {
    return runtime_features.Has(
        network::AttributionReportingRuntimeFeature::kCrossAppWeb);
  }

  static bool Read(
      network::mojom::AttributionReportingRuntimeFeaturesDataView data,
      network::AttributionReportingRuntimeFeatures* out) {
    if (data.cross_app_web_enabled()) {
      out->Put(network::AttributionReportingRuntimeFeature::kCrossAppWeb);
    }
    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_ATTRIBUTION_MOJOM_TRAITS_H_
