/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2008 Apple Inc.  All right reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_BIDI_CHARACTER_RUN_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_BIDI_CHARACTER_RUN_H_

#include "third_party/blink/renderer/platform/text/bidi_context.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"

namespace blink {

struct BidiCharacterRun {
  USING_FAST_MALLOC(BidiCharacterRun);

 public:
  BidiCharacterRun(bool override,
                   unsigned char level,
                   int start,
                   int stop,
                   WTF::unicode::CharDirection dir,
                   WTF::unicode::CharDirection override_dir)
      : override_(override),
        level_(level),
        next_(nullptr),
        start_(start),
        stop_(stop) {
    DCHECK_LE(start_, stop_);
    if (dir == WTF::unicode::kOtherNeutral)
      dir = override_dir;

    level_ = level;

    // add level of run (cases I1 & I2)
    if (level_ % 2) {
      if (dir == WTF::unicode::kLeftToRight ||
          dir == WTF::unicode::kArabicNumber ||
          dir == WTF::unicode::kEuropeanNumber)
        level_++;
    } else {
      if (dir == WTF::unicode::kRightToLeft)
        level_++;
      else if (dir == WTF::unicode::kArabicNumber ||
               dir == WTF::unicode::kEuropeanNumber)
        level_ += 2;
    }
  }

  BidiCharacterRun(int start, int stop, unsigned char level)
      : override_(false),
        level_(level),
        next_(nullptr),
        start_(start),
        stop_(stop) {}

  int Start() const { return start_; }
  int Stop() const { return stop_; }
  unsigned char Level() const { return level_; }
  bool Reversed(bool visually_ordered) const {
    return level_ % 2 && !visually_ordered;
  }
  bool DirOverride(bool visually_ordered) {
    return override_ || visually_ordered;
  }
  TextDirection Direction() const {
    return Reversed(false) ? TextDirection::kRtl : TextDirection::kLtr;
  }

  BidiCharacterRun* Next() const { return next_; }
  void SetNext(BidiCharacterRun* next) { next_ = next; }

  // Do not add anything apart from bitfields until after m_next. See
  // https://bugs.webkit.org/show_bug.cgi?id=100173
  bool override_ : 1;
  // Used by BidiRun subclass which is a layering violation but enables us to
  // save 8 bytes per object on 64-bit.
  bool has_hyphen_ : 1;
  unsigned char level_;
  BidiCharacterRun* next_;
  int start_;
  int stop_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_BIDI_CHARACTER_RUN_H_
