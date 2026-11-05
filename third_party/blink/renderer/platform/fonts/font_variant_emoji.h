// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_VARIANT_EMOJI_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_VARIANT_EMOJI_H_

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// https://drafts.csswg.org/css-fonts/#font-variant-emoji-prop
enum class FontVariantEmoji { kNormal, kText, kEmoji, kUnicode };

String ToString(FontVariantEmoji);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_VARIANT_EMOJI_H_
