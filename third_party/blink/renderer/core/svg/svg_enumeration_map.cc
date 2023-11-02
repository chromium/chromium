// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_enumeration_map.h"

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

uint16_t SVGEnumerationMap::ValueFromName(const String& name) const {
  for (const Entry& entry : *this) {
    if (name == entry.name)
      return entry.value;
  }
  return 0;
}

}  // namespace blink
