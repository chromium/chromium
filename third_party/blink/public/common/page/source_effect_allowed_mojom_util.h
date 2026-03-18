// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_SOURCE_EFFECT_ALLOWED_MOJOM_UTIL_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_SOURCE_EFFECT_ALLOWED_MOJOM_UTIL_H_

#include <string_view>

#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/drag/drag.mojom-shared.h"

namespace blink {

BLINK_COMMON_EXPORT blink::mojom::SourceEffectAllowed
SourceEffectAllowedFromString(std::string_view source_effect_allowed);

BLINK_COMMON_EXPORT std::string_view SourceEffectAllowedToString(
    blink::mojom::SourceEffectAllowed source_effect_allowed);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_SOURCE_EFFECT_ALLOWED_MOJOM_UTIL_H_
