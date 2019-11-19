// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/touch_action_optional_mojom_traits.h"

#include "content/public/common/common_param_traits_macros.h"

namespace mojo {
bool StructTraits<
    content::mojom::TouchActionOptionalDataView,
    cc::TouchAction>::Read(content::mojom::TouchActionOptionalDataView r,
                           cc::TouchAction* out) {
  return r.ReadTouchAction(out);
}
}  // namespace mojo
