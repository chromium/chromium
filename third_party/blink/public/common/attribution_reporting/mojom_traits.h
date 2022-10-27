// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_ATTRIBUTION_REPORTING_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_ATTRIBUTION_REPORTING_MOJOM_TRAITS_H_

#include <string>
#include <utility>

#include "components/attribution_reporting/parse.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-shared.h"
#include "url/gurl.h"
#include "url/mojom/origin_mojom_traits.h"
#include "url/mojom/url_gurl_mojom_traits.h"
#include "url/origin.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::AttributionOsSourceDataView,
                 attribution_reporting::OsSource> {
  static const GURL& url(const attribution_reporting::OsSource& os_source) {
    return os_source.url();
  }

  static const std::string& os_destination(
      const attribution_reporting::OsSource& os_source) {
    return os_source.os_destination();
  }

  static const url::Origin& web_destination(
      const attribution_reporting::OsSource& os_source) {
    return os_source.web_destination();
  }

  // TODO(apaseltiner): Define this in a separate .cc file.
  static bool Read(blink::mojom::AttributionOsSourceDataView data,
                   attribution_reporting::OsSource* out) {
    GURL url;
    if (!data.ReadUrl(&url))
      return false;

    std::string os_destination;
    if (!data.ReadOsDestination(&os_destination))
      return false;

    url::Origin web_destination;
    if (!data.ReadWebDestination(&web_destination))
      return false;

    absl::optional<attribution_reporting::OsSource> os_source =
        attribution_reporting::OsSource::Create(std::move(url),
                                                std::move(os_destination),
                                                std::move(web_destination));
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
