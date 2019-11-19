// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/file_system_url.h"

#include <stddef.h>

#include <utility>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#define FPL FILE_PATH_LITERAL

#if defined(FILE_PATH_USES_DRIVE_LETTERS)
#define DRIVE FPL("C:")
#else
#define DRIVE FPL("/a/")
#endif

using storage::FileSystemURL;
using storage::kFileSystemTypeExternal;
using storage::kFileSystemTypeIsolated;
using storage::kFileSystemTypePersistent;
using storage::kFileSystemTypeTemporary;
using storage::VirtualPath;

namespace content {

namespace {

FileSystemURL CreateFileSystemURL(const std::string& url_string) {
  FileSystemURL url = FileSystemURL::CreateForTest(GURL(url_string));
  EXPECT_TRUE(url.type() != kFileSystemTypeExternal &&
              url.type() != kFileSystemTypeIsolated);
  return url;
}

std::string NormalizedUTF8Path(const base::FilePath& path) {
  return path.NormalizePathSeparators().AsUTF8Unsafe();
}

}  // namespace

TEST(FileSystemURLTest, ParsePersistent) {
  FileSystemURL url = CreateFileSystemURL(
      "filesystem:http://chromium.org/persistent/directory/file");
  ASSERT_TRUE(url.is_valid());
  EXPECT_EQ("http://chromium.org/", url.origin().GetURL().spec());
  EXPECT_EQ(kFileSystemTypePersistent, url.type());
  EXPECT_EQ(FPL("file"), VirtualPath::BaseName(url.path()).value());
  EXPECT_EQ(FPL("directory"), url.path().DirName().value());
}

TEST(FileSystemURLTest, ParseTemporary) {
  FileSystemURL url = CreateFileSystemURL(
      "filesystem:http://chromium.org/temporary/directory/file");
  ASSERT_TRUE(url.is_valid());
  EXPECT_EQ("http://chromium.org/", url.origin().GetURL().spec());
  EXPECT_EQ(kFileSystemTypeTemporary, url.type());
  EXPECT_EQ(FPL("file"), VirtualPath::BaseName(url.path()).value());
  EXPECT_EQ(FPL("directory"), url.path().DirName().value());
}

TEST(FileSystemURLTest, EnsureFilePathIsRelative) {
  FileSystemURL url = CreateFileSystemURL(
      "filesystem:http://chromium.org/temporary/////directory/file");
  ASSERT_TRUE(url.is_valid());
  EXPECT_EQ("http://chromium.org/", url.origin().GetURL().spec());
  EXPECT_EQ(kFileSystemTypeTemporary, url.type());
  EXPECT_EQ(FPL("file"), VirtualPath::BaseName(url.path()).value());
  EXPECT_EQ(FPL("directory"), url.path().DirName().value());
  EXPECT_FALSE(url.path().IsAbsolute());
}

TEST(FileSystemURLTest, RejectBadSchemes) {
  EXPECT_FALSE(CreateFileSystemURL("http://chromium.org/").is_valid());
  EXPECT_FALSE(CreateFileSystemURL("https://chromium.org/").is_valid());
  EXPECT_FALSE(CreateFileSystemURL("file:///foo/bar").is_valid());
  EXPECT_FALSE(CreateFileSystemURL("foobar:///foo/bar").is_valid());
}

TEST(FileSystemURLTest, UnescapePath) {
  FileSystemURL url = CreateFileSystemURL(
      "filesystem:http://chromium.org/persistent/%7Echromium/space%20bar");
  ASSERT_TRUE(url.is_valid());
  EXPECT_EQ(FPL("space bar"), VirtualPath::BaseName(url.path()).value());
  EXPECT_EQ(FPL("~chromium"), url.path().DirName().value());
}

TEST(FileSystemURLTest, RejectBadType) {
  EXPECT_FALSE(
      CreateFileSystemURL("filesystem:http://c.org/foobar/file").is_valid());
  EXPECT_FALSE(CreateFileSystemURL("filesystem:http://c.org/temporaryfoo/file")
                   .is_valid());
}

TEST(FileSystemURLTest, RejectMalformedURL) {
  EXPECT_FALSE(CreateFileSystemURL("filesystem:///foobar/file").is_valid());
  EXPECT_FALSE(CreateFileSystemURL("filesystem:foobar/file").is_valid());
}

TEST(FileSystemURLTest, CompareURLs) {
  const GURL urls[] = {
      GURL("filesystem:http://chromium.org/temporary/dir a/file a"),
      GURL("filesystem:http://chromium.org/temporary/dir a/file a"),
      GURL("filesystem:http://chromium.org/temporary/dir a/file b"),
      GURL("filesystem:http://chromium.org/temporary/dir a/file aa"),
      GURL("filesystem:http://chromium.org/temporary/dir b/file a"),
      GURL("filesystem:http://chromium.org/temporary/dir aa/file b"),
      GURL("filesystem:http://chromium.com/temporary/dir a/file a"),
      GURL("filesystem:https://chromium.org/temporary/dir a/file a")};

  FileSystemURL::Comparator compare;
  for (size_t i = 0; i < base::size(urls); ++i) {
    for (size_t j = 0; j < base::size(urls); ++j) {
      SCOPED_TRACE(testing::Message() << i << " < " << j);
      EXPECT_EQ(urls[i] < urls[j],
                compare(FileSystemURL::CreateForTest(urls[i]),
                        FileSystemURL::CreateForTest(urls[j])));
    }
  }

  const FileSystemURL a = CreateFileSystemURL(
      "filesystem:http://chromium.org/temporary/dir a/file a");
  const FileSystemURL b = CreateFileSystemURL(
      "filesystem:http://chromium.org/persistent/dir a/file a");
  EXPECT_EQ(a.type() < b.type(), compare(a, b));
  EXPECT_EQ(b.type() < a.type(), compare(b, a));
}

TEST(FileSystemURLTest, IsParent) {
  const std::string root1 =
      GetFileSystemRootURI(GURL("http://example.com"), kFileSystemTypeTemporary)
          .spec();
  const std::string root2 = GetFileSystemRootURI(GURL("http://example.com"),
                                                 kFileSystemTypePersistent)
                                .spec();
  const std::string root3 = GetFileSystemRootURI(GURL("http://chromium.org"),
                                                 kFileSystemTypeTemporary)
                                .spec();

  const std::string parent("dir");
  const std::string child("dir/child");
  const std::string other("other");

  // True cases.
  EXPECT_TRUE(CreateFileSystemURL(root1 + parent)
                  .IsParent(CreateFileSystemURL(root1 + child)));
  EXPECT_TRUE(CreateFileSystemURL(root2 + parent)
                  .IsParent(CreateFileSystemURL(root2 + child)));

  // False cases: the path is not a child.
  EXPECT_FALSE(CreateFileSystemURL(root1 + parent)
                   .IsParent(CreateFileSystemURL(root1 + other)));
  EXPECT_FALSE(CreateFileSystemURL(root1 + parent)
                   .IsParent(CreateFileSystemURL(root1 + parent)));
  EXPECT_FALSE(CreateFileSystemURL(root1 + child)
                   .IsParent(CreateFileSystemURL(root1 + parent)));

  // False case: different types.
  EXPECT_FALSE(CreateFileSystemURL(root1 + parent)
                   .IsParent(CreateFileSystemURL(root2 + child)));

  // False case: different origins.
  EXPECT_FALSE(CreateFileSystemURL(root1 + parent)
                   .IsParent(CreateFileSystemURL(root3 + child)));
}

TEST(FileSystemURLTest, ToGURL) {
  EXPECT_TRUE(FileSystemURL().ToGURL().is_empty());
  const char* kTestURL[] = {
      "filesystem:http://chromium.org/persistent/directory/file0",
      "filesystem:http://chromium.org/temporary/directory/file1",
      "filesystem:http://chromium.org/isolated/directory/file2",
      "filesystem:http://chromium.org/external/directory/file2",
      "filesystem:http://chromium.org/test/directory/file3",
      "filesystem:http://chromium.org/test/plus%2B/space%20/colon%3A",
  };

  for (const char* url : kTestURL)
    EXPECT_EQ(url, FileSystemURL::CreateForTest(GURL(url)).ToGURL().spec());
}

TEST(FileSystemURLTest, DebugString) {
  const GURL kOrigin("http://example.com");
  const base::FilePath kPath(FPL("dir/file"));

  const FileSystemURL kURL1 = FileSystemURL::CreateForTest(
      url::Origin::Create(kOrigin), kFileSystemTypeTemporary, kPath);
  EXPECT_EQ(
      "filesystem:http://example.com/temporary/" + NormalizedUTF8Path(kPath),
      kURL1.DebugString());
}

TEST(FileSystemURLTest, IsInSameFileSystem) {
  FileSystemURL url_foo_temp_a = FileSystemURL::CreateForTest(
      url::Origin::Create(GURL("http://foo")), kFileSystemTypeTemporary,
      base::FilePath::FromUTF8Unsafe("a"));
  FileSystemURL url_foo_temp_b = FileSystemURL::CreateForTest(
      url::Origin::Create(GURL("http://foo")), kFileSystemTypeTemporary,
      base::FilePath::FromUTF8Unsafe("b"));
  FileSystemURL url_foo_perm_a = FileSystemURL::CreateForTest(
      url::Origin::Create(GURL("http://foo")), kFileSystemTypePersistent,
      base::FilePath::FromUTF8Unsafe("a"));
  FileSystemURL url_bar_temp_a = FileSystemURL::CreateForTest(
      url::Origin::Create(GURL("http://bar")), kFileSystemTypeTemporary,
      base::FilePath::FromUTF8Unsafe("a"));
  FileSystemURL url_bar_perm_a = FileSystemURL::CreateForTest(
      url::Origin::Create(GURL("http://bar")), kFileSystemTypePersistent,
      base::FilePath::FromUTF8Unsafe("a"));

  EXPECT_TRUE(url_foo_temp_a.IsInSameFileSystem(url_foo_temp_a));
  EXPECT_TRUE(url_foo_temp_a.IsInSameFileSystem(url_foo_temp_b));
  EXPECT_FALSE(url_foo_temp_a.IsInSameFileSystem(url_foo_perm_a));
  EXPECT_FALSE(url_foo_temp_a.IsInSameFileSystem(url_bar_temp_a));
  EXPECT_FALSE(url_foo_temp_a.IsInSameFileSystem(url_bar_perm_a));
}

TEST(FileSystemURLTest, ValidAfterMoves) {
  // Move constructor.
  {
    FileSystemURL original = FileSystemURL::CreateForTest(
        url::Origin::Create(GURL("http://foo")), kFileSystemTypeTemporary,
        base::FilePath::FromUTF8Unsafe("a"));
    EXPECT_TRUE(original.is_valid());
    FileSystemURL new_url(std::move(original));
    EXPECT_TRUE(new_url.is_valid());
    EXPECT_TRUE(original.is_valid());
  }

  // Move operator.
  {
    FileSystemURL original = FileSystemURL::CreateForTest(
        url::Origin::Create(GURL("http://foo")), kFileSystemTypeTemporary,
        base::FilePath::FromUTF8Unsafe("a"));
    EXPECT_TRUE(original.is_valid());
    FileSystemURL new_url;
    new_url = std::move(std::move(original));
    EXPECT_TRUE(new_url.is_valid());
    EXPECT_TRUE(original.is_valid());
  }
}

}  // namespace content
