// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_NG_SVG_CHARACTER_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_NG_SVG_CHARACTER_DATA_H_

#include <iosfwd>

#include "third_party/blink/renderer/core/layout/svg/svg_character_data.h"

namespace blink {

// An extension of blink::SVGCharacterData for LayoutNG.
// This struct adds |anchored_chunk| data member to SVGCharacterData.
struct NGSvgCharacterData final : public SVGCharacterData {
  bool anchored_chunk = false;
};

std::ostream& operator<<(std::ostream& ostream, const NGSvgCharacterData& data);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_NG_SVG_CHARACTER_DATA_H_
