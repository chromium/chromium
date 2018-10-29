/*
 * Copyright (C) 2012 Koji Ishii <kojiishi@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/opentype/open_type_types.h"

namespace blink {

struct TestTable : open_type::TableBase {
  open_type::Fixed version;
  open_type::Int16 ascender;

  template <typename T>
  const T* ValidateOffset(const Vector<char>& buffer, uint16_t offset) const {
    return TableBase::ValidateOffset<T>(buffer, offset);
  }
};

TEST(OpenTypeVerticalDataTest, ValidateTableTest) {
  Vector<char> buffer(sizeof(TestTable));
  const TestTable* table = open_type::ValidateTable<TestTable>(buffer);
  EXPECT_TRUE(table);

  buffer = Vector<char>(sizeof(TestTable) - 1);
  table = open_type::ValidateTable<TestTable>(buffer);
  EXPECT_FALSE(table);

  buffer = Vector<char>(sizeof(TestTable) + 1);
  table = open_type::ValidateTable<TestTable>(buffer);
  EXPECT_TRUE(table);
}

TEST(OpenTypeVerticalDataTest, ValidateOffsetTest) {
  Vector<char> buffer(sizeof(TestTable));
  const TestTable* table = open_type::ValidateTable<TestTable>(buffer);
  ASSERT_TRUE(table);

  // Test overflow
  EXPECT_FALSE(table->ValidateOffset<uint8_t>(buffer, 0xFFFF));

  // uint8_t is valid for all offsets
  for (uint16_t offset = 0; offset < sizeof(TestTable); offset++)
    EXPECT_TRUE(table->ValidateOffset<uint8_t>(buffer, offset));
  EXPECT_FALSE(table->ValidateOffset<uint8_t>(buffer, sizeof(TestTable)));
  EXPECT_FALSE(table->ValidateOffset<uint8_t>(buffer, sizeof(TestTable) + 1));

  // For uint16_t, the last byte is invalid
  for (uint16_t offset = 0; offset < sizeof(TestTable) - 1; offset++)
    EXPECT_TRUE(table->ValidateOffset<uint16_t>(buffer, offset));
  EXPECT_FALSE(table->ValidateOffset<uint16_t>(buffer, sizeof(TestTable) - 1));
}

}  // namespace blink
