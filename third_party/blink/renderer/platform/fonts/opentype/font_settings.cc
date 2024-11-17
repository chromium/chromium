// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/opentype/font_settings.h"

#include <array>

#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hasher.h"

namespace blink {

uint32_t AtomicStringToFourByteTag(const AtomicString& tag) {
  DCHECK_EQ(tag.length(), 4u);
  return (((tag[0]) << 24) | ((tag[1]) << 16) | ((tag[2]) << 8) | (tag[3]));
}

AtomicString FourByteTagToAtomicString(uint32_t tag) {
  const std::array<LChar, 4> tag_string = {
      static_cast<LChar>(tag >> 24), static_cast<LChar>(tag >> 16),
      static_cast<LChar>(tag >> 8), static_cast<LChar>(tag)};
  return AtomicString(tag_string);
}

unsigned FontVariationSettings::GetHash() const {
  unsigned computed_hash = size() ? 5381 : 0;
  unsigned num_features = size();
  for (unsigned i = 0; i < num_features; ++i) {
    WTF::AddIntToHash(computed_hash, at(i).Tag());
    WTF::AddFloatToHash(computed_hash, at(i).Value());
  }
  return computed_hash;
}

}  // namespace blink
