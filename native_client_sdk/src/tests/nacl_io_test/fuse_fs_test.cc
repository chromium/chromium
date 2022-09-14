// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "nacl_io/fuse.h"
#include "nacl_io/fusefs/fuse_fs.h"
#include "nacl_io/kernel_handle.h"
#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_proxy.h"
#include "nacl_io/ostime.h"

using namespace nacl_io;

namespace {

class FuseFsForTesting : public FuseFs {
 public:
  explicit FuseFsForTesting(fuse_operations* fuse_ops) {
    FsInitArgs args;
    args.fuse_ops = fuse_ops;
    EXPECT_EQ(0, Init(args));
  }
};

// Implementation of a simple flat memory filesystem.
struct File {
  File() : mode(0666) { memset(&times, 0, sizeof(times)); }

  std::string name;
  std::vector<uint8_t> data;
  mode_t mode;
  timespec times[2];
};

typedef std::vector<File> Files;
Files g_files;

bool IsValidPath(const char* path) {
  if (path == NULL)
    return false;

  if (strlen(path) <= 1)
    return false;

  if (path[0] != '/')
    return false;

  return true;
}

File* FindFile(const char* path) {
  if (!IsValidPath(path))
    return NULL;

  for (Files::iterator iter = g_files.begin(); iter != g_files.end(); ++iter) {
    if (iter->name == &path[1])
      return &*iter;
  }

  return NULL;
}

int testfs_getattr(const char* path, struct stat* stbuf) {
  memset(stbuf, 0, sizeof(struct stat));

  if (strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    return 0;
  }

  if (strcmp(path, "/foo") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    return 0;
  }

  File* file = FindFile(path);
  if (file == NULL)
    return -ENOENT;

  stbuf->st_mode = S_IFREG | file->mode;
  stbuf->st_size = file->data.size();
  stbuf->st_atime = file->times[0].tv_sec;
  stbuf->st_atimensec = file->times[0].tv_nsec;
  stbuf->st_mtime = file->times[1].tv_sec;
  stbuf->st_mtimensec = file->times[1].tv_nsec;
  return 0;
}

int testfs_readdir(const char* path,
                   void* buf,
                   fuse_fill_dir_t filler,
                   off_t offset,
                   struct fuse_file_info*) {
  if (strcmp(path, "/") != 0)
    return -ENOENT;

  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);
  for (Files::iterator iter = g_files.begin(); iter != g_files.end(); ++iter) {
    filler(buf, iter->name.c_str(), NULL, 0);
  }
  return 0;
}

int testfs_create(const char* path, mode_t mode, struct fuse_file_info* fi) {
  if (!IsValidPath(path))
    return -ENOENT;

  File* file = FindFile(path);
  if (file != NULL) {
    if (fi->flags & O_EXCL)
      return -EEXIST;
  } else {
    g_files.push_back(File());
    file = &g_files.back();
    file->name = &path[1];  // Skip initial /
  }
  file->mode = mode;

  return 0;
}

int testfs_open(const char* path, struct fuse_file_info*) {
  // open is only called to open an existing file, otherwise create is
  // called. We don't need to do any additional work here, the path will be
  // passed to any other operations.
  return FindFile(path) != NULL;
}

int testfs_read(const char* path,
                char* buf,
                size_t size,
                off_t offset,
                struct fuse_file_info* fi) {
  File* file = FindFile(path);
  if (file == NULL)
    return -ENOENT;

  size_t filesize = file->data.size();
  // Trying to read past the end of the file.
  if (offset >= filesize)
    return 0;

  if (offset + size > filesize)
    size = filesize - offset;

  memcpy(buf, file->data.data() + offset, size);
  return size;
}

int testfs_write(const char* path,
                 const char* buf,
                 size_t size,
                 off_t offset,
                 struct fuse_file_info*) {
  File* file = FindFile(path);
  if (file == NULL)
    return -ENOENT;

  size_t filesize = file->data.size();

  if (offset + size > filesize)
    file->data.resize(offset + size);

  memcpy(file->data.data() + offset, buf, size);
  return size;
}

int testfs_utimens(const char* path, const struct timespec times[2]) {
  File* file = FindFile(path);
  if (file == NULL)
    return -ENOENT;

  file->times[0] = times[0];
  file->times[1] = times[1];
  return 0;
}

int testfs_chmod(const char* path, mode_t mode) {
  File* file = FindFile(path);
  if (file == NULL)
    return -ENOENT;

  file->mode = mode;
  return 0;
}

const char hello_world[] = "Hello, World!\n";

fuse_operations g_fuse_operations = {
    0,               // flag_nopath
    0,               // flag_reserved
    testfs_getattr,  // getattr
    NULL,            // readlink
    NULL,            // mknod
    NULL,            // mkdir
    NULL,            // unlink
    NULL,            // rmdir
    NULL,            // symlink
    NULL,            // rename
    NULL,            // link
    testfs_chmod,    // chmod
    NULL,            // chown
    NULL,            // truncate
    testfs_open,     // open
    testfs_read,     // read
    testfs_write,    // write
    NULL,            // statfs
    NULL,            // flush
    NULL,            // release
    NULL,            // fsync
    NULL,            // setxattr
    NULL,            // getxattr
    NULL,            // listxattr
    NULL,            // removexattr
    NULL,            // opendir
    testfs_readdir,  // readdir
    NULL,            // releasedir
    NULL,            // fsyncdir
    NULL,            // init
    NULL,            // destroy
    NULL,            // access
    testfs_create,   // create
    NULL,            // ftruncate
    NULL,            // fgetattr
    NULL,            // lock
    testfs_utimens,  // utimens
    NULL,            // bmap
    NULL,            // ioctl
    NULL,            // poll
    NULL,            // write_buf
    NULL,            // read_buf
    NULL,            // flock
    NULL,            // fallocate
};

class FuseFsTest : public ::testing::Test {
 public:
  FuseFsTest();

  void SetUp();

 protected:
  FuseFsForTesting fs_;
};

FuseFsTest::FuseFsTest() : fs_(&g_fuse_operations) {}

void FuseFsTest::SetUp() {
  // Reset the filesystem.
  g_files.clear();

  // Add a built-in file.
  size_t hello_len = strlen(hello_world);

  File hello;
  hello.name = "hello";
  hello.data.resize(hello_len);
  memcpy(hello.data.data(), hello_world, hello_len);
  g_files.push_back(hello);
}

}  // namespace

TEST_F(FuseFsTest, OpenAndRead) {
  ScopedNode node;
  ASSERT_EQ(0, fs_.Open(Path("/hello"), O_RDONLY, &node));

  char buffer[15] = {0};
  int bytes_read = 0;
  HandleAttr attr;
  ASSERT_EQ(0, node->Read(attr, &buffer[0], sizeof(buffer), &bytes_read));
  ASSERT_EQ(strlen(hello_world), bytes_read);
  ASSERT_STREQ(hello_world, buffer);

  // Try to read past the end of the file.
  attr.offs = strlen(hello_world) - 7;
  ASSERT_EQ(0, node->Read(attr, &buffer[0], sizeof(buffer), &bytes_read));
  ASSERT_EQ(7, bytes_read);
  ASSERT_STREQ("World!\n", buffer);
}

TEST_F(FuseFsTest, CreateWithMode) {
  ScopedNode node;
  struct stat statbuf;

  ASSERT_EQ(0, fs_.OpenWithMode(Path("/hello"),
                                O_RDWR | O_CREAT, 0723, &node));
  EXPECT_EQ(0, node->GetStat(&statbuf));
  EXPECT_TRUE(S_ISREG(statbuf.st_mode));
  EXPECT_EQ(0723, statbuf.st_mode & S_MODEBITS);
}

TEST_F(FuseFsTest, CreateAndWrite) {
  ScopedNode node;
  ASSERT_EQ(0, fs_.Open(Path("/foobar"), O_RDWR | O_CREAT, &node));

  HandleAttr attr;
  const char message[] = "Something interesting";
  int bytes_written;
  ASSERT_EQ(0, node->Write(attr, &message[0], strlen(message), &bytes_written));
  ASSERT_EQ(bytes_written, strlen(message));

  // Now try to read the data back.
  char buffer[40] = {0};
  int bytes_read = 0;
  ASSERT_EQ(0, node->Read(attr, &buffer[0], sizeof(buffer), &bytes_read));
  ASSERT_EQ(strlen(message), bytes_read);
  ASSERT_STREQ(message, buffer);
}

TEST_F(FuseFsTest, GetStat) {
  struct stat statbuf;
  ScopedNode node;

  ASSERT_EQ(0, fs_.Open(Path("/hello"), O_RDONLY, &node));
  EXPECT_EQ(0, node->GetStat(&statbuf));
  EXPECT_TRUE(S_ISREG(statbuf.st_mode));
  EXPECT_EQ(0666, statbuf.st_mode & S_MODEBITS);
  EXPECT_EQ(strlen(hello_world), statbuf.st_size);

  ASSERT_EQ(0, fs_.Open(Path("/"), O_RDONLY, &node));
  EXPECT_EQ(0, node->GetStat(&statbuf));
  EXPECT_TRUE(S_ISDIR(statbuf.st_mode));
  EXPECT_EQ(0755, statbuf.st_mode & S_MODEBITS);

  // Create a file and stat.
  ASSERT_EQ(0, fs_.Open(Path("/foobar"), O_RDWR | O_CREAT, &node));
  EXPECT_EQ(0, node->GetStat(&statbuf));
  EXPECT_TRUE(S_ISREG(statbuf.st_mode));
  EXPECT_EQ(0666, statbuf.st_mode & S_MODEBITS);
  EXPECT_EQ(0, statbuf.st_size);
}

TEST_F(FuseFsTest, GetDents) {
  ScopedNode root;

  ASSERT_EQ(0, fs_.Open(Path("/"), O_RDONLY, &root));

  struct dirent entries[4];
  int bytes_read;

  // Try reading everything.
  ASSERT_EQ(0, root->GetDents(0, &entries[0], sizeof(entries), &bytes_read));
  ASSERT_EQ(3 * sizeof(dirent), bytes_read);
  EXPECT_STREQ(".", entries[0].d_name);
  EXPECT_STREQ("..", entries[1].d_name);
  EXPECT_STREQ("hello", entries[2].d_name);

  // Try reading from an offset.
  memset(&entries, 0, sizeof(entries));
  ASSERT_EQ(0, root->GetDents(sizeof(dirent), &entries[0], 2 * sizeof(dirent),
                              &bytes_read));
  ASSERT_EQ(2 * sizeof(dirent), bytes_read);
  EXPECT_STREQ("..", entries[0].d_name);
  EXPECT_STREQ("hello", entries[1].d_name);

  // Add a file and read again.
  ScopedNode node;
  ASSERT_EQ(0, fs_.Open(Path("/foobar"), O_RDWR | O_CREAT, &node));
  ASSERT_EQ(0, root->GetDents(0, &entries[0], sizeof(entries), &bytes_read));
  ASSERT_EQ(4 * sizeof(dirent), bytes_read);
  EXPECT_STREQ(".", entries[0].d_name);
  EXPECT_STREQ("..", entries[1].d_name);
  EXPECT_STREQ("hello", entries[2].d_name);
  EXPECT_STREQ("foobar", entries[3].d_name);
}

TEST_F(FuseFsTest, Utimens) {
  struct stat statbuf;
  ScopedNode node;

  struct timespec times[2];
  times[0].tv_sec = 1000;
  times[0].tv_nsec = 2000;
  times[1].tv_sec = 3000;
  times[1].tv_nsec = 4000;

  ASSERT_EQ(0, fs_.Open(Path("/hello"), O_RDONLY, &node));
  EXPECT_EQ(0, node->Futimens(times));

  EXPECT_EQ(0, node->GetStat(&statbuf));
  EXPECT_EQ(times[0].tv_sec, statbuf.st_atime);
  EXPECT_EQ(times[0].tv_nsec, statbuf.st_atimensec);
  EXPECT_EQ(times[1].tv_sec, statbuf.st_mtime);
  EXPECT_EQ(times[1].tv_nsec, statbuf.st_mtimensec);
}

TEST_F(FuseFsTest, Fchmod) {
  struct stat statbuf;
  ScopedNode node;

  ASSERT_EQ(0, fs_.Open(Path("/hello"), O_RDONLY, &node));
  ASSERT_EQ(0, node->GetStat(&statbuf));
  EXPECT_EQ(0666, statbuf.st_mode & S_MODEBITS);

  ASSERT_EQ(0, node->Fchmod(0777));

  ASSERT_EQ(0, node->GetStat(&statbuf));
  EXPECT_EQ(0777, statbuf.st_mode & S_MODEBITS);
}

namespace {

class KernelProxyFuseTest : public ::testing::Test {
 public:
  KernelProxyFuseTest() {}

  void SetUp();
  void TearDown();

 private:
  KernelProxy kp_;
};

void KernelProxyFuseTest::SetUp() {
  ASSERT_EQ(0, ki_push_state_for_testing());
  ASSERT_EQ(0, ki_init(&kp_));

  // Register a fuse filesystem.
  nacl_io_register_fs_type("flatfs", &g_fuse_operations);

  // Unmount the passthrough FS and mount our fuse filesystem.
  EXPECT_EQ(0, kp_.umount("/"));
  EXPECT_EQ(0, kp_.mount("", "/", "flatfs", 0, NULL));
}

void KernelProxyFuseTest::TearDown() {
  nacl_io_unregister_fs_type("flatfs");
  ki_uninit();
}

}  // namespace

TEST_F(KernelProxyFuseTest, Basic) {
  // Write a file.
  int fd = ki_open("/hello", O_WRONLY | O_CREAT, 0777);
  ASSERT_GT(fd, -1);
  ASSERT_EQ(sizeof(hello_world),
            ki_write(fd, hello_world, sizeof(hello_world)));
  EXPECT_EQ(0, ki_close(fd));

  // Then read it back in.
  fd = ki_open("/hello", O_RDONLY, 0);
  ASSERT_GT(fd, -1);

  char buffer[30];
  memset(buffer, 0, sizeof(buffer));
  ASSERT_EQ(sizeof(hello_world), ki_read(fd, buffer, sizeof(buffer)));
  EXPECT_STREQ(hello_world, buffer);
  EXPECT_EQ(0, ki_close(fd));
}

TEST_F(KernelProxyFuseTest, Chdir) {
  ASSERT_EQ(0, ki_chdir("/foo"));
}
