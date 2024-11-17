// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/finder/find_results.h"

#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"

namespace blink {

class FindResultsTest : public EditingTestBase {
 protected:
  // The result replaces '_' in `literal` with '\0'.
  template <size_t N>
  static Vector<UChar> MakeBuffer(const UChar (&literal)[N]) {
    Vector<UChar> buffer;
    buffer.reserve(N);
    buffer.Append(literal, N);
    for (auto& ch : buffer) {
      if (ch == '_') {
        ch = 0;
      }
    }
    return buffer;
  }

  Vector<unsigned> ResultOffsets(FindResults& results) {
    Vector<unsigned> offsets;
    for (const auto match : results) {
      offsets.push_back(match.start);
    }
    return offsets;
  }

  TextSearcherICU main_searcher_;
};

TEST_F(FindResultsTest, MultipleIdenticalCorpora) {
  Vector<UChar> base_buffer = MakeBuffer(u"foo foo foo");
  Vector<Vector<UChar>> extra_buffers = {base_buffer, base_buffer};
  String query(u"foo");
  FindBuffer* find_buffer = nullptr;
  FindResults results(find_buffer, &main_searcher_, base_buffer, &extra_buffers,
                      query, FindOptions());

  // We have three identical buffers, and each buffer contains three matches.
  // FindResults should merge nine matches into three.
  Vector<unsigned> offsets = ResultOffsets(results);
  EXPECT_EQ((Vector<unsigned>{0u, 4u, 8u}), offsets);
}

TEST_F(FindResultsTest, MultipleCorpora) {
  Vector<UChar> buffer0 = MakeBuffer(u"foo fo__o foo");
  Vector<UChar> buffer1 = MakeBuffer(u"foo __foo foo");
  Vector<Vector<UChar>> extra_buffers = {buffer1};
  String query(u"foo");
  FindBuffer* find_buffer = nullptr;
  FindResults results(find_buffer, &main_searcher_, buffer0, &extra_buffers,
                      query, FindOptions());

  Vector<unsigned> offsets = ResultOffsets(results);
  EXPECT_EQ((Vector<unsigned>{0u, 4u, 6u, 10u}), offsets);
}

TEST_F(FindResultsTest, AnIteratorReachesToEnd) {
  Vector<UChar> buffer0 = MakeBuffer(u"foo fo__o");
  Vector<UChar> buffer1 = MakeBuffer(u"foo __foo");
  Vector<Vector<UChar>> extra_buffers = {buffer1};
  String query(u"foo");
  FindBuffer* find_buffer = nullptr;
  FindResults results(find_buffer, &main_searcher_, buffer0, &extra_buffers,
                      query, FindOptions());

  Vector<unsigned> offsets = ResultOffsets(results);
  EXPECT_EQ((Vector<unsigned>{0u, 4u, 6u}), offsets);
}

}  // namespace blink
