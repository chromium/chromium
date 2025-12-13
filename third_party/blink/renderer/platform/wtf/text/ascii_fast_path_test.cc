// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/ascii_fast_path.h"

#include <vector>

#include "base/containers/span.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

template <typename T>
class AsciiFastPathCharacterAttributesTest : public ::testing::Test {};

using CharacterTypes = ::testing::Types<LChar, UChar>;
TYPED_TEST_SUITE(AsciiFastPathCharacterAttributesTest, CharacterTypes);

TYPED_TEST(AsciiFastPathCharacterAttributesTest, CharacterAttributes) {
  using CharType = TypeParam;

  // All lowercase ASCII.
  const std::vector<CharType> all_lower = {
      'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
      'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'};
  AsciiStringAttributes attributes =
      CharacterAttributes(base::span<const CharType>(all_lower));
  EXPECT_TRUE(attributes.contains_only_ascii);
  EXPECT_TRUE(attributes.is_lower_ascii);

  // Uppercase bounds checking.
  const std::vector<CharType> a_bound = {' ', 'A', 'a', 'a',
                                         'a', 'a', 'a', 'a'};
  attributes = CharacterAttributes(base::span<const CharType>(a_bound));
  EXPECT_TRUE(attributes.contains_only_ascii);
  EXPECT_FALSE(attributes.is_lower_ascii);

  // Uppercase bounds checking.
  const std::vector<CharType> z_bound = {'z', 'Z', 'h', 'e',
                                         'm', '_', 's', 't'};
  attributes = CharacterAttributes(base::span<const CharType>(z_bound));
  EXPECT_TRUE(attributes.contains_only_ascii);
  EXPECT_FALSE(attributes.is_lower_ascii);

  // Mixed case ASCII.
  const std::vector<CharType> mixed_case = {
      'a', 'b', 'c', 'D', 'e', 'f', 'G', 'h', 'i', 'J', 'k', 'l', 'M',
      'n', 'o', 'P', 'q', 'r', 'S', 't', 'u', 'V', 'w', 'x', 'Y', 'z'};
  attributes = CharacterAttributes(base::span<const CharType>(mixed_case));
  EXPECT_TRUE(attributes.contains_only_ascii);
  EXPECT_FALSE(attributes.is_lower_ascii);

  // All uppercase ASCII.
  const std::vector<CharType> all_upper = {
      'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
      'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'};
  attributes = CharacterAttributes(base::span<const CharType>(all_upper));
  EXPECT_TRUE(attributes.contains_only_ascii);
  EXPECT_FALSE(attributes.is_lower_ascii);

  const std::vector<CharType> brackets(8, '[');  // Z+1
  attributes = CharacterAttributes(base::span<const CharType>(brackets));
  EXPECT_TRUE(attributes.contains_only_ascii);
  EXPECT_TRUE(attributes.is_lower_ascii);

  const std::vector<CharType> at(8, '@');  // A - 1
  attributes = CharacterAttributes(base::span<const CharType>(at));
  EXPECT_TRUE(attributes.contains_only_ascii);
  EXPECT_TRUE(attributes.is_lower_ascii);

  // Non-ASCII.
  const std::vector<CharType> non_ascii = {'a', 'b', 'c', 0x80, 'd', 'e', 'f'};
  attributes = CharacterAttributes(base::span<const CharType>(non_ascii));
  EXPECT_FALSE(attributes.contains_only_ascii);

  // Foó
  const std::vector<CharType> foo_ascii = {'F', 'o', 0xC3, 0xB3};
  attributes = CharacterAttributes(base::span<const CharType>(foo_ascii));
  EXPECT_FALSE(attributes.contains_only_ascii);
  EXPECT_FALSE(attributes.is_lower_ascii);
}

TEST(AsciiFastPathTest, AsciiAndCapsRange) {
  for (size_t i = 0; i < 256; i++) {
    LChar c = i;
    std::vector<LChar> chars(10, c);
    AsciiStringAttributes attributes =
        CharacterAttributes(base::span<const LChar>(chars));
    EXPECT_EQ((i >= 0 && i <= 127), attributes.contains_only_ascii);
    EXPECT_EQ(!(i >= 'A' && i <= 'Z'), attributes.is_lower_ascii);
  }
}

TEST(AsciiFastPathTest, AsciiAndCapsRangeUChar) {
  for (size_t i = 0; i < 1024; i++) {
    UChar c = i;
    std::vector<UChar> chars(10, c);
    AsciiStringAttributes attributes =
        CharacterAttributes(base::span<const UChar>(chars));
    EXPECT_EQ((i >= 0 && i <= 127), attributes.contains_only_ascii);
    EXPECT_EQ(!(i >= 'A' && i <= 'Z'), attributes.is_lower_ascii);
  }
}

template <typename CharacterType>
void TestCharacterAttributes() {
  constexpr size_t kWordSize = sizeof(MachineWord);
  constexpr size_t kBufferSize = kWordSize * 4;
  alignas(MachineWord) CharacterType buffer[kBufferSize];
  base::span<CharacterType> buffer_span(buffer);

  for (size_t length = 1; length <= kWordSize * 2; ++length) {
    SCOPED_TRACE(base::StringPrintf("Length: %zu", length));

    for (size_t offset = 0; offset < kWordSize; ++offset) {
      SCOPED_TRACE(base::StringPrintf("Offset: %zu", offset));

      if (offset + length > kBufferSize) {
        continue;
      }

      base::span<CharacterType> data = buffer_span.subspan(offset, length);

      // All lowercase ASCII.
      for (size_t i = 0; i < length; ++i) {
        data[i] = 'a';
      }
      base::span<const CharacterType> const_data = data;
      AsciiStringAttributes attributes = CharacterAttributes(const_data);
      EXPECT_TRUE(attributes.contains_only_ascii);
      EXPECT_TRUE(attributes.is_lower_ascii);

      // Mixed case ASCII.
      for (size_t i = 0; i < length; ++i) {
        data[i] = (i % 2 == 0) ? 'A' : 'a';
      }
      const_data = data;
      attributes = CharacterAttributes(const_data);
      EXPECT_TRUE(attributes.contains_only_ascii);
      EXPECT_FALSE(attributes.is_lower_ascii);

      // One upper case ASCII.
      for (size_t i = 0; i < length; ++i) {
        data[i] = 'a';
      }
      data[length / 2] = 'A';
      const_data = data;
      attributes = CharacterAttributes(const_data);
      EXPECT_TRUE(attributes.contains_only_ascii);
      EXPECT_FALSE(attributes.is_lower_ascii);

      // All uppercase ASCII.
      for (size_t i = 0; i < length; ++i) {
        data[i] = 'A';
      }
      const_data = data;
      attributes = CharacterAttributes(const_data);
      EXPECT_TRUE(attributes.contains_only_ascii);
      EXPECT_FALSE(attributes.is_lower_ascii);

      // Non-ASCII.
      for (size_t i = 0; i < length; ++i) {
        data[i] = 'a';
      }
      data[length / 2] = 0x80;
      const_data = data;
      attributes = CharacterAttributes(const_data);
      EXPECT_FALSE(attributes.contains_only_ascii);
    }
  }
}

TEST(AsciiFastPathTest, CharacterAttributesAlignmentAndLength) {
  TestCharacterAttributes<LChar>();
  TestCharacterAttributes<UChar>();
}

}  // namespace

}  // namespace blink
