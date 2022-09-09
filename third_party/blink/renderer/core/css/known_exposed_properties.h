// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_KNOWN_EXPOSED_PROPERTIES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_KNOWN_EXPOSED_PROPERTIES_H_

#include <bitset>
#include "third_party/blink/renderer/core/css/properties/css_bitset.h"

namespace blink {

// For properties that are not behind runtime flags (which are nearly all,
// in practice), we can avoid resolving and looking them up to check the
// exposure; we can just check this bitmap instead (which fits neatly into
// two rather hot cache lines). This saves a little time in parsing.
extern const CSSBitset kKnownExposedProperties;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_KNOWN_EXPOSED_PROPERTIES_H_
