/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2009, 2010 Apple Inc.
 * All right reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_BIDI_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_BIDI_CONTEXT_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"

namespace blink {

enum BidiEmbeddingSource { kFromStyleOrDOM, kFromUnicode };

// Used to keep track of explicit embeddings.
class PLATFORM_EXPORT BidiContext : public ThreadSafeRefCounted<BidiContext> {
 public:
  static scoped_refptr<BidiContext> Create(
      unsigned char level,
      WTF::unicode::CharDirection,
      bool override = false,
      BidiEmbeddingSource = kFromStyleOrDOM,
      BidiContext* parent = nullptr);

  BidiContext* Parent() const { return parent_.get(); }
  unsigned char Level() const { return level_; }
  WTF::unicode::CharDirection Dir() const {
    return static_cast<WTF::unicode::CharDirection>(direction_);
  }
  bool Override() const { return override_; }
  BidiEmbeddingSource Source() const {
    return static_cast<BidiEmbeddingSource>(source_);
  }

  scoped_refptr<BidiContext> CopyStackRemovingUnicodeEmbeddingContexts();

  // http://www.unicode.org/reports/tr9/#Modifications
  // 6.3 raised the limit from 61 to 125.
  // http://unicode.org/reports/tr9/#BD2
  static const unsigned char kMaxLevel = 125;

 private:
  BidiContext(unsigned char level,
              WTF::unicode::CharDirection direction,
              bool override,
              BidiEmbeddingSource source,
              BidiContext* parent)
      : level_(level),
        direction_(direction),
        override_(override),
        source_(source),
        parent_(parent) {
    DCHECK(level <= kMaxLevel);
  }

  static scoped_refptr<BidiContext> CreateUncached(unsigned char level,
                                                   WTF::unicode::CharDirection,
                                                   bool override,
                                                   BidiEmbeddingSource,
                                                   BidiContext* parent);

  // The maximium bidi level is 125:
  // http://unicode.org/reports/tr9/#Explicit_Levels_and_Directions
  unsigned level_ : 7;
  unsigned direction_ : 5;  // Direction
  unsigned override_ : 1;
  unsigned source_ : 1;  // BidiEmbeddingSource
  scoped_refptr<BidiContext> parent_;
};

inline unsigned char NextGreaterOddLevel(unsigned char level) {
  return (level + 1) | 1;
}

inline unsigned char NextGreaterEvenLevel(unsigned char level) {
  return (level + 2) & ~1;
}

PLATFORM_EXPORT bool operator==(const BidiContext&, const BidiContext&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_BIDI_CONTEXT_H_
