// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/cache_hint_attribute_value.h"

#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

CacheHintAttributeValue GetCacheHintAttributeValue(StringView value) {
  if (value == "eager") {
    return CacheHintAttributeValue::kEager;
  }
  if (value == "never") {
    return CacheHintAttributeValue::kNever;
  }
  return CacheHintAttributeValue::kDefault;
}

}  // namespace blink
