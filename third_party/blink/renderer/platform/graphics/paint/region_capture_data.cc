// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/region_capture_data.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

String RegionCaptureData::ToString() const {
  StringBuilder sb;
  sb.Append("{");
  for (auto it = map.begin(); it != map.end(); ++it) {
    if (it != map.begin()) {
      sb.Append(", ");
    }
    sb.Append("{");
    sb.Append(it->first->ToString().c_str());
    sb.Append(": ");
    sb.Append(it->second.ToString().c_str());
    sb.Append("}");
  }
  sb.Append("}");
  return sb.ToString();
}

}  // namespace blink
