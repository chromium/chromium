// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_MEMFS_MEM_FS_H_
#define LIBRARIES_NACL_IO_MEMFS_MEM_FS_H_

#include "nacl_io/filesystem.h"
#include "nacl_io/typed_fs_factory.h"

namespace nacl_io {

class MemFs : public Filesystem {
 public:
  MemFs(const MemFs&) = delete;
  MemFs& operator=(const MemFs&) = delete;

 protected:
  MemFs();

  using Filesystem::Init;
  virtual Error Init(const FsInitArgs& args);

  // The protected functions are only used internally and will not
  // acquire or release the filesystem's lock themselves.  The caller is
  // required to use correct locking as needed.

  // Allocate or free an INODE number.
  int AllocateINO();
  void FreeINO(int ino);

  // Find a Node specified node optionally failing if type does not match.
  virtual Error FindNode(const Path& path, int type, ScopedNode* out_node);

 public:
  virtual Error OpenWithMode(const Path& path, int open_flags, mode_t mode,
                             ScopedNode* out_node);
  virtual Error Unlink(const Path& path);
  virtual Error Mkdir(const Path& path, int perm);
  virtual Error Rmdir(const Path& path);
  virtual Error Remove(const Path& path);
  virtual Error Rename(const Path& path, const Path& newpath);

 private:
  static const int REMOVE_DIR = 1;
  static const int REMOVE_FILE = 2;
  static const int REMOVE_ALL = REMOVE_DIR | REMOVE_FILE;

  Error RemoveInternal(const Path& path, int remove_type);

  ScopedNode root_;

  friend class TypedFsFactory<MemFs>;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_MEMFS_MEM_FS_H_
