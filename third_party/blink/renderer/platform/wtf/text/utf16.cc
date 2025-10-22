// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/utf16.h"

#include "base/containers/span.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_fast_path.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"

namespace blink {

// Word-at-a-time latin1 check.
bool ContainsOnlyLatin1(base::span<const UChar> text) {
  constexpr size_t kCharsPerWord = sizeof(MachineWord) / sizeof(UChar);
  static_assert(kCharsPerWord == 2 || kCharsPerWord == 4);
  constexpr MachineWord kLatin1Mask =
      (kCharsPerWord == 4) ? 0xFF00FF00FF00FF00ULL : 0xFF00FF00UL;

  // Process the first unaligned characters.
  while (text.size() && !IsAlignedToMachineWord(text.data())) {
    if (text.front() & 0xFF00) {
      return false;
    }
    text = text.subspan(1u);
  }

  // Efficiently process the aligned words.
  while (text.size() >= kCharsPerWord) {
    const MachineWord word = *reinterpret_cast<const MachineWord*>(text.data());
    if (word & kLatin1Mask) {
      return false;
    }
    text = text.subspan(kCharsPerWord);
  }

  // Process the remaining chars.
  while (!text.empty()) {
    if (text.front() & 0xFF00) {
      return false;
    }
    text = text.subspan(1u);
  }

  return true;
}

}  // namespace blink
