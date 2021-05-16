// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/widget/screen_infos_mojom_traits.h"

#include "third_party/blink/public/mojom/widget/screen_info.mojom.h"

namespace mojo {

// static
bool StructTraits<blink::mojom::ScreenInfosDataView, blink::ScreenInfos>::Read(
    blink::mojom::ScreenInfosDataView data,
    blink::ScreenInfos* out) {
  if (!data.ReadScreenInfos(&out->screen_infos))
    return false;
  out->current_display_id = data.current_display_id();

  // Ensure ScreenInfo ids are unique and contain the `current_display_id`.
  base::flat_set<int64_t> display_ids;
  display_ids.reserve(out->screen_infos.size());
  for (const auto& screen_info : out->screen_infos) {
    if (!display_ids.insert(screen_info.display_id).second)
      return false;
  }
  return display_ids.find(out->current_display_id) != display_ids.end();
}

}  // namespace mojo
