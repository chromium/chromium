// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef LIBRARIES_NACL_IO_PASSTHROUGHFS_REAL_NODE_H_
#define LIBRARIES_NACL_IO_PASSTHROUGHFS_REAL_NODE_H_

#include "nacl_io/node.h"

namespace nacl_io {

class RealNode : public Node {
 public:
  RealNode(Filesystem* filesystem, int real_fd, bool close_on_destroy = false);

 protected:
  virtual Error Init(int flags) { return 0; }

  virtual void Destroy();

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
  virtual Error GetDents(size_t offs,
                         struct dirent* pdir,
                         size_t count,
                         int* out_bytes);
  virtual Error GetStat(struct stat* stat);
  virtual Error Isatty();
  virtual Error MMap(void* addr,
                     size_t length,
                     int prot,
                     int flags,
                     size_t offset,
                     void** out_addr);

 private:
  int real_fd_;
  bool close_on_destroy_;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_PASSTHROUGHFS_REAL_NODE_H_
