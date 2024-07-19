// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "storage/browser/file_system/external_mount_points.h"

#include <stddef.h>

#include <string>

#include "base/files/file_path.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_mount_option.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

#define FPL FILE_PATH_LITERAL

#if defined(FILE_PATH_USES_DRIVE_LETTERS)
#define DRIVE FPL("C:")
#else
#define DRIVE
#endif

class GURL;

namespace storage {

TEST(ExternalMountPointsTest, AddMountPoint) {
  scoped_refptr<ExternalMountPoints> mount_points =
      ExternalMountPoints::CreateRefCounted();

  struct TestCase {
    // The mount point's name.
    const char* const name;
    // The mount point's path.
    const base::FilePath::CharType* const path;
    // Whether the mount point registration should succeed.
    bool success;
    // Path returned by GetRegisteredPath. nullptr if the method is expected to
    // fail.
    const base::FilePath::CharType* const registered_path;
  };

  const TestCase kTestCases[] = {
    // Valid mount point.
    {"test", DRIVE FPL("/foo/test"), true, DRIVE FPL("/foo/test")},
    // Valid mount point with only one path component.
    {"bbb", DRIVE FPL("/bbb"), true, DRIVE FPL("/bbb")},
    // Existing mount point path is substring of the mount points path.
    {"test11", DRIVE FPL("/foo/test11"), true, DRIVE FPL("/foo/test11")},
    // Path substring of an existing path.
    {"test1", DRIVE FPL("/foo/test1"), true, DRIVE FPL("/foo/test1")},
    // Empty mount point name and path.
    {"", DRIVE FPL(""), false, nullptr},
    // Empty mount point name.
    {"", DRIVE FPL("/ddd"), false, nullptr},
    // Empty mount point path.
    {"empty_path", FPL(""), true, FPL("")},
    // Name different from path's base name.
    {"not_base_name", DRIVE FPL("/x/y/z"), true, DRIVE FPL("/x/y/z")},
    // References parent.
    {"invalid", DRIVE FPL("../foo/invalid"), false, nullptr},
    // Relative path.
    {"relative", DRIVE FPL("foo/relative"), false, nullptr},
    // Existing mount point path.
    {"path_exists", DRIVE FPL("/foo/test"), false, nullptr},
    // Mount point with the same name exists.
    {"test", DRIVE FPL("/foo/a/test_name_exists"), false,
     DRIVE FPL("/foo/test")},
    // Child of an existing mount point.
    {"a1", DRIVE FPL("/foo/test/a"), false, nullptr},
    // Parent of an existing mount point.
    {"foo1", DRIVE FPL("/foo"), false, nullptr},
    // Bit bigger depth.
    {"g", DRIVE FPL("/foo/a/b/c/d/e/f/g"), true,
     DRIVE FPL("/foo/a/b/c/d/e/f/g")},
    // Sibling mount point (with similar name) exists.
    {"ff", DRIVE FPL("/foo/a/b/c/d/e/ff"), true,
     DRIVE FPL("/foo/a/b/c/d/e/ff")},
    // Lexicographically last among existing mount points.
    {"yyy", DRIVE FPL("/zzz/yyy"), true, DRIVE FPL("/zzz/yyy")},
    // Parent of the lexicographically last mount point.
    {"zzz1", DRIVE FPL("/zzz"), false, nullptr},
    // Child of the lexicographically last mount point.
    {"xxx1", DRIVE FPL("/zzz/yyy/xxx"), false, nullptr},
    // Lexicographically first among existing mount points.
    {"b", DRIVE FPL("/a/b"), true, DRIVE FPL("/a/b")},
    // Parent of lexicographically first mount point.
    {"a2", DRIVE FPL("/a"), false, nullptr},
    // Child of lexicographically last mount point.
    {"c1", DRIVE FPL("/a/b/c"), false, nullptr},
    // Parent to all of the mount points.
    {"root", DRIVE FPL("/"), false, nullptr},
    // Path contains .. component.
    {"funky", DRIVE FPL("/tt/fun/../funky"), false, nullptr},
  // Windows separators.
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
    {"win", DRIVE FPL("\\try\\separators\\win"), true,
     DRIVE FPL("\\try\\separators\\win")},
    {"win1", DRIVE FPL("\\try/separators\\win1"), true,
     DRIVE FPL("\\try/separators\\win1")},
    {"win2", DRIVE FPL("\\try/separators\\win"), false, nullptr},
#else
    {"win", DRIVE FPL("\\separators\\win"), false, nullptr},
    {"win1", DRIVE FPL("\\try/separators\\win1"), false, nullptr},
#endif
    // Win separators, but relative path.
    {"win2", DRIVE FPL("try\\separators\\win2"), false, nullptr},
  };

  // Test adding mount points.
  for (const auto& test : kTestCases) {
    EXPECT_EQ(test.success,
              mount_points->RegisterFileSystem(test.name, kFileSystemTypeLocal,
                                               FileSystemMountOption(),
                                               base::FilePath(test.path)))
        << "Adding mount point: " << test.name << " with path " << test.path;
  }

  // Test that final mount point presence state is as expected.
  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    base::FilePath found_path;
    EXPECT_EQ(kTestCases[i].registered_path != nullptr,
              mount_points->GetRegisteredPath(kTestCases[i].name, &found_path))
        << "Test case: " << i;

    if (kTestCases[i].registered_path) {
      base::FilePath expected_path(kTestCases[i].registered_path);
      EXPECT_EQ(expected_path.NormalizePathSeparators(), found_path);
    }
  }
}

TEST(ExternalMountPointsTest, GetVirtualPath) {
  scoped_refptr<ExternalMountPoints> mount_points =
      ExternalMountPoints::CreateRefCounted();

  mount_points->RegisterFileSystem("c", kFileSystemTypeLocal,
                                   FileSystemMountOption(),
                                   base::FilePath(DRIVE FPL("/a/b/c")));
  // Note that "/a/b/c" < "/a/b/c(1)" < "/a/b/c/".
  mount_points->RegisterFileSystem("c(1)", kFileSystemTypeLocal,
                                   FileSystemMountOption(),
                                   base::FilePath(DRIVE FPL("/a/b/c(1)")));
  mount_points->RegisterFileSystem("x", kFileSystemTypeLocal,
                                   FileSystemMountOption(),
                                   base::FilePath(DRIVE FPL("/z/y/x")));
  mount_points->RegisterFileSystem("o", kFileSystemTypeLocal,
                                   FileSystemMountOption(),
                                   base::FilePath(DRIVE FPL("/m/n/o")));
  // A mount point whose name does not match its path base name.
  mount_points->RegisterFileSystem("mount", kFileSystemTypeLocal,
                                   FileSystemMountOption(),
                                   base::FilePath(DRIVE FPL("/root/foo")));
  // A mount point with an empty path.
  mount_points->RegisterFileSystem("empty_path", kFileSystemTypeLocal,
                                   FileSystemMountOption(), base::FilePath());

  struct TestCase {
    const base::FilePath::CharType* const local_path;
    bool success;
    const base::FilePath::CharType* const virtual_path;
  };

  const TestCase kTestCases[] = {
    // Empty path.
    {FPL(""), false, FPL("")},
    // No registered mount point (but is parent to a mount point).
    {DRIVE FPL("/a/b"), false, FPL("")},
    // No registered mount point (but is parent to a mount point).
    {DRIVE FPL("/z/y"), false, FPL("")},
    // No registered mount point (but is parent to a mount point).
    {DRIVE FPL("/m/n"), false, FPL("")},
    // No registered mount point.
    {DRIVE FPL("/foo/mount"), false, FPL("")},
    // An existing mount point path is substring.
    {DRIVE FPL("/a/b/c1"), false, FPL("")},
    // No leading /.
    {DRIVE FPL("a/b/c"), false, FPL("")},
    // Sibling to a root path.
    {DRIVE FPL("/a/b/d/e"), false, FPL("")},
    // Sibling to a root path.
    {DRIVE FPL("/z/y/v/u"), false, FPL("")},
    // Sibling to a root path.
    {DRIVE FPL("/m/n/p/q"), false, FPL("")},
    // Mount point root path.
    {DRIVE FPL("/a/b/c"), true, FPL("c")},
    // Mount point root path.
    {DRIVE FPL("/z/y/x"), true, FPL("x")},
    // Mount point root path.
    {DRIVE FPL("/m/n/o"), true, FPL("o")},
    // Mount point child path.
    {DRIVE FPL("/a/b/c/d/e"), true, FPL("c/d/e")},
    // Mount point child path.
    {DRIVE FPL("/z/y/x/v/u"), true, FPL("x/v/u")},
    // Mount point child path.
    {DRIVE FPL("/m/n/o/p/q"), true, FPL("o/p/q")},
    // Name doesn't match mount point path base name.
    {DRIVE FPL("/root/foo/a/b/c"), true, FPL("mount/a/b/c")},
    {DRIVE FPL("/root/foo"), true, FPL("mount")},
    // Mount point contains character whose ASCII code is smaller than file path
    // separator's.
    {DRIVE FPL("/a/b/c(1)/d/e"), true, FPL("c(1)/d/e")},
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
    // Path with win separators mixed in.
    {DRIVE FPL("/a\\b\\c/d"), true, FPL("c/d")},
#endif
  };

  for (const auto& test_case : kTestCases) {
    // Initialize virtual path with a value.
    base::FilePath virtual_path(DRIVE FPL("/mount"));
    base::FilePath local_path(test_case.local_path);
    EXPECT_EQ(test_case.success,
              mount_points->GetVirtualPath(local_path, &virtual_path))
        << "Resolving " << test_case.local_path;

    // There are no guarantees for |virtual_path| value if |GetVirtualPath|
    // fails.
    if (!test_case.success)
      continue;

    base::FilePath expected_virtual_path(test_case.virtual_path);
    EXPECT_EQ(expected_virtual_path.NormalizePathSeparators(), virtual_path)
        << "Resolving " << test_case.local_path;
  }
}

TEST(ExternalMountPointsTest, HandlesFileSystemMountType) {
  scoped_refptr<ExternalMountPoints> mount_points =
      ExternalMountPoints::CreateRefCounted();

  const GURL test_origin("http://chromium.org");
  const base::FilePath test_path(FPL("/mount"));

  // Should handle External File System.
  EXPECT_TRUE(
      mount_points->HandlesFileSystemMountType(kFileSystemTypeExternal));

  // Shouldn't handle the rest.
  EXPECT_FALSE(
      mount_points->HandlesFileSystemMountType(kFileSystemTypeIsolated));
  EXPECT_FALSE(
      mount_points->HandlesFileSystemMountType(kFileSystemTypeTemporary));
  EXPECT_FALSE(
      mount_points->HandlesFileSystemMountType(kFileSystemTypePersistent));
  EXPECT_FALSE(mount_points->HandlesFileSystemMountType(kFileSystemTypeTest));
  // Not even if it's external subtype.
  EXPECT_FALSE(mount_points->HandlesFileSystemMountType(kFileSystemTypeLocal));
  EXPECT_FALSE(
      mount_points->HandlesFileSystemMountType(kFileSystemTypeDriveFs));
  EXPECT_FALSE(
      mount_points->HandlesFileSystemMountType(kFileSystemTypeSyncable));
}

TEST(ExternalMountPointsTest, CreateCrackedFileSystemURL) {
  scoped_refptr<ExternalMountPoints> mount_points =
      ExternalMountPoints::CreateRefCounted();

  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("http://chromium.org");

  mount_points->RegisterFileSystem("c", kFileSystemTypeLocal,
                                   FileSystemMountOption(),
                                   base::FilePath(DRIVE FPL("/a/b/c")));
  mount_points->RegisterFileSystem("c(1)", kFileSystemTypeDriveFs,
                                   FileSystemMountOption(),
                                   base::FilePath(DRIVE FPL("/a/b/c(1)")));
  mount_points->RegisterFileSystem("empty_path", kFileSystemTypeSyncable,
                                   FileSystemMountOption(), base::FilePath());
  mount_points->RegisterFileSystem("mount", kFileSystemTypeDriveFs,
                                   FileSystemMountOption(),
                                   base::FilePath(DRIVE FPL("/root")));

  // Try cracking invalid GURL.
  FileSystemURL invalid = mount_points->CrackURL(
      GURL("http://chromium.og"),
      blink::StorageKey::CreateFromStringForTesting("http://chromium.og"));
  EXPECT_FALSE(invalid.is_valid());

  // Try cracking isolated path.
  FileSystemURL isolated = mount_points->CreateCrackedFileSystemURL(
      kTestStorageKey, kFileSystemTypeIsolated, base::FilePath(FPL("c")));
  EXPECT_FALSE(isolated.is_valid());

  // Try native local which is not cracked.
  FileSystemURL native_local = mount_points->CreateCrackedFileSystemURL(
      kTestStorageKey, kFileSystemTypeLocal, base::FilePath(FPL("c")));
  EXPECT_FALSE(native_local.is_valid());

  struct TestCase {
    const base::FilePath::CharType* const path;
    bool expect_valid;
    FileSystemType expect_type;
    const base::FilePath::CharType* const expect_path;
    const char* const expect_fs_id;
  };

  const TestCase kTestCases[] = {
    {FPL("c/d/e"), true, kFileSystemTypeLocal, DRIVE FPL("/a/b/c/d/e"), "c"},
    {FPL("c(1)/d/e"), true, kFileSystemTypeDriveFs, DRIVE FPL("/a/b/c(1)/d/e"),
     "c(1)"},
    {FPL("c(1)"), true, kFileSystemTypeDriveFs, DRIVE FPL("/a/b/c(1)"), "c(1)"},
    {FPL("empty_path/a"), true, kFileSystemTypeSyncable, FPL("a"),
     "empty_path"},
    {FPL("empty_path"), true, kFileSystemTypeSyncable, FPL(""), "empty_path"},
    {FPL("mount/a/b"), true, kFileSystemTypeDriveFs, DRIVE FPL("/root/a/b"),
     "mount"},
    {FPL("mount"), true, kFileSystemTypeDriveFs, DRIVE FPL("/root"), "mount"},
    {FPL("cc"), false, kFileSystemTypeUnknown, FPL(""), ""},
    {FPL(""), false, kFileSystemTypeUnknown, FPL(""), ""},
    {FPL(".."), false, kFileSystemTypeUnknown, FPL(""), ""},
    // Absolute paths.
    {FPL("/c/d/e"), false, kFileSystemTypeUnknown, FPL(""), ""},
    {FPL("/c(1)/d/e"), false, kFileSystemTypeUnknown, FPL(""), ""},
    {FPL("/empty_path"), false, kFileSystemTypeUnknown, FPL(""), ""},
    // PAth references parent.
    {FPL("c/d/../e"), false, kFileSystemTypeUnknown, FPL(""), ""},
    {FPL("/empty_path/a/../b"), false, kFileSystemTypeUnknown, FPL(""), ""},
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
    {FPL("c/d\\e"), true, kFileSystemTypeLocal, DRIVE FPL("/a/b/c/d/e"), "c"},
    {FPL("mount\\a\\b"), true, kFileSystemTypeDriveFs, DRIVE FPL("/root/a/b"),
     "mount"},
#endif
  };

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    FileSystemURL cracked = mount_points->CreateCrackedFileSystemURL(
        kTestStorageKey, kFileSystemTypeExternal,
        base::FilePath(kTestCases[i].path));

    EXPECT_EQ(kTestCases[i].expect_valid, cracked.is_valid())
        << "Test case index: " << i;

    if (!kTestCases[i].expect_valid)
      continue;

    EXPECT_EQ(kTestStorageKey.origin(), cracked.origin())
        << "Test case index: " << i;
    EXPECT_EQ(kTestCases[i].expect_type, cracked.type())
        << "Test case index: " << i;
    EXPECT_EQ(
        base::FilePath(kTestCases[i].expect_path).NormalizePathSeparators(),
        cracked.path())
        << "Test case index: " << i;
    EXPECT_EQ(base::FilePath(kTestCases[i].path).NormalizePathSeparators(),
              cracked.virtual_path())
        << "Test case index: " << i;
    EXPECT_EQ(kTestCases[i].expect_fs_id, cracked.filesystem_id())
        << "Test case index: " << i;
    EXPECT_EQ(kFileSystemTypeExternal, cracked.mount_type())
        << "Test case index: " << i;
  }
}

TEST(ExternalMountPointsTest, CrackVirtualPath) {
  scoped_refptr<ExternalMountPoints> mount_points =
      ExternalMountPoints::CreateRefCounted();

  const GURL kTestOrigin("http://chromium.org");

  mount_points->RegisterFileSystem("c", kFileSystemTypeLocal,
                                   FileSystemMountOption(),
                                   base::FilePath(DRIVE FPL("/a/b/c")));
  mount_points->RegisterFileSystem("c(1)", kFileSystemTypeDriveFs,
                                   FileSystemMountOption(),
                                   base::FilePath(DRIVE FPL("/a/b/c(1)")));
  mount_points->RegisterFileSystem("empty_path", kFileSystemTypeSyncable,
                                   FileSystemMountOption(), base::FilePath());
  mount_points->RegisterFileSystem("mount", kFileSystemTypeDriveFs,
                                   FileSystemMountOption(),
                                   base::FilePath(DRIVE FPL("/root")));

  struct TestCase {
    const base::FilePath::CharType* const path;
    bool expect_valid;
    FileSystemType expect_type;
    const base::FilePath::CharType* const expect_path;
    const char* const expect_name;
  };

  const TestCase kTestCases[] = {
    {FPL("c/d/e"), true, kFileSystemTypeLocal, DRIVE FPL("/a/b/c/d/e"), "c"},
    {FPL("c(1)/d/e"), true, kFileSystemTypeDriveFs, DRIVE FPL("/a/b/c(1)/d/e"),
     "c(1)"},
    {FPL("c(1)"), true, kFileSystemTypeDriveFs, DRIVE FPL("/a/b/c(1)"), "c(1)"},
    {FPL("empty_path/a"), true, kFileSystemTypeSyncable, FPL("a"),
     "empty_path"},
    {FPL("empty_path"), true, kFileSystemTypeSyncable, FPL(""), "empty_path"},
    {FPL("mount/a/b"), true, kFileSystemTypeDriveFs, DRIVE FPL("/root/a/b"),
     "mount"},
    {FPL("mount"), true, kFileSystemTypeDriveFs, DRIVE FPL("/root"), "mount"},
    {FPL("cc"), false, kFileSystemTypeUnknown, FPL(""), ""},
    {FPL(""), false, kFileSystemTypeUnknown, FPL(""), ""},
    {FPL(".."), false, kFileSystemTypeUnknown, FPL(""), ""},
    // Absolute paths.
    {FPL("/c/d/e"), false, kFileSystemTypeUnknown, FPL(""), ""},
    {FPL("/c(1)/d/e"), false, kFileSystemTypeUnknown, FPL(""), ""},
    {FPL("/empty_path"), false, kFileSystemTypeUnknown, FPL(""), ""},
    // PAth references parent.
    {FPL("c/d/../e"), false, kFileSystemTypeUnknown, FPL(""), ""},
    {FPL("/empty_path/a/../b"), false, kFileSystemTypeUnknown, FPL(""), ""},
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
    {FPL("c/d\\e"), true, kFileSystemTypeLocal, DRIVE FPL("/a/b/c/d/e"), "c"},
    {FPL("mount\\a\\b"), true, kFileSystemTypeDriveFs, DRIVE FPL("/root/a/b"),
     "mount"},
#endif
  };

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    std::string cracked_name;
    FileSystemType cracked_type;
    std::string cracked_id;
    base::FilePath cracked_path;
    FileSystemMountOption cracked_option;
    EXPECT_EQ(kTestCases[i].expect_valid,
              mount_points->CrackVirtualPath(
                  base::FilePath(kTestCases[i].path), &cracked_name,
                  &cracked_type, &cracked_id, &cracked_path, &cracked_option))
        << "Test case index: " << i;

    if (!kTestCases[i].expect_valid)
      continue;

    EXPECT_EQ(kTestCases[i].expect_type, cracked_type)
        << "Test case index: " << i;
    EXPECT_EQ(
        base::FilePath(kTestCases[i].expect_path).NormalizePathSeparators(),
        cracked_path)
        << "Test case index: " << i;
    EXPECT_EQ(kTestCases[i].expect_name, cracked_name)
        << "Test case index: " << i;
    // As of now we don't mount other filesystems with non-empty filesystem_id
    // onto external mount points.
    EXPECT_TRUE(cracked_id.empty()) << "Test case index: " << i;
  }
}

TEST(ExternalMountPointsTest, MountOption) {
  scoped_refptr<ExternalMountPoints> mount_points =
      ExternalMountPoints::CreateRefCounted();

  mount_points->RegisterFileSystem(
      "nosync", kFileSystemTypeLocal,
      FileSystemMountOption(FlushPolicy::NO_FLUSH_ON_COMPLETION),
      base::FilePath(DRIVE FPL("/nosync")));
  mount_points->RegisterFileSystem(
      "sync", kFileSystemTypeLocal,
      FileSystemMountOption(FlushPolicy::FLUSH_ON_COMPLETION),
      base::FilePath(DRIVE FPL("/sync")));

  std::string name;
  FileSystemType type;
  std::string cracked_id;
  FileSystemMountOption option;
  base::FilePath path;
  EXPECT_TRUE(mount_points->CrackVirtualPath(base::FilePath(FPL("nosync/file")),
                                             &name, &type, &cracked_id, &path,
                                             &option));
  EXPECT_EQ(FlushPolicy::NO_FLUSH_ON_COMPLETION, option.flush_policy());
  EXPECT_TRUE(mount_points->CrackVirtualPath(base::FilePath(FPL("sync/file")),
                                             &name, &type, &cracked_id, &path,
                                             &option));
  EXPECT_EQ(FlushPolicy::FLUSH_ON_COMPLETION, option.flush_policy());
}

}  // namespace storage
