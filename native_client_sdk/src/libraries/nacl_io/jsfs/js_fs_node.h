// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_JSFS_JS_FS_NODE_H_
#define LIBRARIES_NACL_IO_JSFS_JS_FS_NODE_H_

#include <pthread.h>
#include <stdarg.h>

#include <map>

#include <ppapi/c/pp_var.h>

#include "nacl_io/jsfs/js_fs.h"
#include "nacl_io/node.h"

namespace nacl_io {

class JsFsNode : public Node {
 public:
  typedef JsFs::RequestId RequestId;

  JsFsNode(const JsFsNode&) = delete;
  JsFsNode& operator=(const JsFsNode&) = delete;

 protected:
  JsFsNode(Filesystem* filesystem, int32_t fd);
  virtual void Destroy();

 public:
  virtual bool CanOpen(int open_flags);
  virtual Error GetStat(struct stat* stat);
  virtual Error GetSize(off_t* out_size);

  virtual Error FSync();
  virtual Error FTruncate(off_t length);
  virtual Error Read(const HandleAttr& attr,
                     void* buf,
                     size_t count,
                     int* out_bytes);
  virtual Error Write(const HandleAttr& attr,
                      const void* buf,
                      size_t count,
                      int* out_bytes);
  virtual Error GetDents(size_t offs,
                         struct dirent* pdir,
                         size_t count,
                         int* out_bytes);

  int32_t fd() { return fd_; }

 private:
  bool SendRequestAndWait(ScopedVar* out_response, const char* format, ...);
  int ScanVar(PP_Var var, const char* format, ...);

  JsFs* filesystem() { return static_cast<JsFs*>(filesystem_); }

  PepperInterface* ppapi_;
  VarArrayInterface* array_iface_;
  VarArrayBufferInterface* buffer_iface_;
  VarInterface* var_iface_;
  int32_t fd_;

  friend class JsFs;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_JSFS_JS_FS_NODE_H_
