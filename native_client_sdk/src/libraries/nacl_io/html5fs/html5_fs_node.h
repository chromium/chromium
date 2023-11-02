// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_HTML5FS_HTML5_FS_NODE_H_
#define LIBRARIES_NACL_IO_HTML5FS_HTML5_FS_NODE_H_

#include <ppapi/c/pp_instance.h>
#include <ppapi/c/pp_resource.h>
#include "nacl_io/node.h"

namespace nacl_io {

class Html5Fs;
class FileIoInterface;
class FileRefInterface;
class VarInterface;

class Html5FsNode : public Node {
 public:
  // Normal OS operations on a node (file), can be called by the kernel
  // directly so it must lock and unlock appropriately.  These functions
  // must not be called by the filesystem.
  virtual Error FSync();
  virtual Error GetDents(size_t offs,
                         struct dirent* pdir,
                         size_t count,
                         int* out_bytes);
  virtual Error GetStat(struct stat* stat);
  virtual Error Read(const HandleAttr& attr,
                     void* buf,
                     size_t count,
                     int* out_bytes);
  virtual Error FTruncate(off_t size);
  virtual Error Write(const HandleAttr& attr,
                      const void* buf,
                      size_t count,
                      int* out_bytes);

  virtual int GetType();
  virtual Error GetSize(off_t* out_size);
  virtual Error Fchmod(mode_t mode);

 protected:
  Html5FsNode(Filesystem* filesystem, PP_Resource fileref);

  // Init with standard open flags
  virtual Error Init(int open_flags);
  virtual void Destroy();

 private:
  FileIoInterface* file_io_iface_;
  FileRefInterface* file_ref_iface_;
  VarInterface* var_iface_;
  PP_Resource fileref_resource_;
  PP_Resource fileio_resource_;  // 0 if the file is a directory.

  friend class Html5Fs;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_HTML5FS_HTML5_FS_NODE_H_
