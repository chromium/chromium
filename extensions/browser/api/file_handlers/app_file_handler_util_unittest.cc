// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/file_handlers/app_file_handler_util.h"

#include "base/files/file_path.h"
#include "base/test/gtest_util.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/file_handler_info.h"
#include "extensions/browser/entry_info.h"
#include "testing/gtest/include/gtest/gtest.h"

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

}  // namespace app_file_handler_util
}  // namespace extensions
