// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_CHARACTER_PROPERTY_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_CHARACTER_PROPERTY_DATA_H_

#include <unicode/uobject.h>

#include <array>

namespace blink {

// clang-format off
// Do not add entries to this array that are already covered by
// the logic in character_property_data_generator.cc for emoji sequences.
static constexpr auto kIsCjkIdeographOrSymbolArray = std::to_array<UChar32>({
    // 0x2C7 Caron, Mandarin Chinese 3rd Tone
    0x2C7,
    // 0x2CA Modifier Letter Acute Accent, Mandarin Chinese 2nd Tone
    0x2CA,
    // 0x2CB Modifier Letter Grave Access, Mandarin Chinese 4th Tone
    0x2CB,
    // 0x2D9 Dot Above, Mandarin Chinese 5th Tone
    0x2D9, 0x2020, 0x2021, 0x2030, 0x203B, 0x203C, 0x2042, 0x2047, 0x2048,
    0x2049, 0x2051, 0x20DD, 0x20DE, 0x2100, 0x2103, 0x2105, 0x2109, 0x210A,
    0x2113, 0x2116, 0x2121, 0x212B, 0x213B, 0x2150, 0x2151, 0x2152, 0x217F,
    0x2189, 0x2307, 0x2312, 0x23CE, 0x2423, 0x25A0, 0x25A1, 0x25A2, 0x25AA,
    0x25AB, 0x25B1, 0x25B2, 0x25B3, 0x25B6, 0x25B7, 0x25BC, 0x25BD, 0x25C0,
    0x25C1, 0x25C6, 0x25C7, 0x25C9, 0x25CB, 0x25CC, 0x25EF, 0x2605, 0x2606,
    0x260E, 0x2616, 0x2617, 0x26A0, 0x2713, 0x271A, 0x273F, 0x2740, 0x2756,
    0x2763, 0x2B1A, 0xFE10, 0xFE11, 0xFE12, 0xFE19, 0xFF1D,
    // Emoji.
    0x1F100, 0x1F200, 0x1F237, 0x1F32C, 0x1F336, 0x1F37D, 0x1F43F, 0x1F54F,
    0x1F93B, 0x1F946
});

// Do not add entries to this array that are already covered by
// the logic in character_property_data_generator.cc for emoji sequences.
static constexpr auto kIsCjkIdeographOrSymbolRanges = std::to_array<UChar32>({
    // cjkIdeographRanges
    // CJK Radicals Supplement and Kangxi Radicals.
    0x2E80, 0x2FDF,
    // CJK Strokes.
    0x31C0, 0x31EF,
    // CJK Unified Ideographs Extension A.
    0x3400, 0x4DBF,
    // The basic CJK Unified Ideographs block.
    0x4E00, 0x9FFF,
    // CJK Compatibility Ideographs.
    0xF900, 0xFAFF,
    // Unicode Plane 2: Supplementary Ideographic Plane. This plane includes:
    // CJK Unified Ideographs Extension B to F.
    // CJK Compatibility Ideographs Supplement.
    0x20000, 0x2FFFF,

    // cjkSymbolRanges
    0x2156, 0x215A, 0x2160, 0x216B, 0x2170, 0x217B, 0x23BE, 0x23CC,
    0x2460, 0x2492, 0x249C, 0x24FF, 0x25CE, 0x25D3, 0x25E2, 0x25E6,
    0x2600, 0x2603, 0x2660, 0x266F,
    // Emoji HEAVY HEART EXCLAMATION MARK ORNAMENT..HEAVY BLACK HEART
    // Needed in order not to break Emoji heart-kiss sequences in
    // CachingWordShapeIterator.
    // cmp. http://www.unicode.org/emoji/charts/emoji-zwj-sequences.html
    0x2672, 0x267D, 0x2776, 0x277F,
    // Ideographic Description Characters, with CJK Symbols and Punctuation,
    // excluding 0x3030.
    // Exclude Hangul Tone Marks (0x302E .. 0x302F) because Hangul is not Han
    // and no other Hangul are included.
    // Then Hiragana 0x3040 .. 0x309F, Katakana 0x30A0 .. 0x30FF, Bopomofo
    // 0x3100 .. 0x312F
    0x2FF0, 0x302D, 0x3031, 0x312F,
    // More Bopomofo and Bopomofo Extended 0x31A0 .. 0x31BF
    0x3190, 0x31BF,
    // Enclosed CJK Letters and Months (0x3200 .. 0x32FF).
    // CJK Compatibility (0x3300 .. 0x33FF).
    0x3200, 0x33FF,
    // Yijing Hexagram Symbols
    0x4DC0, 0x4DFF,
    // http://www.unicode.org/Public/MAPPINGS/VENDORS/APPLE/JAPANESE.TXT
    0xF860, 0xF862,
    // CJK Compatibility Forms.
    // Small Form Variants (for CNS 11643).
    0xFE30, 0xFE6F,
    // Halfwidth and Fullwidth Forms
    // Usually only used in CJK
    0xFF00, 0xFF0C, 0xFF0E, 0xFF1A, 0xFF1F, 0xFFEF,
    // Ideographic Symbols and Punctuation
    0x16FE0, 0x16FFF,
    // Tangut
    0x17000, 0x187FF,
    // Tangut Components
    0x18800, 0x18AFF,
    // Kana Supplement
    0x1B000, 0x1B0FF,
    // Kana Extended-A
    0x1B100, 0x1B12F,
    // Nushu
    0x1B170, 0x1B2FF,
    // Emoji.
    0x1F110, 0x1F129, 0x1F130, 0x1F149, 0x1F150, 0x1F169, 0x1F170, 0x1F189,
    0x1F202, 0x1F219, 0x1F21B, 0x1F22E, 0x1F230, 0x1F231, 0x1F23B, 0x1F24F,
    0x1F252, 0x1F2FF, 0x1F321, 0x1F32A, 0x1F394, 0x1F39F, 0x1F3CD, 0x1F3CE,
    0x1F3D4, 0x1F3DF, 0x1F3F1, 0x1F3F2, 0x1F3F5, 0x1F3F7, 0x1F4FD, 0x1F4FE,
    0x1F53E, 0x1F54A, 0x1F568, 0x1F573, 0x1F576, 0x1F579, 0x1F57B, 0x1F58F,
    0x1F591, 0x1F594, 0x1F597, 0x1F5A3, 0x1F5A5, 0x1F5E7, 0x1F5E9, 0x1F5FA,
    0x1F650, 0x1F67F, 0x1F6C6, 0x1F6CB, 0x1F6CD, 0x1F6CF, 0x1F6D3, 0x1F6D4,
    0x1F6D9, 0x1F6DB, 0x1F6E0, 0x1F6EA, 0x1F6ED, 0x1F6F3, 0x1F6FD, 0x1F6FF,
    0x1F900, 0x1F90B, 0x1FAC9, 0x1FACC,
});

// https://html.spec.whatwg.org/C/#prod-potentialcustomelementname
static constexpr auto kIsPotentialCustomElementNameCharArray =
    std::to_array<UChar32>({
        '-', '.', '_', 0xB7,
    });

static constexpr auto kIsPotentialCustomElementNameCharRanges =
    std::to_array<UChar32>({
        '0',    '9',    'a',    'z',    0xC0,    0xD6,    0xD8,   0xF6,
        0xF8,   0x2FF,  0x300,  0x37D,  0x37F,   0x1FFF,  0x200C, 0x200D,
        0x203F, 0x2040, 0x2070, 0x218F, 0x2C00,  0x2FEF,  0x3001, 0xD7FF,
        0xF900, 0xFDCF, 0xFDF0, 0xFFFD, 0x10000, 0xEFFFF,
    });

// http://unicode.org/reports/tr9/#Directional_Formatting_Characters
static constexpr auto kIsBidiControlArray =
    std::to_array<UChar32>({0x061C, 0x200E, 0x200F});

static constexpr auto kIsBidiControlRanges = std::to_array<UChar32>({
    0x202A, 0x202E, 0x2066, 0x2069,
});

// https://unicode.org/Public/UNIDATA/Blocks.txt
static constexpr auto kIsHangulRanges = std::to_array<UChar32>({
    // Hangul Jamo
    0x1100, 0x11FF,
    // Hangul Compatibility Jamo
    0x3130, 0x318F,
    // Hangul Jamo Extended-A
    0xA960, 0xA97F,
    // Hangul Syllables
    0xAC00, 0xD7AF,
    // Hangul Jamo Extended-B
    0xD7B0, 0xD7FF,
    // Halfwidth Hangul Jamo
    // https://www.unicode.org/charts/nameslist/c_FF00.html
    0xFFA0, 0xFFDC,
});
// clang-format on

// Freezed trie tree, see character_property_data_generator.cc.
extern const int32_t kSerializedCharacterDataSize;
extern const uint8_t kSerializedCharacterData[];

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_CHARACTER_PROPERTY_DATA_H_
