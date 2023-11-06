// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/character_property_data.h"

#include <stdio.h>
#include <unicode/ucptrie.h>
#include <unicode/umutablecptrie.h>
#include <unicode/uniset.h>
#include <unicode/unistr.h>

#include <cassert>
#include <cstring>
#include <iterator>
#include <memory>

#include "base/check_op.h"
#include "third_party/blink/renderer/platform/text/character_property.h"
#include "third_party/blink/renderer/platform/text/han_kerning_char_type.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {
namespace {

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

static void GenerateUTrieSerialized(FILE* fp, int32_t size, uint8_t* array) {
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

  std::unique_ptr<uint8_t[]> serialized(new uint8_t[serialized_size]);
  // Ensure 32-bit alignment, as ICU requires that to the ucptrie_toBinary call.
  CHECK(!(reinterpret_cast<intptr_t>(serialized.get()) % 4));

  serialized_size = ucptrie_toBinary(immutable_trie.get(), serialized.get(),
                                     serialized_size, &error);
  assert(error == U_ZERO_ERROR);

  GenerateUTrieSerialized(fp, serialized_size, serialized.get());
}

}  // namespace
}  // namespace blink

int main(int argc, char** argv) {
  // Write the serialized array to the source file.
  if (argc <= 1) {
    blink::GenerateCharacterPropertyData(stdout);
  } else {
    FILE* fp = fopen(argv[1], "wb");
    blink::GenerateCharacterPropertyData(fp);
    fclose(fp);
  }

  return 0;
}
