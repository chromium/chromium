// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_CACHE_HINT_ATTRIBUTE_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_CACHE_HINT_ATTRIBUTE_VALUE_H_

#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

// Enums for the `cachehint` attribute (in proposal).
// https://github.com/explainers-by-googlers/inline-script-cache-hint
enum class CacheHintAttributeValue { kDefault, kEager, kNever };

CacheHintAttributeValue GetCacheHintAttributeValue(StringView value);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_CACHE_HINT_ATTRIBUTE_VALUE_H_
