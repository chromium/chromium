// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_CHARACTER_RANGE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_CHARACTER_RANGE_H_

namespace blink {

struct CharacterRange {
  CharacterRange(float from, float to, float ascent, float descent)
      : start(from), end(to), ascent(ascent), descent(descent) {
#if DCHECK_IS_ON()
    if (isnan(start)) {
      // start/end can saturate in tests, but not a real world case.
      DCHECK(isnan(end));
    } else {
      DCHECK_LE(start, end);
    }
#endif
  }

  float Width() const { return end - start; }
  float Height() const { return ascent + descent; }

  float start;
  float end;

  float ascent;
  float descent;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_CHARACTER_RANGE_H_
