// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/dir_node.h"

#include <errno.h>
#include <string.h>

#include "nacl_io/log.h"
#include "nacl_io/osdirent.h"
#include "nacl_io/osinttypes.h"
#include "nacl_io/osstat.h"
#include "sdk_util/auto_lock.h"
#include "sdk_util/macros.h"

namespace nacl_io {

namespace {

// TODO(binji): For now, just use a dummy value for the parent ino.
const ino_t kParentDirIno = -1;
}

DirNode::DirNode(Filesystem* filesystem, mode_t mode)
    : Node(filesystem),
      cache_(stat_.st_ino, kParentDirIno),
      cache_built_(false) {
  SetType(S_IFDIR);
  SetMode(mode);
  UpdateTime(UPDATE_ATIME | UPDATE_MTIME | UPDATE_CTIME);
}

DirNode::~DirNode() {
  for (NodeMap_t::iterator it = map_.begin(); it != map_.end(); ++it) {
    it->second->Unlink();
  }
}

Error DirNode::Read(const HandleAttr& attr,
                    void* buf,
                    size_t count,
                    int* out_bytes) {
  *out_bytes = 0;
  LOG_TRACE("Can't read a directory.");
  return EISDIR;
}

Error DirNode::FTruncate(off_t size) {
  LOG_TRACE("Can't truncate a directory.");
  return EISDIR;
}

Error DirNode::Write(const HandleAttr& attr,
                     const void* buf,
                     size_t count,
                     int* out_bytes) {
  *out_bytes = 0;
  LOG_TRACE("Can't write to a directory.");
  return EISDIR;
}

Error DirNode::GetDents(size_t offs,
                        dirent* pdir,
                        size_t size,
                        int* out_bytes) {
  AUTO_LOCK(node_lock_);
  BuildCache_Locked();
  UpdateTime(UPDATE_ATIME);
  return cache_.GetDents(offs, pdir, size, out_bytes);
}

Error DirNode::Fchmod(mode_t mode) {
  AUTO_LOCK(node_lock_);
  SetMode(mode);
  UpdateTime(UPDATE_CTIME);
  return 0;
}

Error DirNode::AddChild(const std::string& name, const ScopedNode& node) {
  AUTO_LOCK(node_lock_);

  if (name.empty()) {
    LOG_ERROR("Can't add child with no name.");
    return ENOENT;
  }

  if (name.length() >= MEMBER_SIZE(dirent, d_name)) {
    LOG_ERROR("Child name is too long: %" PRIuS " >= %" PRIuS,
              name.length(),
              MEMBER_SIZE(dirent, d_name));
    return ENAMETOOLONG;
  }

  NodeMap_t::iterator it = map_.find(name);
  if (it != map_.end()) {
    LOG_TRACE("Can't add child \"%s\", it already exists.", name.c_str());
    return EEXIST;
  }

  UpdateTime(UPDATE_MTIME | UPDATE_CTIME);

  node->Link();
  map_[name] = node;
  ClearCache_Locked();
  return 0;
}

Error DirNode::RemoveChild(const std::string& name) {
  AUTO_LOCK(node_lock_);
  NodeMap_t::iterator it = map_.find(name);
  if (it != map_.end()) {
    UpdateTime(UPDATE_MTIME | UPDATE_CTIME);
    it->second->Unlink();
    map_.erase(it);
    ClearCache_Locked();
    return 0;
  }
  return ENOENT;
}

Error DirNode::FindChild(const std::string& name, ScopedNode* out_node) {
  out_node->reset(NULL);

  AUTO_LOCK(node_lock_);
  NodeMap_t::iterator it = map_.find(name);
  if (it == map_.end())
    return ENOENT;

  *out_node = it->second;
  return 0;
}

int DirNode::ChildCount() {
  AUTO_LOCK(node_lock_);
  return map_.size();
}

void DirNode::BuildCache_Locked() {
  if (cache_built_)
    return;

  for (NodeMap_t::iterator it = map_.begin(), end = map_.end(); it != end;
       ++it) {
    const std::string& name = it->first;
    ino_t ino = it->second->stat_.st_ino;
    cache_.AddDirent(ino, name.c_str(), name.length());
  }

  cache_built_ = true;
}

void DirNode::ClearCache_Locked() {
  cache_built_ = false;
  cache_.Reset();
}

}  // namespace nacl_io
