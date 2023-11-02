// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <string>

#include "dev_fs_for_testing.h"
#include "gtest/gtest.h"
#include "nacl_io/filesystem.h"
#include "nacl_io/ioctl.h"
#include "nacl_io/kernel_handle.h"
#include "nacl_io/memfs/mem_fs.h"
#include "nacl_io/osdirent.h"
#include "nacl_io/osunistd.h"

using namespace nacl_io;

namespace {

class MemFsForTesting : public MemFs {
 public:
  MemFsForTesting() {
    FsInitArgs args(1);
    EXPECT_EQ(0, Init(args));
  }

  bool Exists(const char* filename) {
    ScopedNode node;
    if (Open(Path(filename), O_RDONLY, &node))
      return false;

    struct stat buf;
    return node->GetStat(&buf) == 0;
  }

  int num_nodes() { return (int)inode_pool_.size(); }
};

}  // namespace

TEST(FilesystemTest, Sanity) {
  MemFsForTesting fs;

  ScopedNode file;
  ScopedNode root;
  ScopedNode result_node;

  off_t result_size = 0;
  int result_bytes = 0;
  struct stat buf;
  char buf1[1024];

  // A memory filesystem starts with one directory node: the root.
  EXPECT_EQ(1, fs.num_nodes());

  // Fail to open non existent file
  EXPECT_EQ(ENOENT, fs.Open(Path("/foo"), O_RDWR, &result_node));
  EXPECT_EQ(NULL, result_node.get());
  EXPECT_EQ(1, fs.num_nodes());

  // Create a file
  EXPECT_EQ(0, fs.Open(Path("/foo"), O_RDWR | O_CREAT, &file));
  ASSERT_NE(NULL_NODE, file.get());

  // We now have a directory and a file.  The file has a two references
  // one returned to the test, one for the name->inode map.
  ASSERT_EQ(2, fs.num_nodes());
  ASSERT_EQ(2, file->RefCount());
  ASSERT_EQ(0, file->GetStat(&buf));
  ASSERT_EQ(0, buf.st_mode & S_IXUSR);

  // All access should be allowed on the root directory.
  EXPECT_EQ(0, fs.Open(Path("/"), O_RDONLY, &root));
  ASSERT_EQ(0, root->GetStat(&buf));
  ASSERT_EQ(S_IRWXU, buf.st_mode & S_IRWXU);

  // Open the root directory, should not create a new file
  EXPECT_EQ(0, fs.Open(Path("/"), O_RDONLY, &root));
  EXPECT_EQ(2, fs.num_nodes());
  ASSERT_NE(NULL_NODE, root.get());
  struct dirent dirs[4];
  int len;
  EXPECT_EQ(0, root->GetDents(0, dirs, sizeof(dirs), &len));
  // 3 == "foo", ".", ".."
  EXPECT_EQ(3 * sizeof(struct dirent), len);

  // Fail to re-create the same file
  EXPECT_EQ(EEXIST,
            fs.Open(Path("/foo"), O_RDWR | O_CREAT | O_EXCL, &result_node));
  EXPECT_EQ(NULL_NODE, result_node.get());
  EXPECT_EQ(2, fs.num_nodes());

  // Fail to create a directory with the same name
  EXPECT_EQ(EEXIST, fs.Mkdir(Path("/foo"), O_RDWR));
  EXPECT_EQ(2, fs.num_nodes());

  HandleAttr attrs;

  // Attempt to READ/WRITE
  EXPECT_EQ(0, file->GetSize(&result_size));
  EXPECT_EQ(0, result_size);
  EXPECT_EQ(0, file->Write(attrs, buf1, sizeof(buf1), &result_bytes));
  EXPECT_EQ(sizeof(buf1), result_bytes);
  EXPECT_EQ(0, file->GetSize(&result_size));
  EXPECT_EQ(sizeof(buf1), result_size);
  EXPECT_EQ(0, file->Read(attrs, buf1, sizeof(buf1), &result_bytes));
  EXPECT_EQ(sizeof(buf1), result_bytes);
  EXPECT_EQ(2, fs.num_nodes());
  EXPECT_EQ(2, file->RefCount());

  // Attempt to open the same file, create another ref to it, but does not
  // create a new file.
  EXPECT_EQ(0, fs.Open(Path("/foo"), O_RDWR | O_CREAT, &result_node));
  EXPECT_EQ(3, file->RefCount());
  EXPECT_EQ(2, fs.num_nodes());
  EXPECT_EQ(file.get(), result_node.get());
  EXPECT_EQ(0, file->GetSize(&result_size));
  EXPECT_EQ(sizeof(buf1), result_size);

  // Remove our references so that only the Filesystem holds it
  file.reset();
  result_node.reset();
  EXPECT_EQ(2, fs.num_nodes());

  // This should have deleted the object
  EXPECT_EQ(0, fs.Unlink(Path("/foo")));
  EXPECT_EQ(1, fs.num_nodes());

  // We should fail to find it
  EXPECT_EQ(ENOENT, fs.Unlink(Path("/foo")));
  EXPECT_EQ(1, fs.num_nodes());

  // Recreate foo as a directory
  EXPECT_EQ(0, fs.Mkdir(Path("/foo"), O_RDWR));
  EXPECT_EQ(2, fs.num_nodes());

  // Create a file (exclusively)
  EXPECT_EQ(0, fs.Open(Path("/foo/bar"), O_RDWR | O_CREAT | O_EXCL, &file));
  ASSERT_NE(NULL_NODE, file.get());
  EXPECT_EQ(2, file->RefCount());
  EXPECT_EQ(3, fs.num_nodes());

  // Attempt to delete the directory and fail
  EXPECT_EQ(ENOTEMPTY, fs.Rmdir(Path("/foo")));
  EXPECT_EQ(2, root->RefCount());
  EXPECT_EQ(2, file->RefCount());
  EXPECT_EQ(3, fs.num_nodes());

  // Unlink the file, we should have the only file ref at this point.
  EXPECT_EQ(0, fs.Unlink(Path("/foo/bar")));
  EXPECT_EQ(2, root->RefCount());
  EXPECT_EQ(1, file->RefCount());
  EXPECT_EQ(3, fs.num_nodes());

  // Deref the file, to make it go away
  file.reset();
  EXPECT_EQ(2, fs.num_nodes());

  // Deref the directory
  EXPECT_EQ(0, fs.Rmdir(Path("/foo")));
  EXPECT_EQ(1, fs.num_nodes());

  // Verify the directory is gone
  EXPECT_EQ(ENOENT, fs.Open(Path("/foo"), O_RDONLY, &file));
  EXPECT_EQ(NULL_NODE, file.get());
}

TEST(FilesystemTest, OpenMode_TRUNC) {
  MemFsForTesting fs;
  ScopedNode file;
  ScopedNode root;
  ScopedNode result_node;
  HandleAttr attrs;
  int result_bytes;

  // Open a file and write something to it.
  const char* buf = "hello";
  ASSERT_EQ(0, fs.Open(Path("/foo"), O_RDWR | O_CREAT, &file));
  ASSERT_EQ(0, file->Write(attrs, buf, strlen(buf), &result_bytes));
  ASSERT_EQ(strlen(buf), result_bytes);

  // Open it again with TRUNC and make sure it is empty
  char read_buf[10];
  ASSERT_EQ(0, fs.Open(Path("/foo"), O_RDWR | O_TRUNC, &file));
  ASSERT_EQ(0, file->Read(attrs, read_buf, sizeof(read_buf), &result_bytes));
  ASSERT_EQ(0, result_bytes);
}

TEST(FilesystemTest, MemFsRemove) {
  MemFsForTesting fs;
  ScopedNode file;
  ScopedNode result_node;

  ASSERT_EQ(0, fs.Mkdir(Path("/dir"), O_RDWR));
  ASSERT_EQ(0, fs.Open(Path("/file"), O_RDWR | O_CREAT | O_EXCL, &file));
  EXPECT_NE(NULL_NODE, file.get());
  EXPECT_EQ(3, fs.num_nodes());
  file.reset();

  EXPECT_EQ(0, fs.Remove(Path("/dir")));
  EXPECT_EQ(2, fs.num_nodes());
  EXPECT_EQ(0, fs.Remove(Path("/file")));
  EXPECT_EQ(1, fs.num_nodes());

  ASSERT_EQ(ENOENT, fs.Open(Path("/dir/foo"), O_CREAT | O_RDWR, &result_node));
  ASSERT_EQ(NULL_NODE, result_node.get());
  ASSERT_EQ(ENOENT, fs.Open(Path("/file"), O_RDONLY, &result_node));
  ASSERT_EQ(NULL_NODE, result_node.get());
}

TEST(FilesystemTest, MemFsRename) {
  MemFsForTesting fs;
  ASSERT_EQ(0, fs.Mkdir(Path("/dir1"), O_RDWR));
  ASSERT_EQ(0, fs.Mkdir(Path("/dir2"), O_RDWR));
  ASSERT_EQ(3, fs.num_nodes());

  ScopedNode file;
  ASSERT_EQ(0, fs.Open(Path("/dir1/file"), O_RDWR | O_CREAT | O_EXCL, &file));
  ASSERT_TRUE(fs.Exists("/dir1/file"));
  ASSERT_EQ(4, fs.num_nodes());

  // Move from one directory to another should ok
  ASSERT_EQ(0, fs.Rename(Path("/dir1/file"), Path("/dir2/new_file")));
  ASSERT_FALSE(fs.Exists("/dir1/file"));
  ASSERT_TRUE(fs.Exists("/dir2/new_file"));
  ASSERT_EQ(4, fs.num_nodes());

  // Move within the same directory
  ASSERT_EQ(0, fs.Rename(Path("/dir2/new_file"), Path("/dir2/new_file2")));
  ASSERT_FALSE(fs.Exists("/dir2/new_file"));
  ASSERT_TRUE(fs.Exists("/dir2/new_file2"));
  ASSERT_EQ(4, fs.num_nodes());

  // Move to another directory but without a filename
  ASSERT_EQ(0, fs.Rename(Path("/dir2/new_file2"), Path("/dir1")));
  ASSERT_FALSE(fs.Exists("/dir2/new_file2"));
  ASSERT_TRUE(fs.Exists("/dir1/new_file2"));
  ASSERT_EQ(4, fs.num_nodes());
}

TEST(FilesystemTest, MemFsRenameDir) {
  MemFsForTesting fs;

  ASSERT_EQ(0, fs.Mkdir(Path("/dir1"), O_RDWR));
  ASSERT_EQ(0, fs.Mkdir(Path("/dir2"), O_RDWR));
  EXPECT_EQ(3, fs.num_nodes());

  // Renaming one directory to another should work
  ASSERT_EQ(0, fs.Rename(Path("/dir1"), Path("/dir2")));
  ASSERT_FALSE(fs.Exists("/dir1"));
  ASSERT_TRUE(fs.Exists("/dir2"));
  EXPECT_EQ(2, fs.num_nodes());

  // Reset to initial state
  ASSERT_EQ(0, fs.Mkdir(Path("/dir1"), O_RDWR));
  EXPECT_EQ(3, fs.num_nodes());

  // Renaming a directory to a new name within another
  ASSERT_EQ(0, fs.Rename(Path("/dir1"), Path("/dir2/foo")));
  ASSERT_TRUE(fs.Exists("/dir2"));
  ASSERT_TRUE(fs.Exists("/dir2/foo"));
  EXPECT_EQ(3, fs.num_nodes());

  // Reset to initial state
  ASSERT_EQ(0, fs.Rmdir(Path("/dir2/foo")));
  ASSERT_EQ(0, fs.Mkdir(Path("/dir1"), O_RDWR));
  EXPECT_EQ(3, fs.num_nodes());

  // Renaming one directory to another should fail if the target is non-empty
  ASSERT_EQ(0, fs.Mkdir(Path("/dir2/dir3"), O_RDWR));
  ASSERT_EQ(ENOTEMPTY, fs.Rename(Path("/dir1"), Path("/dir2")));
}

TEST(FilesystemTest, DevAccess) {
  // Should not be able to open non-existent file.
  FakePepperInterface pepper;
  DevFsForTesting fs(&pepper);
  ScopedNode invalid_node, valid_node;
  ASSERT_FALSE(fs.Exists("/foo"));
  // Creating non-existent file should return EACCES
  ASSERT_EQ(EACCES, fs.Open(Path("/foo"), O_CREAT | O_RDWR, &invalid_node));

  // We should be able to open all existing nodes with O_CREAT and O_RDWR.
  ASSERT_EQ(0, fs.Open(Path("/null"), O_CREAT | O_RDWR, &valid_node));
  ASSERT_EQ(0, fs.Open(Path("/zero"), O_CREAT | O_RDWR, &valid_node));
  ASSERT_EQ(0, fs.Open(Path("/urandom"), O_CREAT | O_RDWR, &valid_node));
  ASSERT_EQ(0, fs.Open(Path("/console0"), O_CREAT | O_RDWR, &valid_node));
  ASSERT_EQ(0, fs.Open(Path("/console1"), O_CREAT | O_RDWR, &valid_node));
  ASSERT_EQ(0, fs.Open(Path("/console3"), O_CREAT | O_RDWR, &valid_node));
  ASSERT_EQ(0, fs.Open(Path("/tty"), O_CREAT | O_RDWR, &valid_node));
  ASSERT_EQ(0, fs.Open(Path("/stdin"), O_CREAT | O_RDWR, &valid_node));
  ASSERT_EQ(0, fs.Open(Path("/stdout"), O_CREAT | O_RDWR, &valid_node));
  ASSERT_EQ(0, fs.Open(Path("/stderr"), O_CREAT | O_RDWR, &valid_node));
}

TEST(FilesystemTest, DevNull) {
  FakePepperInterface pepper;
  DevFsForTesting fs(&pepper);
  ScopedNode dev_null;
  int result_bytes = 0;
  struct stat buf;

  ASSERT_EQ(0, fs.Open(Path("/null"), O_RDWR, &dev_null));
  ASSERT_NE(NULL_NODE, dev_null.get());
  ASSERT_EQ(0, dev_null->GetStat(&buf));
  ASSERT_EQ(S_IRUSR | S_IWUSR, buf.st_mode & S_IRWXU);

  // Writing to /dev/null should write everything.
  const char msg[] = "Dummy test message.";
  HandleAttr attrs;
  EXPECT_EQ(0, dev_null->Write(attrs, &msg[0], strlen(msg), &result_bytes));
  EXPECT_EQ(strlen(msg), result_bytes);

  // Reading from /dev/null should read nothing.
  const int kBufferLength = 100;
  char buffer[kBufferLength];
  EXPECT_EQ(0, dev_null->Read(attrs, &buffer[0], kBufferLength, &result_bytes));
  EXPECT_EQ(0, result_bytes);
}

TEST(FilesystemTest, DevZero) {
  FakePepperInterface pepper;
  DevFsForTesting fs(&pepper);
  ScopedNode dev_zero;
  int result_bytes = 0;
  struct stat buf;

  ASSERT_EQ(0, fs.Open(Path("/zero"), O_RDWR, &dev_zero));
  ASSERT_NE(NULL_NODE, dev_zero.get());
  ASSERT_EQ(0, dev_zero->GetStat(&buf));
  ASSERT_EQ(S_IRUSR | S_IWUSR, buf.st_mode & S_IRWXU);

  // Writing to /dev/zero should write everything.
  HandleAttr attrs;
  const char msg[] = "Dummy test message.";
  EXPECT_EQ(0, dev_zero->Write(attrs, &msg[0], strlen(msg), &result_bytes));
  EXPECT_EQ(strlen(msg), result_bytes);

  // Reading from /dev/zero should read all zeroes.
  const int kBufferLength = 100;
  char buffer[kBufferLength];
  // First fill with all 1s.
  memset(&buffer[0], 0x1, kBufferLength);
  EXPECT_EQ(0, dev_zero->Read(attrs, &buffer[0], kBufferLength, &result_bytes));
  EXPECT_EQ(kBufferLength, result_bytes);

  char zero_buffer[kBufferLength];
  memset(&zero_buffer[0], 0, kBufferLength);
  EXPECT_EQ(0, memcmp(&buffer[0], &zero_buffer[0], kBufferLength));
}

// Disabled due to intermittent failures on linux: http://crbug.com/257257
TEST(FilesystemTest, DISABLED_DevUrandom) {
  FakePepperInterface pepper;
  DevFsForTesting fs(&pepper);
  ScopedNode dev_urandom;
  int result_bytes = 0;
  struct stat buf;

  ASSERT_EQ(0, fs.Open(Path("/urandom"), O_RDWR, &dev_urandom));
  ASSERT_NE(NULL_NODE, dev_urandom.get());
  ASSERT_EQ(0, dev_urandom->GetStat(&buf));
  ASSERT_EQ(S_IRUSR | S_IWUSR, buf.st_mode & S_IRWXU);

  // Writing to /dev/urandom should write everything.
  const char msg[] = "Dummy test message.";
  HandleAttr attrs;
  EXPECT_EQ(0, dev_urandom->Write(attrs, &msg[0], strlen(msg), &result_bytes));
  EXPECT_EQ(strlen(msg), result_bytes);

  // Reading from /dev/urandom should read random bytes.
  const int kSampleBatches = 1000;
  const int kSampleBatchSize = 1000;
  const int kTotalSamples = kSampleBatches * kSampleBatchSize;

  int byte_count[256] = {0};

  unsigned char buffer[kSampleBatchSize];
  for (int batch = 0; batch < kSampleBatches; ++batch) {
    int bytes_read = 0;
    EXPECT_EQ(
        0, dev_urandom->Read(attrs, &buffer[0], kSampleBatchSize, &bytes_read));
    EXPECT_EQ(kSampleBatchSize, bytes_read);

    for (int i = 0; i < bytes_read; ++i) {
      byte_count[buffer[i]]++;
    }
  }

  double expected_count = kTotalSamples / 256.;
  double chi_squared = 0;
  for (int i = 0; i < 256; ++i) {
    double difference = byte_count[i] - expected_count;
    chi_squared += difference * difference / expected_count;
  }

  // Approximate chi-squared value for p-value 0.05, 255 degrees-of-freedom.
  EXPECT_LE(chi_squared, 293.24);
}
