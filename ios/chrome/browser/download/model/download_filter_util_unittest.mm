// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_filter_util.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class DownloadFilterTest : public PlatformTest {};

// Test IsDownloadFilterMatch function with various MIME types and filter types
TEST_F(DownloadFilterTest, TestIsDownloadFilterMatch_PDF) {
  EXPECT_TRUE(
      IsDownloadFilterMatch("application/pdf", DownloadFilterType::kPDF));
  EXPECT_FALSE(IsDownloadFilterMatch("image/jpeg", DownloadFilterType::kPDF));
  EXPECT_FALSE(IsDownloadFilterMatch("video/mp4", DownloadFilterType::kPDF));
}

TEST_F(DownloadFilterTest, TestIsDownloadFilterMatch_Video) {
  EXPECT_TRUE(IsDownloadFilterMatch("video/mp4", DownloadFilterType::kVideo));
  EXPECT_TRUE(IsDownloadFilterMatch("video/avi", DownloadFilterType::kVideo));
  EXPECT_TRUE(IsDownloadFilterMatch("video/webm", DownloadFilterType::kVideo));
  EXPECT_FALSE(IsDownloadFilterMatch("audio/mp3", DownloadFilterType::kVideo));
  EXPECT_FALSE(IsDownloadFilterMatch("image/jpeg", DownloadFilterType::kVideo));
}

TEST_F(DownloadFilterTest, TestIsDownloadFilterMatch_Audio) {
  EXPECT_TRUE(IsDownloadFilterMatch("audio/mp3", DownloadFilterType::kAudio));
  EXPECT_TRUE(IsDownloadFilterMatch("audio/wav", DownloadFilterType::kAudio));
  EXPECT_TRUE(IsDownloadFilterMatch("audio/flac", DownloadFilterType::kAudio));
  EXPECT_FALSE(IsDownloadFilterMatch("video/mp4", DownloadFilterType::kAudio));
  EXPECT_FALSE(IsDownloadFilterMatch("text/plain", DownloadFilterType::kAudio));
}

TEST_F(DownloadFilterTest, TestIsDownloadFilterMatch_Image) {
  EXPECT_TRUE(IsDownloadFilterMatch("image/jpeg", DownloadFilterType::kImage));
  EXPECT_TRUE(IsDownloadFilterMatch("image/png", DownloadFilterType::kImage));
  EXPECT_TRUE(IsDownloadFilterMatch("image/gif", DownloadFilterType::kImage));
  EXPECT_FALSE(IsDownloadFilterMatch("text/plain", DownloadFilterType::kImage));
  EXPECT_FALSE(IsDownloadFilterMatch("video/mp4", DownloadFilterType::kImage));
}

TEST_F(DownloadFilterTest, TestIsDownloadFilterMatch_Document) {
  EXPECT_TRUE(
      IsDownloadFilterMatch("text/plain", DownloadFilterType::kDocument));
  EXPECT_TRUE(
      IsDownloadFilterMatch("text/html", DownloadFilterType::kDocument));
  EXPECT_TRUE(IsDownloadFilterMatch("text/css", DownloadFilterType::kDocument));
  EXPECT_FALSE(
      IsDownloadFilterMatch("image/jpeg", DownloadFilterType::kDocument));
  EXPECT_FALSE(
      IsDownloadFilterMatch("application/pdf", DownloadFilterType::kDocument));
}

TEST_F(DownloadFilterTest, TestIsDownloadFilterMatch_Other) {
  // Should match file types that don't belong to specific categories.
  EXPECT_TRUE(
      IsDownloadFilterMatch("application/zip", DownloadFilterType::kOther));
  EXPECT_TRUE(
      IsDownloadFilterMatch("application/json", DownloadFilterType::kOther));
  EXPECT_TRUE(IsDownloadFilterMatch("application/octet-stream",
                                    DownloadFilterType::kOther));
  EXPECT_TRUE(
      IsDownloadFilterMatch("unknown/type", DownloadFilterType::kOther));
  EXPECT_TRUE(
      IsDownloadFilterMatch("application/msword", DownloadFilterType::kOther));

  // Should not match specifically categorized file types - these are handled
  // by earlier cases in the switch statement.
  EXPECT_FALSE(
      IsDownloadFilterMatch("application/pdf", DownloadFilterType::kOther));
  EXPECT_FALSE(IsDownloadFilterMatch("video/mp4", DownloadFilterType::kOther));
  EXPECT_FALSE(IsDownloadFilterMatch("audio/mp3", DownloadFilterType::kOther));
  EXPECT_FALSE(IsDownloadFilterMatch("image/jpeg", DownloadFilterType::kOther));
  EXPECT_FALSE(IsDownloadFilterMatch("text/plain", DownloadFilterType::kOther));
}

TEST_F(DownloadFilterTest, TestIsDownloadFilterMatch_All) {
  EXPECT_TRUE(
      IsDownloadFilterMatch("application/pdf", DownloadFilterType::kAll));
  EXPECT_TRUE(IsDownloadFilterMatch("video/mp4", DownloadFilterType::kAll));
  EXPECT_TRUE(IsDownloadFilterMatch("audio/mp3", DownloadFilterType::kAll));
  EXPECT_TRUE(IsDownloadFilterMatch("image/jpeg", DownloadFilterType::kAll));
  EXPECT_TRUE(IsDownloadFilterMatch("text/plain", DownloadFilterType::kAll));
  EXPECT_TRUE(IsDownloadFilterMatch("unknown/type", DownloadFilterType::kAll));
  EXPECT_TRUE(IsDownloadFilterMatch("", DownloadFilterType::kAll));
}

TEST_F(DownloadFilterTest, TestIsDownloadFilterMatch_EmptyMIME) {
  EXPECT_TRUE(IsDownloadFilterMatch("", DownloadFilterType::kOther));
  EXPECT_FALSE(IsDownloadFilterMatch("", DownloadFilterType::kPDF));
  EXPECT_FALSE(IsDownloadFilterMatch("", DownloadFilterType::kVideo));
  EXPECT_FALSE(IsDownloadFilterMatch("", DownloadFilterType::kAudio));
  EXPECT_FALSE(IsDownloadFilterMatch("", DownloadFilterType::kImage));
  EXPECT_FALSE(IsDownloadFilterMatch("", DownloadFilterType::kDocument));
  EXPECT_TRUE(IsDownloadFilterMatch("", DownloadFilterType::kAll));
}

TEST_F(DownloadFilterTest, TestIsDownloadFilterMatch_CaseInsensitive) {
  EXPECT_TRUE(IsDownloadFilterMatch("VIDEO/MP4", DownloadFilterType::kVideo));
  EXPECT_TRUE(IsDownloadFilterMatch("AUDIO/MP3", DownloadFilterType::kAudio));
  EXPECT_TRUE(IsDownloadFilterMatch("IMAGE/JPEG", DownloadFilterType::kImage));
  EXPECT_TRUE(
      IsDownloadFilterMatch("TEXT/PLAIN", DownloadFilterType::kDocument));
  EXPECT_TRUE(
      IsDownloadFilterMatch("APPLICATION/PDF", DownloadFilterType::kPDF));
}

}  // namespace
