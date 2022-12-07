// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_RESOLVED_FONT_FEATURES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_RESOLVED_FONT_FEATURES_H_

#include <utility>
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

using ResolvedFontFeatures = Vector<std::pair<uint32_t, uint32_t>>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_RESOLVED_FONT_FEATURES_H_
