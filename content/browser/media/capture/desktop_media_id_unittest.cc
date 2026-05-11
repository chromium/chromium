// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/desktop_media_id.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(DesktopMediaIDTest, ParsePlatformNative) {
  DesktopMediaID id = DesktopMediaID::Parse("window:1:2");
  EXPECT_EQ(DesktopMediaID::TYPE_WINDOW, id.type);
  EXPECT_EQ(1, id.id);
  EXPECT_EQ(2, id.window_id);
  EXPECT_EQ(DesktopMediaID::IdType::kPlatformNative, id.id_type);
}

TEST(DesktopMediaIDTest, ParseNativePickerSession) {
  DesktopMediaID id = DesktopMediaID::Parse("window:1:2:s");
  EXPECT_EQ(DesktopMediaID::TYPE_WINDOW, id.type);
  EXPECT_EQ(1, id.id);
  EXPECT_EQ(2, id.window_id);
  EXPECT_EQ(DesktopMediaID::IdType::kNativePickerSession, id.id_type);
}

TEST(DesktopMediaIDTest, ToStringPlatformNative) {
  DesktopMediaID id(DesktopMediaID::TYPE_WINDOW, 1);
  id.window_id = 2;
  id.id_type = DesktopMediaID::IdType::kPlatformNative;
  EXPECT_EQ("window:1:2", id.ToString());
}

TEST(DesktopMediaIDTest, ToStringNativePickerSession) {
  DesktopMediaID id(DesktopMediaID::TYPE_WINDOW, 1);
  id.window_id = 2;
  id.id_type = DesktopMediaID::IdType::kNativePickerSession;
  EXPECT_EQ("window:1:2:s", id.ToString());
}

TEST(DesktopMediaIDTest, ParseInvalid) {
  EXPECT_EQ(DesktopMediaID::TYPE_NONE, DesktopMediaID::Parse("invalid").type);
  EXPECT_EQ(DesktopMediaID::TYPE_NONE, DesktopMediaID::Parse("window:1").type);
  EXPECT_EQ(DesktopMediaID::TYPE_NONE,
            DesktopMediaID::Parse("window:1:2:x").type);
}

}  // namespace content
