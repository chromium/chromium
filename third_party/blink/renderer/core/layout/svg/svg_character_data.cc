// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/svg/svg_character_data.h"

#include <iostream>

namespace blink {

std::ostream& operator<<(std::ostream& ostream, const SvgCharacterData& data) {
  if (data.HasX() || data.HasY() || data.HasDx() || data.HasDy() ||
      data.HasRotate() || data.anchored_chunk) {
    ostream << "SvgCharacterData {";
    if (data.HasX()) {
      ostream << "x=" << data.x << " ";
    }
    if (data.HasY()) {
      ostream << "y=" << data.y << " ";
    }
    if (data.HasDx()) {
      ostream << "dx=" << data.dx << " ";
    }
    if (data.HasDy()) {
      ostream << "dy=" << data.dy << " ";
    }
    if (data.HasRotate()) {
      ostream << "rotate=" << data.rotate << " ";
    }
    if (data.anchored_chunk) {
      ostream << "anchored_chunk";
    }
    ostream << "}";
  } else {
    ostream << "SvgCharacterData {default}";
  }
  return ostream;
}

}  // namespace blink
