// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/out_of_process_instance.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_pdf {

TEST(OutOfProcessInstanceTest, GetFileNameFromUrl) {
  EXPECT_EQ("b.pdf",
            OutOfProcessInstance::GetFileNameFromUrl("https://test/a/b.pdf"));

  // File extensions should be kept as-is.
  EXPECT_EQ("b.hat",
            OutOfProcessInstance::GetFileNameFromUrl("https://test/a/b.hat"));

  // Most escaped characters should be unescaped.
  EXPECT_EQ("a b.pdf", OutOfProcessInstance::GetFileNameFromUrl(
                           "https://test/%61%20b.pdf"));

  // Escaped file path delimiters and control codes should be replaced by a
  // placeholder.
  EXPECT_EQ("a_b_.pdf", OutOfProcessInstance::GetFileNameFromUrl(
                            "https://test/a%2Fb%01.pdf"));

  // UTF-8 characters, including ones left escaped by UnescapeURLComponent() for
  // security reasons, are allowed in file paths.
  EXPECT_EQ("\xF0\x9F\x94\x92", OutOfProcessInstance::GetFileNameFromUrl(
                                    "https://test/%F0%9F%94%92"));
}

}  // namespace chrome_pdf
