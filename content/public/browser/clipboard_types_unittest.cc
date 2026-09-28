// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/clipboard_types.h"

#include <string>

#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {

namespace {

constexpr size_t kUtf16UnitSize = sizeof(std::u16string::value_type);

}  // namespace

TEST(ClipboardPasteDataTest, SizeOfEmptyData) {
  EXPECT_EQ(ClipboardPasteData().size(), 0u);
}

TEST(ClipboardPasteDataTest, SizeCountsUtf16FieldsInBytes) {
  ClipboardPasteData text;
  text.text = u"text";
  EXPECT_EQ(text.size(), 4 * kUtf16UnitSize);

  ClipboardPasteData html;
  html.html = u"<p>html</p>";
  EXPECT_EQ(html.size(), 11 * kUtf16UnitSize);

  ClipboardPasteData svg;
  svg.svg = u"<svg></svg>";
  EXPECT_EQ(svg.size(), 11 * kUtf16UnitSize);
}

TEST(ClipboardPasteDataTest, SizeCountsBinaryFieldsInBytes) {
  ClipboardPasteData rtf;
  rtf.rtf = "{\\rtf1}";
  EXPECT_EQ(rtf.size(), 7u);

  ClipboardPasteData png;
  png.png = {1, 2, 3};
  EXPECT_EQ(png.size(), 3u);

  ClipboardPasteData bitmap;
  bitmap.bitmap.allocN32Pixels(2, 3);
  EXPECT_EQ(bitmap.size(), bitmap.bitmap.computeByteSize());
  EXPECT_EQ(bitmap.size(), 2u * 3u * 4u);
}

TEST(ClipboardPasteDataTest, SizeCountsCustomDataTypesAndValues) {
  ClipboardPasteData data;
  data.custom_data[u"type"] = u"value";
  EXPECT_EQ(data.size(), (4 + 5) * kUtf16UnitSize);

  data.custom_data[u"other"] = u"";
  EXPECT_EQ(data.size(), (4 + 5 + 5) * kUtf16UnitSize);
}

// Pages control custom data types, so arbitrary data hidden in a type must
// count towards the size even if its value is tiny.
TEST(ClipboardPasteDataTest, SizeCountsLargeCustomDataType) {
  ClipboardPasteData data;
  data.custom_data[std::u16string(1000, u'x')] = u"A";
  EXPECT_EQ(data.size(), 1001 * kUtf16UnitSize);
}

TEST(ClipboardPasteDataTest, SizeIgnoresFilePaths) {
  ClipboardPasteData data;
  data.file_paths = {base::FilePath(FILE_PATH_LITERAL("/some/file"))};
  EXPECT_EQ(data.size(), 0u);
}

}  // namespace content
