// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_WIDGET_SCREEN_INFOS_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_WIDGET_SCREEN_INFOS_MOJOM_TRAITS_H_

#include "base/containers/flat_set.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/widget/screen_infos.h"
#include "third_party/blink/public/mojom/widget/screen_infos.mojom-shared.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ScreenInfosDataView, blink::ScreenInfos> {
  static const std::vector<blink::ScreenInfo>& screen_infos(
      const blink::ScreenInfos& r) {
    // Ensure ScreenInfo ids are unique and contain `current_display_id`.
    base::flat_set<int64_t> display_ids;
    display_ids.reserve(r.screen_infos.size());
    for (const blink::ScreenInfo& screen_info : r.screen_infos)
      CHECK(display_ids.insert(screen_info.display_id).second);
    CHECK(display_ids.find(r.current_display_id) != display_ids.end());
    return r.screen_infos;
  }

  static int64_t current_display_id(const blink::ScreenInfos& r) {
    // One `r.screen_infos` item must have a matching `display_id`; see above.
    return r.current_display_id;
  }

  static bool Read(blink::mojom::ScreenInfosDataView r,
                   blink::ScreenInfos* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_WIDGET_SCREEN_INFOS_MOJOM_TRAITS_H_
