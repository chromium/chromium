// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_TOUCH_ACTION_OPTIONAL_MOJOM_TRAITS_H_
#define CONTENT_COMMON_INPUT_TOUCH_ACTION_OPTIONAL_MOJOM_TRAITS_H_

#include "base/optional.h"
#include "content/common/input/input_handler.mojom.h"

namespace mojo {

template <>
struct StructTraits<content::mojom::TouchActionOptionalDataView,
                    cc::TouchAction> {
  static cc::TouchAction touch_action(cc::TouchAction action) { return action; }
  static bool Read(content::mojom::TouchActionOptionalDataView r,
                   cc::TouchAction* out);
};

}  // namespace mojo

#endif  // CONTENT_COMMON_INPUT_TOUCH_ACTION_OPTIONAL_MOJOM_TRAITS_H_
