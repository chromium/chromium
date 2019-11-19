// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/file_handlers/app_file_handler_util.h"

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

}  // namespace app_file_handler_util
}  // namespace extensions
