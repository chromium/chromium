/*
 * Copyright (C) 2007 Apple Computer, Inc.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/text/unicode_range.h"

namespace blink {

/**********************************************************************
 * Unicode subranges as defined in unicode 3.0
 * x-western, x-central-euro, tr, x-baltic  -> latin
 *  0000 - 036f
 *  1e00 - 1eff
 *  2000 - 206f  (general punctuation)
 *  20a0 - 20cf  (currency symbols)
 *  2100 - 214f  (letterlike symbols)
 *  2150 - 218f  (Number Forms)
 * el         -> greek
 *  0370 - 03ff
 *  1f00 - 1fff
 * x-cyrillic -> cyrillic
 *  0400 - 04ff
 * he         -> hebrew
 *  0590 - 05ff
 * ar         -> arabic
 *  0600 - 06ff
 *  fb50 - fdff (arabic presentation forms)
 *  fe70 - feff (arabic presentation forms b)
 * th - thai
 *  0e00 - 0e7f
 * ko        -> korean
 *  ac00 - d7af  (hangul Syllables)
 *  1100 - 11ff    (jamo)
 *  3130 - 318f (hangul compatibility jamo)
 * ja
 *  3040 - 309f (hiragana)
 *  30a0 - 30ff (katakana)
 * zh-CN
 * zh-TW
 *
 * CJK
 *  3100 - 312f (bopomofo)
 *  31a0 - 31bf (bopomofo extended)
 *  3000 - 303f (CJK Symbols and Punctuation)
 *  2e80 - 2eff (CJK radicals supplement)
 *  2f00 - 2fdf (Kangxi Radicals)
 *  2ff0 - 2fff (Ideographic Description Characters)
 *  3190 - 319f (kanbun)
 *  3200 - 32ff (Enclosed CJK letters and Months)
 *  3300 - 33ff (CJK compatibility)
 *  3400 - 4dbf (CJK Unified Ideographs Extension A)
 *  4e00 - 9faf (CJK Unified Ideographs)
 *  f900 - fa5f (CJK Compatibility Ideographs)
 *  fe30 - fe4f (CJK compatibility Forms)
 *  ff00 - ffef (halfwidth and fullwidth forms)
 *
 * Armenian
 *  0530 - 058f
 * Sriac
 *  0700 - 074f
 * Thaana
 *  0780 - 07bf
 * Devanagari
 *  0900 - 097f
 * Bengali
 *  0980 - 09ff
 * Gurmukhi
 *  0a00 - 0a7f
 * Gujarati
 *  0a80 - 0aff
 * Oriya
 *  0b00 - 0b7f
 * Tamil
 *  0b80 - 0bff
 * Telugu
 *  0c00 - 0c7f
 * Kannada
 *  0c80 - 0cff
 * Malayalam
 *  0d00 - 0d7f
 * Sinhala
 *  0d80 - 0def
 * Lao
 *  0e80 - 0eff
 * Tibetan
 *  0f00 - 0fbf
 * Myanmar
 *  1000 - 109f
 * Georgian
 *  10a0 - 10ff
 * Ethiopic
 *  1200 - 137f
 * Cherokee
 *  13a0 - 13ff
 * Canadian Aboriginal Syllabics
 *  1400 - 167f
 * Ogham
 *  1680 - 169f
 * Runic
 *  16a0 - 16ff
 * Khmer
 *  1780 - 17ff
 * Mongolian
 *  1800 - 18af
 * Misc - superscripts and subscripts
 *  2070 - 209f
 * Misc - Combining Diacritical Marks for Symbols
 *  20d0 - 20ff
 * Misc - Arrows
 *  2190 - 21ff
 * Misc - Mathematical Operators
 *  2200 - 22ff
 * Misc - Miscellaneous Technical
 *  2300 - 23ff
 * Misc - Control picture
 *  2400 - 243f
 * Misc - Optical character recognition
 *  2440 - 2450
 * Misc - Enclose Alphanumerics
 *  2460 - 24ff
 * Misc - Box Drawing
 *  2500 - 257f
 * Misc - Block Elements
 *  2580 - 259f
 * Misc - Geometric Shapes
 *  25a0 - 25ff
 * Misc - Miscellaneous Symbols
 *  2600 - 267f
 * Misc - Dingbats
 *  2700 - 27bf
 * Misc - Braille Patterns
 *  2800 - 28ff
 * Yi Syllables
 *  a000 - a48f
 * Yi radicals
 *  a490 - a4cf
 * Alphabetic Presentation Forms
 *  fb00 - fb4f
 * Misc - Combining half Marks
 *  fe20 - fe2f
 * Misc - small form variants
 *  fe50 - fe6f
 * Misc - Specials
 *  fff0 - ffff
 *********************************************************************/

static const unsigned kCNumSubTables = 9;
static const unsigned kCSubTableSize = 16;

static const unsigned char
    kGUnicodeSubrangeTable[kCNumSubTables][kCSubTableSize] = {
        {
            // table for X---
            kCRangeTableBase + 1,  // u0xxx
            kCRangeTableBase + 2,  // u1xxx
            kCRangeTableBase + 3,  // u2xxx
            kCRangeSetCJK,         // u3xxx
            kCRangeSetCJK,         // u4xxx
            kCRangeSetCJK,         // u5xxx
            kCRangeSetCJK,         // u6xxx
            kCRangeSetCJK,         // u7xxx
            kCRangeSetCJK,         // u8xxx
            kCRangeSetCJK,         // u9xxx
            kCRangeTableBase + 4,  // uaxxx
            kCRangeKorean,         // ubxxx
            kCRangeKorean,         // ucxxx
            kCRangeTableBase + 5,  // udxxx
            kCRangePrivate,        // uexxx
            kCRangeTableBase + 6   // ufxxx
        },
        {
            // table for 0X--
            kCRangeSetLatin,  // u00xx
            kCRangeSetLatin,  // u01xx
            kCRangeSetLatin,  // u02xx
            kCRangeGreek,     // u03xx     XXX 0300-036f is in fact
                              // cRangeCombiningDiacriticalMarks
            kCRangeCyrillic,  // u04xx
            kCRangeTableBase +
                7,  // u05xx, includes Cyrillic supplement, Hebrew, and Armenian
            kCRangeArabic,         // u06xx
            kCRangeTertiaryTable,  // u07xx
            kCRangeUnassigned,     // u08xx
            kCRangeTertiaryTable,  // u09xx
            kCRangeTertiaryTable,  // u0axx
            kCRangeTertiaryTable,  // u0bxx
            kCRangeTertiaryTable,  // u0cxx
            kCRangeTertiaryTable,  // u0dxx
            kCRangeTertiaryTable,  // u0exx
            kCRangeTibetan,        // u0fxx
        },
        {
            // table for 1x--
            kCRangeTertiaryTable,  // u10xx
            kCRangeKorean,         // u11xx
            kCRangeEthiopic,       // u12xx
            kCRangeTertiaryTable,  // u13xx
            kCRangeCanadian,       // u14xx
            kCRangeCanadian,       // u15xx
            kCRangeTertiaryTable,  // u16xx
            kCRangeKhmer,          // u17xx
            kCRangeMongolian,      // u18xx
            kCRangeUnassigned,     // u19xx
            kCRangeUnassigned,     // u1axx
            kCRangeUnassigned,     // u1bxx
            kCRangeUnassigned,     // u1cxx
            kCRangeUnassigned,     // u1dxx
            kCRangeSetLatin,       // u1exx
            kCRangeGreek,          // u1fxx
        },
        {
            // table for 2x--
            kCRangeSetLatin,               // u20xx
            kCRangeSetLatin,               // u21xx
            kCRangeMathOperators,          // u22xx
            kCRangeMiscTechnical,          // u23xx
            kCRangeControlOpticalEnclose,  // u24xx
            kCRangeBoxBlockGeometrics,     // u25xx
            kCRangeMiscSymbols,            // u26xx
            kCRangeDingbats,               // u27xx
            kCRangeBraillePattern,         // u28xx
            kCRangeUnassigned,             // u29xx
            kCRangeUnassigned,             // u2axx
            kCRangeUnassigned,             // u2bxx
            kCRangeUnassigned,             // u2cxx
            kCRangeUnassigned,             // u2dxx
            kCRangeSetCJK,                 // u2exx
            kCRangeSetCJK,                 // u2fxx
        },
        {
            // table for ax--
            kCRangeYi,          // ua0xx
            kCRangeYi,          // ua1xx
            kCRangeYi,          // ua2xx
            kCRangeYi,          // ua3xx
            kCRangeYi,          // ua4xx
            kCRangeUnassigned,  // ua5xx
            kCRangeUnassigned,  // ua6xx
            kCRangeUnassigned,  // ua7xx
            kCRangeUnassigned,  // ua8xx
            kCRangeUnassigned,  // ua9xx
            kCRangeUnassigned,  // uaaxx
            kCRangeUnassigned,  // uabxx
            kCRangeKorean,      // uacxx
            kCRangeKorean,      // uadxx
            kCRangeKorean,      // uaexx
            kCRangeKorean,      // uafxx
        },
        {
            // table for dx--
            kCRangeKorean,     // ud0xx
            kCRangeKorean,     // ud1xx
            kCRangeKorean,     // ud2xx
            kCRangeKorean,     // ud3xx
            kCRangeKorean,     // ud4xx
            kCRangeKorean,     // ud5xx
            kCRangeKorean,     // ud6xx
            kCRangeKorean,     // ud7xx
            kCRangeSurrogate,  // ud8xx
            kCRangeSurrogate,  // ud9xx
            kCRangeSurrogate,  // udaxx
            kCRangeSurrogate,  // udbxx
            kCRangeSurrogate,  // udcxx
            kCRangeSurrogate,  // uddxx
            kCRangeSurrogate,  // udexx
            kCRangeSurrogate,  // udfxx
        },
        {
            // table for fx--
            kCRangePrivate,  // uf0xx
            kCRangePrivate,  // uf1xx
            kCRangePrivate,  // uf2xx
            kCRangePrivate,  // uf3xx
            kCRangePrivate,  // uf4xx
            kCRangePrivate,  // uf5xx
            kCRangePrivate,  // uf6xx
            kCRangePrivate,  // uf7xx
            kCRangePrivate,  // uf8xx
            kCRangeSetCJK,   // uf9xx
            kCRangeSetCJK,   // ufaxx
            kCRangeArabic,   // ufbxx, includes alphabic presentation form
            kCRangeArabic,   // ufcxx
            kCRangeArabic,   // ufdxx
            kCRangeArabic,   // ufexx, includes Combining half marks,
                            //                CJK compatibility forms,
                            //                CJK compatibility forms,
                            //                small form variants
            kCRangeTableBase +
                8,  // uffxx, halfwidth and fullwidth forms, includes Specials
        },
        {
            // table for 0x0500 - 0x05ff
            kCRangeCyrillic,  // u050x
            kCRangeCyrillic,  // u051x
            kCRangeCyrillic,  // u052x
            kCRangeArmenian,  // u053x
            kCRangeArmenian,  // u054x
            kCRangeArmenian,  // u055x
            kCRangeArmenian,  // u056x
            kCRangeArmenian,  // u057x
            kCRangeArmenian,  // u058x
            kCRangeHebrew,    // u059x
            kCRangeHebrew,    // u05ax
            kCRangeHebrew,    // u05bx
            kCRangeHebrew,    // u05cx
            kCRangeHebrew,    // u05dx
            kCRangeHebrew,    // u05ex
            kCRangeHebrew,    // u05fx
        },
        {
            // table for 0xff00 - 0xffff
            kCRangeSetCJK,    // uff0x, fullwidth latin
            kCRangeSetCJK,    // uff1x, fullwidth latin
            kCRangeSetCJK,    // uff2x, fullwidth latin
            kCRangeSetCJK,    // uff3x, fullwidth latin
            kCRangeSetCJK,    // uff4x, fullwidth latin
            kCRangeSetCJK,    // uff5x, fullwidth latin
            kCRangeSetCJK,    // uff6x, halfwidth katakana
            kCRangeSetCJK,    // uff7x, halfwidth katakana
            kCRangeSetCJK,    // uff8x, halfwidth katakana
            kCRangeSetCJK,    // uff9x, halfwidth katakana
            kCRangeSetCJK,    // uffax, halfwidth hangul jamo
            kCRangeSetCJK,    // uffbx, halfwidth hangul jamo
            kCRangeSetCJK,    // uffcx, halfwidth hangul jamo
            kCRangeSetCJK,    // uffdx, halfwidth hangul jamo
            kCRangeSetCJK,    // uffex, fullwidth symbols
            kCRangeSpecials,  // ufffx, Specials
        },
};

// Most scripts between U+0700 and U+16FF are assigned a chunk of 128 (0x80)
// code points so that the number of entries in the tertiary range
// table for that range is obtained by dividing (0x1700 - 0x0700) by 128.
// Exceptions: Ethiopic, Tibetan, Hangul Jamo and Canadian aboriginal
// syllabaries take multiple chunks and Ogham and Runic share a single chunk.
static const unsigned kCTertiaryTableSize = ((0x1700 - 0x0700) / 0x80);

static const unsigned char kGUnicodeTertiaryRangeTable[kCTertiaryTableSize] = {
    // table for 0x0700 - 0x1600
    kCRangeSyriac,      // u070x
    kCRangeThaana,      // u078x
    kCRangeUnassigned,  // u080x  place holder(resolved in the 2ndary tab.)
    kCRangeUnassigned,  // u088x  place holder(resolved in the 2ndary tab.)
    kCRangeDevanagari,  // u090x
    kCRangeBengali,     // u098x
    kCRangeGurmukhi,    // u0a0x
    kCRangeGujarati,    // u0a8x
    kCRangeOriya,       // u0b0x
    kCRangeTamil,       // u0b8x
    kCRangeTelugu,      // u0c0x
    kCRangeKannada,     // u0c8x
    kCRangeMalayalam,   // u0d0x
    kCRangeSinhala,     // u0d8x
    kCRangeThai,        // u0e0x
    kCRangeLao,         // u0e8x
    kCRangeTibetan,     // u0f0x  place holder(resolved in the 2ndary tab.)
    kCRangeTibetan,     // u0f8x  place holder(resolved in the 2ndary tab.)
    kCRangeMyanmar,     // u100x
    kCRangeGeorgian,    // u108x
    kCRangeKorean,      // u110x  place holder(resolved in the 2ndary tab.)
    kCRangeKorean,      // u118x  place holder(resolved in the 2ndary tab.)
    kCRangeEthiopic,    // u120x  place holder(resolved in the 2ndary tab.)
    kCRangeEthiopic,    // u128x  place holder(resolved in the 2ndary tab.)
    kCRangeEthiopic,    // u130x
    kCRangeCherokee,    // u138x
    kCRangeCanadian,    // u140x  place holder(resolved in the 2ndary tab.)
    kCRangeCanadian,    // u148x  place holder(resolved in the 2ndary tab.)
    kCRangeCanadian,    // u150x  place holder(resolved in the 2ndary tab.)
    kCRangeCanadian,    // u158x  place holder(resolved in the 2ndary tab.)
    kCRangeCanadian,    // u160x
    kCRangeOghamRunic,  // u168x  this contains two scripts, Ogham & Runic
};

// A two level index is almost enough for locating a range, with the
// exception of u03xx and u05xx. Since we don't really care about range for
// combining diacritical marks in our font application, they are
// not discriminated further.  Future adoption of this method for other use
// should be aware of this limitation. The implementation can be extended if
// there is such a need.
// For Indic, Southeast Asian scripts and some other scripts between
// U+0700 and U+16FF, it's extended to the third level.
unsigned FindCharUnicodeRange(UChar32 ch) {
  if (ch >= 0xFFFF)
    return 0;

  unsigned range;

  // search the first table
  range = kGUnicodeSubrangeTable[0][ch >> 12];

  if (range < kCRangeTableBase)
    // we try to get a specific range
    return range;

  // otherwise, we have one more table to look at
  range = kGUnicodeSubrangeTable[range - kCRangeTableBase][(ch & 0x0f00) >> 8];
  if (range < kCRangeTableBase)
    return range;
  if (range < kCRangeTertiaryTable)
    return kGUnicodeSubrangeTable[range - kCRangeTableBase][(ch & 0x00f0) >> 4];

  // Yet another table to look at : U+0700 - U+16FF : 128 code point blocks
  return kGUnicodeTertiaryRangeTable[(ch - 0x0700) >> 7];
}

}  // namespace blink
