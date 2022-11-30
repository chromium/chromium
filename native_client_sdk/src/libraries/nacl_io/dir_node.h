// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_DIR_NODE_H_
#define LIBRARIES_NACL_IO_DIR_NODE_H_

#include <map>
#include <string>
#include <vector>

#include "nacl_io/getdents_helper.h"
#include "nacl_io/node.h"

namespace nacl_io {

class DevFs;
class Html5Fs;
class HttpFs;
class MemFs;
class DirNode;

typedef sdk_util::ScopedRef<DirNode> ScopedDirNode;

class DirNode : public Node {
 protected:
  DirNode(Filesystem* fs, mode_t mode);
  virtual ~DirNode();

 public:
  typedef std::map<std::string, ScopedNode> NodeMap_t;

  virtual Error FTruncate(off_t size);
  virtual Error GetDents(size_t offs,
                         struct dirent* pdir,
                         size_t count,
                         int* out_bytes);
  virtual Error Read(const HandleAttr& attr,
                     void* buf,
                     size_t count,
                     int* out_bytes);
  virtual Error Write(const HandleAttr& attr,
                      const void* buf,
                      size_t count,
                      int* out_bytes);
  virtual Error Fchmod(mode_t mode);

  // Adds a finds or adds a directory entry as an INO, updating the refcount
  virtual Error AddChild(const std::string& name, const ScopedNode& node);
  virtual Error RemoveChild(const std::string& name);
  virtual Error FindChild(const std::string& name, ScopedNode* out_node);
  virtual int ChildCount();

 protected:
  void BuildCache_Locked();
  void ClearCache_Locked();

 private:
  GetDentsHelper cache_;
  NodeMap_t map_;
  bool cache_built_;

  friend class DevFs;
  friend class Html5Fs;
  friend class HttpFs;
  friend class MemFs;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_DIR_NODE_H_
