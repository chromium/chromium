// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_palette.h"

#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

#include "third_party/skia/include/core/SkFontArguments.h"

namespace blink {

unsigned FontPalette::GetHash() const {
  unsigned computed_hash = 0;
  WTF::AddIntToHash(computed_hash, palette_keyword_);
  WTF::AddIntToHash(computed_hash,
                    AtomicStringHash::GetHash(palette_values_name_));
  return computed_hash;
}

bool FontPalette::operator==(const FontPalette& other) const {
  return palette_keyword_ == other.palette_keyword_ &&
         palette_values_name_ == other.palette_values_name_;
}

}  // namespace blink
