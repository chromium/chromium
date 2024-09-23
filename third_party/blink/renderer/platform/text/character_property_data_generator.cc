// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/text/character_property_data.h"

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

  CharacterPropertyValues() : values_(new CharacterProperty[kSize]) {
    Initialize();
  }

  CharacterProperty operator[](UChar32 index) const { return values_[index]; }

 private:
  void Initialize() {
    memset(values_.get(), 0, sizeof(CharacterProperty) * kSize);

#define SET(name)                                     \
  SetForRanges(name##Ranges, std::size(name##Ranges), \
               CharacterProperty::name);              \
  SetForValues(name##Array, std::size(name##Array), CharacterProperty::name);

    SET(kIsCJKIdeographOrSymbol);
    SET(kIsPotentialCustomElementNameChar);
    SET(kIsBidiControl);
#undef SET
    SetForRanges(kIsHangulRanges, std::size(kIsHangulRanges),
                 CharacterProperty::kIsHangul);
    SetHanKerning();
  }

  void SetHanKerning() {
    // https://drafts.csswg.org/css-text-4/#text-spacing-classes
    Set(kLeftSingleQuotationMarkCharacter, HanKerningCharType::kOpenQuote);
    Set(kLeftDoubleQuotationMarkCharacter, HanKerningCharType::kOpenQuote);
    Set(kRightSingleQuotationMarkCharacter, HanKerningCharType::kCloseQuote);
    Set(kRightDoubleQuotationMarkCharacter, HanKerningCharType::kCloseQuote);
    Set(kIdeographicSpaceCharacter, HanKerningCharType::kMiddle);
    Set(kIdeographicCommaCharacter, HanKerningCharType::kDot);
    Set(kIdeographicFullStopCharacter, HanKerningCharType::kDot);
    Set(kFullwidthComma, HanKerningCharType::kDot);
    Set(kFullwidthFullStop, HanKerningCharType::kDot);
    Set(kFullwidthColon, HanKerningCharType::kColon);
    Set(kFullwidthSemicolon, HanKerningCharType::kSemicolon);
    Set(kMiddleDotCharacter, HanKerningCharType::kMiddle);
    Set(kHyphenationPointCharacter, HanKerningCharType::kMiddle);
    Set(kKatakanaMiddleDot, HanKerningCharType::kMiddle);
    SetForUnicodeSet("[[:blk=CJK_Symbols:][:ea=F:] & [:gc=Ps:]]",
                     HanKerningCharType::kOpen);
    SetForUnicodeSet("[[:blk=CJK_Symbols:][:ea=F:] & [:gc=Pe:]]",
                     HanKerningCharType::kClose);
    SetForUnicodeSet("[[:gc=Ps:] - [:blk=CJK_Symbols:] - [:ea=F:]]",
                     HanKerningCharType::kOpenNarrow);
    SetForUnicodeSet("[[:gc=Pe:] - [:blk=CJK_Symbols:] - [:ea=F:]]",
                     HanKerningCharType::kCloseNarrow);
  }

  static CharacterProperty ToCharacterProperty(HanKerningCharType value) {
    CHECK_EQ((static_cast<unsigned>(value) &
              ~static_cast<unsigned>(CharacterProperty::kHanKerningMask)),
             0u);
    return static_cast<CharacterProperty>(
        static_cast<unsigned>(value)
        << static_cast<unsigned>(CharacterProperty::kHanKerningShift));
  }

  void SetForUnicodeSet(const char* pattern, HanKerningCharType type) {
    SetForUnicodeSet(pattern, ToCharacterProperty(type),
                     CharacterProperty::kHanKerningShiftedMask);
  }

  // For `patterns`, see:
  // https://unicode-org.github.io/icu/userguide/strings/unicodeset.html#unicodeset-patterns
  void SetForUnicodeSet(const char* pattern,
                        CharacterProperty value,
                        CharacterProperty mask) {
    UErrorCode error = U_ZERO_ERROR;
    icu::UnicodeSet set(icu::UnicodeString(pattern), error);
    CHECK_EQ(error, U_ZERO_ERROR);
    const int32_t range_count = set.getRangeCount();
    for (int32_t i = 0; i < range_count; ++i) {
      const UChar32 end = set.getRangeEnd(i);
      for (UChar32 ch = set.getRangeStart(i); ch <= end; ++ch) {
        CHECK_EQ(static_cast<unsigned>(values_[ch] & mask), 0u);
        values_[ch] |= value;
      }
    }
  }

  void SetForRanges(const UChar32* ranges,
                    size_t length,
                    CharacterProperty value) {
    CHECK_EQ(length % 2, 0u);
    const UChar32* end = ranges + length;
    for (; ranges != end; ranges += 2) {
      CHECK_LE(ranges[0], ranges[1]);
      CHECK_LE(ranges[1], kMaxCodepoint);
      for (UChar32 c = ranges[0]; c <= ranges[1]; c++) {
        values_[c] |= value;
      }
    }
  }

  void SetForValues(const UChar32* begin,
                    size_t length,
                    CharacterProperty value) {
    const UChar32* end = begin + length;
    for (; begin != end; begin++) {
      CHECK_LE(*begin, kMaxCodepoint);
      values_[*begin] |= value;
    }
  }

  void Set(UChar32 ch, HanKerningCharType type) {
    const CharacterProperty value = ToCharacterProperty(type);
    CHECK_EQ(static_cast<unsigned>(values_[ch] &
                                   CharacterProperty::kHanKerningShiftedMask),
             0u);
    values_[ch] |= value;
  }

  std::unique_ptr<CharacterProperty[]> values_;
};

static void GenerateUTrieSerialized(FILE* fp,
                                    int32_t size,
                                    base::span<uint8_t> array) {
  fprintf(fp,
          "#include <cstdint>\n\n"
          "namespace blink {\n\n"
          "extern const int32_t kSerializedCharacterDataSize = %d;\n"
          // The utrie2_openFromSerialized function requires character data to
          // be aligned to 4 bytes.
          "alignas(4) extern const uint8_t kSerializedCharacterData[] = {",
          size);
  for (int32_t i = 0; i < size;) {
    fprintf(fp, "\n   ");
    for (int col = 0; col < 16 && i < size; col++, i++)
      fprintf(fp, " 0x%02X,", array[i]);
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
    if (static_cast<uint32_t>(value)) {
      umutablecptrie_setRange(trie.get(), start, c - 1,
                              static_cast<uint32_t>(value), &error);
      assert(error == U_ZERO_ERROR);
    }
    if (c >= CharacterPropertyValues::kSize) {
      break;
    }
    start = c;
    value = values[start];
  }

  // Convert to immutable UCPTrie in order to be able to serialize.
  std::unique_ptr<UCPTrie, decltype(&ucptrie_close)> immutable_trie(
      umutablecptrie_buildImmutable(trie.get(), UCPTrieType::UCPTRIE_TYPE_FAST,
                                    UCPTrieValueWidth::UCPTRIE_VALUE_BITS_16,
                                    &error),
      ucptrie_close);

  assert(error == U_ZERO_ERROR);

  int32_t serialized_size =
      ucptrie_toBinary(immutable_trie.get(), nullptr, 0, &error);
  error = U_ZERO_ERROR;

  auto serialized = base::HeapArray<uint8_t>::Uninit(serialized_size);
  // Ensure 32-bit alignment, as ICU requires that to the ucptrie_toBinary call.
  CHECK(!(reinterpret_cast<intptr_t>(serialized.data()) % 4));

  serialized_size = ucptrie_toBinary(immutable_trie.get(), serialized.data(),
                                     serialized.size(), &error);
  assert(error == U_ZERO_ERROR);

  GenerateUTrieSerialized(fp, serialized_size, serialized);
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
    icu::BreakIterator* break_iterator =
        icu::BreakIterator::createLineInstance(locale, status);
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

    fprintf(fp, "constexpr UChar kFastLineBreakMinChar = 0x%02X;\n", kMinChar);
    fprintf(fp, "constexpr UChar kFastLineBreakMaxChar = 0x%02X;\n", kMaxChar);

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
      fprintf(fp, ch < 0x7F ? " %c" : "%02X", ch);
    }
    fprintf(fp, " */\n");

    // Print the data array.
    for (int y = 0; y < kNumChars; ++y) {
      const UChar ch = y + kMinChar;
      fprintf(fp, "/* %02X %c */ {B(", ch, ch < 0x7F ? ch : ' ');
      const char* prefix = "";
      for (int x = 0; x < kNumCharsRoundUp8; ++x) {
        fprintf(fp, "%s%d", prefix, pair_[y][x]);
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
    pair_[ch1 - kMinChar][ch2 - kMinChar] = value;
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
  const char* path = argv[index];
  if (!*path) {
    return;
  }

  if (strcmp(path, "-") == 0) {
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
