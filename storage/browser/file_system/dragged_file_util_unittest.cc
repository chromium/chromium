// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"
#include "storage/browser/file_system/dragged_file_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/browser/file_system/local_file_util.h"
#include "storage/browser/file_system/native_file_util.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/file_system_test_file_set.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::AsyncFileTestHelper;
using storage::FileSystemContext;
using storage::FileSystemOperationContext;
using storage::FileSystemType;
using storage::FileSystemURL;

namespace content {

namespace {

using FileEntryList = AsyncFileTestHelper::FileEntryList;

// Used in DraggedFileUtilTest::SimulateDropFiles().
// Random root paths in which we create each file/directory of the
// RegularTestCases (so that we can simulate a drop with files/directories
// from multiple directories).
constexpr const base::FilePath::CharType* kRootPaths[] = {
    FILE_PATH_LITERAL("a"),
    FILE_PATH_LITERAL("b/c"),
    FILE_PATH_LITERAL("etc"),
};

base::FilePath GetTopLevelPath(const base::FilePath& path) {
  std::vector<base::FilePath::StringType> components;
  path.GetComponents(&components);
  return base::FilePath(components[0]);
}

bool IsDirectoryEmpty(FileSystemContext* context, const FileSystemURL& url) {
  FileEntryList entries;
  EXPECT_EQ(base::File::FILE_OK,
            AsyncFileTestHelper::ReadDirectory(context, url, &entries));
  return entries.empty();
}

FileSystemURL GetEntryURL(FileSystemContext* file_system_context,
                          const FileSystemURL& dir,
                          const base::FilePath::StringType& name) {
  return file_system_context->CreateCrackedFileSystemURL(
      dir.origin().GetURL(), dir.mount_type(), dir.virtual_path().Append(name));
}

base::FilePath GetRelativeVirtualPath(const FileSystemURL& root,
                                      const FileSystemURL& url) {
  if (root.virtual_path().empty())
    return url.virtual_path();
  base::FilePath relative;
  const bool success =
      root.virtual_path().AppendRelativePath(url.virtual_path(), &relative);
  DCHECK(success);
  return relative;
}

FileSystemURL GetOtherURL(FileSystemContext* file_system_context,
                          const FileSystemURL& root,
                          const FileSystemURL& other_root,
                          const FileSystemURL& url) {
  return file_system_context->CreateCrackedFileSystemURL(
      other_root.origin().GetURL(), other_root.mount_type(),
      other_root.virtual_path().Append(GetRelativeVirtualPath(root, url)));
}

}  // namespace

class DraggedFileUtilTest : public testing::Test {
 public:
  DraggedFileUtilTest() = default;

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(partition_dir_.CreateUniqueTempDir());
    file_util_.reset(new storage::DraggedFileUtil());

    // Register the files/directories of RegularTestCases (with random
    // root paths) as dropped files.
    SimulateDropFiles();

    file_system_context_ = CreateFileSystemContextForTesting(
        nullptr /* quota_manager */, partition_dir_.GetPath());

    isolated_context()->AddReference(filesystem_id_);
  }

  void TearDown() override {
    isolated_context()->RemoveReference(filesystem_id_);
  }

 protected:
  storage::IsolatedContext* isolated_context() const {
    return storage::IsolatedContext::GetInstance();
  }
  const base::FilePath& root_path() const { return data_dir_.GetPath(); }
  FileSystemContext* file_system_context() const {
    return file_system_context_.get();
  }
  storage::FileSystemFileUtil* file_util() const { return file_util_.get(); }
  std::string filesystem_id() const { return filesystem_id_; }

  base::FilePath GetTestCasePlatformPath(
      const base::FilePath::StringType& path) {
    return toplevel_root_map_[GetTopLevelPath(base::FilePath(path))]
        .Append(path)
        .NormalizePathSeparators();
  }

  base::FilePath GetTestCaseLocalPath(const base::FilePath& path) {
    base::FilePath relative;
    if (data_dir_.GetPath().AppendRelativePath(path, &relative))
      return relative;
    return path;
  }

  FileSystemURL GetFileSystemURL(const base::FilePath& path) const {
    base::FilePath virtual_path =
        isolated_context()->CreateVirtualRootPath(filesystem_id()).Append(path);
    return file_system_context_->CreateCrackedFileSystemURL(
        GURL("http://example.com"), storage::kFileSystemTypeIsolated,
        virtual_path);
  }

  FileSystemURL GetOtherFileSystemURL(const base::FilePath& path) const {
    return file_system_context()->CreateCrackedFileSystemURL(
        GURL("http://example.com"), storage::kFileSystemTypeTemporary,
        base::FilePath().AppendASCII("dest").Append(path));
  }

  void VerifyFilesHaveSameContent(const FileSystemURL& url1,
                                  const FileSystemURL& url2) {
    // Get the file info and the platform path for url1.
    base::File::Info info1;
    ASSERT_EQ(base::File::FILE_OK, AsyncFileTestHelper::GetMetadata(
                                       file_system_context(), url1, &info1));
    base::FilePath platform_path1;
    ASSERT_EQ(base::File::FILE_OK,
              AsyncFileTestHelper::GetPlatformPath(file_system_context(), url1,
                                                   &platform_path1));

    // Get the file info and the platform path  for url2.
    base::File::Info info2;
    ASSERT_EQ(base::File::FILE_OK, AsyncFileTestHelper::GetMetadata(
                                       file_system_context(), url2, &info2));
    base::FilePath platform_path2;
    ASSERT_EQ(base::File::FILE_OK,
              AsyncFileTestHelper::GetPlatformPath(file_system_context(), url2,
                                                   &platform_path2));

    // See if file info matches with the other one.
    EXPECT_EQ(info1.is_directory, info2.is_directory);
    EXPECT_EQ(info1.size, info2.size);
    EXPECT_EQ(info1.is_symbolic_link, info2.is_symbolic_link);
    EXPECT_NE(platform_path1, platform_path2);

    std::string content1, content2;
    EXPECT_TRUE(base::ReadFileToString(platform_path1, &content1));
    EXPECT_TRUE(base::ReadFileToString(platform_path2, &content2));
    EXPECT_EQ(content1, content2);
  }

  void VerifyDirectoriesHaveSameContent(const FileSystemURL& root1,
                                        const FileSystemURL& root2) {
    base::FilePath root_path1 = root1.path();
    base::FilePath root_path2 = root2.path();

    FileEntryList entries;
    base::queue<FileSystemURL> directories;

    directories.push(root1);
    std::set<base::FilePath> file_set1;
    while (!directories.empty()) {
      FileSystemURL dir = directories.front();
      directories.pop();

      ASSERT_EQ(base::File::FILE_OK, AsyncFileTestHelper::ReadDirectory(
                                         file_system_context(), dir, &entries));
      for (size_t i = 0; i < entries.size(); ++i) {
        FileSystemURL url =
            GetEntryURL(file_system_context(), dir, entries[i].name.value());
        if (entries[i].type == filesystem::mojom::FsFileType::DIRECTORY) {
          directories.push(url);
          continue;
        }
        file_set1.insert(GetRelativeVirtualPath(root1, url));
      }
    }

    directories.push(root2);
    while (!directories.empty()) {
      FileSystemURL dir = directories.front();
      directories.pop();

      ASSERT_EQ(base::File::FILE_OK, AsyncFileTestHelper::ReadDirectory(
                                         file_system_context(), dir, &entries));
      for (size_t i = 0; i < entries.size(); ++i) {
        FileSystemURL url2 =
            GetEntryURL(file_system_context(), dir, entries[i].name.value());
        FileSystemURL url1 =
            GetOtherURL(file_system_context(), root2, root1, url2);
        if (entries[i].type == filesystem::mojom::FsFileType::DIRECTORY) {
          directories.push(url2);
          EXPECT_EQ(IsDirectoryEmpty(file_system_context(), url1),
                    IsDirectoryEmpty(file_system_context(), url2));
          continue;
        }
        base::FilePath relative = GetRelativeVirtualPath(root2, url2);
        EXPECT_TRUE(file_set1.find(relative) != file_set1.end());
        VerifyFilesHaveSameContent(url1, url2);
      }
    }
  }

  std::unique_ptr<storage::FileSystemOperationContext> GetOperationContext() {
    return std::make_unique<storage::FileSystemOperationContext>(
        file_system_context());
  }

 private:
  void SimulateDropFiles() {
    size_t root_path_index = 0;

    storage::IsolatedContext::FileInfoSet toplevels;
    for (size_t i = 0; i < kRegularFileSystemTestCaseSize; ++i) {
      const FileSystemTestCaseRecord& test_case =
          kRegularFileSystemTestCases[i];
      base::FilePath path(test_case.path);
      base::FilePath toplevel = GetTopLevelPath(path);

      // We create the test case files under one of the kRootPaths
      // to simulate a drop with multiple directories.
      if (toplevel_root_map_.find(toplevel) == toplevel_root_map_.end()) {
        base::FilePath root = root_path().Append(
            kRootPaths[(root_path_index++) % base::size(kRootPaths)]);
        toplevel_root_map_[toplevel] = root;
        toplevels.AddPath(root.Append(path), nullptr);
      }

      SetUpOneFileSystemTestCase(toplevel_root_map_[toplevel], test_case);
    }

    // Register the toplevel entries.
    filesystem_id_ = isolated_context()->RegisterDraggedFileSystem(toplevels);
  }

  base::ScopedTempDir data_dir_;
  base::ScopedTempDir partition_dir_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  std::string filesystem_id_;
  scoped_refptr<FileSystemContext> file_system_context_;
  std::map<base::FilePath, base::FilePath> toplevel_root_map_;
  std::unique_ptr<storage::DraggedFileUtil> file_util_;
  DISALLOW_COPY_AND_ASSIGN(DraggedFileUtilTest);
};

TEST_F(DraggedFileUtilTest, BasicTest) {
  for (size_t i = 0; i < kRegularFileSystemTestCaseSize; ++i) {
    SCOPED_TRACE(testing::Message() << "Testing RegularTestCases " << i);
    const FileSystemTestCaseRecord& test_case = kRegularFileSystemTestCases[i];

    FileSystemURL url = GetFileSystemURL(base::FilePath(test_case.path));

    // See if we can query the file info via the isolated FileUtil.
    // (This should succeed since we have registered all the top-level
    // entries of the test cases in SetUp())
    base::File::Info info;
    base::FilePath platform_path;
    FileSystemOperationContext context(file_system_context());
    ASSERT_EQ(base::File::FILE_OK,
              file_util()->GetFileInfo(&context, url, &info, &platform_path));

    // See if the obtained file info is correct.
    if (!test_case.is_directory)
      ASSERT_EQ(test_case.data_file_size, info.size);
    ASSERT_EQ(test_case.is_directory, info.is_directory);
    ASSERT_EQ(GetTestCasePlatformPath(test_case.path),
              platform_path.NormalizePathSeparators());
  }
}

TEST_F(DraggedFileUtilTest, UnregisteredPathsTest) {
  static const FileSystemTestCaseRecord kUnregisteredCases[] = {
      {true, FILE_PATH_LITERAL("nonexistent"), 0},
      {true, FILE_PATH_LITERAL("nonexistent/dir foo"), 0},
      {false, FILE_PATH_LITERAL("nonexistent/false"), 0},
      {false, FILE_PATH_LITERAL("foo"), 30},
      {false, FILE_PATH_LITERAL("bar"), 20},
  };

  for (size_t i = 0; i < base::size(kUnregisteredCases); ++i) {
    SCOPED_TRACE(testing::Message() << "Creating kUnregisteredCases " << i);
    const FileSystemTestCaseRecord& test_case = kUnregisteredCases[i];

    // Prepare the test file/directory.
    SetUpOneFileSystemTestCase(root_path(), test_case);

    // Make sure regular GetFileInfo succeeds.
    base::File::Info info;
    ASSERT_TRUE(base::GetFileInfo(root_path().Append(test_case.path), &info));
    if (!test_case.is_directory)
      ASSERT_EQ(test_case.data_file_size, info.size);
    ASSERT_EQ(test_case.is_directory, info.is_directory);
  }

  for (size_t i = 0; i < base::size(kUnregisteredCases); ++i) {
    SCOPED_TRACE(testing::Message() << "Creating kUnregisteredCases " << i);
    const FileSystemTestCaseRecord& test_case = kUnregisteredCases[i];
    FileSystemURL url = GetFileSystemURL(base::FilePath(test_case.path));

    // We should not be able to get the valid URL for unregistered files.
    ASSERT_FALSE(url.is_valid());
  }
}

TEST_F(DraggedFileUtilTest, ReadDirectoryTest) {
  for (size_t i = 0; i < kRegularFileSystemTestCaseSize; ++i) {
    const FileSystemTestCaseRecord& test_case = kRegularFileSystemTestCases[i];
    if (!test_case.is_directory)
      continue;

    SCOPED_TRACE(testing::Message()
                 << "Testing RegularTestCases " << i << ": " << test_case.path);

    // Read entries in the directory to construct the expected results map.
    using EntryMap =
        std::map<base::FilePath::StringType, filesystem::mojom::DirectoryEntry>;
    EntryMap expected_entry_map;

    base::FilePath dir_path = GetTestCasePlatformPath(test_case.path);
    base::FileEnumerator file_enum(
        dir_path, false /* not recursive */,
        base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
    base::FilePath current;
    while (!(current = file_enum.Next()).empty()) {
      base::FileEnumerator::FileInfo file_info = file_enum.GetInfo();
      filesystem::mojom::DirectoryEntry entry;
      entry.type = file_info.IsDirectory()
                       ? filesystem::mojom::FsFileType::DIRECTORY
                       : filesystem::mojom::FsFileType::REGULAR_FILE;

      entry.name = current.BaseName();
      expected_entry_map[entry.name.value()] = entry;

#if defined(OS_POSIX)
      // Creates a symlink for each file/directory.
      // They should be ignored by ReadDirectory, so we don't add them
      // to expected_entry_map.
      base::CreateSymbolicLink(current,
                               dir_path.Append(current.BaseName().AddExtension(
                                   FILE_PATH_LITERAL("link"))));
#endif
    }

    // Perform ReadDirectory in the isolated filesystem.
    FileSystemURL url = GetFileSystemURL(base::FilePath(test_case.path));
    FileEntryList entries;
    ASSERT_EQ(base::File::FILE_OK, AsyncFileTestHelper::ReadDirectory(
                                       file_system_context(), url, &entries));

    EXPECT_EQ(expected_entry_map.size(), entries.size());
    for (size_t i = 0; i < entries.size(); ++i) {
      const filesystem::mojom::DirectoryEntry& entry = entries[i];
      auto found = expected_entry_map.find(entry.name.value());
      EXPECT_TRUE(found != expected_entry_map.end());
      EXPECT_EQ(found->second.name, entry.name);
      EXPECT_EQ(found->second.type, entry.type);
    }
  }
}

TEST_F(DraggedFileUtilTest, GetLocalFilePathTest) {
  for (size_t i = 0; i < kRegularFileSystemTestCaseSize; ++i) {
    const FileSystemTestCaseRecord& test_case = kRegularFileSystemTestCases[i];
    FileSystemURL url = GetFileSystemURL(base::FilePath(test_case.path));

    FileSystemOperationContext context(file_system_context());

    base::FilePath local_file_path;
    EXPECT_EQ(base::File::FILE_OK,
              file_util()->GetLocalFilePath(&context, url, &local_file_path));
    EXPECT_EQ(GetTestCasePlatformPath(test_case.path).value(),
              local_file_path.value());
  }
}

TEST_F(DraggedFileUtilTest, CopyOutFileTest) {
  FileSystemURL src_root = GetFileSystemURL(base::FilePath());
  FileSystemURL dest_root = GetOtherFileSystemURL(base::FilePath());

  FileEntryList entries;
  base::queue<FileSystemURL> directories;
  directories.push(src_root);

  ASSERT_EQ(base::File::FILE_OK, AsyncFileTestHelper::CreateDirectory(
                                     file_system_context(), dest_root));

  while (!directories.empty()) {
    FileSystemURL dir = directories.front();
    directories.pop();
    ASSERT_EQ(base::File::FILE_OK, AsyncFileTestHelper::ReadDirectory(
                                       file_system_context(), dir, &entries));
    for (size_t i = 0; i < entries.size(); ++i) {
      FileSystemURL src_url =
          GetEntryURL(file_system_context(), dir, entries[i].name.value());
      FileSystemURL dest_url =
          GetOtherURL(file_system_context(), src_root, dest_root, src_url);

      if (entries[i].type == filesystem::mojom::FsFileType::DIRECTORY) {
        ASSERT_EQ(base::File::FILE_OK, AsyncFileTestHelper::CreateDirectory(
                                           file_system_context(), dest_url));
        directories.push(src_url);
        continue;
      }
      SCOPED_TRACE(testing::Message()
                   << "Testing file copy " << src_url.path().value());
      ASSERT_EQ(
          base::File::FILE_OK,
          AsyncFileTestHelper::Copy(file_system_context(), src_url, dest_url));
      VerifyFilesHaveSameContent(src_url, dest_url);
    }
  }
}

TEST_F(DraggedFileUtilTest, CopyOutDirectoryTest) {
  FileSystemURL src_root = GetFileSystemURL(base::FilePath());
  FileSystemURL dest_root = GetOtherFileSystemURL(base::FilePath());

  ASSERT_EQ(base::File::FILE_OK, AsyncFileTestHelper::CreateDirectory(
                                     file_system_context(), dest_root));

  FileEntryList entries;
  ASSERT_EQ(base::File::FILE_OK,
            AsyncFileTestHelper::ReadDirectory(file_system_context(), src_root,
                                               &entries));
  for (size_t i = 0; i < entries.size(); ++i) {
    if (entries[i].type != filesystem::mojom::FsFileType::DIRECTORY)
      continue;
    FileSystemURL src_url =
        GetEntryURL(file_system_context(), src_root, entries[i].name.value());
    FileSystemURL dest_url =
        GetOtherURL(file_system_context(), src_root, dest_root, src_url);
    SCOPED_TRACE(testing::Message()
                 << "Testing file copy " << src_url.path().value());
    ASSERT_EQ(
        base::File::FILE_OK,
        AsyncFileTestHelper::Copy(file_system_context(), src_url, dest_url));
    VerifyDirectoriesHaveSameContent(src_url, dest_url);
  }
}

TEST_F(DraggedFileUtilTest, TouchTest) {
  for (size_t i = 0; i < kRegularFileSystemTestCaseSize; ++i) {
    const FileSystemTestCaseRecord& test_case = kRegularFileSystemTestCases[i];
    if (test_case.is_directory)
      continue;
    SCOPED_TRACE(testing::Message() << test_case.path);
    FileSystemURL url = GetFileSystemURL(base::FilePath(test_case.path));

    base::Time last_access_time = base::Time::FromTimeT(1000);
    base::Time last_modified_time = base::Time::FromTimeT(2000);

    EXPECT_EQ(base::File::FILE_OK,
              file_util()->Touch(GetOperationContext().get(), url,
                                 last_access_time, last_modified_time));

    // Verification.
    base::File::Info info;
    base::FilePath platform_path;
    ASSERT_EQ(base::File::FILE_OK,
              file_util()->GetFileInfo(GetOperationContext().get(), url, &info,
                                       &platform_path));
    EXPECT_EQ(last_access_time.ToTimeT(), info.last_accessed.ToTimeT());
    EXPECT_EQ(last_modified_time.ToTimeT(), info.last_modified.ToTimeT());
  }
}

TEST_F(DraggedFileUtilTest, TruncateTest) {
  for (size_t i = 0; i < kRegularFileSystemTestCaseSize; ++i) {
    const FileSystemTestCaseRecord& test_case = kRegularFileSystemTestCases[i];
    if (test_case.is_directory)
      continue;

    SCOPED_TRACE(testing::Message() << test_case.path);
    FileSystemURL url = GetFileSystemURL(base::FilePath(test_case.path));

    // Truncate to 0.
    base::File::Info info;
    base::FilePath platform_path;
    EXPECT_EQ(base::File::FILE_OK,
              file_util()->Truncate(GetOperationContext().get(), url, 0));
    ASSERT_EQ(base::File::FILE_OK,
              file_util()->GetFileInfo(GetOperationContext().get(), url, &info,
                                       &platform_path));
    EXPECT_EQ(0, info.size);

    // Truncate (extend) to 999.
    EXPECT_EQ(base::File::FILE_OK,
              file_util()->Truncate(GetOperationContext().get(), url, 999));
    ASSERT_EQ(base::File::FILE_OK,
              file_util()->GetFileInfo(GetOperationContext().get(), url, &info,
                                       &platform_path));
    EXPECT_EQ(999, info.size);
  }
}

TEST_F(DraggedFileUtilTest, EnumerateTest) {
  FileSystemURL url =
      GetFileSystemURL(base::FilePath(FILE_PATH_LITERAL("dir a")));
  auto enumerator = file_util()->CreateFileEnumerator(
      GetOperationContext().get(), url, false);
  std::vector<base::FilePath> contents;
  for (base::FilePath path = enumerator->Next(); !path.empty();
       path = enumerator->Next()) {
    base::FilePath relative;
    root_path()
        .Append(base::FilePath(FILE_PATH_LITERAL("a")))
        .AppendRelativePath(path, &relative);
    contents.push_back(relative);
  }
  EXPECT_THAT(contents, testing::UnorderedElementsAre(
                            base::FilePath(FILE_PATH_LITERAL("dir a/dir A"))
                                .NormalizePathSeparators(),
                            base::FilePath(FILE_PATH_LITERAL("dir a/dir d"))
                                .NormalizePathSeparators(),
                            base::FilePath(FILE_PATH_LITERAL("dir a/file 0"))
                                .NormalizePathSeparators()));
}

TEST_F(DraggedFileUtilTest, EnumerateRecursivelyTest) {
  FileSystemURL url =
      GetFileSystemURL(base::FilePath(FILE_PATH_LITERAL("dir a")));
  auto enumerator =
      file_util()->CreateFileEnumerator(GetOperationContext().get(), url, true);
  std::vector<base::FilePath> contents;
  for (base::FilePath path = enumerator->Next(); !path.empty();
       path = enumerator->Next()) {
    base::FilePath relative;
    root_path()
        .Append(base::FilePath(FILE_PATH_LITERAL("a")))
        .AppendRelativePath(path, &relative);
    contents.push_back(relative);
  }
  EXPECT_THAT(
      contents,
      testing::UnorderedElementsAre(
          base::FilePath(FILE_PATH_LITERAL("dir a/dir A"))
              .NormalizePathSeparators(),
          base::FilePath(FILE_PATH_LITERAL("dir a/dir d"))
              .NormalizePathSeparators(),
          base::FilePath(FILE_PATH_LITERAL("dir a/file 0"))
              .NormalizePathSeparators(),
          base::FilePath(FILE_PATH_LITERAL("dir a/dir d/dir e"))
              .NormalizePathSeparators(),
          base::FilePath(FILE_PATH_LITERAL("dir a/dir d/dir e/dir f"))
              .NormalizePathSeparators(),
          base::FilePath(FILE_PATH_LITERAL("dir a/dir d/dir e/dir g"))
              .NormalizePathSeparators(),
          base::FilePath(FILE_PATH_LITERAL("dir a/dir d/dir e/dir g/file 0"))
              .NormalizePathSeparators(),
          base::FilePath(FILE_PATH_LITERAL("dir a/dir d/dir e/dir g/file 1"))
              .NormalizePathSeparators(),
          base::FilePath(FILE_PATH_LITERAL("dir a/dir d/dir e/dir g/file 2"))
              .NormalizePathSeparators(),
          base::FilePath(FILE_PATH_LITERAL("dir a/dir d/dir e/dir g/file 3"))
              .NormalizePathSeparators(),
          base::FilePath(FILE_PATH_LITERAL("dir a/dir d/dir e/dir h"))
              .NormalizePathSeparators()));
}

}  // namespace content
