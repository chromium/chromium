// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ui/file_name.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_pdf {

TEST(FileNameTest, GetFileNameForSaveFromUrlAndSuggestion) {
  EXPECT_EQ("b.pdf",
            GetFileNameForSaveFromUrlAndSuggestion("https://test/a/b.pdf", ""));
  EXPECT_EQ("custom.pdf", GetFileNameForSaveFromUrlAndSuggestion(
                              "https://test/a/b.pdf", "custom.pdf"));

  // File extensions should be kept as-is.
  EXPECT_EQ("b.hat",
            GetFileNameForSaveFromUrlAndSuggestion("https://test/a/b.hat", ""));
  EXPECT_EQ("custom.hat", GetFileNameForSaveFromUrlAndSuggestion(
                              "https://test/a/b.pdf", "custom.hat"));

  // Most escaped characters should be unescaped.
  // Note that the suggested file name is never escaped, so not testing with
  // that input.
  EXPECT_EQ("a b.pdf", GetFileNameForSaveFromUrlAndSuggestion(
                           "https://test/%61%20b.pdf", ""));

  // Escaped file path delimiters and control codes should be replaced by a
  // placeholder.
  EXPECT_EQ("a_b_.pdf", GetFileNameForSaveFromUrlAndSuggestion(
                            "https://test/a%2Fb%01.pdf", ""));
  // Whereas net::GetSuggestedFilename() checks for illegal characters in the
  // suggested name.
  EXPECT_EQ("a_b.pdf", GetFileNameForSaveFromUrlAndSuggestion(
                           "https://test/a/b.pdf", "a/b.pdf"));

  // UTF-8 characters, including ones left escaped by UnescapeURLComponent() for
  // security reasons, are allowed in file paths.
  EXPECT_EQ("\xF0\x9F\x94\x92", GetFileNameForSaveFromUrlAndSuggestion(
                                    "https://test/%F0%9F%94%92", ""));
  EXPECT_EQ("\xF0\x9F\x94\x92.pdf",
            GetFileNameForSaveFromUrlAndSuggestion("https://test/a/b.pdf",
                                                   "\xF0\x9F\x94\x92.pdf"));

  // File names without extensions are kept as-is.
  EXPECT_EQ("b",
            GetFileNameForSaveFromUrlAndSuggestion("https://test/a/b", ""));
  EXPECT_EQ("custom", GetFileNameForSaveFromUrlAndSuggestion(
                          "https://test/a/b.pdf", "custom"));

  // URLs with query parameters.
  EXPECT_EQ("download.php", GetFileNameForSaveFromUrlAndSuggestion(
                                "https://test/a/download.php?id=123", ""));
  // Or use the suggested name if available.
  EXPECT_EQ("custom.pdf",
            GetFileNameForSaveFromUrlAndSuggestion(
                "https://test/a/download.php?id=123", "custom.pdf"));

  // URLs without paths fall back to the host name.
  EXPECT_EQ("test", GetFileNameForSaveFromUrlAndSuggestion("https://test", ""));
  // Or use the suggested name if available.
  EXPECT_EQ("custom.pdf", GetFileNameForSaveFromUrlAndSuggestion("https://test",
                                                                 "custom.pdf"));

  // URLs with empty paths fall back to the host name.
  EXPECT_EQ("test",
            GetFileNameForSaveFromUrlAndSuggestion("https://test/", ""));
  // Or use the suggested name if available.
  EXPECT_EQ("custom.pdf", GetFileNameForSaveFromUrlAndSuggestion(
                              "https://test/", "custom.pdf"));

  // Leading dots in suggested names are trimmed.
  EXPECT_EQ("custom.pdf", GetFileNameForSaveFromUrlAndSuggestion(
                              "https://test/a/b.pdf", "..custom.pdf"));
}

}  // namespace chrome_pdf
