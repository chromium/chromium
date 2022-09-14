// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_MEMFS_MEM_FS_NODE_H_
#define LIBRARIES_NACL_IO_MEMFS_MEM_FS_NODE_H_

#include "nacl_io/node.h"

namespace nacl_io {

class MemFsNode : public Node {
 public:
  explicit MemFsNode(Filesystem* filesystem);

 protected:
  virtual ~MemFsNode();

 public:
  // Normal read/write operations on a file
  virtual Error Read(const HandleAttr& attr,
                     void* buf,
                     size_t count,
                     int* out_bytes);
  virtual Error Write(const HandleAttr& attr,
                      const void* buf,
                      size_t count,
                      int* out_bytes);
  virtual Error FTruncate(off_t size);
  virtual Error Futimens(const struct timespec times[2]);
  virtual Error Fchmod(mode_t mode);

 private:
  Error Resize(off_t size);

  char* data_;
  size_t data_capacity_;
  friend class MemFs;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_MEMFS_MEM_FS_NODE_H_
