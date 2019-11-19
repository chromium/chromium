/*
 * Copyright (C) 2013, Opera Software ASA. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/track/vtt/buffered_line_reader.h"

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

TEST(BufferedLineReaderTest, Constructor) {
  BufferedLineReader reader;
  ASSERT_FALSE(reader.IsAtEndOfStream());
  String line;
  ASSERT_FALSE(reader.GetLine(line));
}

TEST(BufferedLineReaderTest, EOSNoInput) {
  BufferedLineReader reader;
  String line;
  ASSERT_FALSE(reader.GetLine(line));
  reader.SetEndOfStream();
  // No input observed, so still no line.
  ASSERT_FALSE(reader.GetLine(line));
}

TEST(BufferedLineReaderTest, EOSInput) {
  BufferedLineReader reader;
  reader.Append("A");
  reader.SetEndOfStream();
  String line;
  ASSERT_TRUE(reader.GetLine(line));
  ASSERT_EQ(line, "A");
}

TEST(BufferedLineReaderTest, EOSMultipleReads_1) {
  BufferedLineReader reader;
  reader.Append("A");
  reader.SetEndOfStream();
  String line;
  ASSERT_TRUE(reader.GetLine(line));
  ASSERT_EQ(line, "A");
  // No more lines returned.
  ASSERT_FALSE(reader.GetLine(line));
  ASSERT_FALSE(reader.GetLine(line));
}

TEST(BufferedLineReaderTest, EOSMultipleReads_2) {
  BufferedLineReader reader;
  reader.Append("A\n");
  reader.SetEndOfStream();
  String line;
  ASSERT_TRUE(reader.GetLine(line));
  ASSERT_EQ(line, "A");
  // No more lines returned.
  ASSERT_FALSE(reader.GetLine(line));
  ASSERT_FALSE(reader.GetLine(line));
}

TEST(BufferedLineReaderTest, LineEndingCR) {
  BufferedLineReader reader;
  reader.Append("X\rY");
  reader.SetEndOfStream();
  String line;
  ASSERT_TRUE(reader.GetLine(line));
  ASSERT_EQ(line, "X");
  ASSERT_TRUE(reader.GetLine(line));
  ASSERT_EQ(line, "Y");
}

TEST(BufferedLineReaderTest, LineEndingCR_EOS) {
  BufferedLineReader reader;
  reader.Append("X\r");
  reader.SetEndOfStream();
  String line;
  ASSERT_TRUE(reader.GetLine(line));
  ASSERT_EQ(line, "X");
  ASSERT_FALSE(reader.GetLine(line));
}

TEST(BufferedLineReaderTest, LineEndingLF) {
  BufferedLineReader reader;
  reader.Append("X\nY");
  reader.SetEndOfStream();
  String line;
  ASSERT_TRUE(reader.GetLine(line));
  ASSERT_EQ(line, "X");
  ASSERT_TRUE(reader.GetLine(line));
  ASSERT_EQ(line, "Y");
}

TEST(BufferedLineReaderTest, LineEndingLF_EOS) {
  BufferedLineReader reader;
  reader.Append("X\n");
  reader.SetEndOfStream();
  String line;
  ASSERT_TRUE(reader.GetLine(line));
  ASSERT_EQ(line, "X");
  ASSERT_FALSE(reader.GetLine(line));
}

TEST(BufferedLineReaderTest, LineEndingCRLF) {
  BufferedLineReader reader;
  reader.Append("X\r\nY");
  reader.SetEndOfStream();
  String line;
  ASSERT_TRUE(reader.GetLine(line));
  ASSERT_EQ(line, "X");
  ASSERT_TRUE(reader.GetLine(line));
  ASSERT_EQ(line, "Y");
}

TEST(BufferedLineReaderTest, LineEndingCRLF_EOS) {
  BufferedLineReader reader;
  reader.Append("X\r\n");
  reader.SetEndOfStream();
  String line;
  ASSERT_TRUE(reader.GetLine(line));
  ASSERT_EQ(line, "X");
  ASSERT_FALSE(reader.GetLine(line));
}

enum NewlineType { kCr, kLf, kCrLf };

String LineBreakString(NewlineType type) {
  static const char kBreakStrings[] = "\r\n";
  return String(type == kLf ? kBreakStrings + 1 : kBreakStrings,
                type == kCrLf ? 2u : 1u);
}

String MakeTestData(const char** lines, const NewlineType* breaks, int count) {
  StringBuilder builder;
  for (int i = 0; i < count; ++i) {
    builder.Append(lines[i]);
    builder.Append(LineBreakString(breaks[i]));
  }
  return builder.ToString();
}

const wtf_size_t kBlockSizes[] = {64, 32, 16, 8,  4,  2,  1,  3,
                                  5,  7,  9,  11, 13, 17, 19, 23};

TEST(BufferedLineReaderTest, BufferSizes) {
  const char* lines[] = {"aaaaaaaaaaaaaaaa", "bbbbbbbbbb", "ccccccccccccc", "",
                         "dddddd",           "",           "eeeeeeeeee"};
  const NewlineType kBreaks[] = {kLf, kLf, kLf, kLf, kLf, kLf, kLf};
  const size_t num_test_lines = base::size(lines);
  static_assert(num_test_lines == base::size(kBreaks),
                "number of test lines and breaks should be the same");
  String data = MakeTestData(lines, kBreaks, num_test_lines);

  for (size_t k = 0; k < base::size(kBlockSizes); ++k) {
    size_t line_count = 0;
    BufferedLineReader reader;
    wtf_size_t block_size = kBlockSizes[k];
    for (wtf_size_t i = 0; i < data.length(); i += block_size) {
      reader.Append(data.Substring(i, block_size));

      String line;
      while (reader.GetLine(line)) {
        ASSERT_LT(line_count, num_test_lines);
        ASSERT_EQ(line, lines[line_count++]);
      }
    }
    ASSERT_EQ(line_count, num_test_lines);
  }
}

TEST(BufferedLineReaderTest, BufferSizesMixedEndings) {
  const char* lines[] = {
      "aaaaaaaaaaaaaaaa", "bbbbbbbbbb", "ccccccccccccc",      "",
      "dddddd",           "eeeeeeeeee", "fffffffffffffffffff"};
  const NewlineType kBreaks[] = {kCr, kLf, kCrLf, kCr, kLf, kCrLf, kLf};
  const size_t num_test_lines = base::size(lines);
  static_assert(num_test_lines == base::size(kBreaks),
                "number of test lines and breaks should be the same");
  String data = MakeTestData(lines, kBreaks, num_test_lines);

  for (size_t k = 0; k < base::size(kBlockSizes); ++k) {
    size_t line_count = 0;
    BufferedLineReader reader;
    wtf_size_t block_size = kBlockSizes[k];
    for (wtf_size_t i = 0; i < data.length(); i += block_size) {
      reader.Append(data.Substring(i, block_size));

      String line;
      while (reader.GetLine(line)) {
        ASSERT_LT(line_count, num_test_lines);
        ASSERT_EQ(line, lines[line_count++]);
      }
    }
    ASSERT_EQ(line_count, num_test_lines);
  }
}

TEST(BufferedLineReaderTest, BufferBoundaryInCRLF_1) {
  BufferedLineReader reader;
  reader.Append("X\r");
  String line;
  ASSERT_TRUE(reader.GetLine(line));
  ASSERT_EQ(line, "X");
  reader.Append("\n");
  ASSERT_FALSE(reader.GetLine(line));
}

TEST(BufferedLineReaderTest, BufferBoundaryInCRLF_2) {
  BufferedLineReader reader;
  reader.Append("X\r");
  String line;
  ASSERT_TRUE(reader.GetLine(line));
  ASSERT_EQ(line, "X");
  ASSERT_FALSE(reader.GetLine(line));
  reader.Append("\n");
  ASSERT_FALSE(reader.GetLine(line));
  reader.Append("Y\n");
  ASSERT_TRUE(reader.GetLine(line));
  ASSERT_EQ(line, "Y");
}

TEST(BufferedLineReaderTest, NormalizedNUL) {
  BufferedLineReader reader;
  reader.Append(String("X\0Y\n", 4u));
  String line;
  ASSERT_TRUE(reader.GetLine(line));
  ASSERT_EQ(line[1], kReplacementCharacter);
}

}  // namespace blink
