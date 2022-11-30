// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/touch_action_rect.h"

#include "cc/base/region.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

String TouchActionRect::ToString() const {
  return String(rect.ToString()) + " " +
         cc::TouchActionToString(allowed_touch_action);
}

std::ostream& operator<<(std::ostream& os,
                         const TouchActionRect& hit_test_rect) {
  return os << hit_test_rect.ToString();
}

}  // namespace blink
