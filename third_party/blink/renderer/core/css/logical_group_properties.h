// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_LOGICAL_GROUP_PROPERTIES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_LOGICAL_GROUP_PROPERTIES_H_

#include <bitset>
#include "third_party/blink/renderer/core/css/properties/css_bitset.h"

namespace blink {

// Properties whose presence signals that we may have to go through
// the logic of logical properties replacing other properties, if present.
// Equivalent to checking prop.IsInLogicalPropertyGroup() && prop.IsSurrogate(),
// but faster.
extern const CSSBitset kLogicalGroupProperties;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_LOGICAL_GROUP_PROPERTIES_H_
