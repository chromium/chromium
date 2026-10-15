// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/file_handlers/app_file_handler_util.h"

#include <set>
#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "components/file_access/scoped_file_access.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "components/file_access/test/mock_scoped_file_access_delegate.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/file_handler_info.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/entry_info.h"
#include "extensions/browser/granted_file_entry.h"
#include "storage/common/file_system/file_system_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {
namespace app_file_handler_util {
namespace {

apps::FileHandlerInfo CreateHandlerInfoFromExtension(
    const std::string& extension) {
  apps::FileHandlerInfo handler_info;
  handler_info.extensions.insert(extension);
  return handler_info;
}

apps::FileHandlerInfo CreateHandlerInfoFromIncludeDirectories(
    bool include_directories) {
  apps::FileHandlerInfo handler_info;
  handler_info.include_directories = include_directories;
  return handler_info;
}

apps::FileHandler CreateWebAppFileHandlerFromMimeType(
    const std::string& mime_type) {
  apps::FileHandler file_handler;
  apps::FileHandler::AcceptEntry accept_entry;
  accept_entry.mime_type = mime_type;
  file_handler.accept.push_back(accept_entry);
  return file_handler;
}

apps::FileHandler CreateWebAppFileHandlerFromFileExtension(
    const std::string& file_extension) {
  apps::FileHandler file_handler;
  apps::FileHandler::AcceptEntry accept_entry;
  accept_entry.file_extensions.insert(file_extension);
  file_handler.accept.push_back(accept_entry);
  return file_handler;
}

}  // namespace

TEST(FileHandlersAppFileHandlerUtilTest, FileHandlerCanHandleEntry) {
  // File handler for extension "gz" should accept "*.gz", including "*.tar.gz".
  EXPECT_TRUE(FileHandlerCanHandleEntry(
      CreateHandlerInfoFromExtension("gz"),
      EntryInfo(base::FilePath::FromUTF8Unsafe("foo.gz"),
                "application/octet-stream", false)));
  EXPECT_FALSE(FileHandlerCanHandleEntry(
      CreateHandlerInfoFromExtension("gz"),
      EntryInfo(base::FilePath::FromUTF8Unsafe("foo.tgz"),
                "application/octet-stream", false)));
  EXPECT_TRUE(FileHandlerCanHandleEntry(
      CreateHandlerInfoFromExtension("gz"),
      EntryInfo(base::FilePath::FromUTF8Unsafe("foo.tar.gz"),
                "application/octet-stream", false)));
  EXPECT_FALSE(FileHandlerCanHandleEntry(
      CreateHandlerInfoFromExtension("tar.gz"),
      EntryInfo(base::FilePath::FromUTF8Unsafe("foo.gz"),
                "application/octet-stream", false)));
  EXPECT_TRUE(FileHandlerCanHandleEntry(
      CreateHandlerInfoFromExtension("tar.gz"),
      EntryInfo(base::FilePath::FromUTF8Unsafe("foo.tar.gz"),
                "application/octet-stream", false)));
  EXPECT_FALSE(FileHandlerCanHandleEntry(
      CreateHandlerInfoFromExtension("gz"),
      EntryInfo(base::FilePath::FromUTF8Unsafe("directory"), "", true)));

  EXPECT_FALSE(FileHandlerCanHandleEntry(
      CreateHandlerInfoFromIncludeDirectories(false),
      EntryInfo(base::FilePath::FromUTF8Unsafe("directory"), "", true)));
  EXPECT_TRUE(FileHandlerCanHandleEntry(
      CreateHandlerInfoFromIncludeDirectories(true),
      EntryInfo(base::FilePath::FromUTF8Unsafe("directory"), "", true)));
}

TEST(FileHandlersAppFileHandlerUtilTest, WebAppFileHandlerMatch) {
  apps::FileHandler file_handler;
  apps::FileHandler::AcceptEntry accept_entry;
  accept_entry.mime_type = "application/foo";
  accept_entry.file_extensions.insert(".foo");
  file_handler.accept.push_back(accept_entry);

  // Match true on MIME type for a single entry.
  {
    WebAppFileHandlerMatch match(&file_handler);
    EXPECT_FALSE(match.matched_mime_type());
    EXPECT_FALSE(match.matched_file_extension());

    EntryInfo entry(base::FilePath(FILE_PATH_LITERAL("file.bar")),
                    "application/foo", false);
    EXPECT_TRUE(match.DoMatch(entry));
    EXPECT_TRUE(match.matched_mime_type());
    EXPECT_FALSE(match.matched_file_extension());
  }

  // Match true on file extension for a single entry.
  {
    WebAppFileHandlerMatch match(&file_handler);
    EXPECT_FALSE(match.matched_mime_type());
    EXPECT_FALSE(match.matched_file_extension());

    EntryInfo entry(base::FilePath(FILE_PATH_LITERAL("file.foo")),
                    "application/bar", false);
    EXPECT_TRUE(match.DoMatch(entry));
    EXPECT_FALSE(match.matched_mime_type());
    EXPECT_TRUE(match.matched_file_extension());
  }

  // Match true on both MIME type and file extension.
  {
    WebAppFileHandlerMatch match(&file_handler);
    EXPECT_FALSE(match.matched_mime_type());
    EXPECT_FALSE(match.matched_file_extension());

    EntryInfo entry1(base::FilePath(FILE_PATH_LITERAL("file.bar")),
                     "application/foo", false);
    EXPECT_TRUE(match.DoMatch(entry1));
    EXPECT_TRUE(match.matched_mime_type());
    EXPECT_FALSE(match.matched_file_extension());

    EntryInfo entry2(base::FilePath(FILE_PATH_LITERAL("file.foo")),
                     "application/bar", false);
    EXPECT_TRUE(match.DoMatch(entry2));
    EXPECT_TRUE(match.matched_mime_type());
    EXPECT_TRUE(match.matched_file_extension());
  }

  // Match false on both MIME type and file extension.
  {
    WebAppFileHandlerMatch match(&file_handler);
    EXPECT_FALSE(match.matched_mime_type());
    EXPECT_FALSE(match.matched_file_extension());

    EntryInfo entry1(base::FilePath(FILE_PATH_LITERAL("file.bar")),
                     "application/bar", false);
    EXPECT_FALSE(match.DoMatch(entry1));
    EXPECT_FALSE(match.matched_mime_type());
    EXPECT_FALSE(match.matched_file_extension());

    EntryInfo entry2(base::FilePath(FILE_PATH_LITERAL("file.baz")),
                     "application/baz", false);
    EXPECT_FALSE(match.DoMatch(entry2));
    EXPECT_FALSE(match.matched_mime_type());
    EXPECT_FALSE(match.matched_file_extension());
  }

  // Match false on directory.
  {
    WebAppFileHandlerMatch match(&file_handler);

    EntryInfo entry(base::FilePath(FILE_PATH_LITERAL("file.foo")),
                    "application/foo", true);
    EXPECT_FALSE(match.DoMatch(entry));
    EXPECT_FALSE(match.matched_mime_type());
    EXPECT_FALSE(match.matched_file_extension());
  }
}

TEST(FileHandlersAppFileHandlerUtilTest, WebAppFileHandlerCanHandleEntry) {
  // File handler should successfully match on MIME type.
  EXPECT_TRUE(WebAppFileHandlerCanHandleEntry(
      CreateWebAppFileHandlerFromMimeType("application/octet-stream"),
      EntryInfo(base::FilePath(FILE_PATH_LITERAL("foo.gz")),
                "application/octet-stream", false)));
  EXPECT_FALSE(WebAppFileHandlerCanHandleEntry(
      CreateWebAppFileHandlerFromMimeType("text/plain"),
      EntryInfo(base::FilePath(FILE_PATH_LITERAL("foo.gz")),
                "application/octet-stream", false)));

  // File handler for extension "gz" should accept "*.gz", including "*.tar.gz".
  EXPECT_TRUE(WebAppFileHandlerCanHandleEntry(
      CreateWebAppFileHandlerFromFileExtension(".gz"),
      EntryInfo(base::FilePath(FILE_PATH_LITERAL("foo.gz")),
                "application/octet-stream", false)));
  EXPECT_FALSE(WebAppFileHandlerCanHandleEntry(
      CreateWebAppFileHandlerFromFileExtension(".gz"),
      EntryInfo(base::FilePath(FILE_PATH_LITERAL("foo.tgz")),
                "application/octet-stream", false)));
  EXPECT_TRUE(WebAppFileHandlerCanHandleEntry(
      CreateWebAppFileHandlerFromFileExtension(".gz"),
      EntryInfo(base::FilePath(FILE_PATH_LITERAL("foo.tar.gz")),
                "application/octet-stream", false)));
  EXPECT_FALSE(WebAppFileHandlerCanHandleEntry(
      CreateWebAppFileHandlerFromFileExtension(".tar.gz"),
      EntryInfo(base::FilePath(FILE_PATH_LITERAL("foo.gz")),
                "application/octet-stream", false)));
  EXPECT_TRUE(WebAppFileHandlerCanHandleEntry(
      CreateWebAppFileHandlerFromFileExtension(".tar.gz"),
      EntryInfo(base::FilePath(FILE_PATH_LITERAL("foo.tar.gz")),
                "application/octet-stream", false)));

  // File handler should not match on directory.
  EXPECT_FALSE(WebAppFileHandlerCanHandleEntry(
      CreateWebAppFileHandlerFromFileExtension(".gz"),
      EntryInfo(base::FilePath(FILE_PATH_LITERAL("foo.gz")),
                "application/octet-stream", true)));
}

TEST(FileHandlersAppFileHandlerUtilTest, CreateEntryInfos) {
  const auto is_same_entry_info = [](const EntryInfo& info1,
                                     const EntryInfo& info2) -> bool {
    return info1.path == info2.path && info1.mime_type == info2.mime_type &&
           info1.is_directory == info2.is_directory;
  };

  // Empty case.
  {
    std::vector<extensions::EntryInfo> entry_infos =
        CreateEntryInfos({}, {}, {});
    EXPECT_TRUE(entry_infos.empty());
  }

  // Mix of data, including MIME fallbacks and a directory.
  {
    std::vector<extensions::EntryInfo> entry_infos =
        CreateEntryInfos({base::FilePath(FILE_PATH_LITERAL("foo.html")),
                          base::FilePath(FILE_PATH_LITERAL("/x/bar.jpg")),
                          base::FilePath(FILE_PATH_LITERAL("a/b/c")),
                          base::FilePath(FILE_PATH_LITERAL("a/b/bez.rat"))},
                         {"text/html", "image/jpeg", "", ""},
                         {base::FilePath(FILE_PATH_LITERAL("a")),
                          base::FilePath(FILE_PATH_LITERAL("a/b")),
                          base::FilePath(FILE_PATH_LITERAL("a/b/c"))});
    EXPECT_EQ(4U, entry_infos.size());
    EXPECT_TRUE(is_same_entry_info(
        EntryInfo(base::FilePath(FILE_PATH_LITERAL("foo.html")), "text/html",
                  false),
        entry_infos[0]));
    EXPECT_TRUE(is_same_entry_info(
        EntryInfo(base::FilePath(FILE_PATH_LITERAL("/x/bar.jpg")), "image/jpeg",
                  false),
        entry_infos[1]));
    EXPECT_TRUE(
        is_same_entry_info(EntryInfo(base::FilePath(FILE_PATH_LITERAL("a/b/c")),
                                     "application/octet-stream", true),
                           entry_infos[2]));
    EXPECT_TRUE(is_same_entry_info(
        EntryInfo(base::FilePath(FILE_PATH_LITERAL("a/b/bez.rat")),
                  "application/octet-stream", false),
        entry_infos[3]));
  }

  // Param size mismatch (missing |mime_types| entry).
  {
    EXPECT_CHECK_DEATH(
        std::vector<extensions::EntryInfo> entry_infos =
            CreateEntryInfos({base::FilePath(FILE_PATH_LITERAL("foo.html")),
                              base::FilePath(FILE_PATH_LITERAL("/x/bar.jpg")),
                              base::FilePath(FILE_PATH_LITERAL("a/b/c")),
                              base::FilePath(FILE_PATH_LITERAL("a/b/bez.rat"))},
                             {"text/html", "image/jpeg", ""},
                             {base::FilePath(FILE_PATH_LITERAL("a")),
                              base::FilePath(FILE_PATH_LITERAL("a/b")),
                              base::FilePath(FILE_PATH_LITERAL("a/b/c"))}));
  }
}

class PrepareFilesForWritableAppTest : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(base_.CreateUniqueTempDir());
    base::FilePath base_dir = base_.GetPath();
    file1 = base_.GetPath().Append(FILE_PATH_LITERAL("a.txt"));
    file2 = base_.GetPath().Append(FILE_PATH_LITERAL("b.txt"));
    file3 = base_.GetPath()
                .Append(FILE_PATH_LITERAL("a"))
                .Append(FILE_PATH_LITERAL("b"))
                .Append(FILE_PATH_LITERAL("c.txt"));
    base::File(file1, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE)
        .Flush();
  }

 protected:
  // Runs PrepareFilesForWritableApp() for |path| under |root_path| and returns
  // whether it succeeded.
  bool PrepareUnderRoot(const base::FilePath& path,
                        bool is_directory,
                        const base::FilePath& root_path) {
    base::test::TestFuture<bool> future;
    std::set<base::FilePath> directory_paths;
    if (is_directory) {
      directory_paths.insert(path);
    }
    PrepareFilesForWritableApp({path}, &context_, directory_paths,
                               base::BindOnce(future.GetCallback(), true),
                               base::IgnoreArgs<const base::FilePath&>(
                                   base::BindOnce(future.GetCallback(), false)),
                               root_path);
    return future.Get();
  }

  // File exists.
  base::FilePath file1;
  // File does not exist, but the directory of the file.
  base::FilePath file2;
  // File does not exist neither does its directory.
  base::FilePath file3;

  content::BrowserTaskEnvironment browser_task_environment_;
  content::TestBrowserContext context_;

 private:
  ExtensionsAPIClient api_client_;
  base::ScopedTempDir base_;
};

TEST_F(PrepareFilesForWritableAppTest, SingleFileThatDoesExists) {
  testing::StrictMock<base::MockOnceCallback<void()>> success_callback;
  testing::StrictMock<base::MockOnceCallback<void(const base::FilePath& path)>>
      fail_callback;

  base::RunLoop run_loop;
  EXPECT_CALL(success_callback, Run).WillOnce([&run_loop] { run_loop.Quit(); });

  PrepareFilesForWritableApp({file1}, &context_, {}, success_callback.Get(),
                             fail_callback.Get());
  run_loop.Run();
}

TEST_F(PrepareFilesForWritableAppTest, SingleFileThatDoesNotExists) {
  testing::StrictMock<base::MockOnceCallback<void()>> success_callback;
  testing::StrictMock<base::MockOnceCallback<void(const base::FilePath& path)>>
      fail_callback;

  base::RunLoop run_loop;
  EXPECT_CALL(success_callback, Run).WillOnce([&run_loop] { run_loop.Quit(); });

  PrepareFilesForWritableApp({file2}, &context_, {}, success_callback.Get(),
                             fail_callback.Get());
  run_loop.Run();
}

TEST_F(PrepareFilesForWritableAppTest, SingleFileInDirectoryThatDoesNotExists) {
  testing::StrictMock<base::MockOnceCallback<void()>> success_callback;
  testing::StrictMock<base::MockOnceCallback<void(const base::FilePath& path)>>
      fail_callback;

  base::RunLoop run_loop;
  EXPECT_CALL(fail_callback, Run).WillOnce([&run_loop] { run_loop.Quit(); });

  PrepareFilesForWritableApp({file3}, &context_, {}, success_callback.Get(),
                             fail_callback.Get());
  run_loop.Run();
}

TEST_F(PrepareFilesForWritableAppTest,
       OneFileInDirectoryThatDoesNotExistOneFileThatExists) {
  testing::StrictMock<base::MockOnceCallback<void()>> success_callback;
  testing::StrictMock<base::MockOnceCallback<void(const base::FilePath& path)>>
      fail_callback;

  base::RunLoop run_loop;
  EXPECT_CALL(fail_callback, Run)
      .WillOnce([this, &run_loop](const base::FilePath& path) {
        EXPECT_EQ(file3, path);
        run_loop.Quit();
      });

  PrepareFilesForWritableApp({file3, file1}, &context_, {},
                             success_callback.Get(), fail_callback.Get());
  run_loop.Run();
}

TEST_F(PrepareFilesForWritableAppTest,
       OneFileThatExistsOneFileInDirectoryThatDoesNotExist) {
  testing::StrictMock<base::MockOnceCallback<void()>> success_callback;
  testing::StrictMock<base::MockOnceCallback<void(const base::FilePath& path)>>
      fail_callback;

  base::RunLoop run_loop;
  EXPECT_CALL(fail_callback, Run)
      .WillOnce([this, &run_loop](const base::FilePath& path) {
        EXPECT_EQ(file3, path);
        run_loop.Quit();
      });

  PrepareFilesForWritableApp({file3, file1}, &context_, {},
                             success_callback.Get(), fail_callback.Get());
  run_loop.Run();
}

TEST_F(PrepareFilesForWritableAppTest, FileUnderRoot) {
  base::FilePath root = file1.DirName();
  base::FilePath dir = root.Append(FILE_PATH_LITERAL("dir"));
  ASSERT_TRUE(base::CreateDirectory(dir));

  EXPECT_TRUE(PrepareUnderRoot(root, /*is_directory=*/true, root));
  EXPECT_TRUE(PrepareUnderRoot(file1, /*is_directory=*/false, file1));
  EXPECT_TRUE(PrepareUnderRoot(dir, /*is_directory=*/true, root));
  EXPECT_TRUE(PrepareUnderRoot(dir.Append(FILE_PATH_LITERAL("new.txt")),
                               /*is_directory=*/false, root));
  EXPECT_FALSE(PrepareUnderRoot(file1, /*is_directory=*/false, dir));
}

#if BUILDFLAG(IS_CHROMEOS)

TEST_F(PrepareFilesForWritableAppTest, SingleFileThatExistsDlpGrantsAccess) {
  testing::StrictMock<base::MockOnceCallback<void()>> success_callback;
  testing::StrictMock<base::MockOnceCallback<void(const base::FilePath& path)>>
      fail_callback;
  file_access::MockScopedFileAccessDelegate scoped_file_access_delegate;
  base::RunLoop run_loop;
  EXPECT_CALL(scoped_file_access_delegate, RequestFilesAccessForSystem)
      .WillOnce([this](const std::vector<base::FilePath>& paths,
                       base::OnceCallback<void(file_access::ScopedFileAccess)>
                           callback) {
        EXPECT_EQ(1ul, paths.size());
        EXPECT_EQ(file1, paths[0]);
        std::move(callback).Run(file_access::ScopedFileAccess::Allowed());
      });
  EXPECT_CALL(success_callback, Run).WillOnce([&run_loop] { run_loop.Quit(); });

  PrepareFilesForWritableApp({file1}, &context_, {}, success_callback.Get(),
                             fail_callback.Get());
  run_loop.Run();
}

TEST_F(PrepareFilesForWritableAppTest, SingleFileThatExistsDlpDeniesAccess) {
  testing::StrictMock<base::MockOnceCallback<void()>> success_callback;
  testing::StrictMock<base::MockOnceCallback<void(const base::FilePath& path)>>
      fail_callback;
  file_access::MockScopedFileAccessDelegate scoped_file_access_delegate;
  base::RunLoop run_loop;
  EXPECT_CALL(scoped_file_access_delegate, RequestFilesAccessForSystem)
      .WillOnce([this](const std::vector<base::FilePath>& paths,
                       base::OnceCallback<void(file_access::ScopedFileAccess)>
                           callback) {
        EXPECT_EQ(1ul, paths.size());
        EXPECT_EQ(file1, paths[0]);
        std::move(callback).Run(
            file_access::ScopedFileAccess(false, base::ScopedFD()));
      });
  EXPECT_CALL(success_callback, Run).WillOnce([&run_loop] { run_loop.Quit(); });

  PrepareFilesForWritableApp({file1}, &context_, {}, success_callback.Get(),
                             fail_callback.Get());
  run_loop.Run();
}

#endif

#if BUILDFLAG(IS_POSIX)
TEST_F(PrepareFilesForWritableAppTest, SymlinkToExistingFile) {
  base::FilePath target = file1;
  base::FilePath symlink =
      file1.DirName().Append(FILE_PATH_LITERAL("symlink.txt"));
  ASSERT_TRUE(base::CreateSymbolicLink(target, symlink));

  testing::StrictMock<base::MockOnceCallback<void()>> success_callback;
  testing::StrictMock<base::MockOnceCallback<void(const base::FilePath& path)>>
      fail_callback;

  base::RunLoop run_loop;
  EXPECT_CALL(fail_callback, Run)
      .WillOnce([&run_loop, &symlink](const base::FilePath& path) {
        EXPECT_EQ(symlink, path);
        run_loop.Quit();
      });

  PrepareFilesForWritableApp({symlink}, &context_, {}, success_callback.Get(),
                             fail_callback.Get());
  run_loop.Run();
}

TEST_F(PrepareFilesForWritableAppTest, DanglingSymlink) {
  base::FilePath target =
      file1.DirName().Append(FILE_PATH_LITERAL("non_existent.txt"));
  base::FilePath symlink =
      file1.DirName().Append(FILE_PATH_LITERAL("dangling.txt"));
  ASSERT_TRUE(base::CreateSymbolicLink(target, symlink));

  testing::StrictMock<base::MockOnceCallback<void()>> success_callback;
  testing::StrictMock<base::MockOnceCallback<void(const base::FilePath& path)>>
      fail_callback;

  base::RunLoop run_loop;
  EXPECT_CALL(fail_callback, Run)
      .WillOnce([&run_loop, &symlink](const base::FilePath& path) {
        EXPECT_EQ(symlink, path);
        run_loop.Quit();
      });

  PrepareFilesForWritableApp({symlink}, &context_, {}, success_callback.Get(),
                             fail_callback.Get());
  run_loop.Run();

  // Verify that the target of the dangling symlink was not created.
  EXPECT_FALSE(base::PathExists(target));
}

TEST_F(PrepareFilesForWritableAppTest, SymlinkToExistingDirectory) {
  base::FilePath target =
      file1.DirName().Append(FILE_PATH_LITERAL("target_dir"));
  ASSERT_TRUE(base::CreateDirectory(target));
  base::FilePath symlink =
      file1.DirName().Append(FILE_PATH_LITERAL("symlink_dir"));
  ASSERT_TRUE(base::CreateSymbolicLink(target, symlink));

  testing::StrictMock<base::MockOnceCallback<void()>> success_callback;
  testing::StrictMock<base::MockOnceCallback<void(const base::FilePath& path)>>
      fail_callback;

  base::RunLoop run_loop;
  EXPECT_CALL(fail_callback, Run)
      .WillOnce([&run_loop, &symlink](const base::FilePath& path) {
        EXPECT_EQ(symlink, path);
        run_loop.Quit();
      });

  PrepareFilesForWritableApp({symlink}, &context_, {symlink},
                             success_callback.Get(), fail_callback.Get());
  run_loop.Run();
}

TEST_F(PrepareFilesForWritableAppTest, SymlinkedDirectoryUnderRoot) {
  base::ScopedTempDir outside;
  ASSERT_TRUE(outside.CreateUniqueTempDir());
  base::FilePath sub = outside.GetPath().Append(FILE_PATH_LITERAL("sub"));
  ASSERT_TRUE(base::CreateDirectory(sub));
  base::FilePath root = file1.DirName();
  base::FilePath link = root.Append(FILE_PATH_LITERAL("link"));
  ASSERT_TRUE(base::CreateSymbolicLink(outside.GetPath(), link));

  EXPECT_FALSE(PrepareUnderRoot(link.Append(FILE_PATH_LITERAL("new.txt")),
                                /*is_directory=*/false, root));
  EXPECT_FALSE(PrepareUnderRoot(link.Append(FILE_PATH_LITERAL("sub"))
                                    .Append(FILE_PATH_LITERAL("new.txt")),
                                /*is_directory=*/false, root));
  EXPECT_FALSE(PrepareUnderRoot(link.Append(FILE_PATH_LITERAL("sub")),
                                /*is_directory=*/true, root));
  EXPECT_FALSE(
      base::PathExists(outside.GetPath().Append(FILE_PATH_LITERAL("new.txt"))));
  EXPECT_FALSE(base::PathExists(sub.Append(FILE_PATH_LITERAL("new.txt"))));

  // Without a root, only the final component is checked.
  EXPECT_TRUE(PrepareUnderRoot(link.Append(FILE_PATH_LITERAL("new.txt")),
                               /*is_directory=*/false, base::FilePath()));
}

TEST_F(PrepareFilesForWritableAppTest, SymlinkAboveRoot) {
  base::ScopedTempDir parent;
  ASSERT_TRUE(parent.CreateUniqueTempDir());
  base::FilePath root = parent.GetPath().Append(FILE_PATH_LITERAL("link"));
  ASSERT_TRUE(base::CreateSymbolicLink(file1.DirName(), root));

  EXPECT_TRUE(PrepareUnderRoot(root.Append(FILE_PATH_LITERAL("new.txt")),
                               /*is_directory=*/false, root));
}
#endif

TEST(FileHandlersAppFileHandlerUtilTest, ValidateFileEntryAndGetPathRoot) {
  content::BrowserTaskEnvironment task_environment;
  content::TestBrowserContext context;
  content::MockRenderProcessHost process(&context);
  base::ScopedTempDir root;
  ASSERT_TRUE(root.CreateUniqueTempDir());
  GrantedFileEntry entry = CreateFileEntryWithPermissions(
      process.GetDeprecatedID(), root.GetPath(), /*can_write=*/false,
      /*can_create=*/false, /*can_delete=*/false);
  std::string filesystem_name = storage::GetIsolatedFileSystemName(
      GURL("chrome-extension://abcdefghijklmnopabcdefghijklmnop/"),
      entry.filesystem_id);

  base::FilePath file_path;
  base::FilePath root_path;
  std::string error;
  ASSERT_TRUE(ValidateFileEntryAndGetPath(
      filesystem_name, entry.registered_name + "/dir/file.txt",
      process.GetDeprecatedID(), &file_path, &error, &root_path))
      << error;
  EXPECT_EQ(root.GetPath(), root_path);
  EXPECT_EQ(root.GetPath()
                .Append(FILE_PATH_LITERAL("dir"))
                .Append(FILE_PATH_LITERAL("file.txt")),
            file_path);
}

}  // namespace app_file_handler_util
}  // namespace extensions
