// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_GOOGLEDRIVEFS_GOOGLEDRIVEFS_NODE_H_
#define LIBRARIES_NACL_IO_GOOGLEDRIVEFS_GOOGLEDRIVEFS_NODE_H_

#include <sys/stat.h>

#include "nacl_io/googledrivefs/googledrivefs.h"
#include "nacl_io/kernel_handle.h"
#include "nacl_io/node.h"
#include "nacl_io/osdirent.h"

namespace nacl_io {

// This is not further implemented.
// PNaCl is on a path to deprecation, and WebAssembly is
// the focused technology.

class GoogleDriveFsNode : public Node {
 public:
  GoogleDriveFsNode(GoogleDriveFs* googledrivefs);

  Error GetDents(size_t offs, struct dirent* pdir, size_t size, int* out_bytes);
  Error Write(const HandleAttr& attr,
              const void* buf,
              size_t count,
              int* out_bytes);
  Error FTruncate(off_t length);
  Error Read(const HandleAttr& attr, void* buf, size_t count, int* out_bytes);
  Error GetSize(off_t* out_size);
  Error GetStat(struct stat* pstat);
  Error Init(int open_flags);
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_GOOGLEDRIVEFS_GOOGLEDRIVEFS_NODE_H_
