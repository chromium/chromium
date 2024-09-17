// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "storage/browser/file_system/file_system_url.h"

#include <stddef.h>

#include <utility>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/safe_base_name.h"
#include "base/strings/string_number_conversions.h"
#include "storage/browser/file_system/file_system_features.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

#define FPL FILE_PATH_LITERAL

#if defined(FILE_PATH_USES_DRIVE_LETTERS)
#define DRIVE FPL("C:")
#else
#define DRIVE FPL("/a/")
#endif

constexpr int kBucketId = 1;

namespace storage {

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

BucketLocator CreateNonDefaultBucket() {
  return BucketLocator(
      BucketId::FromUnsafeValue(kBucketId),
      blink::StorageKey::CreateFromStringForTesting("http://www.example.com/"),
      blink::mojom::StorageType::kTemporary, /*is_default=*/false);
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

TEST(FileSystemURLTest, CreateSibling) {
  // sibling_name is the common CreateSibling argument used further below.
  //
  // Another CreateSibling precondition is that the sibling_name is non-empty.
  // We don't test for that here because a base::SafeBaseName is designed to be
  // non-empty by construction: the base::SafeBaseName::Create factory function
  // returns std::optional<base::SafeBaseName> not base::SafeBaseName.
  //
  // See also TODO(crbug.com/40205226)
  const base::SafeBaseName sibling_name =
      *base::SafeBaseName::Create(FPL("sister"));

  // Test the happy case.
  {
    FileSystemURL original = CreateFileSystemURL(
        "filesystem:http://chromium.org/temporary/parent/brother");
    FileSystemURL sibling = original.CreateSibling(sibling_name);

    ASSERT_TRUE(original.is_valid());
    ASSERT_TRUE(sibling.is_valid());
    EXPECT_EQ("filesystem:http://chromium.org/temporary/parent/sister",
              sibling.ToGURL().spec());
    EXPECT_EQ("http://chromium.org/", sibling.origin().GetURL().spec());
    EXPECT_EQ(kFileSystemTypeTemporary, sibling.type());

    EXPECT_FALSE(original.virtual_path().empty());
    EXPECT_FALSE(original.path().empty());

    EXPECT_FALSE(sibling.virtual_path().empty());
    EXPECT_EQ(FPL("parent"),
              VirtualPath::DirName(sibling.virtual_path()).value());
    EXPECT_EQ(FPL("sister"),
              VirtualPath::BaseName(sibling.virtual_path()).value());

    EXPECT_FALSE(sibling.path().empty());
    EXPECT_EQ(FPL("parent"), sibling.path().DirName().value());
    EXPECT_EQ(FPL("sister"), sibling.path().BaseName().value());
  }

  // Test starting from an empty virtual path (whether by a "" or "/." suffix).
  // CreateSibling is also happy here.
  const std::string string_forms[] = {
      "filesystem:http://chromium.org/temporary",
      "filesystem:http://chromium.org/temporary/.",
  };
  for (const auto& string_form : string_forms) {
    FileSystemURL original = CreateFileSystemURL(string_form);
    FileSystemURL sibling = original.CreateSibling(sibling_name);

    SCOPED_TRACE(testing::Message() << "string_form=" << string_form);

    ASSERT_TRUE(original.is_valid());
    ASSERT_TRUE(sibling.is_valid());
    EXPECT_EQ("filesystem:http://chromium.org/temporary/sister",
              sibling.ToGURL().spec());
    EXPECT_EQ("http://chromium.org/", sibling.origin().GetURL().spec());
    EXPECT_EQ(kFileSystemTypeTemporary, sibling.type());

    EXPECT_TRUE(original.virtual_path().empty());
    EXPECT_TRUE(original.path().empty());

    EXPECT_FALSE(sibling.virtual_path().empty());
    EXPECT_EQ(FPL("."), VirtualPath::DirName(sibling.virtual_path()).value());
    EXPECT_EQ(FPL("sister"),
              VirtualPath::BaseName(sibling.virtual_path()).value());

    EXPECT_TRUE(sibling.path().empty());
  }

  // Test starting from an invalid URL.
  {
    FileSystemURL original;
    FileSystemURL sibling = original.CreateSibling(sibling_name);
    EXPECT_FALSE(sibling.is_valid());
  }

  const base::FilePath::StringType path_pairs[][2] = {
      // Empty cracked path.
      {FPL(""), FPL("")},
      // Absolute cracked paths.
      {FPL("/apple"), FPL("/sister")},
      {FPL("/banana"), FPL("")},
      {FPL("/green/apple"), FPL("/green/sister")},
      {FPL("/green/banana"), FPL("")},
      {FPL("/its/a/trapple"), FPL("")},
      // Relative cracked paths.
      {FPL("."), FPL("")},
      {FPL("./apple"), FPL("sister")},
      {FPL("./banana"), FPL("")},
      {FPL("apple"), FPL("sister")},
      {FPL("banana"), FPL("")},
      {FPL("green/apple"), FPL("green/sister")},
      {FPL("green/banana"), FPL("")},
      {FPL("its/a/trapple"), FPL("")},
  };
  for (const auto& path_pair : path_pairs) {
    // Interesting CreateForTest arguments, derived from the path_pair.
    const base::FilePath virtual_path(FPL("red/apple"));
    const base::FilePath cracked_path =
        base::FilePath(path_pair[0]).NormalizePathSeparators();

    // Non-interesting CreateForTest arguments, independent of path_pair.
    const blink::StorageKey storage_key =
        blink::StorageKey::CreateFromStringForTesting("http://foo");
    const FileSystemType mount_type = kFileSystemTypeExternal;
    const std::string mount_filesystem_id = "";
    const FileSystemType cracked_type = kFileSystemTypeTest;
    const std::string filesystem_id = "";
    const FileSystemMountOption mount_option;

    SCOPED_TRACE(testing::Message() << "cracked_path=" << cracked_path);

    FileSystemURL original = FileSystemURL::CreateForTest(
        storage_key, mount_type, virtual_path, mount_filesystem_id,
        cracked_type, cracked_path, filesystem_id, mount_option);
    FileSystemURL sibling = original.CreateSibling(sibling_name);

    // Expected values.
    const base::FilePath expected_sibling_path =
        base::FilePath(path_pair[1]).NormalizePathSeparators();
    const bool expected_sibling_is_valid =
        cracked_path.empty() || !expected_sibling_path.empty();

    EXPECT_EQ(expected_sibling_is_valid, sibling.is_valid());
    EXPECT_EQ(expected_sibling_path, sibling.path());
  }
}

TEST(FileSystemURLTest, CreateSiblingPreservesBuckets) {
  BucketLocator bucket = CreateNonDefaultBucket();

  FileSystemURL a_bucket = CreateFileSystemURL(
      "filesystem:http://chromium.org/temporary/i/has/a.bucket");
  a_bucket.SetBucket(bucket);
  FileSystemURL with =
      a_bucket.CreateSibling(*base::SafeBaseName::Create(FPL("with")));

  FileSystemURL no_bucket = CreateFileSystemURL(
      "filesystem:http://chromium.org/temporary/i/has/no.bucket");
  FileSystemURL without =
      no_bucket.CreateSibling(*base::SafeBaseName::Create(FPL("without")));

  EXPECT_EQ(with.bucket(), bucket);
  EXPECT_EQ(without.bucket(), std::nullopt);
}

#if BUILDFLAG(IS_ANDROID)
// Android content-URIs do not support siblings.
TEST(FileSystemURLTest, CreateSiblingNotSupportedForContentUri) {
  FileSystemURL url = FileSystemURL::CreateForTest(
      blink::StorageKey::CreateFromStringForTesting("http://foo"),
      kFileSystemTypeTemporary,
      base::FilePath::FromUTF8Unsafe("content://provider/a"));
  FileSystemURL sibling = url.CreateSibling(*base::SafeBaseName::Create("b"));
  EXPECT_FALSE(sibling.is_valid());
}
#endif

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
  for (size_t i = 0; i < std::size(urls); ++i) {
    for (size_t j = 0; j < std::size(urls); ++j) {
      SCOPED_TRACE(testing::Message() << i << " < " << j);
      EXPECT_EQ(urls[i] < urls[j],
                compare(FileSystemURL::CreateForTest(urls[i]),
                        FileSystemURL::CreateForTest(urls[j])));
    }
  }

  FileSystemURL a = CreateFileSystemURL(
      "filesystem:http://chromium.org/temporary/dir a/file a");
  FileSystemURL b = CreateFileSystemURL(
      "filesystem:http://chromium.org/persistent/dir a/file a");
  EXPECT_EQ(a.type() < b.type(), compare(a, b));
  EXPECT_EQ(b.type() < a.type(), compare(b, a));

  // Testing comparison between FileSystemURLs that have a non-empty,
  // non-default bucket value set.
  BucketLocator bucket = CreateNonDefaultBucket();
  a.SetBucket(bucket);
  b.SetBucket(bucket);
  // An identical bucket added to each URL does not alter type mismatch.
  EXPECT_EQ(a.type() < b.type(), compare(a, b));
  // c is a copy of a, just without a bucket value set.
  const FileSystemURL c = CreateFileSystemURL(
      "filesystem:http://chromium.org/temporary/dir a/file a");
  // Ensure that buckets are taken into consideration for comparison.
  EXPECT_EQ(a.bucket() < c.bucket(), compare(a, c));
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
  const std::string grandchild("dir/child/grandchild");
  const std::string other("other");

  // True cases.
  EXPECT_TRUE(
      CreateFileSystemURL(root1).IsParent(CreateFileSystemURL(root1 + parent)));
  EXPECT_TRUE(
      CreateFileSystemURL(root1).IsParent(CreateFileSystemURL(root1 + child)));
  EXPECT_TRUE(CreateFileSystemURL(root1).IsParent(
      CreateFileSystemURL(root1 + grandchild)));
  EXPECT_TRUE(CreateFileSystemURL(root1 + parent)
                  .IsParent(CreateFileSystemURL(root1 + child)));
  EXPECT_TRUE(CreateFileSystemURL(root1 + parent)
                  .IsParent(CreateFileSystemURL(root1 + grandchild)));
  EXPECT_TRUE(CreateFileSystemURL(root1 + child)
                  .IsParent(CreateFileSystemURL(root1 + grandchild)));
  EXPECT_TRUE(
      CreateFileSystemURL(root2).IsParent(CreateFileSystemURL(root2 + parent)));
  EXPECT_TRUE(CreateFileSystemURL(root2 + parent)
                  .IsParent(CreateFileSystemURL(root2 + child)));
  EXPECT_TRUE(CreateFileSystemURL(root2 + parent)
                  .IsParent(CreateFileSystemURL(root2 + grandchild)));

  // False cases: the path is not a child.
  EXPECT_FALSE(CreateFileSystemURL(root1).IsParent(CreateFileSystemURL(root1)));
  EXPECT_FALSE(CreateFileSystemURL(root1 + parent)
                   .IsParent(CreateFileSystemURL(root1 + other)));
  EXPECT_FALSE(CreateFileSystemURL(root1 + parent)
                   .IsParent(CreateFileSystemURL(root1 + parent)));
  EXPECT_FALSE(CreateFileSystemURL(root1 + child)
                   .IsParent(CreateFileSystemURL(root1 + parent)));

  // False case: different types.
  EXPECT_FALSE(CreateFileSystemURL(root1 + parent)
                   .IsParent(CreateFileSystemURL(root2 + child)));
  EXPECT_FALSE(
      CreateFileSystemURL(root1).IsParent(CreateFileSystemURL(root2 + parent)));

  // False case: different origins.
  EXPECT_FALSE(CreateFileSystemURL(root1 + parent)
                   .IsParent(CreateFileSystemURL(root3 + child)));
  EXPECT_FALSE(
      CreateFileSystemURL(root1).IsParent(CreateFileSystemURL(root3 + parent)));
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
  const base::FilePath kPath(FPL("dir/file"));

  const FileSystemURL kURL1 = FileSystemURL::CreateForTest(
      blink::StorageKey::CreateFromStringForTesting("http://example.com"),
      kFileSystemTypeTemporary, kPath);
  EXPECT_EQ("{ uri: filesystem:http://example.com/temporary/" +
                NormalizedUTF8Path(kPath) +
                ", storage key: " + kURL1.storage_key().GetDebugString() + " }",
            kURL1.DebugString());
  const FileSystemURL kURL2 = FileSystemURL::CreateForTest(
      blink::StorageKey::CreateFromStringForTesting("http://example.com"),
      kFileSystemTypeLocal, kPath);
  EXPECT_EQ("{ path: " + NormalizedUTF8Path(kPath) +
                ", storage key: " + kURL1.storage_key().GetDebugString() + " }",
            kURL2.DebugString());
  FileSystemURL kURL3 = FileSystemURL::CreateForTest(
      blink::StorageKey::CreateFromStringForTesting("http://example.com"),
      kFileSystemTypeTemporary, kPath);
  kURL3.SetBucket(CreateNonDefaultBucket());
  EXPECT_EQ("{ uri: filesystem:http://example.com/temporary/" +
                NormalizedUTF8Path(kPath) +
                ", storage key: " + kURL3.storage_key().GetDebugString() +
                ", bucket id: " + base::NumberToString(kBucketId) + " }",
            kURL3.DebugString());
}

TEST(FileSystemURLTest, IsInSameFileSystem) {
  FileSystemURL url_foo_temp_a = FileSystemURL::CreateForTest(
      blink::StorageKey::CreateFromStringForTesting("http://foo"),
      kFileSystemTypeTemporary, base::FilePath::FromUTF8Unsafe("a"));
  FileSystemURL url_foo_temp_b = FileSystemURL::CreateForTest(
      blink::StorageKey::CreateFromStringForTesting("http://foo"),
      kFileSystemTypeTemporary, base::FilePath::FromUTF8Unsafe("b"));
  FileSystemURL url_foo_perm_a = FileSystemURL::CreateForTest(
      blink::StorageKey::CreateFromStringForTesting("http://foo"),
      kFileSystemTypePersistent, base::FilePath::FromUTF8Unsafe("a"));
  FileSystemURL url_bar_temp_a = FileSystemURL::CreateForTest(
      blink::StorageKey::CreateFromStringForTesting("http://bar"),
      kFileSystemTypeTemporary, base::FilePath::FromUTF8Unsafe("a"));
  FileSystemURL url_bar_perm_a = FileSystemURL::CreateForTest(
      blink::StorageKey::CreateFromStringForTesting("http://bar"),
      kFileSystemTypePersistent, base::FilePath::FromUTF8Unsafe("a"));
  FileSystemURL url_opaque_a =
      FileSystemURL::CreateForTest(blink::StorageKey(), kFileSystemTypeLocal,
                                   base::FilePath::FromUTF8Unsafe("a"));
  FileSystemURL url_opaque_b =
      FileSystemURL::CreateForTest(blink::StorageKey(), kFileSystemTypeLocal,
                                   base::FilePath::FromUTF8Unsafe("b"));
  FileSystemURL url_invalid_a;
  FileSystemURL url_invalid_b = url_invalid_a;

  EXPECT_TRUE(url_foo_temp_a.IsInSameFileSystem(url_foo_temp_a));
  EXPECT_TRUE(url_foo_temp_a.IsInSameFileSystem(url_foo_temp_b));
  EXPECT_FALSE(url_foo_temp_a.IsInSameFileSystem(url_foo_perm_a));
  EXPECT_FALSE(url_foo_temp_a.IsInSameFileSystem(url_bar_temp_a));
  EXPECT_FALSE(url_foo_temp_a.IsInSameFileSystem(url_bar_perm_a));
  EXPECT_FALSE(url_foo_temp_a.IsInSameFileSystem(url_opaque_a));
  EXPECT_FALSE(url_foo_temp_a.IsInSameFileSystem(url_opaque_b));
  EXPECT_FALSE(url_foo_temp_a.IsInSameFileSystem(url_invalid_a));
  EXPECT_FALSE(url_foo_temp_a.IsInSameFileSystem(url_invalid_b));

  // Test that non-empty, non-default bucket values are taken into account when
  // determining of two URLs are in the same FileSystem.
  BucketLocator bucket = CreateNonDefaultBucket();
  url_foo_temp_a.SetBucket(bucket);
  url_foo_temp_b.SetBucket(bucket);
  EXPECT_TRUE(url_foo_temp_a.IsInSameFileSystem(url_foo_temp_b));
  // url_foo_temp_c is identical to url_foo_temp_a but without a bucket value.
  FileSystemURL url_foo_temp_c = FileSystemURL::CreateForTest(
      blink::StorageKey::CreateFromStringForTesting("http://foo"),
      kFileSystemTypeTemporary, base::FilePath::FromUTF8Unsafe("a"));
  EXPECT_FALSE(url_foo_temp_a.IsInSameFileSystem(url_foo_temp_c));

  // Test that opaque origins with differing nonces are considered to be in
  // the same file system.
  EXPECT_NE(url_opaque_a, url_opaque_b);
  EXPECT_TRUE(url_opaque_a.IsInSameFileSystem(url_opaque_b));

  // Test that identical, invalid URLs are considered not to be in the same
  // file system.
  EXPECT_EQ(url_invalid_a, url_invalid_b);
  EXPECT_FALSE(url_invalid_a.IsInSameFileSystem(url_invalid_b));

#if BUILDFLAG(IS_ANDROID)
  // Android content-URIs are never considered same-file-system.
  url_foo_temp_a = FileSystemURL::CreateForTest(
      blink::StorageKey::CreateFromStringForTesting("http://foo"),
      kFileSystemTypeTemporary, base::FilePath::FromUTF8Unsafe("a"));
  FileSystemURL url_foo_temp_cu_a = FileSystemURL::CreateForTest(
      blink::StorageKey::CreateFromStringForTesting("http://foo"),
      kFileSystemTypeTemporary,
      base::FilePath::FromUTF8Unsafe("content://provider/a"));
  FileSystemURL url_foo_temp_cu_b = FileSystemURL::CreateForTest(
      blink::StorageKey::CreateFromStringForTesting("http://foo"),
      kFileSystemTypeTemporary,
      base::FilePath::FromUTF8Unsafe("content://provider/b"));
  EXPECT_FALSE(url_foo_temp_cu_a.IsInSameFileSystem(url_foo_temp_cu_a));
  EXPECT_FALSE(url_foo_temp_cu_a.IsInSameFileSystem(url_foo_temp_cu_b));
  EXPECT_FALSE(url_foo_temp_cu_a.IsInSameFileSystem(url_foo_temp_a));
#endif
}

TEST(FileSystemURLTest, ValidAfterMoves) {
  // Move constructor.
  {
    FileSystemURL original = FileSystemURL::CreateForTest(
        blink::StorageKey::CreateFromStringForTesting("http://foo"),
        kFileSystemTypeTemporary, base::FilePath::FromUTF8Unsafe("a"));
    EXPECT_TRUE(original.is_valid());
    FileSystemURL new_url(std::move(original));
    EXPECT_TRUE(new_url.is_valid());
    EXPECT_TRUE(original.is_valid());
  }

  // Move operator.
  {
    FileSystemURL original = FileSystemURL::CreateForTest(
        blink::StorageKey::CreateFromStringForTesting("http://foo"),
        kFileSystemTypeTemporary, base::FilePath::FromUTF8Unsafe("a"));
    EXPECT_TRUE(original.is_valid());
    FileSystemURL new_url;
    new_url = std::move(std::move(original));
    EXPECT_TRUE(new_url.is_valid());
    EXPECT_TRUE(original.is_valid());
  }
}

}  // namespace storage
