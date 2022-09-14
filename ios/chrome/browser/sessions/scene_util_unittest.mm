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

// Creates a temporary directory with `filenames` files below. The file names
// may contain path separator in which case the whole directory structure will
// be created. The file themselves will be created empty with default access.
base::ScopedTempDir CreateScopedTempDirWithContent(
    std::vector<base::StringPiece> filenames) {
  base::ScopedTempDir temp_directory;
  if (!temp_directory.CreateUniqueTempDir())
    return temp_directory;

  for (const base::StringPiece filename : filenames) {
    base::FilePath path = temp_directory.GetPath().Append(filename);
    if (!base::CreateDirectory(path.DirName()))
      return base::ScopedTempDir();

    if (!base::WriteFile(path, ""))
      return base::ScopedTempDir();
  }

  return temp_directory;
}

// Returns the list of files recursively found below `directory`. The path will
// be relative to `directory`.
std::vector<base::FilePath> GetDirectoryContent(
    const base::FilePath& directory) {
  base::FileEnumerator enumerator(directory, /*recursive=*/true,
                                  base::FileEnumerator::FILES);

  std::vector<base::FilePath> filenames;
  while (true) {
    const base::FilePath filename = enumerator.Next();
    if (filename.empty())
      break;

    base::FilePath relative_filename;
    if (!directory.AppendRelativePath(filename, &relative_filename))
      return std::vector<base::FilePath>();

    filenames.push_back(relative_filename);
  }

  if (enumerator.GetError() != base::File::FILE_OK)
    return std::vector<base::FilePath>();

  std::sort(filenames.begin(), filenames.end());
  return filenames;
}

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

TEST_F(SceneUtilTest, MigrateSessionStorageForDirectory_SessionExists) {
  base::ScopedTempDir temp_directory = CreateScopedTempDirWithContent({
      "Sessions/session-id/Snapshots/1.png",
      "Sessions/session-id/Snapshots/2.png",
      "Sessions/session-id/session.plist",
  });

  ASSERT_TRUE(temp_directory.IsValid());

  const base::FilePath directory = temp_directory.GetPath();
  MigrateSessionStorageForDirectory(directory, @"session-id", nil);

  const std::vector<base::FilePath> expected = {
      base::FilePath(FILE_PATH_LITERAL("Sessions/session-id/Snapshots/1.png")),
      base::FilePath(FILE_PATH_LITERAL("Sessions/session-id/Snapshots/2.png")),
      base::FilePath(FILE_PATH_LITERAL("Sessions/session-id/session.plist")),
  };

  EXPECT_EQ(expected, GetDirectoryContent(directory));
}

TEST_F(SceneUtilTest, MigrateSessionStorageForDirectory_FromM86OrOlder) {
  base::ScopedTempDir temp_directory = CreateScopedTempDirWithContent({
      "Snapshots/1.png",
      "Snapshots/2.png",
      "session.plist",
  });

  ASSERT_TRUE(temp_directory.IsValid());

  const base::FilePath directory = temp_directory.GetPath();
  MigrateSessionStorageForDirectory(directory, @"session-id", nil);

  const std::vector<base::FilePath> expected = {
      base::FilePath(FILE_PATH_LITERAL("Sessions/session-id/Snapshots/1.png")),
      base::FilePath(FILE_PATH_LITERAL("Sessions/session-id/Snapshots/2.png")),
      base::FilePath(FILE_PATH_LITERAL("Sessions/session-id/session.plist")),
  };

  EXPECT_EQ(expected, GetDirectoryContent(directory));
}

TEST_F(SceneUtilTest, MigrateSessionStorageForDirectory_ToMultiWindow) {
  // The test can only be run on a device that supports multiple scenes.
  if (!base::ios::IsMultipleScenesSupported())
    return;

  base::ScopedTempDir temp_directory = CreateScopedTempDirWithContent({
      "Sessions/{SyntheticIdentifier}/Snapshots/1.png",
      "Sessions/{SyntheticIdentifier}/Snapshots/2.png",
      "Sessions/{SyntheticIdentifier}/session.plist",
  });

  ASSERT_TRUE(temp_directory.IsValid());

  const base::FilePath directory = temp_directory.GetPath();
  MigrateSessionStorageForDirectory(directory, @"session-id", nil);

  const std::vector<base::FilePath> expected = {
      base::FilePath(FILE_PATH_LITERAL("Sessions/session-id/Snapshots/1.png")),
      base::FilePath(FILE_PATH_LITERAL("Sessions/session-id/Snapshots/2.png")),
      base::FilePath(FILE_PATH_LITERAL("Sessions/session-id/session.plist")),
  };

  EXPECT_EQ(expected, GetDirectoryContent(directory));
}

TEST_F(SceneUtilTest, MigrateSessionStorageForDirectory_FromMultiWindow) {
  // The test can only be run on a device that does not support multiple scenes.
  if (base::ios::IsMultipleScenesSupported())
    return;

  base::ScopedTempDir temp_directory = CreateScopedTempDirWithContent({
      "Sessions/previous-id/Snapshots/1.png",
      "Sessions/previous-id/Snapshots/2.png",
      "Sessions/previous-id/session.plist",
  });

  ASSERT_TRUE(temp_directory.IsValid());

  const base::FilePath directory = temp_directory.GetPath();
  MigrateSessionStorageForDirectory(directory, @"session-id", @"previous-id");

  const std::vector<base::FilePath> expected = {
      base::FilePath(FILE_PATH_LITERAL("Sessions/session-id/Snapshots/1.png")),
      base::FilePath(FILE_PATH_LITERAL("Sessions/session-id/Snapshots/2.png")),
      base::FilePath(FILE_PATH_LITERAL("Sessions/session-id/session.plist")),
  };

  EXPECT_EQ(expected, GetDirectoryContent(directory));
}

TEST_F(SceneUtilTest, MigrateSessionStorageForDirectory_NothingToMigrate) {
  base::ScopedTempDir temp_directory = CreateScopedTempDirWithContent({});
  ASSERT_TRUE(temp_directory.IsValid());

  const base::FilePath directory = temp_directory.GetPath();
  const base::FilePath session_directory =
      SessionsDirectoryForDirectory(directory).Append(
          FILE_PATH_LITERAL("session-id"));

  ASSERT_FALSE(base::DirectoryExists(session_directory));
  MigrateSessionStorageForDirectory(directory, @"session-id", nil);

  EXPECT_TRUE(base::DirectoryExists(session_directory));

  const std::vector<base::FilePath> expected = {};
  EXPECT_EQ(expected, GetDirectoryContent(directory));
}
