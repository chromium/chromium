// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_ATTRIBUTION_REPORTING_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_ATTRIBUTION_REPORTING_MOJOM_TRAITS_H_

#include <stdint.h>

#include <utility>

#include "components/attribution_reporting/os_registration.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-shared.h"
#include "url/gurl.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::AttributionDebugKeyDataView, uint64_t> {
  static uint64_t value(uint64_t debug_key) { return debug_key; }

  static bool Read(blink::mojom::AttributionDebugKeyDataView data,
                   uint64_t* out) {
    *out = data.value();
    return true;
  }
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::AttributionTriggerDedupKeyDataView, uint64_t> {
  static uint64_t value(uint64_t debug_key) { return debug_key; }

  static bool Read(blink::mojom::AttributionTriggerDedupKeyDataView data,
                   uint64_t* out) {
    *out = data.value();
    return true;
  }
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::AttributionOsSourceDataView,
                 attribution_reporting::OsSource> {
  static const GURL& url(const attribution_reporting::OsSource& os_source) {
    return os_source.url();
  }

  // TODO(apaseltiner): Define this in a separate .cc file.
  static bool Read(blink::mojom::AttributionOsSourceDataView data,
                   attribution_reporting::OsSource* out) {
    GURL url;
    if (!data.ReadUrl(&url))
      return false;

    absl::optional<attribution_reporting::OsSource> os_source =
        attribution_reporting::OsSource::Create(std::move(url));
    if (!os_source.has_value())
      return false;

    *out = std::move(*os_source);
    return true;
  }
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::AttributionOsTriggerDataView,
                 attribution_reporting::OsTrigger> {
  static const GURL& url(const attribution_reporting::OsTrigger& os_trigger) {
    return os_trigger.url();
  }

  // TODO(apaseltiner): Define this in a separate .cc file.
  static bool Read(blink::mojom::AttributionOsTriggerDataView data,
                   attribution_reporting::OsTrigger* out) {
    GURL url;
    if (!data.ReadUrl(&url))
      return false;

    absl::optional<attribution_reporting::OsTrigger> os_trigger =
        attribution_reporting::OsTrigger::Create(std::move(url));
    if (!os_trigger.has_value())
      return false;

    *out = std::move(*os_trigger);
    return true;
  }
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_ATTRIBUTION_REPORTING_MOJOM_TRAITS_H_
