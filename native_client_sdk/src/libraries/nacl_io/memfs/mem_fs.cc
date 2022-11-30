// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/memfs/mem_fs.h"

#include <errno.h>
#include <fcntl.h>

#include <string>

#include "nacl_io/dir_node.h"
#include "nacl_io/filesystem.h"
#include "nacl_io/memfs/mem_fs_node.h"
#include "nacl_io/node.h"
#include "nacl_io/osstat.h"
#include "nacl_io/osunistd.h"
#include "nacl_io/path.h"
#include "sdk_util/auto_lock.h"
#include "sdk_util/ref_object.h"

namespace nacl_io {

MemFs::MemFs() : root_(NULL) {
}

Error MemFs::Init(const FsInitArgs& args) {
  Error error = Filesystem::Init(args);
  if (error)
    return error;

  root_.reset(new DirNode(this, S_IRALL | S_IWALL | S_IXALL));
  error = root_->Init(0);
  if (error) {
    root_.reset(NULL);
    return error;
  }
  return 0;
}

Error MemFs::FindNode(const Path& path, int type, ScopedNode* out_node) {
  out_node->reset(NULL);
  ScopedNode node = root_;

  // If there is no root there, we have an error.
  if (node == NULL)
    return ENOTDIR;

  // We are expecting an "absolute" path from this mount point.
  if (!path.IsAbsolute())
    return EINVAL;

  // Starting at the root, traverse the path parts.
  for (size_t index = 1; node && index < path.Size(); index++) {
    // If not a directory, then we have an error so return.
    if (!node->IsaDir())
      return ENOTDIR;

    // Find the child node
    Error error = node->FindChild(path.Part(index), &node);
    if (error)
      return error;
  }

  // If a directory is expected, but it's not a directory, then fail.
  if ((type & S_IFDIR) && !node->IsaDir())
    return ENOTDIR;

  // If a file is expected, but it's not a file, then fail.
  if ((type & S_IFREG) && node->IsaDir())
    return EISDIR;

  // We now have a valid object of the expected type, so return it.
  *out_node = node;
  return 0;
}

Error MemFs::OpenWithMode(const Path& path, int open_flags, mode_t mode,
                          ScopedNode* out_node) {
  out_node->reset(NULL);
  ScopedNode node;

  Error error = FindNode(path, 0, &node);
  if (error) {
    // If the node does not exist and we can't create it, fail
    if ((open_flags & O_CREAT) == 0)
      return ENOENT;

    // Now first find the parent directory to see if we can add it
    ScopedNode parent;
    error = FindNode(path.Parent(), S_IFDIR, &parent);
    if (error)
      return error;

    node.reset(new MemFsNode(this));
    error = node->Init(open_flags);
    if (error)
      return error;
    node->SetMode(mode);

    error = parent->AddChild(path.Basename(), node);
    if (error)
      return error;

  } else {
    // Opening an existing file.

    // If we were expected to create it exclusively, fail
    if (open_flags & O_EXCL)
      return EEXIST;

    if (open_flags & O_TRUNC)
      node->FTruncate(0);
  }

  *out_node = node;
  return 0;
}

Error MemFs::Mkdir(const Path& path, int mode) {
  // The root of the filesystem is already created by the filesystem
  if (path.Size() == 1)
    return EEXIST;

  ScopedNode parent;
  int error = FindNode(path.Parent(), S_IFDIR, &parent);
  if (error)
    return error;

  ScopedNode node;
  error = parent->FindChild(path.Basename(), &node);
  if (!error)
    return EEXIST;

  if (error != ENOENT)
    return error;

  // Allocate a node, with a RefCount of 1.  If added to the parent
  // it will get ref counted again.  In either case, release the
  // recount we have on exit.
  node.reset(new DirNode(this, mode));
  error = node->Init(0);
  if (error)
    return error;

  return parent->AddChild(path.Basename(), node);
}

Error MemFs::Unlink(const Path& path) {
  return RemoveInternal(path, REMOVE_FILE);
}

Error MemFs::Rmdir(const Path& path) {
  return RemoveInternal(path, REMOVE_DIR);
}

Error MemFs::Remove(const Path& path) {
  return RemoveInternal(path, REMOVE_ALL);
}

Error MemFs::Rename(const Path& src_path, const Path& target_path) {
  ScopedNode src_node;
  ScopedNode src_parent;
  ScopedNode target_node;
  ScopedNode target_parent;
  int error = FindNode(src_path, 0, &src_node);
  if (error)
    return error;

  // The source must exist
  error = FindNode(src_path.Parent(), S_IFDIR, &src_parent);
  if (error)
    return error;

  // The parent of the target must exist
  error = FindNode(target_path.Parent(), 0, &target_parent);
  if (error)
    return error;

  std::string target_name = target_path.Basename();

  // The target itself need not exist but if it does there are
  // certain restrictions
  error = FindNode(target_path, 0, &target_node);
  bool replacing_target = error == 0;
  if (replacing_target) {
    if (target_node->IsaDir()) {
      // If the target is a direcotry it must be empty
      if (target_node->ChildCount()) {
        return ENOTEMPTY;
      }

      if (src_node->IsaDir()) {
        // Replacing an existing directory.
        RemoveInternal(target_path, REMOVE_ALL);
      } else {
        // Renaming into an existing directory.
        target_name = src_path.Basename();
        target_parent = target_node;
      }
    } else {
      if (src_node->IsaDir())
        // Can't replace a file with a direcotory
        return EISDIR;

      // Replacing an existing file.
      target_parent->RemoveChild(target_path.Basename());
    }
  }

  // Perform that actual rename. Simply re-parent the original source node
  // onto its new parent node.
  error = src_parent->RemoveChild(src_path.Basename());
  if (error)
    return error;

  error = target_parent->AddChild(target_name, src_node);
  if (error) {
    // Re-parent the old target_node if we failed to add the new one.
    if (replacing_target)
      target_parent->AddChild(target_path.Basename(), target_node);
    // Re-parent the src_node
    target_parent->AddChild(target_path.Basename(), src_node);
    return error;
  }

  return 0;
}

Error MemFs::RemoveInternal(const Path& path, int remove_type) {
  bool dir_only = remove_type == REMOVE_DIR;
  bool file_only = remove_type == REMOVE_FILE;
  bool remove_dir = (remove_type & REMOVE_DIR) != 0;

  if (dir_only) {
    // The root of the filesystem is already created by the filesystem
    if (path.Size() == 1)
      return EEXIST;
  }

  ScopedNode parent;
  int error = FindNode(path.Parent(), S_IFDIR, &parent);
  if (error)
    return error;

  // Verify we find a child which is a directory.
  ScopedNode child;
  error = parent->FindChild(path.Basename(), &child);
  if (error)
    return error;

  if (dir_only && !child->IsaDir())
    return ENOTDIR;

  if (file_only && child->IsaDir())
    return EISDIR;

  if (remove_dir && child->ChildCount() > 0)
    return ENOTEMPTY;

  return parent->RemoveChild(path.Basename());
}

}  // namespace nacl_io
