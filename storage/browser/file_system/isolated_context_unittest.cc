// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "storage/browser/file_system/isolated_context.h"

#include <stddef.h>

#include <string>

#include "base/containers/contains.h"
#include "storage/browser/file_system/file_system_url.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

#define FPL(x) FILE_PATH_LITERAL(x)

#if defined(FILE_PATH_USES_DRIVE_LETTERS)
#define DRIVE FPL("C:")
#else
#define DRIVE
#endif

namespace storage {

using FileInfo = IsolatedContext::MountPointInfo;

namespace {

const base::FilePath kTestPaths[] = {
    base::FilePath(DRIVE FPL("/a/b.txt")),
    base::FilePath(DRIVE FPL("/c/d/e")),
    base::FilePath(DRIVE FPL("/h/")),
    base::FilePath(DRIVE FPL("/")),
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
    base::FilePath(DRIVE FPL("\\foo\\bar")),
    base::FilePath(DRIVE FPL("\\")),
#endif
    // For duplicated base name test.
    base::FilePath(DRIVE FPL("/")),
    base::FilePath(DRIVE FPL("/f/e")),
    base::FilePath(DRIVE FPL("/f/b.txt")),
};

}  // namespace

class IsolatedContextTest : public testing::Test {
 public:
  IsolatedContextTest() {
    for (const auto& path : kTestPaths)
      fileset_.insert(path.NormalizePathSeparators());
  }

  IsolatedContextTest(const IsolatedContextTest&) = delete;
  IsolatedContextTest& operator=(const IsolatedContextTest&) = delete;

  void SetUp() override {
    IsolatedContext::FileInfoSet files;
    for (const auto& path : kTestPaths) {
      std::string name;
      ASSERT_TRUE(files.AddPath(path.NormalizePathSeparators(), &name));
      names_.push_back(name);
    }
    id_ = IsolatedContext::GetInstance()->RegisterDraggedFileSystem(files);
    IsolatedContext::GetInstance()->AddReference(id_);
    ASSERT_FALSE(id_.empty());
  }

  void TearDown() override {
    IsolatedContext::GetInstance()->RemoveReference(id_);
  }

  IsolatedContext* isolated_context() const {
    return IsolatedContext::GetInstance();
  }

 protected:
  std::string id_;
  std::multiset<base::FilePath> fileset_;
  std::vector<std::string> names_;
};

TEST_F(IsolatedContextTest, RegisterAndRevokeTest) {
  // See if the returned top-level entries match with what we registered.
  std::vector<FileInfo> toplevels;
  ASSERT_TRUE(isolated_context()->GetDraggedFileInfo(id_, &toplevels));
  ASSERT_EQ(fileset_.size(), toplevels.size());
  for (const auto& toplevel : toplevels) {
    ASSERT_TRUE(base::Contains(fileset_, toplevel.path));
  }

  // See if the name of each registered kTestPaths (that is what we
  // register in SetUp() by RegisterDraggedFileSystem) is properly cracked as
  // a valid virtual path in the isolated filesystem.
  for (size_t i = 0; i < std::size(kTestPaths); ++i) {
    base::FilePath virtual_path =
        isolated_context()->CreateVirtualRootPath(id_).AppendASCII(names_[i]);
    std::string cracked_id;
    base::FilePath cracked_path;
    std::string cracked_inner_id;
    FileSystemType cracked_type;
    FileSystemMountOption cracked_option;
    ASSERT_TRUE(isolated_context()->CrackVirtualPath(
        virtual_path, &cracked_id, &cracked_type, &cracked_inner_id,
        &cracked_path, &cracked_option));
    ASSERT_EQ(kTestPaths[i].NormalizePathSeparators().value(),
              cracked_path.value());
    ASSERT_EQ(id_, cracked_id);
    ASSERT_EQ(kFileSystemTypeDragged, cracked_type);
    EXPECT_TRUE(cracked_inner_id.empty());
  }

  // Make sure GetRegisteredPath returns false for id_ since it is
  // registered for dragged files.
  base::FilePath path;
  ASSERT_FALSE(isolated_context()->GetRegisteredPath(id_, &path));

  // Deref the current one and registering a new one.
  isolated_context()->RemoveReference(id_);

  IsolatedContext::ScopedFSHandle fs2 =
      isolated_context()->RegisterFileSystemForPath(
          kFileSystemTypeLocal, std::string(),
          base::FilePath(DRIVE FPL("/foo")), nullptr);

  // Make sure the GetDraggedFileInfo returns false for both ones.
  ASSERT_FALSE(isolated_context()->GetDraggedFileInfo(fs2.id(), &toplevels));
  ASSERT_FALSE(isolated_context()->GetDraggedFileInfo(id_, &toplevels));

  // Make sure the GetRegisteredPath returns true only for the new one.
  ASSERT_FALSE(isolated_context()->GetRegisteredPath(id_, &path));
  ASSERT_TRUE(isolated_context()->GetRegisteredPath(fs2.id(), &path));

  // Try registering three more file systems for the same path as id2.
  IsolatedContext::ScopedFSHandle fs3 =
      isolated_context()->RegisterFileSystemForPath(
          kFileSystemTypeLocal, std::string(), path, nullptr);
  IsolatedContext::ScopedFSHandle fs4 =
      isolated_context()->RegisterFileSystemForPath(
          kFileSystemTypeLocal, std::string(), path, nullptr);
  IsolatedContext::ScopedFSHandle fs5 =
      isolated_context()->RegisterFileSystemForPath(
          kFileSystemTypeLocal, std::string(), path, nullptr);

  // Remove file system for id4.
  fs4 = IsolatedContext::ScopedFSHandle();

  // Only id4 should become invalid now.
  ASSERT_TRUE(isolated_context()->GetRegisteredPath(fs2.id(), &path));
  ASSERT_TRUE(isolated_context()->GetRegisteredPath(fs3.id(), &path));
  ASSERT_FALSE(isolated_context()->GetRegisteredPath(fs4.id(), &path));
  ASSERT_TRUE(isolated_context()->GetRegisteredPath(fs5.id(), &path));

  // Revoke file system id5, after adding multiple references.
  isolated_context()->AddReference(fs5.id());
  isolated_context()->AddReference(fs5.id());
  isolated_context()->AddReference(fs5.id());
  isolated_context()->RevokeFileSystem(fs5.id());

  // No matter how many references we add id5 must be invalid now.
  ASSERT_TRUE(isolated_context()->GetRegisteredPath(fs2.id(), &path));
  ASSERT_TRUE(isolated_context()->GetRegisteredPath(fs3.id(), &path));
  ASSERT_FALSE(isolated_context()->GetRegisteredPath(fs4.id(), &path));
  ASSERT_FALSE(isolated_context()->GetRegisteredPath(fs5.id(), &path));

  // Revoke the file systems by path.
  isolated_context()->RevokeFileSystemByPath(path);

  // Now all the file systems associated to the path must be invalid.
  ASSERT_FALSE(isolated_context()->GetRegisteredPath(fs2.id(), &path));
  ASSERT_FALSE(isolated_context()->GetRegisteredPath(fs3.id(), &path));
  ASSERT_FALSE(isolated_context()->GetRegisteredPath(fs4.id(), &path));
  ASSERT_FALSE(isolated_context()->GetRegisteredPath(fs5.id(), &path));
}

TEST_F(IsolatedContextTest, CrackWithRelativePaths) {
  const struct {
    base::FilePath::StringType path;
    bool valid;
  } relatives[] = {
    {FPL("foo"), true},
    {FPL("foo/bar"), true},
    {FPL(".."), false},
    {FPL("foo/.."), false},
    {FPL("foo/../bar"), false},
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
#define SHOULD_FAIL_WITH_WIN_SEPARATORS false
#else
#define SHOULD_FAIL_WITH_WIN_SEPARATORS true
#endif
    {FPL("foo\\..\\baz"), SHOULD_FAIL_WITH_WIN_SEPARATORS},
    {FPL("foo/..\\baz"), SHOULD_FAIL_WITH_WIN_SEPARATORS},
  };

  for (size_t i = 0; i < std::size(kTestPaths); ++i) {
    for (size_t j = 0; j < std::size(relatives); ++j) {
      SCOPED_TRACE(testing::Message() << "Testing " << kTestPaths[i].value()
                                      << " " << relatives[j].path);
      base::FilePath virtual_path = isolated_context()
                                        ->CreateVirtualRootPath(id_)
                                        .AppendASCII(names_[i])
                                        .Append(relatives[j].path);
      std::string cracked_id;
      base::FilePath cracked_path;
      FileSystemType cracked_type;
      std::string cracked_inner_id;
      FileSystemMountOption cracked_option;
      if (!relatives[j].valid) {
        ASSERT_FALSE(isolated_context()->CrackVirtualPath(
            virtual_path, &cracked_id, &cracked_type, &cracked_inner_id,
            &cracked_path, &cracked_option));
        continue;
      }
      ASSERT_TRUE(isolated_context()->CrackVirtualPath(
          virtual_path, &cracked_id, &cracked_type, &cracked_inner_id,
          &cracked_path, &cracked_option));
      ASSERT_EQ(kTestPaths[i]
                    .Append(relatives[j].path)
                    .NormalizePathSeparators()
                    .value(),
                cracked_path.value());
      ASSERT_EQ(id_, cracked_id);
      ASSERT_EQ(kFileSystemTypeDragged, cracked_type);
      EXPECT_TRUE(cracked_inner_id.empty());
    }
  }
}

TEST_F(IsolatedContextTest, CrackURLWithRelativePaths) {
  const struct {
    base::FilePath::StringType path;
    bool valid;
  } relatives[] = {
    {FPL("foo"), true},
    {FPL("foo/bar"), true},
    {FPL(".."), false},
    {FPL("foo/.."), false},
    {FPL("foo/../bar"), false},
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
#define SHOULD_FAIL_WITH_WIN_SEPARATORS false
#else
#define SHOULD_FAIL_WITH_WIN_SEPARATORS true
#endif
    {FPL("foo\\..\\baz"), SHOULD_FAIL_WITH_WIN_SEPARATORS},
    {FPL("foo/..\\baz"), SHOULD_FAIL_WITH_WIN_SEPARATORS},
  };

  for (size_t i = 0; i < std::size(kTestPaths); ++i) {
    for (size_t j = 0; j < std::size(relatives); ++j) {
      SCOPED_TRACE(testing::Message() << "Testing " << kTestPaths[i].value()
                                      << " " << relatives[j].path);
      base::FilePath virtual_path = isolated_context()
                                        ->CreateVirtualRootPath(id_)
                                        .AppendASCII(names_[i])
                                        .Append(relatives[j].path);

      FileSystemURL cracked = isolated_context()->CreateCrackedFileSystemURL(
          blink::StorageKey::CreateFromStringForTesting("http://chromium.org"),
          kFileSystemTypeIsolated, virtual_path);

      ASSERT_EQ(relatives[j].valid, cracked.is_valid());

      if (!relatives[j].valid)
        continue;
      ASSERT_EQ("http://chromium.org", cracked.origin().Serialize());
      ASSERT_EQ(kTestPaths[i]
                    .Append(relatives[j].path)
                    .NormalizePathSeparators()
                    .value(),
                cracked.path().value());
      ASSERT_EQ(virtual_path.NormalizePathSeparators(), cracked.virtual_path());
      ASSERT_EQ(id_, cracked.filesystem_id());
      ASSERT_EQ(kFileSystemTypeDragged, cracked.type());
      ASSERT_EQ(kFileSystemTypeIsolated, cracked.mount_type());
    }
  }
}

TEST_F(IsolatedContextTest, TestWithVirtualRoot) {
  std::string cracked_id;
  base::FilePath cracked_path;
  FileSystemMountOption cracked_option;

  // Trying to crack virtual root "/" returns true but with empty cracked path
  // as "/" of the isolated filesystem is a pure virtual directory
  // that has no corresponding platform directory.
  base::FilePath virtual_path = isolated_context()->CreateVirtualRootPath(id_);
  ASSERT_TRUE(isolated_context()->CrackVirtualPath(
      virtual_path, &cracked_id, nullptr, nullptr, &cracked_path,
      &cracked_option));
  ASSERT_EQ(FPL(""), cracked_path.value());
  ASSERT_EQ(id_, cracked_id);

  // Trying to crack "/foo" should fail (because "foo" is not the one
  // included in the kTestPaths).
  virtual_path =
      isolated_context()->CreateVirtualRootPath(id_).AppendASCII("foo");
  ASSERT_FALSE(isolated_context()->CrackVirtualPath(
      virtual_path, &cracked_id, nullptr, nullptr, &cracked_path,
      &cracked_option));
}

TEST_F(IsolatedContextTest, CanHandleURL) {
  const GURL test_origin("http://chromium.org");
  const base::FilePath test_path(FPL("/mount"));

  // Should handle isolated file system.
  EXPECT_TRUE(
      isolated_context()->HandlesFileSystemMountType(kFileSystemTypeIsolated));

  // Shouldn't handle the rest.
  EXPECT_FALSE(
      isolated_context()->HandlesFileSystemMountType(kFileSystemTypeExternal));
  EXPECT_FALSE(
      isolated_context()->HandlesFileSystemMountType(kFileSystemTypeTemporary));
  EXPECT_FALSE(isolated_context()->HandlesFileSystemMountType(
      kFileSystemTypePersistent));
  EXPECT_FALSE(
      isolated_context()->HandlesFileSystemMountType(kFileSystemTypeTest));
  // Not even if it's isolated subtype.
  EXPECT_FALSE(
      isolated_context()->HandlesFileSystemMountType(kFileSystemTypeLocal));
  EXPECT_FALSE(
      isolated_context()->HandlesFileSystemMountType(kFileSystemTypeDragged));
  EXPECT_FALSE(isolated_context()->HandlesFileSystemMountType(
      kFileSystemTypeLocalMedia));
  EXPECT_FALSE(isolated_context()->HandlesFileSystemMountType(
      kFileSystemTypeDeviceMedia));
}

TEST_F(IsolatedContextTest, VirtualFileSystemTests) {
  // Should be able to register empty and non-absolute paths
  std::string empty_fsid = isolated_context()->RegisterFileSystemForVirtualPath(
      kFileSystemTypeIsolated, "_", base::FilePath());
  std::string relative_fsid =
      isolated_context()->RegisterFileSystemForVirtualPath(
          kFileSystemTypeIsolated, "_", base::FilePath(FPL("relpath")));
  ASSERT_FALSE(empty_fsid.empty());
  ASSERT_FALSE(relative_fsid.empty());

  // Make sure that filesystem root is not prepended to cracked virtual paths.
  base::FilePath database_root = base::FilePath(DRIVE FPL("/database_path"));
  std::string database_fsid =
      isolated_context()->RegisterFileSystemForVirtualPath(
          kFileSystemTypeIsolated, "_", database_root);

  base::FilePath test_virtual_path =
      base::FilePath().AppendASCII("virtualdir").AppendASCII("virtualfile.txt");

  base::FilePath whole_virtual_path = isolated_context()
                                          ->CreateVirtualRootPath(database_fsid)
                                          .AppendASCII("_")
                                          .Append(test_virtual_path);

  std::string cracked_id;
  base::FilePath cracked_path;
  std::string cracked_inner_id;
  FileSystemMountOption cracked_option;
  ASSERT_TRUE(isolated_context()->CrackVirtualPath(
      whole_virtual_path, &cracked_id, nullptr, &cracked_inner_id,
      &cracked_path, &cracked_option));
  ASSERT_EQ(database_fsid, cracked_id);
  ASSERT_EQ(test_virtual_path, cracked_path);
  EXPECT_TRUE(cracked_inner_id.empty());
}

}  // namespace storage
