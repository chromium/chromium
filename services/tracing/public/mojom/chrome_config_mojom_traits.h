// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This defines mappings from mojom IPC representations to their native perfetto
// equivalents.

#ifndef SERVICES_TRACING_PUBLIC_MOJOM_CHROME_CONFIG_MOJOM_TRAITS_H_
#define SERVICES_TRACING_PUBLIC_MOJOM_CHROME_CONFIG_MOJOM_TRAITS_H_

#include <string>

#include "base/notreached.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/tracing/public/mojom/perfetto_service.mojom-shared.h"
#include "third_party/perfetto/include/perfetto/tracing/core/chrome_config.h"

namespace mojo {
template <>
class StructTraits<tracing::mojom::ChromeConfigDataView,
                   perfetto::ChromeConfig> {
 public:
  static const std::string& trace_config(const perfetto::ChromeConfig& src) {
    return src.trace_config();
  }

  static bool privacy_filtering_enabled(const perfetto::ChromeConfig& src) {
    return src.privacy_filtering_enabled();
  }

  static bool convert_to_legacy_json(const perfetto::ChromeConfig& src) {
    return src.convert_to_legacy_json();
  }

  static tracing::mojom::TracingClientPriority client_priority(
      const perfetto::ChromeConfig& src) {
    switch (src.client_priority()) {
      case perfetto::protos::gen::ChromeConfig::BACKGROUND:
        return tracing::mojom::TracingClientPriority::kBackground;
      case perfetto::protos::gen::ChromeConfig::USER_INITIATED:
        return tracing::mojom::TracingClientPriority::kUserInitiated;
      case perfetto::protos::gen::ChromeConfig::UNKNOWN:
        return tracing::mojom::TracingClientPriority::kUnknown;
      default:
        NOTREACHED_IN_MIGRATION();
        return tracing::mojom::TracingClientPriority::kUnknown;
    }
  }

  static bool Read(tracing::mojom::ChromeConfigDataView data,
                   perfetto::ChromeConfig* out);
};
}  // namespace mojo
#endif  // SERVICES_TRACING_PUBLIC_MOJOM_CHROME_CONFIG_MOJOM_TRAITS_H_
