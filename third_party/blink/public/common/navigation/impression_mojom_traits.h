// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_NAVIGATION_IMPRESSION_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_NAVIGATION_IMPRESSION_MOJOM_TRAITS_H_

#include "build/build_config.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/mojom/conversions/conversions.mojom.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ImpressionDataView, blink::Impression> {
  static const url::Origin& conversion_destination(const blink::Impression& r) {
    return r.conversion_destination;
  }

  static const absl::optional<url::Origin>& reporting_origin(
      const blink::Impression& r) {
    return r.reporting_origin;
  }

  static uint64_t impression_data(const blink::Impression& r) {
    return r.impression_data;
  }

  static const absl::optional<base::TimeDelta>& expiry(
      const blink::Impression& r) {
    return r.expiry;
  }

  static int64_t priority(const blink::Impression& r) { return r.priority; }

  static bool Read(blink::mojom::ImpressionDataView r, blink::Impression* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_NAVIGATION_IMPRESSION_MOJOM_TRAITS_H_
