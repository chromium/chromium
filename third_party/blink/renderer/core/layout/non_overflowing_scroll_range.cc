// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/non_overflowing_scroll_range.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

String NonOverflowingScrollRange::ToString() const {
  StringBuilder builder;
  builder.Append("{ anchor_element=");
  builder.Append(anchor_element ? anchor_element->ToString() : "(nil)");
  builder.Append(", containing_block_range={x_min=");
  builder.Append(containing_block_range.x_min
                     ? containing_block_range.x_min->ToString()
                     : "(nullopt)");
  builder.Append(", x_max=");
  builder.Append(containing_block_range.x_max
                     ? containing_block_range.x_max->ToString()
                     : "(nullopt)");
  builder.Append(", y_min=");
  builder.Append(containing_block_range.y_min
                     ? containing_block_range.y_min->ToString()
                     : "(nullopt)");
  builder.Append(", y_max=");
  builder.Append(containing_block_range.y_max
                     ? containing_block_range.y_max->ToString()
                     : "(nullopt)");
  builder.Append("}}");
  return builder.ReleaseString();
}

}  // namespace blink
