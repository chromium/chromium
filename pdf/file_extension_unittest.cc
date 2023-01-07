// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/file_extension.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_pdf {

TEST(FileExtensionTest, FileNameToExtensionIndex) {
  // File name with the first known file extension.
  EXPECT_EQ(ExtensionIndex::k3ga, FileNameToExtensionIndex(u"first.3ga"));

  // File name with the last known file extension.
  EXPECT_EQ(ExtensionIndex::kTini, FileNameToExtensionIndex(u"last.tini"));

  // File name without an extension.
  EXPECT_EQ(ExtensionIndex::kEmptyExt,
            FileNameToExtensionIndex(u"file_no_ext"));

  // File name with an unrecognized file extension.
  EXPECT_EQ(ExtensionIndex::kOtherExt, FileNameToExtensionIndex(u"file.xyz"));

  // File name with non-ASCII characters.
  EXPECT_EQ(ExtensionIndex::kPdf, FileNameToExtensionIndex(u"你好.pdf"));

  // Empty file name.
  EXPECT_EQ(ExtensionIndex::kEmptyExt, FileNameToExtensionIndex(u""));

  // File name with an extension which contains non-ASCII characters.
  EXPECT_EQ(ExtensionIndex::kOtherExt, FileNameToExtensionIndex(u"file.你好"));

  // File name which ends with a dot.
  EXPECT_EQ(ExtensionIndex::kOtherExt, FileNameToExtensionIndex(u"file."));
}

}  // namespace chrome_pdf
