// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <gmock/gmock.h>
#include <ppapi/c/ppb_file_io.h>
#include <ppapi/c/pp_errors.h>
#include <ppapi/c/pp_instance.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "fake_ppapi/fake_pepper_interface_url_loader.h"

#include "nacl_io/dir_node.h"
#include "nacl_io/httpfs/http_fs.h"
#include "nacl_io/kernel_handle.h"
#include "nacl_io/kernel_intercept.h"
#include "nacl_io/osdirent.h"
#include "nacl_io/osunistd.h"

using namespace nacl_io;

namespace {

class HttpFsForTesting : public HttpFs {
 public:
  HttpFsForTesting(StringMap_t map, PepperInterface* ppapi) {
    FsInitArgs args(1);
    args.string_map = map;
    args.ppapi = ppapi;
    EXPECT_EQ(0, Init(args));
  }

  using HttpFs::GetNodeCacheForTesting;
  using HttpFs::ParseManifest;
  using HttpFs::FindOrCreateDir;
};

enum {
  kStringMapParamCacheNone = 0,
  kStringMapParamCacheContent = 1,
  kStringMapParamCacheStat = 2,
  kStringMapParamCacheContentStat =
      kStringMapParamCacheContent | kStringMapParamCacheStat,
};
typedef uint32_t StringMapParam;

StringMap_t MakeStringMap(StringMapParam param) {
  StringMap_t smap;
  if (param & kStringMapParamCacheContent)
    smap["cache_content"] = "true";
  else
    smap["cache_content"] = "false";

  if (param & kStringMapParamCacheStat)
    smap["cache_stat"] = "true";
  else
    smap["cache_stat"] = "false";
  return smap;
}

class HttpFsTest : public ::testing::TestWithParam<StringMapParam> {
 public:
  HttpFsTest();

 protected:
  FakePepperInterfaceURLLoader ppapi_;
  HttpFsForTesting fs_;
};

HttpFsTest::HttpFsTest() : fs_(MakeStringMap(GetParam()), &ppapi_) {}

class HttpFsLargeFileTest : public HttpFsTest {
 public:
  HttpFsLargeFileTest() {}
};

}  // namespace

TEST_P(HttpFsTest, OpenAndCloseServerError) {
  EXPECT_TRUE(ppapi_.server_template()->AddError("file", 500));

  ScopedNode node;
  ASSERT_EQ(EIO, fs_.Open(Path("/file"), O_RDONLY, &node));
}

TEST_P(HttpFsTest, ReadPartial) {
  const char contents[] = "0123456789abcdefg";
  ASSERT_TRUE(ppapi_.server_template()->AddEntity("file", contents, NULL));
  ppapi_.server_template()->set_allow_partial(true);

  int result_bytes = 0;

  char buf[10];
  memset(&buf[0], 0, sizeof(buf));

  ScopedNode node;
  ASSERT_EQ(0, fs_.Open(Path("/file"), O_RDONLY, &node));
  HandleAttr attr;
  EXPECT_EQ(0, node->Read(attr, buf, sizeof(buf) - 1, &result_bytes));
  EXPECT_EQ(sizeof(buf) - 1, result_bytes);
  EXPECT_STREQ("012345678", &buf[0]);

  // Read is clamped when reading past the end of the file.
  attr.offs = 10;
  ASSERT_EQ(0, node->Read(attr, buf, sizeof(buf) - 1, &result_bytes));
  ASSERT_EQ(strlen("abcdefg"), result_bytes);
  buf[result_bytes] = 0;
  EXPECT_STREQ("abcdefg", &buf[0]);

  // Read nothing when starting past the end of the file.
  attr.offs = 100;
  EXPECT_EQ(0, node->Read(attr, &buf[0], sizeof(buf), &result_bytes));
  EXPECT_EQ(0, result_bytes);
}

TEST_P(HttpFsTest, ReadPartialNoServerSupport) {
  const char contents[] = "0123456789abcdefg";
  ASSERT_TRUE(ppapi_.server_template()->AddEntity("file", contents, NULL));
  ppapi_.server_template()->set_allow_partial(false);

  int result_bytes = 0;

  char buf[10];
  memset(&buf[0], 0, sizeof(buf));

  ScopedNode node;
  ASSERT_EQ(0, fs_.Open(Path("/file"), O_RDONLY, &node));
  HandleAttr attr;
  EXPECT_EQ(0, node->Read(attr, buf, sizeof(buf) - 1, &result_bytes));
  EXPECT_EQ(sizeof(buf) - 1, result_bytes);
  EXPECT_STREQ("012345678", &buf[0]);

  // Read is clamped when reading past the end of the file.
  attr.offs = 10;
  ASSERT_EQ(0, node->Read(attr, buf, sizeof(buf) - 1, &result_bytes));
  ASSERT_EQ(strlen("abcdefg"), result_bytes);
  buf[result_bytes] = 0;
  EXPECT_STREQ("abcdefg", &buf[0]);

  // Read nothing when starting past the end of the file.
  attr.offs = 100;
  EXPECT_EQ(0, node->Read(attr, &buf[0], sizeof(buf), &result_bytes));
  EXPECT_EQ(0, result_bytes);
}

TEST_P(HttpFsTest, Write) {
  const char contents[] = "contents";
  ASSERT_TRUE(ppapi_.server_template()->AddEntity("file", contents, NULL));

  ScopedNode node;
  ASSERT_EQ(0, fs_.Open(Path("/file"), O_WRONLY, &node));

  // Writing always fails.
  HandleAttr attr;
  attr.offs = 3;
  int bytes_written = 1;  // Set to a non-zero value.
  EXPECT_EQ(EACCES, node->Write(attr, "struct", 6, &bytes_written));
  EXPECT_EQ(0, bytes_written);
}

TEST_P(HttpFsTest, GetStat) {
  const char contents[] = "contents";
  ASSERT_TRUE(ppapi_.server_template()->AddEntity("file", contents, NULL));

  ScopedNode node;
  ASSERT_EQ(0, fs_.Open(Path("/file"), O_RDONLY, &node));

  struct stat statbuf;
  EXPECT_EQ(0, node->GetStat(&statbuf));
  EXPECT_EQ(S_IFREG | S_IRUSR | S_IRGRP | S_IROTH, statbuf.st_mode);
  EXPECT_EQ(strlen(contents), statbuf.st_size);
  // These are not currently set.
  EXPECT_EQ(0, statbuf.st_atime);
  EXPECT_EQ(0, statbuf.st_ctime);
  EXPECT_EQ(0, statbuf.st_mtime);
}

TEST_P(HttpFsTest, FTruncate) {
  const char contents[] = "contents";
  ASSERT_TRUE(ppapi_.server_template()->AddEntity("file", contents, NULL));

  ScopedNode node;
  ASSERT_EQ(0, fs_.Open(Path("/file"), O_RDWR, &node));
  EXPECT_EQ(EACCES, node->FTruncate(4));
}

// Instantiate the above tests for all caching types.
INSTANTIATE_TEST_SUITE_P(
    Default,
    HttpFsTest,
    ::testing::Values((uint32_t)kStringMapParamCacheNone,
                      (uint32_t)kStringMapParamCacheContent,
                      (uint32_t)kStringMapParamCacheStat,
                      (uint32_t)kStringMapParamCacheContentStat));

TEST_P(HttpFsLargeFileTest, ReadPartial) {
  const char contents[] = "0123456789abcdefg";
  off_t size = 0x110000000ll;
  ASSERT_TRUE(
      ppapi_.server_template()->AddEntity("file", contents, size, NULL));
  ppapi_.server_template()->set_send_content_length(true);
  ppapi_.server_template()->set_allow_partial(true);

  int result_bytes = 0;

  char buf[10];
  memset(&buf[0], 0, sizeof(buf));

  ScopedNode node;
  ASSERT_EQ(0, fs_.Open(Path("/file"), O_RDONLY, &node));
  HandleAttr attr;
  EXPECT_EQ(0, node->Read(attr, buf, sizeof(buf) - 1, &result_bytes));
  EXPECT_EQ(sizeof(buf) - 1, result_bytes);
  EXPECT_STREQ("012345678", &buf[0]);

  // Read is clamped when reading past the end of the file.
  attr.offs = size - 7;
  ASSERT_EQ(0, node->Read(attr, buf, sizeof(buf) - 1, &result_bytes));
  ASSERT_EQ(strlen("abcdefg"), result_bytes);
  buf[result_bytes] = 0;
  EXPECT_STREQ("abcdefg", &buf[0]);

  // Read nothing when starting past the end of the file.
  attr.offs = size + 100;
  EXPECT_EQ(0, node->Read(attr, &buf[0], sizeof(buf), &result_bytes));
  EXPECT_EQ(0, result_bytes);
}

TEST_P(HttpFsLargeFileTest, GetStat) {
  const char contents[] = "contents";
  off_t size = 0x110000000ll;
  ASSERT_TRUE(
      ppapi_.server_template()->AddEntity("file", contents, size, NULL));
  // TODO(binji): If the server doesn't send the content length, this operation
  // will be incredibly slow; it will attempt to read all of the data from the
  // server to find the file length. Can we do anything smarter?
  ppapi_.server_template()->set_send_content_length(true);

  ScopedNode node;
  ASSERT_EQ(0, fs_.Open(Path("/file"), O_RDONLY, &node));

  struct stat statbuf;
  EXPECT_EQ(0, node->GetStat(&statbuf));
  EXPECT_TRUE(S_ISREG(statbuf.st_mode));
  EXPECT_EQ(S_IRUSR | S_IRGRP | S_IROTH, statbuf.st_mode & S_MODEBITS);
  EXPECT_EQ(size, statbuf.st_size);
  // These are not currently set.
  EXPECT_EQ(0, statbuf.st_atime);
  EXPECT_EQ(0, statbuf.st_ctime);
  EXPECT_EQ(0, statbuf.st_mtime);
}

// Instantiate the large file tests, only when cache content is off.
// TODO(binji): make cache content smarter, so it doesn't try to cache enormous
// files. See http://crbug.com/369279.
INSTANTIATE_TEST_SUITE_P(Default,
                         HttpFsLargeFileTest,
                         ::testing::Values((uint32_t)kStringMapParamCacheNone,
                                           (uint32_t)kStringMapParamCacheStat));

TEST(HttpFsDirTest, Root) {
  StringMap_t args;
  HttpFsForTesting fs(args, NULL);

  // Check root node is directory
  ScopedNode node;
  ASSERT_EQ(0, fs.Open(Path("/"), O_RDONLY, &node));
  ASSERT_TRUE(node->IsaDir());

  // We have to r+w access to the root node
  struct stat buf;
  ASSERT_EQ(0, node->GetStat(&buf));
  ASSERT_EQ(S_IXUSR | S_IRUSR, buf.st_mode & S_IRWXU);
}

TEST(HttpFsDirTest, Mkdir) {
  StringMap_t args;
  HttpFsForTesting fs(args, NULL);
  char manifest[] = "-r-- 123 /mydir/foo\n-rw- 234 /thatdir/bar\n";
  ASSERT_EQ(0, fs.ParseManifest(manifest));
  // mkdir of existing directories should give "File exists".
  EXPECT_EQ(EEXIST, fs.Mkdir(Path("/"), 0));
  EXPECT_EQ(EEXIST, fs.Mkdir(Path("/mydir"), 0));
  // mkdir of non-existent directories should give "Permission denied".
  EXPECT_EQ(EACCES, fs.Mkdir(Path("/non_existent"), 0));
}

TEST(HttpFsDirTest, Rmdir) {
  StringMap_t args;
  HttpFsForTesting fs(args, NULL);
  char manifest[] = "-r-- 123 /mydir/foo\n-rw- 234 /thatdir/bar\n";
  ASSERT_EQ(0, fs.ParseManifest(manifest));
  // Rmdir on existing dirs should give "Permission Denied"
  EXPECT_EQ(EACCES, fs.Rmdir(Path("/")));
  EXPECT_EQ(EACCES, fs.Rmdir(Path("/mydir")));
  // Rmdir on existing files should give "Not a direcotory"
  EXPECT_EQ(ENOTDIR, fs.Rmdir(Path("/mydir/foo")));
  // Rmdir on non-existent files should give "No such file or directory"
  EXPECT_EQ(ENOENT, fs.Rmdir(Path("/non_existent")));
}

TEST(HttpFsDirTest, Unlink) {
  StringMap_t args;
  HttpFsForTesting fs(args, NULL);
  char manifest[] = "-r-- 123 /mydir/foo\n-rw- 234 /thatdir/bar\n";
  ASSERT_EQ(0, fs.ParseManifest(manifest));
  // Unlink of existing files should give "Permission Denied"
  EXPECT_EQ(EACCES, fs.Unlink(Path("/mydir/foo")));
  // Unlink of existing directory should give "Is a directory"
  EXPECT_EQ(EISDIR, fs.Unlink(Path("/mydir")));
  // Unlink of non-existent files should give "No such file or directory"
  EXPECT_EQ(ENOENT, fs.Unlink(Path("/non_existent")));
}

TEST(HttpFsDirTest, Remove) {
  StringMap_t args;
  HttpFsForTesting fs(args, NULL);
  char manifest[] = "-r-- 123 /mydir/foo\n-rw- 234 /thatdir/bar\n";
  ASSERT_EQ(0, fs.ParseManifest(manifest));
  // Remove of existing files should give "Permission Denied"
  EXPECT_EQ(EACCES, fs.Remove(Path("/mydir/foo")));
  // Remove of existing directory should give "Permission Denied"
  EXPECT_EQ(EACCES, fs.Remove(Path("/mydir")));
  // Unlink of non-existent files should give "No such file or directory"
  EXPECT_EQ(ENOENT, fs.Remove(Path("/non_existent")));
}

TEST(HttpFsDirTest, ParseManifest) {
  StringMap_t args;
  off_t result_size = 0;

  HttpFsForTesting fs(args, NULL);

  // Multiple consecutive newlines or spaces should be ignored.
  char manifest[] = "-r-- 123 /mydir/foo\n\n-rw-   234  /thatdir/bar\n";
  ASSERT_EQ(0, fs.ParseManifest(manifest));

  ScopedNode root;
  EXPECT_EQ(0, fs.FindOrCreateDir(Path("/"), &root));
  ASSERT_NE((Node*)NULL, root.get());
  EXPECT_EQ(2, root->ChildCount());

  ScopedNode dir;
  EXPECT_EQ(0, fs.FindOrCreateDir(Path("/mydir"), &dir));
  ASSERT_NE((Node*)NULL, dir.get());
  EXPECT_EQ(1, dir->ChildCount());

  Node* node = (*fs.GetNodeCacheForTesting())["/mydir/foo"].get();
  EXPECT_NE((Node*)NULL, node);
  EXPECT_EQ(0, node->GetSize(&result_size));
  EXPECT_EQ(123, result_size);

  // Since these files are cached thanks to the manifest, we can open them
  // without accessing the PPAPI URL API.
  ScopedNode foo;
  ASSERT_EQ(0, fs.Open(Path("/mydir/foo"), O_RDONLY, &foo));

  ScopedNode bar;
  ASSERT_EQ(0, fs.Open(Path("/thatdir/bar"), O_RDWR, &bar));

  struct stat sfoo;
  struct stat sbar;

  EXPECT_FALSE(foo->GetStat(&sfoo));
  EXPECT_FALSE(bar->GetStat(&sbar));

  EXPECT_EQ(123, sfoo.st_size);
  EXPECT_EQ(S_IFREG | S_IRALL, sfoo.st_mode);

  EXPECT_EQ(234, sbar.st_size);
  EXPECT_EQ(S_IFREG | S_IRALL | S_IWALL, sbar.st_mode);
}

TEST(HttpFsBlobUrlTest, Basic) {
  const char* kUrl = "blob:http://example.com/6b87a5a6-713e";
  const char* kContent = "hello";
  FakePepperInterfaceURLLoader ppapi;
  ASSERT_TRUE(ppapi.server_template()->SetBlobEntity(kUrl, kContent, NULL));

  StringMap_t args;
  args["SOURCE"] = kUrl;

  HttpFsForTesting fs(args, &ppapi);

  // Any other path than / should fail.
  ScopedNode node;
  ASSERT_EQ(ENOENT, fs.Open(Path("/blah"), R_OK, &node));

  // Check access to blob file
  ASSERT_EQ(0, fs.Open(Path("/"), O_RDONLY, &node));
  ASSERT_EQ(true, node->IsaFile());

  // Verify file size and permissions
  struct stat buf;
  ASSERT_EQ(0, node->GetStat(&buf));
  ASSERT_EQ(S_IRUSR, buf.st_mode & S_IRWXU);
  ASSERT_EQ(strlen(kContent), buf.st_size);
}
