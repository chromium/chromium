/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2010 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 */

#include "third_party/blink/renderer/platform/text/character_break_iterator.h"

namespace blink {

unsigned NumGraphemeClusters(const StringView& string) {
  unsigned string_length = string.length();

  if (!string_length) {
    return 0;
  }

  // The only Latin-1 Extended Grapheme Cluster is CR LF
  if (string.Is8Bit() && !string.contains('\r')) {
    return string_length;
  }

  CharacterBreakIterator it(string);
  if (!it) {
    return string_length;
  }

  unsigned num = 0;
  while (it.Next() != kTextBreakDone) {
    ++num;
  }
  return num;
}

void GraphemesClusterList(const StringView& text,
                          base::span<unsigned> graphemes) {
  const unsigned length = text.length();
  DCHECK_EQ(length, graphemes.size());
  if (!length) {
    return;
  }

  CharacterBreakIterator it(text);
  int cursor_pos = it.Next();
  unsigned count = 0;
  unsigned pos = 0;
  while (cursor_pos >= 0) {
    for (; pos < static_cast<unsigned>(cursor_pos) && pos < length; ++pos) {
      graphemes[pos] = count;
    }
    cursor_pos = it.Next();
    count++;
  }
}

unsigned LengthOfGraphemeCluster(const String& string, unsigned offset) {
  unsigned string_length = string.length();

  if (string_length - offset <= 1) {
    return string_length - offset;
  }

  // The only Latin-1 Extended Grapheme Cluster is CRLF.
  if (string.Is8Bit()) {
    return 1 + (string[offset] == '\r' && string[offset + 1] == '\n');
  }

  CharacterBreakIterator it(string);
  if (!it) {
    return string_length - offset;
  }

  if (it.Following(offset) == kTextBreakDone) {
    return string_length - offset;
  }
  return it.Current() - offset;
}

}  // namespace blink
