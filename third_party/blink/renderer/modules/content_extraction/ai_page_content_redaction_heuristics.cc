// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_extraction/ai_page_content_redaction_heuristics.h"

#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

bool IsSecurityMaskCharacter(UChar c) {
  switch (c) {
    // Standard Asterisks and stars.
    case '*':
    case 0x2731:
    case 0x2732:
    case 0x2733:
    case 0xFF0A:

    // Standard bullets and circles.
    case 0x2022:
    case 0x25CF:
    case 0x25CB:
    case 0x25EF:
    case 0x26AB:
    case 0x2B24:
    case 0x25E6:
    case 0x25C9:

    // Dots and math operators.
    case 0x00B7:
    case 0x2219:
    case 0x22C5:
    case 0x2802:
    case 0x2812:
    case 0x2836:

    // Squares, blocks, and diamonds.
    case 0x25A0:
    case 0x25A1:
    case 0x25AA:
    case 0x25AB:
    case 0x25AE:
    case 0x2588:
    case 0x2589:
    case 0x25C6:
    case 0x25C7:
      return true;
    default:
      return false;
  }
}

}  // namespace

bool IsCSSSecurityMaskingEnabled(const LayoutObject& object) {
  // Checks the computed value of the non-standard CSS property
  // `-webkit-text-security`. Authors sometimes use this on non-password
  // elements to create custom masked "password-like" fields.
  const ComputedStyle* style = object.Style();
  if (!style) {
    return false;
  }
  return style->TextSecurity() != ETextSecurity::kNone;
}

bool IsLikelyJSCustomPasswordField(const String& value) {
  // Heuristic for JS-masked values where most characters are replaced by
  // mask characters but one character (often the last typed character)
  // remains visible.
  if (value.length() < 2) {
    return false;
  }

  wtf_size_t mask_count = 0;
  bool has_visible_last_character = false;
  for (wtf_size_t index = 0; index < value.length(); ++index) {
    const UChar ch = value[index];
    // Whitespace strongly signals this is not password-like.
    if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
      return false;
    }
    if (IsSecurityMaskCharacter(ch)) {
      ++mask_count;
    } else {
      // A visible non-mask character before the final position is unlikely
      // for password-style masking.
      if (index + 1 < value.length()) {
        return false;
      }
      has_visible_last_character = true;
    }
  }

  // Classify early once masking begins to avoid leaking typed characters.
  if (has_visible_last_character) {
    if (mask_count >= 1) {
      return true;
    }
    return value.length() == 2;
  }

  return mask_count >= value.length() - 1;
}

}  // namespace blink
