// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/common/file_system/file_system_util.h"

#include <stddef.h>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using storage::CrackIsolatedFileSystemName;
using storage::GetExternalFileSystemRootURIString;
using storage::GetIsolatedFileSystemName;
using storage::GetIsolatedFileSystemRootURIString;
using storage::ValidateIsolatedFileSystemId;
using storage::VirtualPath;

namespace content {
namespace {

class FileSystemUtilTest : public testing::Test {};

TEST_F(FileSystemUtilTest, ParseFileSystemSchemeURL) {
  GURL uri("filesystem:http://chromium.org/temporary/foo/bar");
  GURL origin_url;
  storage::FileSystemType type;
  base::FilePath virtual_path;
  ParseFileSystemSchemeURL(uri, &origin_url, &type, &virtual_path);
  EXPECT_EQ(GURL("http://chromium.org"), origin_url);
  EXPECT_EQ(storage::kFileSystemTypeTemporary, type);
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
  base::FilePath expected_path(FILE_PATH_LITERAL("foo\\bar"));
#else
  base::FilePath expected_path(FILE_PATH_LITERAL("foo/bar"));
#endif
  EXPECT_EQ(expected_path, virtual_path);
}

TEST_F(FileSystemUtilTest, GetTempFileSystemRootURI) {
  GURL origin_url("http://chromium.org");
  storage::FileSystemType type = storage::kFileSystemTypeTemporary;
  GURL uri = GURL("filesystem:http://chromium.org/temporary/");
  EXPECT_EQ(uri, GetFileSystemRootURI(origin_url, type));
}

TEST_F(FileSystemUtilTest, GetPersistentFileSystemRootURI) {
  GURL origin_url("http://chromium.org");
  storage::FileSystemType type = storage::kFileSystemTypePersistent;
  GURL uri = GURL("filesystem:http://chromium.org/persistent/");
  EXPECT_EQ(uri, GetFileSystemRootURI(origin_url, type));
}

TEST_F(FileSystemUtilTest, VirtualPathBaseName) {
  struct test_data {
    const base::FilePath::StringType path;
    const base::FilePath::StringType base_name;
  } test_cases[] = {
      {FILE_PATH_LITERAL("foo/bar"), FILE_PATH_LITERAL("bar")},
      {FILE_PATH_LITERAL("foo/b:bar"), FILE_PATH_LITERAL("b:bar")},
      {FILE_PATH_LITERAL(""), FILE_PATH_LITERAL("")},
      {FILE_PATH_LITERAL("/"), FILE_PATH_LITERAL("/")},
      {FILE_PATH_LITERAL("foo//////bar"), FILE_PATH_LITERAL("bar")},
      {FILE_PATH_LITERAL("foo/bar/"), FILE_PATH_LITERAL("bar")},
      {FILE_PATH_LITERAL("foo/bar/////"), FILE_PATH_LITERAL("bar")},
      {FILE_PATH_LITERAL("/bar/////"), FILE_PATH_LITERAL("bar")},
      {FILE_PATH_LITERAL("bar/////"), FILE_PATH_LITERAL("bar")},
      {FILE_PATH_LITERAL("bar/"), FILE_PATH_LITERAL("bar")},
      {FILE_PATH_LITERAL("/bar"), FILE_PATH_LITERAL("bar")},
      {FILE_PATH_LITERAL("////bar"), FILE_PATH_LITERAL("bar")},
      {FILE_PATH_LITERAL("bar"), FILE_PATH_LITERAL("bar")}};
  for (const auto& test_case : test_cases) {
    base::FilePath input = base::FilePath(test_case.path);
    base::FilePath base_name = VirtualPath::BaseName(input);
    EXPECT_EQ(test_case.base_name, base_name.value());
  }
}

TEST_F(FileSystemUtilTest, VirtualPathDirName) {
  struct test_data {
    const base::FilePath::StringType path;
    const base::FilePath::StringType dir_name;
  } test_cases[] = {
      {FILE_PATH_LITERAL("foo/bar"), FILE_PATH_LITERAL("foo")},
      {FILE_PATH_LITERAL("foo/b:bar"), FILE_PATH_LITERAL("foo")},
      {FILE_PATH_LITERAL(""), FILE_PATH_LITERAL(".")},
      {FILE_PATH_LITERAL("/"), FILE_PATH_LITERAL("/")},
      {FILE_PATH_LITERAL("foo//////bar"), FILE_PATH_LITERAL("foo")},
      {FILE_PATH_LITERAL("foo/bar/"), FILE_PATH_LITERAL("foo")},
      {FILE_PATH_LITERAL("foo/bar/////"), FILE_PATH_LITERAL("foo")},
      {FILE_PATH_LITERAL("/bar/////"), FILE_PATH_LITERAL("/")},
      {FILE_PATH_LITERAL("bar/////"), FILE_PATH_LITERAL(".")},
      {FILE_PATH_LITERAL("bar/"), FILE_PATH_LITERAL(".")},
      {FILE_PATH_LITERAL("/bar"), FILE_PATH_LITERAL("/")},
      {FILE_PATH_LITERAL("////bar"), FILE_PATH_LITERAL("/")},
      {FILE_PATH_LITERAL("bar"), FILE_PATH_LITERAL(".")},
      {FILE_PATH_LITERAL("c:bar"), FILE_PATH_LITERAL(".")},
#ifdef FILE_PATH_USES_WIN_SEPARATORS
      {FILE_PATH_LITERAL("foo\\bar"), FILE_PATH_LITERAL("foo")},
      {FILE_PATH_LITERAL("foo\\b:bar"), FILE_PATH_LITERAL("foo")},
      {FILE_PATH_LITERAL("\\"), FILE_PATH_LITERAL("\\")},
      {FILE_PATH_LITERAL("foo\\\\\\\\\\\\bar"), FILE_PATH_LITERAL("foo")},
      {FILE_PATH_LITERAL("foo\\bar\\"), FILE_PATH_LITERAL("foo")},
      {FILE_PATH_LITERAL("foo\\bar\\\\\\\\\\"), FILE_PATH_LITERAL("foo")},
      {FILE_PATH_LITERAL("\\bar\\\\\\\\\\"), FILE_PATH_LITERAL("\\")},
      {FILE_PATH_LITERAL("bar\\\\\\\\\\"), FILE_PATH_LITERAL(".")},
      {FILE_PATH_LITERAL("bar\\"), FILE_PATH_LITERAL(".")},
      {FILE_PATH_LITERAL("\\bar"), FILE_PATH_LITERAL("\\")},
      {FILE_PATH_LITERAL("\\\\\\\\bar"), FILE_PATH_LITERAL("\\")},
#endif
  };
  for (const auto& test_case : test_cases) {
    base::FilePath input = base::FilePath(test_case.path);
    base::FilePath dir_name = VirtualPath::DirName(input);
    EXPECT_EQ(test_case.dir_name, dir_name.value());
  }
}

TEST_F(FileSystemUtilTest, GetNormalizedFilePath) {
  struct test_data {
    const base::FilePath::StringType path;
    const base::FilePath::StringType normalized_path;
  } test_cases[] = {
    {FILE_PATH_LITERAL(""), FILE_PATH_LITERAL("/")},
    {FILE_PATH_LITERAL("/"), FILE_PATH_LITERAL("/")},
    {FILE_PATH_LITERAL("foo/bar"), FILE_PATH_LITERAL("/foo/bar")},
    {FILE_PATH_LITERAL("/foo/bar"), FILE_PATH_LITERAL("/foo/bar")},
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
    {FILE_PATH_LITERAL("\\foo"), FILE_PATH_LITERAL("/foo")},
#endif
  };
  for (const auto& test_case : test_cases) {
    base::FilePath input = base::FilePath(test_case.path);
    base::FilePath::StringType normalized_path_string =
        VirtualPath::GetNormalizedFilePath(input);
    EXPECT_EQ(test_case.normalized_path, normalized_path_string);
  }
}

TEST_F(FileSystemUtilTest, IsAbsolutePath) {
  EXPECT_TRUE(VirtualPath::IsAbsolute(FILE_PATH_LITERAL("/")));
  EXPECT_TRUE(VirtualPath::IsAbsolute(FILE_PATH_LITERAL("/foo/bar")));
  EXPECT_FALSE(VirtualPath::IsAbsolute(base::FilePath::StringType()));
  EXPECT_FALSE(VirtualPath::IsAbsolute(FILE_PATH_LITERAL("foo/bar")));
}

TEST_F(FileSystemUtilTest, IsRootPath) {
  EXPECT_TRUE(VirtualPath::IsRootPath(base::FilePath(FILE_PATH_LITERAL(""))));
  EXPECT_TRUE(VirtualPath::IsRootPath(base::FilePath()));
  EXPECT_TRUE(VirtualPath::IsRootPath(base::FilePath(FILE_PATH_LITERAL("/"))));
  EXPECT_TRUE(VirtualPath::IsRootPath(base::FilePath(FILE_PATH_LITERAL("//"))));
  EXPECT_FALSE(
      VirtualPath::IsRootPath(base::FilePath(FILE_PATH_LITERAL("c:/"))));
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
  EXPECT_TRUE(VirtualPath::IsRootPath(base::FilePath(FILE_PATH_LITERAL("\\"))));
  EXPECT_FALSE(
      VirtualPath::IsRootPath(base::FilePath(FILE_PATH_LITERAL("c:\\"))));
#endif
}

TEST_F(FileSystemUtilTest, VirtualPathGetComponents) {
  struct test_data {
    const base::FilePath::StringType path;
    size_t count;
    const base::FilePath::StringType components[2];
  } test_cases[] = {
      {FILE_PATH_LITERAL("foo/bar"),
       2,
       {FILE_PATH_LITERAL("foo"), FILE_PATH_LITERAL("bar")}},
      {FILE_PATH_LITERAL("foo"),
       1,
       {FILE_PATH_LITERAL("foo"), FILE_PATH_LITERAL("")}},
      {FILE_PATH_LITERAL("foo////bar"),
       2,
       {FILE_PATH_LITERAL("foo"), FILE_PATH_LITERAL("bar")}},
      {FILE_PATH_LITERAL("foo/c:bar"),
       2,
       {FILE_PATH_LITERAL("foo"), FILE_PATH_LITERAL("c:bar")}},
      {FILE_PATH_LITERAL("c:foo/bar"),
       2,
       {FILE_PATH_LITERAL("c:foo"), FILE_PATH_LITERAL("bar")}},
      {FILE_PATH_LITERAL("foo/bar"),
       2,
       {FILE_PATH_LITERAL("foo"), FILE_PATH_LITERAL("bar")}},
      {FILE_PATH_LITERAL("/foo/bar"),
       2,
       {FILE_PATH_LITERAL("foo"), FILE_PATH_LITERAL("bar")}},
      {FILE_PATH_LITERAL("c:/bar"),
       2,
       {FILE_PATH_LITERAL("c:"), FILE_PATH_LITERAL("bar")}},
#ifdef FILE_PATH_USES_WIN_SEPARATORS
      {FILE_PATH_LITERAL("c:\\bar"),
       2,
       {FILE_PATH_LITERAL("c:"), FILE_PATH_LITERAL("bar")}},
#endif
  };
  for (const auto& test_case : test_cases) {
    base::FilePath input = base::FilePath(test_case.path);
    std::vector<base::FilePath::StringType> components =
        VirtualPath::GetComponents(input);
    EXPECT_EQ(test_case.count, components.size());
    for (size_t j = 0; j < components.size(); ++j)
      EXPECT_EQ(test_case.components[j], components[j]);
  }
  for (const auto& test_case : test_cases) {
    base::FilePath input = base::FilePath(test_case.path);
    std::vector<std::string> components =
        VirtualPath::GetComponentsUTF8Unsafe(input);
    EXPECT_EQ(test_case.count, components.size());
    for (size_t j = 0; j < components.size(); ++j) {
      EXPECT_EQ(base::FilePath(test_case.components[j]).AsUTF8Unsafe(),
                components[j]);
    }
  }
}

TEST_F(FileSystemUtilTest, GetIsolatedFileSystemName) {
  GURL origin_url("http://foo");
  std::string fsname1 = GetIsolatedFileSystemName(origin_url, "bar");
  EXPECT_EQ("http_foo_0:Isolated_bar", fsname1);
}

TEST_F(FileSystemUtilTest, CrackIsolatedFileSystemName) {
  std::string fsid;
  EXPECT_TRUE(CrackIsolatedFileSystemName("foo:Isolated_bar", &fsid));
  EXPECT_EQ("bar", fsid);
  EXPECT_TRUE(CrackIsolatedFileSystemName("foo:isolated_bar", &fsid));
  EXPECT_EQ("bar", fsid);
  EXPECT_TRUE(CrackIsolatedFileSystemName("foo:Isolated__bar", &fsid));
  EXPECT_EQ("_bar", fsid);
  EXPECT_TRUE(CrackIsolatedFileSystemName("foo::Isolated_bar", &fsid));
  EXPECT_EQ("bar", fsid);
}

TEST_F(FileSystemUtilTest, RejectBadIsolatedFileSystemName) {
  std::string fsid;
  EXPECT_FALSE(CrackIsolatedFileSystemName("foobar", &fsid));
  EXPECT_FALSE(CrackIsolatedFileSystemName("foo:_bar", &fsid));
  EXPECT_FALSE(CrackIsolatedFileSystemName("foo:Isolatedbar", &fsid));
  EXPECT_FALSE(CrackIsolatedFileSystemName("fooIsolatedbar", &fsid));
  EXPECT_FALSE(CrackIsolatedFileSystemName("foo:Persistent", &fsid));
  EXPECT_FALSE(CrackIsolatedFileSystemName("foo:Temporary", &fsid));
  EXPECT_FALSE(CrackIsolatedFileSystemName("foo:External", &fsid));
  EXPECT_FALSE(CrackIsolatedFileSystemName(":Isolated_bar", &fsid));
  EXPECT_FALSE(CrackIsolatedFileSystemName("foo:Isolated_", &fsid));
}

TEST_F(FileSystemUtilTest, ValidateIsolatedFileSystemId) {
  EXPECT_TRUE(ValidateIsolatedFileSystemId("ABCDEF0123456789ABCDEF0123456789"));
  EXPECT_TRUE(ValidateIsolatedFileSystemId("ABCDEFABCDEFABCDEFABCDEFABCDEFAB"));
  EXPECT_TRUE(ValidateIsolatedFileSystemId("01234567890123456789012345678901"));

  const size_t kExpectedFileSystemIdSize = 32;

  // Should not contain lowercase characters.
  const std::string kLowercaseId = "abcdef0123456789abcdef0123456789";
  EXPECT_EQ(kExpectedFileSystemIdSize, kLowercaseId.size());
  EXPECT_FALSE(ValidateIsolatedFileSystemId(kLowercaseId));

  // Should not be shorter/longer than expected.
  EXPECT_FALSE(ValidateIsolatedFileSystemId(std::string()));

  const std::string kShorterId = "ABCDEF0123456789ABCDEF";
  EXPECT_GT(kExpectedFileSystemIdSize, kShorterId.size());
  EXPECT_FALSE(ValidateIsolatedFileSystemId(kShorterId));

  const std::string kLongerId = "ABCDEF0123456789ABCDEF0123456789ABCDEF";
  EXPECT_LT(kExpectedFileSystemIdSize, kLongerId.size());
  EXPECT_FALSE(ValidateIsolatedFileSystemId(kLongerId));

  // Should not contain not alphabetical nor numerical characters.
  const std::string kSlashId = "ABCD/EFGH/IJKL/MNOP/QRST/UVWX/YZ";
  EXPECT_EQ(kExpectedFileSystemIdSize, kSlashId.size());
  EXPECT_FALSE(ValidateIsolatedFileSystemId(kSlashId));

  const std::string kBackslashId = "ABCD\\EFGH\\IJKL\\MNOP\\QRST\\UVWX\\YZ";
  EXPECT_EQ(kExpectedFileSystemIdSize, kBackslashId.size());
  EXPECT_FALSE(ValidateIsolatedFileSystemId(kBackslashId));

  const std::string kSpaceId = "ABCD EFGH IJKL MNOP QRST UVWX YZ";
  EXPECT_EQ(kExpectedFileSystemIdSize, kSpaceId.size());
  EXPECT_FALSE(ValidateIsolatedFileSystemId(kSpaceId));
}

TEST_F(FileSystemUtilTest, GetIsolatedFileSystemRootURIString) {
  const GURL kOriginURL("http://foo");
  // Percents must be escaped, otherwise they will be unintentionally unescaped.
  const std::string kFileSystemId = "A%20B";
  const std::string kRootName = "C%20D";

  const std::string url_string =
      GetIsolatedFileSystemRootURIString(kOriginURL, kFileSystemId, kRootName);
  EXPECT_EQ("filesystem:http://foo/isolated/A%2520B/C%2520D/", url_string);
}

TEST_F(FileSystemUtilTest, GetExternalFileSystemRootURIString) {
  const GURL kOriginURL("http://foo");
  // Percents must be escaped, otherwise they will be unintentionally unescaped.
  const std::string kMountName = "X%20Y";

  const std::string url_string =
      GetExternalFileSystemRootURIString(kOriginURL, kMountName);
  EXPECT_EQ("filesystem:http://foo/external/X%2520Y/", url_string);
}

}  // namespace
}  // namespace content
