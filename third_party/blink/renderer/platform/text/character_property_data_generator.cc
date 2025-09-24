// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <unicode/brkiter.h>
#include <unicode/locid.h>
#include <unicode/ucptrie.h>
#include <unicode/udata.h>
#include <unicode/ulocdata.h>
#include <unicode/umutablecptrie.h>
#include <unicode/uniset.h>
#include <unicode/unistr.h>

#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>

#include "base/check_op.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "third_party/blink/renderer/platform/text/character_property.h"
#include "third_party/blink/renderer/platform/text/character_property_data.h"
#include "third_party/blink/renderer/platform/text/east_asian_spacing_type.h"
#include "third_party/blink/renderer/platform/text/han_kerning_char_type.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {
namespace {

#define CHECK_U_ERROR(error, name) \
  CHECK(U_SUCCESS(error)) << name << ": (" << error << ")" << u_errorName(error)

// Check ICU functions that need the data resources are working.
// https://unicode-org.github.io/icu/userguide/icu/design.html#icu4c-initialization-and-termination
void CheckIcuDataResources() {
  UErrorCode error = U_ZERO_ERROR;
  UVersionInfo version;
  ulocdata_getCLDRVersion(version, &error);
  CHECK_U_ERROR(error, "ulocdata_getCLDRVersion");
}

//
// Load the ICU data file and set it to the ICU.
//
void InitializeIcu(const char* exec_path) {
  // ICU can't load the data file by itself because ICU tries to load the
  // versioned data file (e.g., "icudt73l.dat"), while the Chromium build system
  // creates the unversioned data file (e.g., "icudtl.dat").
  std::filesystem::path path{exec_path};
  path = path.parent_path() / "icudt" U_ICUDATA_TYPE_LETTER ".dat";

  std::ifstream data_ifstream(path, std::ios_base::binary);
  if (!data_ifstream.is_open()) {
    // When the build config is `!use_icu_data_file`, the ICU data is built into
    // the binary.
    CheckIcuDataResources();
    return;
  }
  static std::vector<uint8_t> icu_data;
  CHECK(icu_data.empty());
  std::copy(std::istreambuf_iterator<char>(data_ifstream),
            std::istreambuf_iterator<char>(), std::back_inserter(icu_data));
  UErrorCode error = U_ZERO_ERROR;
  udata_setCommonData(icu_data.data(), &error);
  CHECK_U_ERROR(error, "udata_setCommonData");

  CheckIcuDataResources();
}

class CharacterPropertyValues {
 public:
  constexpr static UChar32 kMaxCodepoint = 0x10FFFF;
  constexpr static UChar32 kSize = kMaxCodepoint + 1;

  CharacterPropertyValues()
      : values_(base::HeapArray<CharacterProperty>::WithSize(kSize)) {
    Initialize();
  }

  CharacterProperty operator[](UChar32 index) const { return values_[index]; }

 private:
  void Initialize() {
    SetIsCJKIdeographOrSymbolForEmoji();

#define SET(name, field_name)                                        \
  const auto field_name##_setter = [](CharacterProperty& property) { \
    property.field_name = true;                                      \
  };                                                                 \
  SetForRanges(name##Ranges, field_name##_setter);                   \
  SetForValues(name##Array, field_name##_setter);

    SET(kIsCJKIdeographOrSymbol, is_cjk_ideograph_or_symbol);
    SET(kIsPotentialCustomElementNameChar,
        is_potential_custom_element_name_char);
    SET(kIsBidiControl, is_bidi_control);
#undef SET
    SetForRanges(kIsHangulRanges, [](CharacterProperty& property) {
      property.is_hangul = true;
    });
    SetHanKerning();
    SetEastAsianSpacing();
  }

  // Set all characters that have the `UCHAR_EMOJI_PRESENTATION` property as CJK
  // symbol characters.
  void SetIsCJKIdeographOrSymbolForEmoji() {
    SetForUnicodePattern("[:Emoji_Presentation:]",
                         [](CharacterProperty& property) {
                           property.is_cjk_ideograph_or_symbol = true;
                         });
  }

  void SetHanKerning() {
    // https://drafts.csswg.org/css-text-4/#text-spacing-classes
    Set(uchar::kLeftSingleQuotationMark, HanKerningCharType::kOpenQuote);
    Set(uchar::kLeftDoubleQuotationMark, HanKerningCharType::kOpenQuote);
    Set(uchar::kRightSingleQuotationMark, HanKerningCharType::kCloseQuote);
    Set(uchar::kRightDoubleQuotationMark, HanKerningCharType::kCloseQuote);
    Set(uchar::kIdeographicSpace, HanKerningCharType::kMiddle);
    Set(uchar::kIdeographicComma, HanKerningCharType::kDot);
    Set(uchar::kIdeographicFullStop, HanKerningCharType::kDot);
    Set(uchar::kFullwidthComma, HanKerningCharType::kDot);
    Set(uchar::kFullwidthFullStop, HanKerningCharType::kDot);
    Set(uchar::kFullwidthColon, HanKerningCharType::kColon);
    Set(uchar::kFullwidthSemicolon, HanKerningCharType::kSemicolon);
    Set(uchar::kMiddleDot, HanKerningCharType::kMiddle);
    Set(uchar::kHyphenationPoint, HanKerningCharType::kMiddle);
    Set(uchar::kKatakanaMiddleDot, HanKerningCharType::kMiddle);
    SetForUnicodePattern("[[:blk=CJK_Symbols:][:ea=F:] & [:gc=Ps:]]",
                         HanKerningCharType::kOpen);
    SetForUnicodePattern("[[:blk=CJK_Symbols:][:ea=F:] & [:gc=Pe:]]",
                         HanKerningCharType::kClose);
    SetForUnicodePattern("[[:gc=Ps:] - [:blk=CJK_Symbols:] - [:ea=F:]]",
                         HanKerningCharType::kOpenNarrow);
    SetForUnicodePattern("[[:gc=Pe:] - [:blk=CJK_Symbols:] - [:ea=F:]]",
                         HanKerningCharType::kCloseNarrow);
  }

  void SetEastAsianSpacing() {
    // Set based on https://www.unicode.org/reports/tr59/#data.
    UErrorCode error = U_ZERO_ERROR;

    icu::UnicodeSet unassigned(icu::UnicodeString("[:General_Category=Cn:]"),
                               error);
    CHECK_EQ(error, U_ZERO_ERROR);

    // 1. Set for the "Wide" property
    //
    // 1.1 Include if the Script property is one of the following values:
    // Bopomofo (Bopo) Han (Hani) Hangul (Hang) Hiragana (Hira) Katakana (Kana)
    // Khitan_Small_Script (Kits) Nushu (Nshu) Tangut (Tang) Yi (Yiii)
    icu::UnicodeSet ideographs(
        icu::UnicodeString(
            "[[:sc=Bopomofo:][:sc=Han:][:sc=Hangul:][:sc=Hiragana:][:sc="
            "Katakana:][:sc=Khitan_Small_Script:][:sc=Nushu:][:sc=Tangut:][:sc="
            "Yi:]]"),
        error);
    CHECK_EQ(error, U_ZERO_ERROR);

    {
      // 1.2. Include if the Script_Extensions property is one of the values
      // above, except when the East_Asian_Width property is “Neutral (N)” or
      // “Narrow (Na)”.
      icu::UnicodeSet temp_set(
          icu::UnicodeString(
              "[[[:scx=Bopo:][:scx=Hani:][:scx=Hang:][:scx=Hira:]["
              ":scx=Kana:][:scx=Kits:][:scx=Nshu:][:scx=Tang:][:"
              "scx=Yiii:]]-[:East_Asian_Width=Narrow:]-[:East_"
              "Asian_Width=Neutral:]]"),
          error);
      CHECK_EQ(error, U_ZERO_ERROR);
      ideographs.addAll(temp_set);
    }
    {
      // 1.3 Exclude if the East_Asian_Width property is “East Asian Halfwidth
      // (H)”.
      // 1.4 Exclude if the General_Category property is “Punctuation (P)” or
      // “Other_Number (No)”.
      // 1.5 Exclude if the General_Category property is “Symbol (S)” except
      // “Modifier_Symbol (Sk)”.
      icu::UnicodeSet temp_set(
          icu::UnicodeString(
              "[[:East_Asian_Width=H:][:General_Category=P:][:"
              "General_Category=No:]"
              "[[:General_Category=S:]-[:General_Category=Sk:]]]"),
          error);
      CHECK_EQ(error, U_ZERO_ERROR);
      ideographs.removeAll(temp_set);
    }
    // 1.6 Include the following code point: U+3013 GETA MARK
    ideographs.add(0x3013);
    ideographs.removeAll(unassigned);
    SetForUnicodeSet(ideographs, EastAsianSpacingType::kWide);

    // 2. Set for the Conditional property
    //
    // 2.1 Include if the General_Category property is “Other_Punctuation (Po)”.
    // 2.2 Exclude if the East_Asian_Width property is “East Asian Fullwidth
    // (F)”, “East Asian Halfwidth (H)”, or “East Asian Wide (W)”.
    icu::UnicodeSet conditional(
        icu::UnicodeString("[[:General_Category=Po:]-[:East_Asian_Width=F:]-[:"
                           "East_Asian_Width=H:]-[:East_Asian_Width=W:]]"),
        error);
    CHECK_EQ(error, U_ZERO_ERROR);
    // 2.3 Exclude the following code points: U+0022 QUOTATION MARK U+0027
    // APOSTROPHE U+002A ASTERISK U+002F SOLIDUS U+00B7 MIDDLE DOT U+2020 DAGGER
    // U+2021 DOUBLE DAGGER U+2026 HORIZONTAL ELLIPSIS
    conditional.remove(0x0022);  // QUOTATION MARK
    conditional.remove(0x0027);  // APOSTROPHE
    conditional.remove(0x002A);  // ASTERISK
    conditional.remove(0x002F);  // SOLIDUS
    conditional.remove(0x00B7);  // MIDDLE DOT
    conditional.remove(0x2020);  // DAGGER
    conditional.remove(0x2021);  // DOUBLE DAGGER
    conditional.remove(0x2026);  // HORIZONTAL ELLIPSIS

    conditional.removeAll(unassigned);
    SetForUnicodeSet(conditional, EastAsianSpacingType::kConditional);

    // 3. Set for the Narrow property
    // 3.1 Include if the General_Category property is “Letter (L)”, “Mark (M)”,
    // or “Decimal_Number (Nd)”.
    // 3.2 xclude if the East_Asian_Width property is “East Asian Fullwidth
    // (F)", “East Asian Halfwidth (H)”, or “East Asian Wide (W)”.
    icu::UnicodeSet narrow(
        icu::UnicodeString(
            "[[:General_Category=Letter:][:General_Category=M:][:"
            "General_Category=Nd:]-[:East_Asian_Width=F:]-[:"
            "East_Asian_Width=H:]-[:East_Asian_Width=W:]]"),
        error);
    CHECK_EQ(error, U_ZERO_ERROR);
    // The intersection set of kWide and kConditional is not empty, so remove
    // the chars which have been assigned the kWide property from narrow.
    narrow.removeAll(ideographs);
    SetForUnicodeSet(narrow, EastAsianSpacingType::kNarrow);

    // The remaining assigned codes are kOther.
    // The flag is initialized by 0, no need to set them.
    static_assert(static_cast<int>(EastAsianSpacingType::kOther) == 0);
  }

  void SetForUnicodeSet(const icu::UnicodeSet& unicode_set,
                        EastAsianSpacingType value) {
    SetForUnicodeSet(unicode_set, [value](CharacterProperty& property) {
      property.east_asian_spacing = value;
    });
  }

  template <typename Setter>
  void SetForUnicodeSet(const icu::UnicodeSet& unicode_set, Setter set_value) {
    const int32_t range_count = unicode_set.getRangeCount();
    for (int32_t i = 0; i < range_count; ++i) {
      const UChar32 end = unicode_set.getRangeEnd(i);
      CHECK_LE(end, kMaxCodepoint);
      for (UChar32 ch = unicode_set.getRangeStart(i); ch <= end; ++ch) {
        set_value(values_[ch]);
      }
    }
  }

  void SetForUnicodePattern(const char* pattern, HanKerningCharType type) {
    SetForUnicodePattern(pattern, [type](CharacterProperty& property) {
      property.han_kerning = type;
    });
  }

  // For `patterns`, see:
  // https://unicode-org.github.io/icu/userguide/strings/unicodeset.html#unicodeset-patterns
  template <typename Setter>
  void SetForUnicodePattern(const char* pattern, Setter set_value) {
    UErrorCode error = U_ZERO_ERROR;
    icu::UnicodeSet set(icu::UnicodeString(pattern), error);
    CHECK_EQ(error, U_ZERO_ERROR);
    SetForUnicodeSet(set, set_value);
  }

  template <typename Setter>
  void SetForRanges(base::span<const UChar32> ranges, Setter set_value) {
    size_t length = ranges.size();
    CHECK_EQ(length % 2, 0u);
    for (size_t i = 0; i < length; i += 2) {
      CHECK_LE(ranges[i], ranges[i + 1]);
      CHECK_LE(ranges[i + 1], kMaxCodepoint);
      for (UChar32 c = ranges[i]; c <= ranges[i + 1]; ++c) {
        set_value(values_[c]);
      }
    }
  }

  template <typename Setter>
  void SetForValues(base::span<const UChar32> code_points, Setter set_value) {
    for (UChar32 code_point : code_points) {
      CHECK_LE(code_point, kMaxCodepoint);
      set_value(values_[code_point]);
    }
  }

  void Set(UChar32 ch, HanKerningCharType type) {
    CHECK_EQ(values_[ch].han_kerning, HanKerningCharType::kOther);
    values_[ch].han_kerning = type;
  }

  base::HeapArray<CharacterProperty> values_;
};

static void GenerateUTrieSerialized(FILE* fp,
                                    size_t size,
                                    base::span<uint8_t> array) {
  fprintf(fp,
          "#include <cstdint>\n\n"
          "namespace blink {\n\n"
          "extern const int32_t kSerializedCharacterDataSize = %zu;\n"
          // The utrie2_openFromSerialized function requires character data to
          // be aligned to 4 bytes.
          "alignas(4) extern const uint8_t kSerializedCharacterData[] = {",
          size);
  for (size_t i = 0; i < size;) {
    fprintf(fp, "\n   ");
    for (size_t col = 0; col < 16 && i < size; ++col, ++i) {
      fprintf(fp, " 0x%02X,", array[i]);
    }
  }
  fprintf(fp,
          "\n};\n\n"
          "} // namespace blink\n");
}

static void GenerateCharacterPropertyData(FILE* fp) {
  // Create a value array of all possible code points.
  CharacterPropertyValues values;

  // Create a trie from the value array.
  UErrorCode error = U_ZERO_ERROR;
  std::unique_ptr<UMutableCPTrie, decltype(&umutablecptrie_close)> trie(
      umutablecptrie_open(0, 0, &error), umutablecptrie_close);
  assert(error == U_ZERO_ERROR);
  UChar32 start = 0;
  CharacterProperty value = values[0];
  for (UChar32 c = 1;; c++) {
    if (c < CharacterPropertyValues::kSize && values[c] == value) {
      continue;
    }
    if (const CharacterPropertyType value_as_unsigned = value.AsUnsigned()) {
      umutablecptrie_setRange(trie.get(), start, c - 1, value_as_unsigned,
                              &error);
      assert(error == U_ZERO_ERROR);
    }
    if (c >= CharacterPropertyValues::kSize) {
      break;
    }
    start = c;
    value = values[start];
  }

  // Convert to immutable UCPTrie in order to be able to serialize.
  static_assert(sizeof(CharacterProperty) == 2);
  std::unique_ptr<UCPTrie, decltype(&ucptrie_close)> immutable_trie(
      umutablecptrie_buildImmutable(trie.get(), UCPTrieType::UCPTRIE_TYPE_FAST,
                                    UCPTrieValueWidth::UCPTRIE_VALUE_BITS_16,
                                    &error),
      ucptrie_close);

  assert(error == U_ZERO_ERROR);

  int32_t serialized_size =
      ucptrie_toBinary(immutable_trie.get(), nullptr, 0, &error);
  CHECK_GE(serialized_size, 0);
  error = U_ZERO_ERROR;

  auto serialized =
      base::HeapArray<uint8_t>::Uninit(static_cast<size_t>(serialized_size));
  // Ensure 32-bit alignment, as ICU requires that to the ucptrie_toBinary call.
  CHECK(!(reinterpret_cast<intptr_t>(serialized.data()) % 4));

  serialized_size = ucptrie_toBinary(immutable_trie.get(), serialized.data(),
                                     serialized.size(), &error);
  CHECK_GE(serialized_size, 0);
  assert(error == U_ZERO_ERROR);

  GenerateUTrieSerialized(fp, static_cast<size_t>(serialized_size), serialized);
}

//
// Generate a line break pair table in `break_iterator_data_inline_header.h`.
//
// See [UAX14](https://unicode.org/reports/tr14/).
//
class LineBreakData {
 public:
  LineBreakData() = default;

  static void Generate(FILE* fp) {
    LineBreakData data;
    data.FillFromIcu();
    data.FillAscii();
    data.Print(fp);
  }

 private:
  // Fill the pair table from the ICU BreakIterator.
  void FillFromIcu() {
    UErrorCode status = U_ZERO_ERROR;
    const icu::Locale locale("en");
    std::unique_ptr<icu::BreakIterator> break_iterator(
        icu::BreakIterator::createLineInstance(locale, status));
    CHECK_U_ERROR(status, "createLineInstance");

    for (UChar ch = kMinChar; ch <= kMaxChar; ++ch) {
      const icu::UnicodeString ch_str(ch);
      for (UChar ch_next = kMinChar; ch_next <= kMaxChar; ++ch_next) {
        const icu::UnicodeString ch_next_str(ch_next);
        const icu::UnicodeString str = ch_str + ch_next_str;
        break_iterator->setText(str);
        SetPairValue(ch, ch_next, break_iterator->isBoundary(1));
      }
    }
  }

  // Line breaking table for printable ASCII characters. Line breaking
  // opportunities in this table are as below:
  // - before opening punctuations such as '(', '<', '[', '{' after certain
  //   characters (compatible with Firefox 3.6);
  // - after '-' and '?' (backward-compatible, and compatible with Internet
  //   Explorer).
  // Please refer to <https://bugs.webkit.org/show_bug.cgi?id=37698> for line
  // breaking matrixes of different browsers and the ICU standard.
  void FillAscii() {
#define ALL_CHAR '!', 0x7F
    SetPairValue(ALL_CHAR, ALL_CHAR, false);
    SetPairValue(ALL_CHAR, '(', '(', true);
    SetPairValue(ALL_CHAR, '<', '<', true);
    SetPairValue(ALL_CHAR, '[', '[', true);
    SetPairValue(ALL_CHAR, '{', '{', true);
    SetPairValue('-', '-', ALL_CHAR, true);
    SetPairValue('?', '?', ALL_CHAR, true);
    SetPairValue('-', '-', '$', '$', false);
    SetPairValue(ALL_CHAR, '!', '!', false);
    SetPairValue('?', '?', '"', '"', false);
    SetPairValue('?', '?', '\'', '\'', false);
    SetPairValue(ALL_CHAR, ')', ')', false);
    SetPairValue(ALL_CHAR, ',', ',', false);
    SetPairValue(ALL_CHAR, '.', '.', false);
    SetPairValue(ALL_CHAR, '/', '/', false);
    // Note: Between '-' and '[0-9]' is hard-coded in `ShouldBreakFast()`.
    SetPairValue('-', '-', '0', '9', false);
    SetPairValue(ALL_CHAR, ':', ':', false);
    SetPairValue(ALL_CHAR, ';', ';', false);
    SetPairValue(ALL_CHAR, '?', '?', false);
    SetPairValue(ALL_CHAR, ']', ']', false);
    SetPairValue(ALL_CHAR, '}', '}', false);
    SetPairValue('$', '$', ALL_CHAR, false);
    SetPairValue('\'', '\'', ALL_CHAR, false);
    SetPairValue('(', '(', ALL_CHAR, false);
    SetPairValue('/', '/', ALL_CHAR, false);
    SetPairValue('0', '9', ALL_CHAR, false);
    SetPairValue('<', '<', ALL_CHAR, false);
    SetPairValue('@', '@', ALL_CHAR, false);
    SetPairValue('A', 'Z', ALL_CHAR, false);
    SetPairValue('[', '[', ALL_CHAR, false);
    SetPairValue('^', '`', ALL_CHAR, false);
    SetPairValue('a', 'z', ALL_CHAR, false);
    SetPairValue('{', '{', ALL_CHAR, false);
    SetPairValue(0x7F, 0x7F, ALL_CHAR, false);
#undef ALL_CHAR
  }

  // Print the C++ source code.
  void Print(FILE* fp) {
    // Print file headers.
    fprintf(fp,
            "#include <cstdint>\n"
            "#include "
            "\"third_party/blink/renderer/platform/wtf/text/wtf_uchar.h\"\n"
            "\nnamespace {\n\n");

    fprintf(fp, "inline constexpr UChar kFastLineBreakMinChar = 0x%02X;\n",
            kMinChar);
    fprintf(fp, "inline constexpr UChar kFastLineBreakMaxChar = 0x%02X;\n",
            kMaxChar);

    // Define macros.
    fprintf(fp,
            "\n#define B(a, b, c, d, e, f, g, h)"
            " ((a) | ((b) << 1) | ((c) << 2) | ((d) << 3) |"
            " ((e) << 4) | ((f) << 5) | ((g) << 6) | ((h) << 7))\n\n");

    fprintf(fp, "const uint8_t kFastLineBreakTable[%d][%d] = {\n", kNumChars,
            kNumCharsRoundUp8 / 8);

    // Print the column comment.
    fprintf(fp, "           /*");
    for (UChar ch = kMinChar; ch <= kMaxChar; ++ch) {
      if (ch != kMinChar && (ch - kMinChar) % 8 == 0) {
        fprintf(fp, "   ");
      }
      UNSAFE_TODO(fprintf(fp, ch < 0x7F ? " %c" : "%02X", ch));
    }
    fprintf(fp, " */\n");

    // Print the data array.
    for (int y = 0; y < kNumChars; ++y) {
      const UChar ch = y + kMinChar;
      fprintf(fp, "/* %02X %c */ {B(", ch, ch < 0x7F ? ch : ' ');
      const char* prefix = "";
      for (int x = 0; x < kNumCharsRoundUp8; ++x) {
        UNSAFE_TODO(fprintf(fp, "%s%d", prefix, pair_[y][x]));
        prefix = (x % 8 == 7) ? "),B(" : ",";
      }
      fprintf(fp, ")},\n");
    }
    fprintf(fp,
            "};\n\n"
            "#undef B\n\n"
            "template <typename T>\n"
            "inline uint8_t GetFastLineBreak(T ch1, T ch2) {\n"
            "  const T i2 = ch2 - kFastLineBreakMinChar;\n"
            "  return kFastLineBreakTable[ch1 - kFastLineBreakMinChar]"
            "[i2 / 8] & (1 << (i2 %% 8));\n"
            "}\n\n"
            "}  // namespace\n");
  }

  void SetPairValue(UChar ch1_min,
                    UChar ch1_max,
                    UChar ch2_min,
                    UChar ch2_max,
                    bool value) {
    for (UChar ch1 = ch1_min; ch1 <= ch1_max; ++ch1) {
      for (UChar ch2 = ch2_min; ch2 <= ch2_max; ++ch2) {
        SetPairValue(ch1, ch2, value);
      }
    }
  }

  // Set the breakability between `ch1` and `ch2`.
  void SetPairValue(UChar ch1, UChar ch2, bool value) {
    CHECK_GE(ch1, kMinChar);
    CHECK_LE(ch1, kMaxChar);
    CHECK_GE(ch2, kMinChar);
    CHECK_LE(ch2, kMaxChar);
    UNSAFE_TODO(pair_[ch1 - kMinChar][ch2 - kMinChar]) = value;
  }

  constexpr static UChar kMinChar = '!';
  constexpr static UChar kMaxChar = 0xFF;
  constexpr static int kNumChars = kMaxChar - kMinChar + 1;
  constexpr static int kNumCharsRoundUp8 = (kNumChars + 7) / 8 * 8;
  bool pair_[kNumChars][kNumCharsRoundUp8]{};
};

void InvokeGenerator(int index,
                     int argc,
                     char** argv,
                     void (*generator)(FILE*)) {
  if (index >= argc) {
    return;
  }
  const char* path = UNSAFE_TODO(argv[index]);
  if (!*path) {
    return;
  }

  if (UNSAFE_TODO(strcmp(path, "-")) == 0) {
    (*generator)(stdout);
    return;
  }

  FILE* fp = fopen(path, "wb");
  (*generator)(fp);
  fclose(fp);
}

}  // namespace
}  // namespace blink

int main(int argc, char** argv) {
  blink::InitializeIcu(argv[0]);
  blink::InvokeGenerator(1, argc, argv, blink::GenerateCharacterPropertyData);
  blink::InvokeGenerator(2, argc, argv, blink::LineBreakData::Generate);

  return 0;
}
