// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_BLINK_AX_ENUM_CONVERSION_H_
#define CONTENT_RENDERER_ACCESSIBILITY_BLINK_AX_ENUM_CONVERSION_H_

#include <stdint.h>

#include "third_party/blink/public/web/web_ax_object.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"

namespace content {

ax::mojom::TextStyle AXTextStyleFromBlink(blink::WebAXTextStyle text_style);

// Provides a conversion between the WebAXObject state
// accessors and a state bitmask stored in an AXNodeData.
// (Note that some rare states are sent as boolean attributes
// in AXNodeData instead.)
void AXStateFromBlink(const blink::WebAXObject& o, ui::AXNodeData* dst);

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_BLINK_AX_ENUM_CONVERSION_H_
