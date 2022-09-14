// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>

#include <map>
#include <string>

#include "gtest/gtest.h"

#include "nacl_io/filesystem.h"
#include "nacl_io/kernel_handle.h"
#include "nacl_io/kernel_object.h"
#include "nacl_io/path.h"

using namespace nacl_io;

namespace {

class NodeForTesting : public Node {
 public:
  explicit NodeForTesting(Filesystem* fs) : Node(fs) {}
};

class FilesystemForTesting : public Filesystem {
 public:
  FilesystemForTesting() {}

 public:
  Error Access(const Path& path, int a_mode) { return ENOSYS; }
  Error OpenWithMode(const Path& path, int open_flags,
                     mode_t mode, ScopedNode* out_node) {
    out_node->reset(NULL);
    return ENOSYS;
  }
  Error Unlink(const Path& path) { return 0; }
  Error Mkdir(const Path& path, int permissions) { return 0; }
  Error Rmdir(const Path& path) { return 0; }
  Error Remove(const Path& path) { return 0; }
  Error Rename(const Path& path, const Path& newpath) { return 0; }
};

class KernelHandleForTesting : public KernelHandle {
 public:
  KernelHandleForTesting(const ScopedFilesystem& fs, const ScopedNode& node)
      : KernelHandle(fs, node) {}
};

class KernelObjectTest : public ::testing::Test {
 public:
  void SetUp() {
    fs.reset(new FilesystemForTesting());
    node.reset(new NodeForTesting(fs.get()));
  }

  void TearDown() {
    // fs is ref-counted, it doesn't need to be explicitly deleted.
    node.reset(NULL);
    fs.reset(NULL);
  }

  KernelObject proxy;
  ScopedFilesystem fs;
  ScopedNode node;
};

}  // namespace

TEST_F(KernelObjectTest, Referencing) {
  // The filesystem and node should have 1 ref count at this point
  EXPECT_EQ(1, fs->RefCount());
  EXPECT_EQ(1, node->RefCount());

  // Pass the filesystem and node into a KernelHandle
  KernelHandle* raw_handle = new KernelHandleForTesting(fs, node);
  ScopedKernelHandle handle_a(raw_handle);

  // The filesystem and node should have 1 ref count at this point
  EXPECT_EQ(1, handle_a->RefCount());
  EXPECT_EQ(2, fs->RefCount());
  EXPECT_EQ(2, node->RefCount());

  ScopedKernelHandle handle_b = handle_a;

  // There should be two references to the KernelHandle, the filesystem and node
  // should be unchanged.
  EXPECT_EQ(2, handle_a->RefCount());
  EXPECT_EQ(2, handle_b->RefCount());
  EXPECT_EQ(handle_a.get(), handle_b.get());
  EXPECT_EQ(2, fs->RefCount());
  EXPECT_EQ(2, node->RefCount());

  // Allocating an FD should cause the KernelProxy to ref the handle and
  // the node and filesystem should be unchanged.
  int fd1 = proxy.AllocateFD(handle_a, "/example");
  EXPECT_EQ(3, handle_a->RefCount());
  EXPECT_EQ(2, fs->RefCount());
  EXPECT_EQ(2, node->RefCount());

  // If we "dup" the handle, we should bump the ref count on the handle
  int fd2 = proxy.AllocateFD(handle_b, "");
  EXPECT_EQ(4, handle_a->RefCount());
  EXPECT_EQ(2, fs->RefCount());
  EXPECT_EQ(2, node->RefCount());

  // Handles are expected to come out in order
  EXPECT_EQ(0, fd1);
  EXPECT_EQ(1, fd2);

  // Now we "free" the handles, since the proxy should hold them.
  handle_a.reset(NULL);
  handle_b.reset(NULL);
  EXPECT_EQ(2, fs->RefCount());
  EXPECT_EQ(2, node->RefCount());

  // We should find the handle by either fd
  EXPECT_EQ(0, proxy.AcquireHandle(fd1, &handle_a));
  EXPECT_EQ(0, proxy.AcquireHandle(fd2, &handle_b));
  EXPECT_EQ(raw_handle, handle_a.get());
  EXPECT_EQ(raw_handle, handle_b.get());

  EXPECT_EQ(4, handle_a->RefCount());
  EXPECT_EQ(2, fs->RefCount());
  EXPECT_EQ(2, node->RefCount());

  // A non existent fd should fail, and handleA should decrement as handleB
  // is released by the call.
  EXPECT_EQ(EBADF, proxy.AcquireHandle(-1, &handle_b));
  EXPECT_EQ(NULL, handle_b.get());
  EXPECT_EQ(3, handle_a->RefCount());

  EXPECT_EQ(EBADF, proxy.AcquireHandle(100, &handle_b));
  EXPECT_EQ(NULL, handle_b.get());

  // Now only the KernelProxy should reference the KernelHandle in the
  // FD to KernelHandle Map.
  handle_a.reset();
  handle_b.reset();

  EXPECT_EQ(2, raw_handle->RefCount());
  EXPECT_EQ(2, fs->RefCount());
  EXPECT_EQ(2, node->RefCount());
  proxy.FreeFD(fd2);
  EXPECT_EQ(1, raw_handle->RefCount());
  EXPECT_EQ(2, fs->RefCount());
  EXPECT_EQ(2, node->RefCount());

  proxy.FreeFD(fd1);
  EXPECT_EQ(1, fs->RefCount());
  EXPECT_EQ(1, node->RefCount());
}

TEST_F(KernelObjectTest, FreeAndReassignFD) {
  // The filesystem and node should have 1 ref count at this point
  EXPECT_EQ(1, fs->RefCount());
  EXPECT_EQ(1, node->RefCount());

  KernelHandle* raw_handle = new KernelHandleForTesting(fs, node);
  ScopedKernelHandle handle(raw_handle);

  EXPECT_EQ(2, fs->RefCount());
  EXPECT_EQ(2, node->RefCount());
  EXPECT_EQ(1, raw_handle->RefCount());

  proxy.AllocateFD(handle, "/example");
  EXPECT_EQ(2, fs->RefCount());
  EXPECT_EQ(2, node->RefCount());
  EXPECT_EQ(2, raw_handle->RefCount());

  proxy.FreeAndReassignFD(5, handle, "/example");
  EXPECT_EQ(2, fs->RefCount());
  EXPECT_EQ(2, node->RefCount());
  EXPECT_EQ(3, raw_handle->RefCount());


  handle.reset();
  EXPECT_EQ(2, raw_handle->RefCount());

  proxy.AcquireHandle(5, &handle);
  EXPECT_EQ(3, raw_handle->RefCount());
  EXPECT_EQ(raw_handle, handle.get());
}
