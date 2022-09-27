// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_fallback_priority.h"

namespace blink {

bool IsNonTextFallbackPriority(FontFallbackPriority fallback_priority) {
  return fallback_priority == FontFallbackPriority::kEmojiText ||
         fallback_priority == FontFallbackPriority::kEmojiEmoji;
}

}  // namespace blink
