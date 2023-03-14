// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/scene_util.h"

#import <UIKit/UIKit.h>

#import <algorithm>

#import "base/files/file_enumerator.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/ios/ios_util.h"
#import "base/time/time.h"
#import "ios/chrome/browser/sessions/scene_util_test_support.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Constants used in tests to construct path names.
const base::FilePath::CharType kRoot[] = FILE_PATH_LITERAL("root");
const char kName[] = "filename";
}

class SceneUtilTest : public PlatformTest {};

TEST_F(SceneUtilTest, SessionsDirectoryForDirectory) {
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("root/Sessions")),
            SessionsDirectoryForDirectory(base::FilePath(kRoot)));
}

TEST_F(SceneUtilTest, SessionPathForDirectory) {
  EXPECT_EQ(base::FilePath("root/filename"),
            SessionPathForDirectory(base::FilePath(kRoot), nil, kName));

  EXPECT_EQ(base::FilePath("root/filename"),
            SessionPathForDirectory(base::FilePath(kRoot), @"", kName));

  EXPECT_EQ(
      base::FilePath("root/Sessions/session-id/filename"),
      SessionPathForDirectory(base::FilePath(kRoot), @"session-id", kName));
}

TEST_F(SceneUtilTest, SessionIdentifierForScene) {
  NSString* identifier = [[NSUUID UUID] UUIDString];
  id scene = FakeSceneWithIdentifier(identifier);

  NSString* expected = @"{SyntheticIdentifier}";
  if (base::ios::IsMultipleScenesSupported())
    expected = identifier;

  EXPECT_NSEQ(expected, SessionIdentifierForScene(scene));
}
