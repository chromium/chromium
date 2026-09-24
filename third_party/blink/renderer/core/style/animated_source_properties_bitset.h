// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_ANIMATED_SOURCE_PROPERTIES_BITSET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_ANIMATED_SOURCE_PROPERTIES_BITSET_H_

#include "base/containers/enum_set.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"

namespace blink {

// Set of CSS properties that track animated sources.
using AnimatedSourceBitset = base::EnumSet<AnimatedSourceProperty>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_ANIMATED_SOURCE_PROPERTIES_BITSET_H_
