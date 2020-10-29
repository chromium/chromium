// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/common/mobile_metrics/mobile_friendliness_mojom_traits.h"
#include "third_party/blink/public/common/mobile_metrics/mobile_friendliness.h"

namespace mojo {

bool StructTraits<blink::mojom::MobileFriendlinessDataView,
                  blink::MobileFriendliness>::
    Read(blink::mojom::MobileFriendlinessDataView data,
         blink::MobileFriendliness* mf) {
  if (data.viewport_initial_scale() < 0 || data.viewport_hardcoded_width() < 0)
    return false;
  mf->viewport_device_width = data.viewport_device_width();
  mf->viewport_initial_scale = data.viewport_initial_scale();
  mf->viewport_hardcoded_width = data.viewport_hardcoded_width();
  mf->allow_user_zoom = data.allow_user_zoom();
  return true;
}

}  // namespace mojo
