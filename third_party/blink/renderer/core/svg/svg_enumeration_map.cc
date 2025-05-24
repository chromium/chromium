// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_enumeration_map.h"

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

uint16_t SVGEnumerationMap::ValueFromName(const String& name) const {
  auto it = std::ranges::find(entries_, name);
  return it != entries_.end()
             ? static_cast<uint16_t>(1 + std::distance(entries_.begin(), it))
             : 0;
}

}  // namespace blink
