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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/text/text_break_iterator.h"

#include <unicode/uchar.h>
#include <unicode/uvernum.h>

#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/break_iterator_data_inline_header.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace blink {

unsigned NumGraphemeClusters(const String& string) {
  unsigned string_length = string.length();

  if (!string_length)
    return 0;

  // The only Latin-1 Extended Grapheme Cluster is CR LF
  if (string.Is8Bit() && !string.Contains('\r'))
    return string_length;

  NonSharedCharacterBreakIterator it(string);
  if (!it)
    return string_length;

  unsigned num = 0;
  while (it.Next() != kTextBreakDone)
    ++num;
  return num;
}

void GraphemesClusterList(const StringView& text, Vector<unsigned>* graphemes) {
  const unsigned length = text.length();
  graphemes->resize(length);
  if (!length)
    return;

  NonSharedCharacterBreakIterator it(text);
  int cursor_pos = it.Next();
  unsigned count = 0;
  unsigned pos = 0;
  while (cursor_pos >= 0) {
    for (; pos < static_cast<unsigned>(cursor_pos) && pos < length; ++pos) {
      (*graphemes)[pos] = count;
    }
    cursor_pos = it.Next();
    count++;
  }
}

unsigned LengthOfGraphemeCluster(const String& string, unsigned offset) {
  unsigned string_length = string.length();

  if (string_length - offset <= 1)
    return string_length - offset;

  // The only Latin-1 Extended Grapheme Cluster is CRLF.
  if (string.Is8Bit()) {
    auto* characters = string.Characters8();
    return 1 + (characters[offset] == '\r' && characters[offset + 1] == '\n');
  }

  NonSharedCharacterBreakIterator it(string);
  if (!it)
    return string_length - offset;

  if (it.Following(offset) == kTextBreakDone)
    return string_length - offset;
  return it.Current() - offset;
}

// Pack 8 bits into one byte
#define B(a, b, c, d, e, f, g, h)                                         \
  ((a) | ((b) << 1) | ((c) << 2) | ((d) << 3) | ((e) << 4) | ((f) << 5) | \
   ((g) << 6) | ((h) << 7))

#define BA_LB_COUNT U_LB_COUNT
// Line breaking table for CSS word-break: break-all. This table differs from
// asciiLineBreakTable in:
// - Indices are Line Breaking Classes defined in UAX#14 Unicode Line Breaking
//   Algorithm: http://unicode.org/reports/tr14/#DescriptionOfProperties
// - 1 indicates additional break opportunities. 0 indicates to fallback to
//   normal line break, not "prohibit break."
// clang-format off
static const unsigned char kBreakAllLineBreakClassTable[][BA_LB_COUNT / 8 + 1] = {
    // XX AI AL B2 BA BB BK CB    CL CM CR EX GL HY ID IN    IS LF NS NU OP PO PR QU    SA SG SP SY ZW NL WJ H2    H3 JL JT JV CP CJ HL RI    EB EM ZWJ AK AP AS VF VI
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // XX
    { B(0, 1, 1, 0, 1, 0, 0, 0), B(0, 0, 0, 0, 0, 1, 0, 0), B(0, 0, 0, 1, 1, 0, 1, 0), B(1, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 1, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // AI
    { B(0, 1, 1, 0, 1, 0, 0, 0), B(0, 0, 0, 0, 0, 1, 0, 0), B(0, 0, 0, 1, 1, 0, 1, 0), B(1, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 1, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // AL
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // B2
    { B(0, 1, 1, 0, 1, 0, 0, 0), B(0, 0, 0, 0, 0, 1, 0, 0), B(0, 0, 0, 1, 1, 0, 1, 0), B(1, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 1, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // BA
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // BB
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // BK
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // CB
    { B(0, 1, 1, 0, 1, 0, 0, 0), B(0, 0, 0, 0, 0, 1, 0, 0), B(0, 0, 0, 1, 0, 0, 1, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 1, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // CL
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // CM
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // CR
    { B(0, 1, 1, 0, 1, 0, 0, 0), B(0, 0, 0, 0, 0, 1, 0, 0), B(0, 0, 0, 1, 0, 1, 1, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 1, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // EX
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // GL
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 1, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // HY
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // ID
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // IN
    { B(0, 1, 1, 0, 1, 0, 0, 0), B(0, 0, 0, 0, 0, 1, 0, 0), B(0, 0, 0, 1, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 1, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // IS
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // LF
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // NS
    { B(0, 1, 1, 0, 1, 0, 0, 0), B(0, 0, 0, 0, 0, 1, 0, 0), B(0, 0, 0, 1, 1, 0, 1, 0), B(1, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 1, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // NU
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // OP
    { B(0, 1, 1, 0, 1, 0, 0, 0), B(0, 0, 0, 0, 0, 1, 0, 0), B(0, 0, 0, 1, 0, 1, 1, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 1, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // PO
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 1, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // PR
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // QU
    { B(0, 1, 1, 0, 1, 0, 0, 0), B(0, 0, 0, 0, 0, 1, 0, 0), B(0, 0, 0, 1, 1, 0, 1, 0), B(1, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 1, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // SA
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // SG
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // SP
    { B(0, 1, 1, 0, 1, 0, 0, 0), B(0, 0, 0, 0, 0, 1, 0, 0), B(0, 0, 0, 1, 1, 0, 1, 0), B(1, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 1, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // SY
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // ZW
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // NL
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // WJ
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // H2
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // H3
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // JL
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // JT
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // JV
    { B(0, 1, 1, 0, 1, 0, 0, 0), B(0, 0, 0, 0, 0, 1, 0, 0), B(0, 0, 0, 1, 0, 0, 1, 0), B(1, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 1, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // CP
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // CJ
    { B(0, 1, 1, 0, 1, 0, 0, 0), B(0, 0, 0, 0, 0, 1, 0, 0), B(0, 0, 0, 1, 1, 0, 1, 0), B(1, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 1, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // HL
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // RI
    // Added in ICU 58.
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // EB
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // EM
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // ZWJ
#if U_ICU_VERSION_MAJOR_NUM >= 74
    // Added in ICU 74. https://icu.unicode.org/download/74
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // AK
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // AP
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // AS
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // VF
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0,  0, 0, 0, 0, 0, 0) }, // VI
#endif  // U_ICU_VERSION_MAJOR_NUM >= 74
};
// clang-format on

#undef B

static_assert(std::size(kBreakAllLineBreakClassTable) == BA_LB_COUNT,
              "breakAllLineBreakClassTable should be consistent");

static inline ULineBreak LineBreakPropertyValue(UChar last_ch, UChar ch) {
  if (ch == '+')  // IE tailors '+' to AL-like class when break-all is enabled.
    return U_LB_ALPHABETIC;
  UChar32 ch32 = U16_IS_LEAD(last_ch) && U16_IS_TRAIL(ch)
                     ? U16_GET_SUPPLEMENTARY(last_ch, ch)
                     : ch;
  return static_cast<ULineBreak>(u_getIntPropertyValue(ch32, UCHAR_LINE_BREAK));
}

static inline bool ShouldBreakAfterBreakAll(ULineBreak last_line_break,
                                            ULineBreak line_break) {
  if (line_break >= 0 && line_break < BA_LB_COUNT && last_line_break >= 0 &&
      last_line_break < BA_LB_COUNT) {
    const unsigned char* table_row =
        kBreakAllLineBreakClassTable[last_line_break];
    return table_row[line_break / 8] & (1 << (line_break % 8));
  }
  return false;
}

// Computes if 'word-break:keep-all' should prevent line break.
// https://drafts.csswg.org/css-text-3/#valdef-word-break-keep-all
// The spec is not very verbose on how this should work. This logic prevents L/M
// general categories and complex line breaking since the spec says "except some
// south east aisans".
// https://github.com/w3c/csswg-drafts/issues/1619
static inline bool ShouldKeepAfterKeepAll(UChar last_ch,
                                          UChar ch,
                                          UChar next_ch) {
  UChar pre_ch = U_MASK(u_charType(ch)) & U_GC_M_MASK ? last_ch : ch;
  return U_MASK(u_charType(pre_ch)) & (U_GC_L_MASK | U_GC_N_MASK) &&
         !WTF::unicode::HasLineBreakingPropertyComplexContext(pre_ch) &&
         U_MASK(u_charType(next_ch)) & (U_GC_L_MASK | U_GC_N_MASK) &&
         !WTF::unicode::HasLineBreakingPropertyComplexContext(next_ch);
}

enum class FastBreakResult : uint8_t { kNoBreak, kCanBreak, kUnknown };

template <typename CharacterType>
struct LazyLineBreakIterator::Context {
  STACK_ALLOCATED();

 public:
  struct ContextChar {
    STACK_ALLOCATED();

   public:
    ContextChar() = default;
    explicit ContextChar(UChar ch) : ch(ch), is_space(IsBreakableSpace(ch)) {}

    UChar ch = 0;
    bool is_space = false;
  };

  Context(const CharacterType* str,
          unsigned len,
          unsigned start_offset,
          unsigned index) {
    DCHECK_GE(index, start_offset);
    CHECK_LE(index, len);
    if (index > start_offset) {
      last = ContextChar(str[index - 1]);
      if (index > start_offset + 1) {
        last_last_ch = str[index - 2];
      }
    }
  }

  bool Fetch(const CharacterType* str, unsigned len, unsigned index) {
    if (index >= len) [[unlikely]] {
      return false;
    }
    current = ContextChar(str[index]);
    return true;
  }

  void Advance(unsigned& index) {
    ++index;
    last_last_ch = last.ch;
    last = current;
  }

  FastBreakResult ShouldBreakFast(bool disable_soft_hyphen) const {
    const UChar last_ch = last.ch;
    const UChar ch = current.ch;
    if (last_ch < kFastLineBreakMinChar || ch < kFastLineBreakMinChar)
        [[unlikely]] {
      return FastBreakResult::kNoBreak;
    }

    // U+002D HYPHEN-MINUS may depend on the context.
    static_assert('-' >= kFastLineBreakMinChar);
    if (last_ch == '-') [[unlikely]] {
      if (ch <= 0x7F) {
        // Up to U+007F is fast-breakable. See `LineBreakData::FillAscii()`.
        if (IsASCIIDigit(ch)) {
          // Don't allow line breaking between '-' and a digit if the '-' may
          // mean a minus sign in the context, while allow breaking in
          // 'ABCD-1234' and '1234-5678' which may be in long URLs.
          return IsASCIIAlphanumeric(last_last_ch) ? FastBreakResult::kCanBreak
                                                   : FastBreakResult::kNoBreak;
        }
      } else if (RuntimeEnabledFeatures::BreakIteratorHyphenMinusEnabled()) {
        // Defer to the Unicode algorithm to take more context into account.
        return FastBreakResult::kUnknown;
      }
    }

    // If both characters are in the fast line break table, use it for enhanced
    // speed. For ASCII characters, it is also for compatibility. The table is
    // generated at the build time, see the `LineBreakData` class.
    if (last_ch <= kFastLineBreakMaxChar && ch <= kFastLineBreakMaxChar) {
      if (!GetFastLineBreak(last_ch, ch)) {
        return FastBreakResult::kNoBreak;
      }
      static_assert(kSoftHyphenCharacter <= kFastLineBreakMaxChar);
      if (disable_soft_hyphen && last_ch == kSoftHyphenCharacter) [[unlikely]] {
        return FastBreakResult::kNoBreak;
      }
      return FastBreakResult::kCanBreak;
    }

    // Otherwise defer to the Unicode algorithm.
    static_assert(kNoBreakSpaceCharacter <= kFastLineBreakMaxChar,
                  "Include NBSP for the performance.");
    return FastBreakResult::kUnknown;
  }

  ContextChar current;
  ContextChar last;
  CharacterType last_last_ch = 0;
};

template <typename CharacterType,
          LineBreakType line_break_type,
          BreakSpaceType break_space>
inline unsigned LazyLineBreakIterator::NextBreakablePosition(
    unsigned pos,
    const CharacterType* str,
    unsigned len) const {
  Context<CharacterType> context(str, len, start_offset_, pos);
  unsigned next_break = 0;
  ULineBreak last_line_break;
  if constexpr (line_break_type == LineBreakType::kBreakAll) {
    last_line_break =
        LineBreakPropertyValue(context.last_last_ch, context.last.ch);
  }
  for (unsigned i = pos; context.Fetch(str, len, i); context.Advance(i)) {
    switch (break_space) {
      case BreakSpaceType::kAfterSpaceRun:
        if (context.current.is_space) {
          continue;
        }
        if (context.last.is_space) {
          return i;
        }
        break;
      case BreakSpaceType::kAfterEverySpace:
        if (context.last.is_space ||
            Character::IsOtherSpaceSeparator(context.last.ch)) {
          return i;
        }
        if ((context.current.is_space ||
             Character::IsOtherSpaceSeparator(context.current.ch)) &&
            i + 1 < len) {
          return i + 1;
        }
        break;
    }

    const FastBreakResult fast_break_result =
        context.ShouldBreakFast(disable_soft_hyphen_);
    if (fast_break_result == FastBreakResult::kCanBreak) {
      return i;
    }

    if constexpr (line_break_type == LineBreakType::kBreakAll) {
      if (!U16_IS_LEAD(context.current.ch)) {
        ULineBreak line_break =
            LineBreakPropertyValue(context.last.ch, context.current.ch);
        if (ShouldBreakAfterBreakAll(last_line_break, line_break)) {
          return i > pos && U16_IS_TRAIL(context.current.ch) ? i - 1 : i;
        }
        if (line_break != U_LB_COMBINING_MARK) {
          last_line_break = line_break;
        }
      }
    } else if constexpr (line_break_type == LineBreakType::kKeepAll) {
      if (ShouldKeepAfterKeepAll(context.last_last_ch, context.last.ch,
                                 context.current.ch)) {
        // word-break:keep-all prevents breaks between East Asian ideographic.
        continue;
      }
    }

    if (fast_break_result == FastBreakResult::kNoBreak) {
      continue;
    }

    if (next_break < i || !next_break) {
      // Don't break if positioned at start of primary context.
      if (i <= start_offset_) [[unlikely]] {
        continue;
      }
      TextBreakIterator* break_iterator = GetIterator();
      if (!break_iterator) [[unlikely]] {
        continue;
      }
      next_break = i - 1;
      for (;;) {
        // Adjust the offset by |start_offset_| because |break_iterator|
        // has text after |start_offset_|.
        DCHECK_GE(next_break, start_offset_);
        const int32_t following = break_iterator->following(
            static_cast<int32_t>(next_break - start_offset_));
        if (following < 0) [[unlikely]] {
          DCHECK_EQ(following, icu::BreakIterator::DONE);
          next_break = len;
          break;
        }
        next_break = following + start_offset_;
        if (disable_soft_hyphen_ && next_break > 0 &&
            str[next_break - 1] == kSoftHyphenCharacter) [[unlikely]] {
          continue;
        }
        break;
      }
    }
    if (i == next_break && !context.last.is_space) {
      return i;
    }
  }

  return len;
}

template <typename CharacterType, LineBreakType lineBreakType>
inline unsigned LazyLineBreakIterator::NextBreakablePosition(
    unsigned pos,
    const CharacterType* str,
    unsigned len) const {
  switch (break_space_) {
    case BreakSpaceType::kAfterSpaceRun:
      return NextBreakablePosition<CharacterType, lineBreakType,
                                   BreakSpaceType::kAfterSpaceRun>(pos, str,
                                                                   len);
    case BreakSpaceType::kAfterEverySpace:
      return NextBreakablePosition<CharacterType, lineBreakType,
                                   BreakSpaceType::kAfterEverySpace>(pos, str,
                                                                     len);
  }
  NOTREACHED();
}

template <LineBreakType lineBreakType>
inline unsigned LazyLineBreakIterator::NextBreakablePosition(
    unsigned pos,
    unsigned len) const {
  if (string_.IsNull()) [[unlikely]] {
    return 0;
  }
  if (string_.Is8Bit()) {
    return NextBreakablePosition<LChar, lineBreakType>(
        pos, string_.Characters8(), len);
  }
  return NextBreakablePosition<UChar, lineBreakType>(
      pos, string_.Characters16(), len);
}

unsigned LazyLineBreakIterator::NextBreakablePositionBreakCharacter(
    unsigned pos) const {
  DCHECK_LE(start_offset_, string_.length());
  NonSharedCharacterBreakIterator iterator(StringView(string_, start_offset_));
  DCHECK_GE(pos, start_offset_);
  pos -= start_offset_;
  // `- 1` because the `Following()` returns the next opportunity after the
  // given `offset`.
  int32_t next =
      iterator.Following(static_cast<int32_t>(pos > 0 ? pos - 1 : 0));
  return next != kTextBreakDone ? next + start_offset_ : string_.length();
}

unsigned LazyLineBreakIterator::NextBreakablePosition(unsigned pos,
                                                      unsigned len) const {
  switch (break_type_) {
    case LineBreakType::kNormal:
    case LineBreakType::kPhrase:
      return NextBreakablePosition<LineBreakType::kNormal>(pos, len);
    case LineBreakType::kBreakAll:
      return NextBreakablePosition<LineBreakType::kBreakAll>(pos, len);
    case LineBreakType::kKeepAll:
      return NextBreakablePosition<LineBreakType::kKeepAll>(pos, len);
    case LineBreakType::kBreakCharacter:
      return NextBreakablePositionBreakCharacter(pos);
  }
  NOTREACHED();
}

unsigned LazyLineBreakIterator::NextBreakOpportunity(unsigned offset) const {
  DCHECK_LE(offset, string_.length());
  return NextBreakablePosition(offset, string_.length());
}

unsigned LazyLineBreakIterator::NextBreakOpportunity(unsigned offset,
                                                     unsigned len) const {
  DCHECK_LE(offset, len);
  DCHECK_LE(len, string_.length());
  return NextBreakablePosition(offset, len);
}

unsigned LazyLineBreakIterator::PreviousBreakOpportunity(unsigned offset,
                                                         unsigned min) const {
  unsigned pos = std::min(offset, string_.length());
  // +2 to ensure at least one code point is included.
  unsigned end = std::min(pos + 2, string_.length());
  while (pos > min) {
    unsigned next_break = NextBreakablePosition(pos, end);
    if (next_break == pos) {
      return next_break;
    }

    // There's no break opportunities at |pos| or after.
    end = pos;
    if (string_.Is8Bit())
      --pos;
    else
      U16_BACK_1(string_.Characters16(), 0, pos);
  }
  return min;
}

std::ostream& operator<<(std::ostream& ostream, LineBreakType line_break_type) {
  switch (line_break_type) {
    case LineBreakType::kNormal:
      return ostream << "Normal";
    case LineBreakType::kBreakAll:
      return ostream << "BreakAll";
    case LineBreakType::kBreakCharacter:
      return ostream << "BreakCharacter";
    case LineBreakType::kKeepAll:
      return ostream << "KeepAll";
    case LineBreakType::kPhrase:
      return ostream << "Phrase";
  }
  NOTREACHED() << "LineBreakType::" << static_cast<int>(line_break_type);
}

std::ostream& operator<<(std::ostream& ostream, BreakSpaceType break_space) {
  switch (break_space) {
    case BreakSpaceType::kAfterSpaceRun:
      return ostream << "kAfterSpaceRun";
    case BreakSpaceType::kAfterEverySpace:
      return ostream << "kAfterEverySpace";
  }
  NOTREACHED() << "BreakSpaceType::" << static_cast<int>(break_space);
}

}  // namespace blink
