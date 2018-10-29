// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/input/touch_action_util.h"

#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {
namespace touch_action_util {

TouchAction ComputeEffectiveTouchAction(const Node& node) {
  if (node.GetComputedStyle())
    return node.GetComputedStyle()->GetEffectiveTouchAction();

  return TouchAction::kTouchActionAuto;
}

}  // namespace touch_action_util
}  // namespace blink
